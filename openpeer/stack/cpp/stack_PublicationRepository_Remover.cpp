/*

 Copyright (c) 2014, Hookflash Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/stack/internal/stack_PublicationRepository.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerDeleteResult.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      ZS_DECLARE_USING_PTR(message::peer_common, MessageFactoryPeerCommon)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerDeleteResult)

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::Remover, Remover)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Remover
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::Remover::Remover(
                                              IMessageQueuePtr queue,
                                              PublicationRepositoryPtr outer,
                                              IPublicationRemoverDelegatePtr delegate,
                                              UsePublicationPtr publication
                                              ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mDelegate(IPublicationRemoverDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mPublication(publication),
        mSucceeded(false),
        mErrorCode(0)
      {
        ZS_LOG_DEBUG(log("created") + publication->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Remover::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationRepository::Remover::~Remover()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Remover => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::RemoverPtr PublicationRepository::Remover::create(
                                                                               IMessageQueuePtr queue,
                                                                               PublicationRepositoryPtr outer,
                                                                               IPublicationRemoverDelegatePtr delegate,
                                                                               UsePublicationPtr publication
                                                                               )
      {
        RemoverPtr pThis(new Remover(queue, outer, delegate, publication));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Remover::setMonitor(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        mMonitor = monitor;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Remover::notifyCompleted()
      {
        ZS_LOG_DEBUG(log("notified complete"))
        AutoRecursiveLock lock(*this);
        mSucceeded = true;
        cancel();
      }

      //-----------------------------------------------------------------------
      RemoverPtr PublicationRepository::Remover::convert(IPublicationRemoverPtr remover)
      {
        return dynamic_pointer_cast<Remover>(remover);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Remover => IPublicationRemover
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Remover::toDebug(IPublicationRemoverPtr remover)
      {
        if (!remover) return ElementPtr();
        return Remover::convert(remover)->toDebug();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Remover::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);

        if (mMonitor) {
          mMonitor->cancel();
          mMonitor.reset();
        }

        RemoverPtr pThis = mThisWeak.lock();

        if (pThis) {
          if (mDelegate) {
            try {
              mDelegate->onPublicationRemoverCompleted(pThis);
            } catch(IPublicationRemoverDelegateProxy::Exceptions::DelegateGone &) {
            }
          }
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Remover::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Remover::wasSuccessful(
                                                         WORD *outErrorResult,
                                                         String *outReason
                                                         ) const
      {
        AutoRecursiveLock lock(*this);
        if (outErrorResult) *outErrorResult = mErrorCode;
        if (outReason) *outReason = mErrorReason;
        return mSucceeded;
      }

      //-----------------------------------------------------------------------
      IPublicationPtr PublicationRepository::Remover::getPublication() const
      {
        AutoRecursiveLock lock(*this);
        return Publication::convert(mPublication);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Remover => IMessageMonitorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      bool PublicationRepository::Remover::handleMessageMonitorMessageReceived(
                                                                               IMessageMonitorPtr monitor,
                                                                               message::MessagePtr message
                                                                               )
      {
        AutoRecursiveLock lock(*this);
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("received result but already cancelled"))
          return false;
        }
        if (mMonitor != monitor) {
          ZS_LOG_WARNING(Detail, log("received result but monitor does not match"))
          return false;
        }

        if (message->messageType() != message::Message::MessageType_Result) {
          ZS_LOG_WARNING(Detail, log("expected result but received something else"))
          return false;
        }

        if ((MessageFactoryPeerCommon::Methods)message->method() != MessageFactoryPeerCommon::Method_PeerDelete) {
          ZS_LOG_WARNING(Detail, log("expected peer delete result but received something else"))
          return false;
        }

        message::MessageResultPtr result = message::MessageResult::convert(message);
        if (result->hasError()) {
          ZS_LOG_WARNING(Detail, log("received error in result"))
          mSucceeded = false;
          mErrorCode = result->errorCode();
          mErrorReason = result->errorReason();
          return true;
        }

        PeerDeleteResultPtr deleteResult = PeerDeleteResult::convert(result);
        mSucceeded = true;

        PublicationRepositoryPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyRemoved(mThisWeak.lock());
        }

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Remover::onMessageMonitorTimedOut(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mMonitor) return;
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Remover => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::Remover::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::Remover");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Remover::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::Remover");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePublication::toDebug(mPublication));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mMonitor));
        IHelper::debugAppend(resultEl, "succeeded", mSucceeded);
        IHelper::debugAppend(resultEl, "error code", mErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mErrorReason);

        return resultEl;
      }

    }

  }
}
