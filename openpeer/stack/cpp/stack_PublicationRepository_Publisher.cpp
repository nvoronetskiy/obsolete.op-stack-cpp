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

#include <openpeer/stack/internal/stack_PublicationRepository_Publisher.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerPublishRequest.h>
#include <openpeer/stack/message/peer-common/PeerPublishResult.h>

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
      ZS_DECLARE_USING_PTR(message::peer_common, PeerPublishResult)

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::Publisher, Publisher)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Publisher
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::Publisher::Publisher(
                                                  IMessageQueuePtr queue,
                                                  PublicationRepositoryPtr outer,
                                                  IPublicationPublisherDelegatePtr delegate,
                                                  UsePublicationPtr publication
                                                  ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mDelegate(IPublicationPublisherDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mPublication(publication),
        mSucceeded(false),
        mErrorCode(0)
      {
        ZS_LOG_DEBUG(log("created new publisher") + publication->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Publisher::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationRepository::Publisher::~Publisher()
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
      #pragma mark PublicationRepository::Publisher => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PublisherPtr PublicationRepository::Publisher::create(
                                                                                   IMessageQueuePtr queue,
                                                                                   PublicationRepositoryPtr outer,
                                                                                   IPublicationPublisherDelegatePtr delegate,
                                                                                   UsePublicationPtr publication
                                                                                   )
      {
        PublisherPtr pThis(new Publisher(queue, outer, delegate, publication));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Publisher::setMonitor(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        mMonitor = monitor;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Publisher::notifyCompleted()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("publisher succeeded"))
        mSucceeded = true;
        cancel();
      }

      //-----------------------------------------------------------------------
      PublisherPtr PublicationRepository::Publisher::convert(IPublicationPublisherPtr publisher)
      {
        return dynamic_pointer_cast<Publisher>(publisher);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Publisher => IPublicationPublisher
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Publisher::toDebug(IPublicationPublisherPtr publisher)
      {
        if (!publisher) return ElementPtr();
        return Publisher::convert(publisher)->toDebug();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Publisher::cancel()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("cancel called"))

        if (mMonitor) {
          mMonitor->cancel();
          mMonitor.reset();
        }

        PublisherPtr pThis = mThisWeak.lock();

        if (pThis) {
          if (mDelegate) {
            try {
              mDelegate->onPublicationPublisherCompleted(mThisWeak.lock());
            } catch(IPublicationPublisherDelegateProxy::Exceptions::DelegateGone &) {
            }
          }

          PublicationRepositoryPtr outer = mOuter.lock();
          if (outer) {
            outer->notifyPublisherCancelled(pThis);
          }
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Publisher::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Publisher::wasSuccessful(
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
      IPublicationPtr PublicationRepository::Publisher::getPublication() const
      {
        AutoRecursiveLock lock(*this);
        return Publication::convert(mPublication);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Publisher => IMessageMonitorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      bool PublicationRepository::Publisher::handleMessageMonitorMessageReceived(
                                                                                 IMessageMonitorPtr monitor,
                                                                                 message::MessagePtr message
                                                                                 )
      {
        AutoRecursiveLock lock(*this);
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("received result but publisher is already cancelled"))
          return false;
        }
        if (mMonitor != monitor) {
          ZS_LOG_WARNING(Detail, log("received result but not for the correct monitor"))
          return false;
        }

        if (message->messageType() != message::Message::MessageType_Result) {
          ZS_LOG_WARNING(Detail, log("expected result but received something else"))
          return false;
        }

        if ((MessageFactoryPeerCommon::Methods)message->method() != MessageFactoryPeerCommon::Method_PeerPublish) {
          ZS_LOG_WARNING(Detail, log("expecting peer publish method but received something else"))
          return false;
        }

        message::MessageResultPtr result = message::MessageResult::convert(message);
        if (result->hasError()) {
          ZS_LOG_WARNING(Detail, log("publish failed"))
          mSucceeded = false;
          mErrorCode = result->errorCode();
          mErrorReason = result->errorReason();
          cancel();
          return true;
        }

        ZS_LOG_DEBUG(log("received publish result"))

        PeerPublishResultPtr publishResult = PeerPublishResult::convert(result);

        PeerPublishRequestPtr originalRequest = PeerPublishRequest::convert(monitor->getMonitoredMessage());

        // now published from the original base version to the current version
        // so the base is now the published version plus one...
        mPublication->setBaseVersion(originalRequest->publishedToVersion()+1);

        notifyCompleted();

        PublicationRepositoryPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyPublished(mThisWeak.lock());
        }
        return true;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Publisher::onMessageMonitorTimedOut(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mMonitor) {
          ZS_LOG_DEBUG(log("ignoring publish request time out since it doesn't match monitor"))
          return;
        }
        ZS_LOG_DEBUG(log("publish request timed out"))
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Publisher => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::Publisher::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::Publisher");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Publisher::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::Publisher");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePublication::toDebug(mPublication));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mMonitor));
        IHelper::debugAppend(resultEl, "succeeded", mSucceeded);
        IHelper::debugAppend(resultEl, "error code", mErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mErrorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      IMessageMonitorPtr PublicationRepository::Publisher::getMonitor() const
      {
        AutoRecursiveLock lock(*this);
        return mMonitor;
      }


    }
  }
}
