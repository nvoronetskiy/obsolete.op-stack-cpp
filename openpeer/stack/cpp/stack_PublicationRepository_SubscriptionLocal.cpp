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

#include <openpeer/stack/internal/stack_PublicationRepository_SubscriptionLocal.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/RegEx.h>
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

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::SubscriptionLocal, SubscriptionLocal)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::SubscriptionLocal
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::SubscriptionLocal::SubscriptionLocal(
                                                                  IMessageQueuePtr queue,
                                                                  PublicationRepositoryPtr outer,
                                                                  IPublicationSubscriptionDelegatePtr delegate,
                                                                  const char *publicationPath,
                                                                  const SubscribeToRelationshipsMap &relationships
                                                                  ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mDelegate(IPublicationSubscriptionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mCurrentState(IPublicationSubscription::PublicationSubscriptionState_Pending)
      {
        UseAccountPtr account = outer->getAccount();
        LocationPtr localLocation;
        if (account) {
          localLocation = Location::convert(ILocation::getForLocal(Account::convert(account)));
        }

        mSubscriptionInfo = IPublicationMetaDataForPublicationRepository::create(0, 0, 0, localLocation, publicationPath, "", IPublicationMetaData::Encoding_JSON, relationships, localLocation);
        ZS_LOG_DEBUG(log("created") + mSubscriptionInfo->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::SubscriptionLocal::init()
      {
        AutoRecursiveLock lock(*this);
        setState(IPublicationSubscription::PublicationSubscriptionState_Pending);
      }

      //-----------------------------------------------------------------------
      PublicationRepository::SubscriptionLocal::~SubscriptionLocal()
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
      #pragma mark PublicationRepository::SubscriptionLocal => friend SubscriptionLocal
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::SubscriptionLocalPtr PublicationRepository::SubscriptionLocal::create(
                                                                                                   IMessageQueuePtr queue,
                                                                                                   PublicationRepositoryPtr outer,
                                                                                                   IPublicationSubscriptionDelegatePtr delegate,
                                                                                                   const char *publicationPath,
                                                                                                   const SubscribeToRelationshipsMap &relationships
                                                                                                   )
      {
        SubscriptionLocalPtr pThis(new SubscriptionLocal(queue, outer, delegate, publicationPath, relationships));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::SubscriptionLocal::notifyUpdated(UsePublicationPtr publication)
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("receive notification of updated document but location subscription is cancelled"))
          return;
        }

        ZS_LOG_TRACE(log("publication is updated") + publication->toDebug())

        String name = publication->getName();
        String regex = mSubscriptionInfo->getName();

        zsLib::RegEx e(regex);
        if (!e.hasMatch(name)) {
          ZS_LOG_TRACE(log("name does not match subscription regex") + ZS_PARAM("name", name) + ZS_PARAM("regex", regex))
          return;
        }

        PublicationRepositoryPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication update notification as publication respository is gone"))
          return;
        }

        UseAccountPtr account = outer->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication update notification as account is gone"))
          return;
        }

        LocationPtr localLocation = ILocationForPublicationRepository::getForLocal(Account::convert(account));

        if (!outer->canSubscribeToPublisher(
                                            publication->getCreatorLocation(),
                                            publication->getRelationships(),
                                            localLocation,
                                            mSubscriptionInfo->getRelationships())) {
          ZS_LOG_TRACE(log("publication/subscriber do not publish/subscribe to each other (thus ignoring notification)"))
          return;
        }

        ZS_LOG_DEBUG(log("notifying about publication update to local subscriber") + publication->toDebug())

        // valid to notify about this document...
        try {
          mDelegate->onPublicationSubscriptionPublicationUpdated(mThisWeak.lock(), publication->toPublicationMetaData());
        } catch(IPublicationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::SubscriptionLocal::notifyGone(UsePublicationPtr publication)
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("received notification that publication is gone but local subscription is already cancelled"))
          return;
        }

        ZS_LOG_TRACE(log("notified publication is gone") + publication->toDebug())

        String name = publication->getName();
        String regex = mSubscriptionInfo->getName();

        zsLib::RegEx e(regex);
        if (!e.hasMatch(name)) {
          ZS_LOG_TRACE(log("name does not match subscription regex") + ZS_PARAM("name", name) + ZS_PARAM("regex", regex))
          return;
        }

        PublicationRepositoryPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication gone notification as publication respository is gone"))
          return;
        }

        UseAccountPtr account = outer->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication gone notification as account is gone"))
          return;
        }

        LocationPtr localLocation = ILocationForPublicationRepository::getForLocal(Account::convert(account));

        if (!outer->canSubscribeToPublisher(
                                            publication->getCreatorLocation(),
                                            publication->getRelationships(),
                                            localLocation,
                                            mSubscriptionInfo->getRelationships())) {
          ZS_LOG_TRACE(log("publication/subscriber do not publish/subscribe to each other (thus ignoring notification)"))
          return;
        }

        ZS_LOG_DEBUG(log("notifying about publication gone to local subscriber") + publication->toDebug())

        // valid to notify about this document...
        try {
          mDelegate->onPublicationSubscriptionPublicationGone(mThisWeak.lock(), publication->toPublicationMetaData());
        } catch(IPublicationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
        }
      }

      //-----------------------------------------------------------------------
      SubscriptionLocalPtr PublicationRepository::SubscriptionLocal::convert(IPublicationSubscriptionPtr subscription)
      {
        return dynamic_pointer_cast<SubscriptionLocal>(subscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::SubscriptionLocal => IPublicationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::SubscriptionLocal::toDebug(SubscriptionLocalPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return subscription->toDebug();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::SubscriptionLocal::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))
        AutoRecursiveLock lock(*this);
        setState(IPublicationSubscription::PublicationSubscriptionState_Shutdown);
        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      IPublicationSubscription::PublicationSubscriptionStates PublicationRepository::SubscriptionLocal::getState() const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      IPublicationMetaDataPtr PublicationRepository::SubscriptionLocal::getSource() const
      {
        AutoRecursiveLock lock(*this);
        return mSubscriptionInfo->toPublicationMetaData();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::SubscriptionLocal => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::SubscriptionLocal::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::SubscriptionLocal");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::SubscriptionLocal::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::SubscriptionLocal");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePublicationMetaData::toDebug(mSubscriptionInfo));
        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::SubscriptionLocal::setState(PublicationSubscriptionStates state)
      {
        if (mCurrentState == state) return;

        ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("old state", IPublicationSubscription::toString(mCurrentState)) + ZS_PARAM("new state", IPublicationSubscription::toString(state)))

        mCurrentState = state;

        SubscriptionLocalPtr pThis = mThisWeak.lock();

        if (IPublicationSubscription::PublicationSubscriptionState_Shutdown == mCurrentState) {
          PublicationRepositoryPtr outer = mOuter.lock();
          if ((outer) &&
              (pThis)) {
            outer->notifyLocalSubscriptionShutdown(pThis);
          }
        }

        if (!mDelegate) {
          ZS_LOG_DEBUG(log("unable to notify subscriber of state change as delegate object is null"))
          return;
        }

        if (pThis) {
          try {
            mDelegate->onPublicationSubscriptionStateChanged(pThis, state);
          } catch(IPublicationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_DEBUG(log("unable to notify subscriber of state change as delegate is gone"))
          }
        }
      }

    }
  }
}
