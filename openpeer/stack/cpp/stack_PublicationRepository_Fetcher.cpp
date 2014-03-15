/*

 Copyright (c) 2013, SMB Phone Inc.
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

#include <openpeer/stack/internal/stack_PublicationRepository_Fetcher.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerGetResult.h>

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
      ZS_DECLARE_USING_PTR(message::peer_common, PeerGetResult)

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::Fetcher, Fetcher)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Fetcher
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::Fetcher::Fetcher(
                                              IMessageQueuePtr queue,
                                              PublicationRepositoryPtr outer,
                                              IPublicationFetcherDelegatePtr delegate,
                                              UsePublicationMetaDataPtr metaData
                                              ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mDelegate(IPublicationFetcherDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mPublicationMetaData(metaData),
        mSucceeded(false),
        mErrorCode(0)
      {
        ZS_LOG_DEBUG(log("created") + metaData->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationRepository::Fetcher::~Fetcher()
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
      #pragma mark PublicationRepository::Fetcher => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::FetcherPtr PublicationRepository::Fetcher::create(
                                                                               IMessageQueuePtr queue,
                                                                               PublicationRepositoryPtr outer,
                                                                               IPublicationFetcherDelegatePtr delegate,
                                                                               UsePublicationMetaDataPtr metaData
                                                                               )
      {
        FetcherPtr pThis(new Fetcher(queue, outer, delegate, metaData));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::setPublication(UsePublicationPtr publication)
      {
        AutoRecursiveLock lock(*this);
        mFetchedPublication = publication;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::setMonitor(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        mMonitor = monitor;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::notifyCompleted()
      {
        AutoRecursiveLock lock(*this);
        mSucceeded = true;
        cancel();
      }

      FetcherPtr PublicationRepository::Fetcher::convert(IPublicationFetcherPtr fetcher)
      {
        return dynamic_pointer_cast<Fetcher>(fetcher);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Fetcher => IPublicationFetcher
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Fetcher::toDebug(IPublicationFetcherPtr fetcher)
      {
        if (!fetcher) return ElementPtr();
        return Fetcher::convert(fetcher)->toDebug();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::cancel()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("cancel called"))

        if (mMonitor) {
          mMonitor->cancel();
          mMonitor.reset();
        }

        FetcherPtr pThis = mThisWeak.lock();

        if (pThis) {
          if (mDelegate) {
            try {
              mDelegate->onPublicationFetcherCompleted(mThisWeak.lock());
            } catch(IPublicationFetcherDelegateProxy::Exceptions::DelegateGone &) {
            }
          }

          PublicationRepositoryPtr outer = mOuter.lock();
          if (outer) {
            outer->notifyFetcherCancelled(pThis);
          }
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Fetcher::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::Fetcher::wasSuccessful(
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
      IPublicationPtr PublicationRepository::Fetcher::getFetchedPublication() const
      {
        AutoRecursiveLock lock(*this);
        return Publication::convert(mFetchedPublication);
      }

      //-----------------------------------------------------------------------
      IPublicationMetaDataPtr PublicationRepository::Fetcher::getPublicationMetaData() const
      {
        AutoRecursiveLock lock(*this);
        return mPublicationMetaData->toPublicationMetaData();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::Fetcher => IMessageMonitorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      bool PublicationRepository::Fetcher::handleMessageMonitorMessageReceived(
                                                                               IMessageMonitorPtr monitor,
                                                                               message::MessagePtr message
                                                                               )
      {
        AutoRecursiveLock lock(*this);
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("received result but fetcher is shutdown"))
          return false;
        }
        if (mMonitor != monitor) {
          ZS_LOG_WARNING(Detail, log("received result but monitor does not match"))
          return false;
        }

        if (message->messageType() != message::Message::MessageType_Result) {
          ZS_LOG_WARNING(Detail, log("expecting result but received something else"))
          return false;
        }

        if ((MessageFactoryPeerCommon::Methods)message->method() != MessageFactoryPeerCommon::Method_PeerGet) {
          ZS_LOG_WARNING(Detail, log("expecting peer get result but received some other method"))
          return false;
        }

        message::MessageResultPtr result = message::MessageResult::convert(message);
        if (result->hasError()) {
          ZS_LOG_WARNING(Detail, log("received a result but result had an error"))
          mSucceeded = false;
          mErrorCode = result->errorCode();
          mErrorReason = result->errorReason();
          cancel();
          return true;
        }

        PeerGetResultPtr getResult = PeerGetResult::convert(result);
        mFetchedPublication = Publication::convert(getResult->publication());

        mSucceeded = (bool)(mFetchedPublication);
        if (!mSucceeded) {
          ZS_LOG_WARNING(Detail, log("received a result but result had no document"))
          mErrorCode = 404;
          mErrorReason = "Not found";
          cancel();
          return true;
        }

        PublicationRepositoryPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyFetched(mThisWeak.lock());
        }

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::Fetcher::onMessageMonitorTimedOut(IMessageMonitorPtr monitor)
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
      #pragma mark PublicationRepository::Fetcher => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::Fetcher::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::Fetcher");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::Fetcher::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::Fetcher");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePublicationMetaData::toDebug(mPublicationMetaData));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mMonitor));
        IHelper::debugAppend(resultEl, "succeeded", mSucceeded);
        IHelper::debugAppend(resultEl, "error code", mErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mErrorReason);
        IHelper::debugAppend(resultEl, UsePublication::toDebug(mFetchedPublication));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      IMessageMonitorPtr PublicationRepository::Fetcher::getMonitor() const
      {
        AutoRecursiveLock lock(*this);
        return mMonitor;
      }

    }
  }
}
