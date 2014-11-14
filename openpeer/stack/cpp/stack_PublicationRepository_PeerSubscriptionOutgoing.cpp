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

#include <openpeer/stack/internal/stack_PublicationRepository_PeerSubscriptionOutgoing.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerSubscribeRequest.h>
#include <openpeer/stack/message/peer-common/PeerSubscribeResult.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

#include <regex>

#define OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS (60)

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
      ZS_DECLARE_USING_PTR(message::peer_common, PeerSubscribeResult)

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::PeerSubscriptionOutgoing, PeerSubscriptionOutgoing)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionOutgoing
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionOutgoing::PeerSubscriptionOutgoing(
                                                                                IMessageQueuePtr queue,
                                                                                PublicationRepositoryPtr outer,
                                                                                IPublicationSubscriptionDelegatePtr delegate,
                                                                                UsePublicationMetaDataPtr subscriptionInfo
                                                                                ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mDelegate(IPublicationSubscriptionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mCurrentState(PublicationSubscriptionState_Pending),
        mSubscriptionInfo(subscriptionInfo)
      {
        ZS_LOG_DEBUG(log("created") + mSubscriptionInfo->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionOutgoing::~PeerSubscriptionOutgoing()
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
      #pragma mark PublicationRepository::PeerSubscriptionOutgoing => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionOutgoingPtr PublicationRepository::PeerSubscriptionOutgoing::create(
                                                                                                                 IMessageQueuePtr queue,
                                                                                                                 PublicationRepositoryPtr outer,
                                                                                                                 IPublicationSubscriptionDelegatePtr delegate,
                                                                                                                 UsePublicationMetaDataPtr subscriptionInfo
                                                                                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!subscriptionInfo)

        PeerSubscriptionOutgoingPtr pThis(new PeerSubscriptionOutgoing(queue, outer, delegate, subscriptionInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::setMonitor(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        mMonitor = monitor;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::notifyUpdated(UsePublicationMetaDataPtr metaData)
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notification up an updated document after cancel called") + metaData->toDebug())
          return;
        }

        String name = metaData->getName();
        String regexStr = mSubscriptionInfo->getName();

        std::regex e(regexStr);
        if (!std::regex_match(name, e)) {
          ZS_LOG_TRACE(log("name does not match subscription regex") + ZS_PARAM("name", name) + ZS_PARAM("regex", regexStr))
          return;
        }

        const char *ignoreReason = NULL;
        if (0 != ILocationForPublicationRepository::locationCompare(mSubscriptionInfo->getPublishedLocation(), metaData->getPublishedLocation(), ignoreReason)) {
          ZS_LOG_TRACE(log("publication update/gone notification for a source other than where subscription was placed (thus ignoring)") + metaData->toDebug())
          return;
        }

        ZS_LOG_DEBUG(log("publication update/gone notification is being notified to outgoing subscription delegate") + metaData->toDebug())

        // this appears to be a match thus notify the subscriber...
        try {
          if (0 == metaData->getVersion()) {
            mDelegate->onPublicationSubscriptionPublicationGone(mThisWeak.lock(), metaData->toPublicationMetaData());
          } else {
            mDelegate->onPublicationSubscriptionPublicationUpdated(mThisWeak.lock(), metaData->toPublicationMetaData());
          }
        } catch(IPublicationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
        }
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionOutgoingPtr PublicationRepository::PeerSubscriptionOutgoing::convert(IPublicationSubscriptionPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(PeerSubscriptionOutgoing, subscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IPublicationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerSubscriptionOutgoing::toDebug(PeerSubscriptionOutgoingPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return subscription->toDebug();
      }

      //-----------------------------------------------------------------------
      IPublicationSubscription::PublicationSubscriptionStates PublicationRepository::PeerSubscriptionOutgoing::getState() const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      IPublicationMetaDataPtr PublicationRepository::PeerSubscriptionOutgoing::getSource() const
      {
        AutoRecursiveLock lock(*this);
        return mSubscriptionInfo->toPublicationMetaData();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IMessageMonitorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      bool PublicationRepository::PeerSubscriptionOutgoing::handleMessageMonitorMessageReceived(
                                                                                                IMessageMonitorPtr monitor,
                                                                                                message::MessagePtr message
                                                                                                )
      {
        AutoRecursiveLock lock(*this);

        if (monitor == mCancelMonitor) {
          ZS_LOG_DEBUG(log("cancel monitor completed"))
          mCancelMonitor->cancel();
          cancel();
          return true;
        }

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("received subscription notification reply but cancel already called"))
          return false;
        }
        if (mMonitor != monitor) {
          ZS_LOG_WARNING(Detail, log("received subscription notification reply but monitor does not match reply"))
          return false;
        }

        if (message->messageType() != message::Message::MessageType_Result) {
          ZS_LOG_WARNING(Detail, log("expecting to receive result but received something else"))
          return false;
        }

        if ((MessageFactoryPeerCommon::Methods)message->method() != MessageFactoryPeerCommon::Method_PeerSubscribe) {
          ZS_LOG_WARNING(Detail, log("expecting to receive peer subscribe result but received something else"))
          return false;
        }

        message::MessageResultPtr result = message::MessageResult::convert(message);
        if (result->hasError()) {
          ZS_LOG_WARNING(Detail, log("failed to place outgoing subscription"))
          mSucceeded = false;
          mErrorCode = result->errorCode();
          mErrorReason = result->errorReason();
          cancel();
          return true;
        }

        ZS_LOG_DEBUG(log("outgoing subscription succeeded"))

        PeerSubscribeResultPtr subscribeResult = PeerSubscribeResult::convert(result);
        mSucceeded = true;

        PublicationRepositoryPtr outer = mOuter.lock();
        if (outer) {
          outer->notifySubscribed(mThisWeak.lock());
        }

        setState(IPublicationSubscription::PublicationSubscriptionState_Established);
        if (mMonitor) {
          mMonitor->cancel();
          mMonitor.reset();
        }
        return true;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::onMessageMonitorTimedOut(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);
        if (monitor == mCancelMonitor)
        {
          ZS_LOG_DEBUG(log("cancel monitor timeout received"))
          cancel();
          return;
        }
        if (monitor != mMonitor) {
          ZS_LOG_WARNING(Detail, log("received timeout on subscription request but it wasn't for the subscription request sent"))
          return;
        }
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionOutgoing => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::PeerSubscriptionOutgoing::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::PeerSubscriptionOutgoing");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerSubscriptionOutgoing::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::PeerSubscriptionOutgoing");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        IHelper::debugAppend(resultEl, UsePublicationMetaData::toDebug(mSubscriptionInfo));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mMonitor));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mCancelMonitor));
        IHelper::debugAppend(resultEl, "succeeded", mSucceeded);
        IHelper::debugAppend(resultEl, "error code", mErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mErrorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::setState(PublicationSubscriptionStates state)
      {
        if (mCurrentState == state) return;

        ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("old state", IPublicationSubscription::toString(mCurrentState)) + ZS_PARAM("new state", IPublicationSubscription::toString(state)))

        mCurrentState = state;

        PeerSubscriptionOutgoingPtr pThis = mThisWeak.lock();

        if (IPublicationSubscription::PublicationSubscriptionState_Shutdown == mCurrentState) {
          PublicationRepositoryPtr outer = mOuter.lock();
          if ((outer) &&
              (pThis)) {
            outer->notifyPeerOutgoingSubscriptionShutdown(pThis);
          }
        }

        if (!mDelegate) return;

        if (pThis) {
          try {
            mDelegate->onPublicationSubscriptionStateChanged(pThis, state);
          } catch(IPublicationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          }
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionOutgoing::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        setState(IPublicationSubscription::PublicationSubscriptionState_ShuttingDown);

        PeerSubscriptionOutgoingPtr pThis = mThisWeak.lock();

        if (!mGracefulShutdownReference) mGracefulShutdownReference = pThis;

        if ((!mCancelMonitor) &&
            (pThis)) {

          PublicationRepositoryPtr outer = mOuter.lock();

          if (outer) {
            UseAccountPtr account = outer->getAccount();

            if (account) {
              ZS_LOG_DEBUG(log("sending request to cancel outgoing subscription"))

              PeerSubscribeRequestPtr request = PeerSubscribeRequest::create();
              request->domain(account->getDomain());

              IPublicationMetaData::SubscribeToRelationshipsMap empty;
              request->publicationMetaData(mSubscriptionInfo->toPublicationMetaData());

              mCancelMonitor = IMessageMonitor::monitorAndSendToLocation(pThis, mSubscriptionInfo->getPublishedLocation(), request, Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS));
              return;
            }
          }
        }

        if (pThis) {
          if (mCancelMonitor)  {
            if (!mCancelMonitor->isComplete()) {
              ZS_LOG_DEBUG(log("waiting for cancel monitor to complete"))
              return;
            }
          }
        }

        setState(IPublicationSubscription::PublicationSubscriptionState_Shutdown);
        mDelegate.reset();

        mGracefulShutdownReference.reset();
      }
    }

  }
}
