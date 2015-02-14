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

#include <openpeer/stack/internal/stack_LocationDatabases.h>

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationDatabase.h>
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/p2p-database/ListSubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/SubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/DataGetRequest.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/IMessageIncoming.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/Numeric.h>
#include <zsLib/XML.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)

    namespace internal
    {
      using zsLib::Numeric;

      ZS_DECLARE_TYPEDEF_PTR(stack::internal::IStackForInternal, UseStack)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)
      ZS_DECLARE_TYPEDEF_PTR(ILocationDatabases::DatabaseInfoList, DatabaseInfoList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesForLocationDatabase
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ILocationDatabasesForLocationDatabase::toDebug(ForLocationDatabasePtr object)
      {
        if (!object) return ElementPtr();
        return LocationDatabases::convert(object)->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesForLocationDatabasesManager
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ILocationDatabasesForLocationDatabasesManager::toDebug(ForLocationDatabasesManagerPtr object)
      {
        if (!object) return ElementPtr();
        return LocationDatabases::convert(object)->toDebug();
      }

      //-----------------------------------------------------------------------
      ILocationDatabasesForLocationDatabasesManager::ForLocationDatabasesManagerPtr ILocationDatabasesForLocationDatabasesManager::create(ILocationPtr location)
      {
        return LocationDatabases::create(location);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabases::LocationDatabases(ILocationPtr location) :
        SharedRecursiveLock(SharedRecursiveLock::create()),
        MessageQueueAssociator(UseStack::queueStack()),
        mLocation(Location::convert(location))
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!mLocation)

        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::init()
      {
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      LocationDatabases::~LocationDatabases()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))

        UseLocationDatabasesManager::notifyDestroyed(*this);
      }

      //-----------------------------------------------------------------------
      LocationDatabasesPtr LocationDatabases::convert(ILocationDatabasesPtr object)
      {
        ILocationDatabasesPtr original = ILocationDatabasesTearAway::original(object);
        return ZS_DYNAMIC_PTR_CAST(LocationDatabases, original);
      }

      //-----------------------------------------------------------------------
      LocationDatabasesPtr LocationDatabases::convert(ForLocationDatabasesManagerPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationDatabases, object);
      }

      //-----------------------------------------------------------------------
      LocationDatabasesPtr LocationDatabases::convert(ForLocationDatabasePtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationDatabases, object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ILocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabases::toDebug(ILocationDatabasesPtr object)
      {
        if (!object) return ElementPtr();
        return LocationDatabases::convert(object)->toDebug();
      }

      //-----------------------------------------------------------------------
      ILocationDatabasesPtr LocationDatabases::open(
                                                    ILocationPtr inLocation,
                                                    ILocationDatabasesDelegatePtr inDelegate
                                                    )
      {
        LocationDatabasesPtr pThis = UseLocationDatabasesManager::getOrCreateForLocation(inLocation);
        if (!pThis) return pThis;

        auto subscription = pThis->subscribe(inDelegate);

        LocationDatabasesTearAwayDataPtr data(new LocationDatabasesTearAwayData);
        data->mSubscription = subscription;

        return ILocationDatabasesTearAway::create(pThis, data);
      }

      //-----------------------------------------------------------------------
      ILocationPtr LocationDatabases::getLocation() const
      {
        return Location::convert(mLocation);
      }

      //-----------------------------------------------------------------------
      DatabaseInfoListPtr LocationDatabases::getUpdates(
                                                        const String &inExistingVersion,
                                                        String &outNewVersion
                                                        ) const
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          outNewVersion = inExistingVersion;
        }

        VersionedData versionData;
        if (!validateVersionInfo(inExistingVersion, versionData)) {
          ZS_LOG_WARNING(Detail, log("version information failed to resolve") + ZS_PARAMIZE(inExistingVersion))
          return DatabaseInfoListPtr();
        }

        return getUpdates(String(), versionData, outNewVersion);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabasesManager
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabasesPtr LocationDatabases::create(ILocationPtr location)
      {
        LocationDatabasesPtr pThis(new LocationDatabases(location));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabase
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyDestroyed(UseLocationDatabase &database)
      {
        String databaseID = database.getDatabaseID();

        ZS_LOG_DEBUG(log("notify database destroyed") + ZS_PARAMIZE(databaseID))

        AutoRecursiveLock lock(*this);

        auto found = mLocationDatabases.find(database.getDatabaseID());
        if (found == mLocationDatabases.end()) {
          ZS_LOG_WARNING(Debug, log("notified of database that is already removed (probably okay)"))
          return;
        }

        mLocationDatabases.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => IPromiseSettledDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onPromiseSettled(PromisePtr promise)
      {
        ZS_LOG_TRACE(log("on promise settled") + ZS_PARAM("promise id", promise->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer"))

        AutoRecursiveLock lock(*this);

        if (timer == mRemoteSubscribeTimeout) {
          ZS_LOG_DEBUG(log("remote subscription has expired (thus must re-subscribe"))
          mRemoteSubscribeTimeout->cancel();
          mRemoteSubscribeTimeout.reset();

          if (mRemoteSubscribeListMonitor) {
            mRemoteSubscribeListMonitor->cancel();
            mRemoteSubscribeListMonitor.reset();
          }
          step();
          return;
        }

        if (timer == mIncomingListSubscriptionTimer) {
          ZS_LOG_TRACE(log("incoming list subscription timer fired"))

          auto now = zsLib::now();

          for (auto iter_doNotUse = mIncomingListSubscription.begin(); iter_doNotUse != mIncomingListSubscription.end();)
          {
            auto current = iter_doNotUse;
            ++iter_doNotUse;

            auto subscription = (*current).second;

            if (subscription->mExpires > now) continue;

            ZS_LOG_WARNING(Debug, log("incoming list subscription has now expired") + subscription->toDebug() + ZS_PARAMIZE(now))
            mIncomingListSubscription.erase(current);
          }
          step();
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => IBackOffTimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onBackOffTimerAttemptAgainNow(IBackOffTimerPtr timer)
      {
        ZS_LOG_DEBUG(log("attempt a retry again now") + IBackOffTimer::toDebug(timer))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onBackOffTimerAttemptTimeout(IBackOffTimerPtr timer)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onBackOffTimerAllAttemptsFailed(IBackOffTimerPtr timer)
      {
        ZS_LOG_WARNING(Detail, log("all retry attempts failed") + IBackOffTimer::toDebug(timer))

        AutoRecursiveLock lock(*this);
        if (timer != mRemoteSubscriptionBackOffTimer) {
          ZS_LOG_WARNING(Detail, log("ignoring back-off timer event on obsolete timer") + IBackOffTimer::toDebug(timer))
          return;
        }
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => IServiceLockboxSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onServiceLockboxSessionStateChanged(
                                                                  IServiceLockboxSessionPtr session,
                                                                  IServiceLockboxSession::SessionStates state
                                                                  )
      {
        ZS_LOG_DEBUG(log("lockbox state changed") + ZS_PARAM("lockbox", session->getID()) + ZS_PARAM("state", IServiceLockboxSession::toString(state)))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ILocationSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabases::onLocationSubscriptionShutdown(ILocationSubscriptionPtr subscription)
      {
        ZS_LOG_TRACE(log("notified location subscription shutdown (thus account must be closed)") + ILocationSubscription::toDebug(subscription))

        AutoRecursiveLock lock(*this);

        mLocationSubscription.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onLocationSubscriptionLocationConnectionStateChanged(
                                                                                   ILocationSubscriptionPtr subscription,
                                                                                   ILocationPtr location,
                                                                                   LocationConnectionStates state
                                                                                   )
      {
        ZS_LOG_TRACE(log("notified location state changed") + ILocationSubscription::toDebug(subscription) + ILocation::toDebug(location) + ZS_PARAM("state", ILocation::toString(state)))

        AutoRecursiveLock lock(*this);
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onLocationSubscriptionMessageIncoming(
                                                                    ILocationSubscriptionPtr subscription,
                                                                    IMessageIncomingPtr message
                                                                    )
      {
        ZS_LOG_TRACE(log("notified location message") + ILocationSubscription::toDebug(subscription) + IMessageIncoming::toDebug(message))

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown (thus cannot handle incoming requests)"))
          cancel();
          return;
        }

        if (isLocal()) {
          {
            auto request = ListSubscribeRequest::convert(message->getMessage());
            if (request) {
              notifyIncoming(message, request);
              return;
            }
          }
        } else {
          {
            auto notify = ListSubscribeNotify::convert(message->getMessage());
            if (notify) {
              notifyIncoming(notify);
              return;
            }
          }
        }
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => IMessageMonitorResultDelegate<ListSubscribeResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool LocationDatabases::handleMessageMonitorResultReceived(
                                                                 IMessageMonitorPtr monitor,
                                                                 ListSubscribeResultPtr result
                                                                 )
      {
        if (monitor != mRemoteSubscribeListMonitor) {
          ZS_LOG_WARNING(Debug, log("received list subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        if (mRemoteSubscriptionBackOffTimer) {
          mRemoteSubscriptionBackOffTimer->cancel();
          mRemoteSubscriptionBackOffTimer.reset();
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::handleMessageMonitorErrorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      ListSubscribeResultPtr ignore,         // will always be NULL
                                                                      message::MessageResultPtr result
                                                                      )
      {
        if (monitor != mRemoteSubscribeListMonitor) {
          ZS_LOG_WARNING(Debug, log("received list subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("failed to subscribe to remote list") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))

        mRemoteSubscribeListMonitor.reset();

        if (IHTTP::HTTPStatusCode_Conflict == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("remote list version conflict detected (all remote databases must be purged)"))

          for (auto iter_doNotUse = mLocationDatabases.begin(); iter_doNotUse != mLocationDatabases.end();)
          {
            auto current = iter_doNotUse;
            ++iter_doNotUse;

            UseLocationDatabasePtr database = (*current).second.lock();

            if (database) {
              ZS_LOG_TRACE(log("notify open database of conflict") + database->toDebug())
              database->notifyConflict();
            }

            mLocationDatabases.erase(current);
          }

          mLocationDatabases.clear();

          UseLocationDatabasesManager::purgePeerLocationDatabases(Location::convert(mLocation), mMasterDatase, mPeerLocationRecord.mIndex);

          // everything about the current peer location has been flushed
          mPeerLocationRecord = PeerLocationRecord();

          // obtain the DB record information about this database location
          mMasterDatase->peerLocationTable()->createOrObtain(mLocation->getPeerURI(), mLocation->getLocationID(), mPeerLocationRecord);

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        if (!mRemoteSubscriptionBackOffTimer) {
          mRemoteSubscriptionBackOffTimer = IBackOffTimer::create<Seconds>(UseSettings::getString(OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_BACK_OFF_TIMER_IN_SECONDS), mThisWeak.lock());
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params LocationDatabases::slog(const char *message)
      {
        return Log::Params(message, "LocationDatabases");
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabases::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("LocationDatabases");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabases::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabases::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabases");

        UseServicesHelper::debugAppend(resultEl, "id", mID);

        UseServicesHelper::debugAppend(resultEl, "ready", mReady);
        UseServicesHelper::debugAppend(resultEl, "shutdown", mShutdown);

        UseServicesHelper::debugAppend(resultEl, "is local", mIsLocal);

        UseServicesHelper::debugAppend(resultEl, UseLocation::toDebug(mLocation));

        UseServicesHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());

        UseServicesHelper::debugAppend(resultEl, "master database", mMasterDatase ? mMasterDatase->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "lockbox subscription", (bool)mLockboxSubscription);
        UseServicesHelper::debugAppend(resultEl, "lockbox", mLockbox ? mLockbox->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "location databases", mLocationDatabases.size());
        return resultEl;
      }

      //-----------------------------------------------------------------------
      ILocationDatabasesSubscriptionPtr LocationDatabases::subscribe(ILocationDatabasesDelegatePtr originalDelegate)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("subscribe called") + ZS_PARAM("delegate", (bool)originalDelegate))

        if (!originalDelegate) return ILocationDatabasesSubscriptionPtr();

        ILocationDatabasesSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate);

        ILocationDatabasesDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          LocationDatabasesPtr pThis = mThisWeak.lock();
          if (isShutdown()) {
            delegate->onLocationDatabasesShutdown(pThis);
          } else {
            if (mLastChangeUpdateNotified.hasData()) {
              delegate->onLocationDatabasesChanged(pThis);
            }
          }
        }

        if (isShutdown()) {
          mSubscriptions.clear();
        }

        return subscription;
      }
      
      //-----------------------------------------------------------------------
      void LocationDatabases::cancel()
      {
        if (mShutdown) {
          ZS_LOG_TRACE(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("cancel called") + toDebug())

        mMasterDatase.reset();

        if (mLockboxSubscription) {
          mLockboxSubscription->cancel();
          mLockboxSubscription.reset();
        }
        mLockbox.reset();

        auto pThis = mThisWeak.lock();
        if (pThis) {
          mSubscriptions.delegate()->onLocationDatabasesShutdown(pThis);
        }

        mSubscriptions.clear();

        mShutdown = true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::step()
      {
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("step calling cancel"))
          return;
        }

        ZS_LOG_DEBUG(log("step") + toDebug())

        if (!stepPrepareMasterDatabase()) goto not_ready;
        if (!stepSubscribeLocationAll()) goto not_ready;
        if (!stepSubscribeLocationRemote()) goto not_ready;

        if (!stepRemoteSubscribeList()) goto not_ready;
        if (!stepRemoteResubscribeTimer()) goto not_ready;

        if (!stepLocalIncomingSubscriptionsTimer()) goto not_ready;
        if (!stepLocalIncomingSubscriptionsNotify()) goto not_ready;

        if (!stepChangeNotify()) goto not_ready;

        mReady = true;

      not_ready:
        {
        }
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepPrepareMasterDatabase()
      {
        if (mMasterDatase) {
          ZS_LOG_TRACE(log("already have a master database for location"))
          return true;
        }

        UseAccountPtr account = mLocation->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("account was not found for location"))
          cancel();
          return false;
        }

        UseLockboxPtr lockbox = ServiceLockboxSession::convert(account->getLockboxSession());
        if (!lockbox) {
          ZS_LOG_WARNING(Detail, log("lockbox session was not found for location"))
          cancel();
          return false;
        }

        IServiceLockboxSession::SessionStates state = lockbox->getState();
        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration:
          case IServiceLockboxSession::SessionState_Ready: {
            break;
          }
          case IServiceLockboxSession::SessionState_Shutdown: {
            ZS_LOG_WARNING(Detail, log("lockbox session has shutdown for location") + lockbox->toDebug())
            cancel();
            return false;
          }
        }

        auto lockboxInfo = lockbox->getLockboxInfo();

        if ((lockboxInfo.mAccountID.isEmpty()) ||
            (lockboxInfo.mDomain.isEmpty())) {
          ZS_LOG_TRACE(log("must wait for lockbox to be ready before continuing") + lockbox->toDebug() + lockboxInfo.toDebug())

          mLockboxSubscription = lockbox->subscribe(mThisWeak.lock());
          mLockbox = lockbox;
          return false;
        }

        String userHash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(lockboxInfo.mDomain + ":" + lockboxInfo.mAccountID));

        ZS_LOG_DEBUG(log("creating master database for location") + ZS_PARAMIZE(userHash))

        mMasterDatase = UseLocationDatabasesManager::getOrCreateForUserHash(Location::convert(mLocation), userHash);

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("unable to create master database") + ZS_PARAMIZE(userHash) + lockboxInfo.toDebug())
          cancel();
          return false;
        }

        if (mLockboxSubscription) {
          mLockboxSubscription->cancel();
          mLockboxSubscription.reset();
        }
        mLockbox.reset();

        ILocationPtr local = ILocation::getForLocal(mLocation->getAccount());

        if (local->getID() == mLocation->getID()) {
          ZS_LOG_DEBUG(log("location is for local databases"))
          mIsLocal = true;
        }

        // obtain the DB record information about this database location
        mMasterDatase->peerLocationTable()->createOrObtain(mLocation->getPeerURI(), mLocation->getLocationID(), mPeerLocationRecord);

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("unable to open master peer location database"))
          cancel();
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepSubscribeLocationAll()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping subscribe all (not a local location)"))
          return true;
        }

        if (mLocationSubscription) {
          ZS_LOG_TRACE(log("already subscribed to all locations"))
          return true;
        }

        ZS_LOG_DEBUG(log("subscribing to all remote locations (needed to maintain information about all remote databases)"))
        mLocationSubscription = ILocationSubscription::subscribeAll(mLocation->getAccount(), mThisWeak.lock());

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepSubscribeLocationRemote()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping subscribe remote (is a local location)"))
          return true;
        }

        if (mLocationSubscription) {
          ZS_LOG_TRACE(log("already subscribed remote location"))
          return true;
        }

        ZS_LOG_DEBUG(log("subscribing to remote location (only need to subscribe to single remote location)"))
        mLocationSubscription = ILocationSubscription::subscribe(Location::convert(mLocation), mThisWeak.lock());

        return true;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabases::stepRemoteSubscribeList()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping remote subscribe list (is a local location)"))
          return true;
        }

        if (mRemoteSubscribeListMonitor) {
          if (!mRemoteSubscribeListMonitor->isComplete()) {
            ZS_LOG_TRACE(log("waiting for subscription to remote list to complete"))
            return false;
          }

          ZS_LOG_TRACE(log("subscription to remote list is ready"))
          return true;
        }

        if (!mLocation->isConnected()) {
          ZS_LOG_TRACE(log("waiting for location to be connected") + mLocation->toDebug())
          return false;
        }

        if (mRemoteSubscriptionBackOffTimer) {
          Time nextRetry = mRemoteSubscriptionBackOffTimer->getNextRetryAfterTime();
          if (nextRetry > zsLib::now()) {
            ZS_LOG_TRACE(log("waiting to resubscribe later"))
            return false;
          }
        }

        ZS_LOG_DEBUG(log("subscribing to remote database list"))

        Seconds expiresTimeout(UseSettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS));
        Time expires = zsLib::now() + expiresTimeout;

        // obtain an updated DB record information about this database location
        mMasterDatase->peerLocationTable()->createOrObtain(mLocation->getPeerURI(), mLocation->getLocationID(), mPeerLocationRecord);

        ListSubscribeRequestPtr request = ListSubscribeRequest::create();
        request->version(mPeerLocationRecord.mLastDownloadedVersion);
        request->expires(expires);

        mRemoteSubscribeListMonitor = IMessageMonitor::monitorAndSendToLocation(IMessageMonitorResultDelegate<ListSubscribeResult>::convert(mThisWeak.lock()), Location::convert(mLocation), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepRemoteResubscribeTimer()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping create remote re-subscribe list timer (is a local location)"))
          return true;
        }

        if (mRemoteSubscribeTimeout) {
          ZS_LOG_TRACE(log("already have remote subscribe list timeout"))
          return true;
        }

        Seconds expiresTimeout(UseSettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS));
        Time expires = zsLib::now() + expiresTimeout;

        ZS_LOG_DEBUG(log("creating re-subscribe timer") + ZS_PARAMIZE(expires) + ZS_PARAMIZE(expiresTimeout))

        if (expiresTimeout > Seconds(120)) {
          ZS_LOG_TRACE(log("setting re-subscribe timer to expire 60 seconds earlier") + ZS_PARAMIZE(expires) + ZS_PARAMIZE(expiresTimeout))
          expires -= Seconds(60);
        }

        mRemoteSubscribeTimeout = Timer::create(mThisWeak.lock(), expires);
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepLocalIncomingSubscriptionsTimer()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping create of incoming subscription timer (is NOT a local location)"))
          return true;
        }

        if (mIncomingListSubscriptionTimer) {
          ZS_LOG_TRACE(log("already have incoming list subscription timer"))
          return true;
        }

        ZS_LOG_DEBUG(log("creating incoming list subscription timer"))

        mIncomingListSubscriptionTimer = Timer::create(mThisWeak.lock(), Seconds(60));
        return true;
      }

      //-----------------------------------------------------------------------
      DatabaseInfoListPtr LocationDatabases::getUpdates(
                                                        const String &peerURI,
                                                        VersionedData &ioVersionData,
                                                        String &outLastVersion
                                                        ) const
      {
        DatabaseInfoListPtr result(new DatabaseInfoList);

        switch (ioVersionData.mType) {
          case VersionedData::VersionType_FullList: {
            UseDatabase::DatabaseRecordListPtr batch;
            if (peerURI.hasData()) {
              batch = mMasterDatase->databaseTable()->getBatchByPeerLocationIndexForPeerURI(peerURI, mPeerLocationRecord.mIndex, ioVersionData.mAfterIndex);
            } else {
              batch = mMasterDatase->databaseTable()->getBatchByPeerLocationIndex(mPeerLocationRecord.mIndex, ioVersionData.mAfterIndex);
            }
            if (!batch) batch = UseDatabase::DatabaseRecordListPtr(new UseDatabase::DatabaseRecordList);

            ZS_LOG_TRACE(log("getting batch of databases to notify") + ZS_PARAM("size", batch->size()))

            for (auto batchIter = batch->begin(); batchIter != batch->end(); ++batchIter) {
              auto databaseRecord = (*batchIter);

              DatabaseInfo info;
              info.mDisposition = DatabaseInfo::Disposition_Add;
              info.mDatabaseID = databaseRecord.mDatabaseID;
              info.mCreated = databaseRecord.mCreated;
              info.mExpires = databaseRecord.mExpires;
              info.mMetaData = databaseRecord.mMetaData;
              info.mVersion = databaseRecord.mUpdateVersion;

              ZS_LOG_TRACE(log("found additional updated record") + info.toDebug())

              result->push_back(info);

              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(databaseRecord.mDatabaseID + ":" + mPeerLocationRecord.mUpdateVersion));
              outLastVersion = String(VersionedData::toString(VersionedData::VersionType_FullList)) + "-" + hash + "-" + string(databaseRecord.mIndex);
            }

            if (batch->size() < 1) {
              ZS_LOG_DEBUG(log("no additional database records found"))
              // nothing more to notify

              if (OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN != ioVersionData.mAfterIndex) {
                ZS_LOG_TRACE(log("switching to change list subscription type"))
                UseDatabase::DatabaseChangeRecord changeRecord;
                auto result = mMasterDatase->databaseChangeTable()->getLast(changeRecord);
                if (!result) {
                  ZS_LOG_WARNING(Detail, log("did not find any database change records (despite having database records)"))
                  return DatabaseInfoListPtr();
                }

                ioVersionData.mType = VersionedData::VersionType_ChangeList;
                ioVersionData.mAfterIndex = changeRecord.mIndex;

                String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(changeRecord.mDisposition) + ":" + string(changeRecord.mIndexPeerLocation) + ":" + changeRecord.mDatabaseID));
                outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);
              } else {
                outLastVersion = String();
              }
            }
            break;
          }
          case VersionedData::VersionType_ChangeList: {
            UseDatabase::DatabaseChangeRecordListPtr batch;
            if (peerURI.hasData()) {
              batch = mMasterDatase->databaseChangeTable()->getChangeBatchForPeerURI(peerURI, mPeerLocationRecord.mIndex, ioVersionData.mAfterIndex);
            } else {
              batch = mMasterDatase->databaseChangeTable()->getChangeBatch(mPeerLocationRecord.mIndex, ioVersionData.mAfterIndex);
            }
            if (!batch) batch = UseDatabase::DatabaseChangeRecordListPtr(new UseDatabase::DatabaseChangeRecordList);

            ZS_LOG_TRACE(log("getting batch of database changes to notify") + ZS_PARAM("size", batch->size()))

            for (auto batchIter = batch->begin(); batchIter != batch->end(); ++batchIter) {
              auto changeRecord = (*batchIter);

              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(changeRecord.mDisposition) + ":" + string(changeRecord.mIndexPeerLocation) + ":" + changeRecord.mDatabaseID));
              outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);

              DatabaseInfo info;
              info.mDisposition = toDisposition(changeRecord.mDisposition);
              info.mDatabaseID = changeRecord.mDatabaseID;

              if (DatabaseInfo::Disposition_Remove != info.mDisposition) {
                UseDatabase::DatabaseRecord databaseRecord;
                auto result = mMasterDatase->databaseTable()->getByDatabaseID(changeRecord.mDatabaseID, databaseRecord);
                if (result) {
                  if (DatabaseInfo::Disposition_Update == info.mDisposition) {
                  } else {
                    info.mMetaData = databaseRecord.mMetaData;
                    info.mCreated = databaseRecord.mCreated;
                  }
                  info.mExpires = databaseRecord.mExpires;
                  info.mVersion = databaseRecord.mUpdateVersion;
                } else {
                  ZS_LOG_WARNING(Detail, log("did not find any related database record (thus must have been removed later)"))
                  info.mDisposition = DatabaseInfo::Disposition_Remove;
                }
              }

              ZS_LOG_TRACE(log("found additional updated record") + info.toDebug())
              result->push_back(info);
            }
            break;
          }
        }

        ZS_LOG_TRACE(log("found changes") + ZS_PARAMIZE(peerURI) + ioVersionData.toDebug() + ZS_PARAMIZE(outLastVersion) + ZS_PARAM("size", result->size()))
        return result;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepLocalIncomingSubscriptionsNotify()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping notification for incoming list subscriptions (is NOT a local location)"))
          return true;
        }

        for (auto iter = mIncomingListSubscription.begin(); iter != mIncomingListSubscription.end(); ++iter) {
          auto subscription = (*iter).second;

          if (subscription->mPendingSendPromise) {
            if (!subscription->mPendingSendPromise->isSettled()) {
              ZS_LOG_TRACE(log("waiting for pending send promise to complete for location") + subscription->mLocation->toDebug() + ZS_PARAM("promise id", subscription->mPendingSendPromise->getID()))
              continue;
            }
            subscription->mPendingSendPromise.reset();
          }

          if (subscription->mNotified) continue;

          ZS_LOG_DEBUG(log("need to send out subscription notification"))

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();

          notify->before(subscription->mLastVersion);

          DatabaseInfoListPtr notifyList = getUpdates(subscription->mLocation->getPeerURI(), *subscription, subscription->mLastVersion);
          if (!notifyList) {
            ZS_LOG_WARNING(Detail, log("did not find any database change records (despite having database records)"))
            goto notify_failure;
          }

          if (notifyList->size() > 0) {
            notify->databases(notifyList);
          } else {
            subscription->mNotified = true;
          }

          notify->version(subscription->mLastVersion);

          if (notify->version() == notify->before()) {
            ZS_LOG_DEBUG(log("nothing more to notify for subscription") + subscription->toDebug())
            subscription->mNotified = true;
            continue;
          }

          ZS_LOG_DEBUG(log("sending list subscribe notifiction") + Message::toDebug(notify))

          subscription->mPendingSendPromise = subscription->mLocation->send(notify);
          subscription->mPendingSendPromise->then(mThisWeak.lock());

        notify_failure:
          {
            ZS_LOG_ERROR(Detail, log("subscription notification failure") + subscription->toDebug())
          }
        }

        return true;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabases::stepChangeNotify()
      {
        if (mPeerLocationRecord.mUpdateVersion == mLastChangeUpdateNotified) {
          ZS_LOG_TRACE(log("no change notification required"))
          return true;
        }

        ZS_LOG_DEBUG(log("notified of databases changed event") + ZS_PARAMIZE(mPeerLocationRecord.mUpdateVersion) + ZS_PARAMIZE(mLastChangeUpdateNotified))

        mSubscriptions.delegate()->onLocationDatabasesChanged(mThisWeak.lock());
        mLastChangeUpdateNotified = mPeerLocationRecord.mUpdateVersion;
        return true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(ListSubscribeNotifyPtr notify)
      {
        ZS_LOG_DEBUG(log("notified of incoming list subscription notification") + Message::toDebug(notify))

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("master database is not ready thus cannot accept list subscribe notification"))
          return;
        }

        if (notify->before() != mPeerLocationRecord.mLastDownloadedVersion) {
          ZS_LOG_WARNING(Detail, log("cannot accept list subscribe notification due to conflicting version") + ZS_PARAM("version", notify->before()) + ZS_PARAM("expecting", mPeerLocationRecord.mLastDownloadedVersion))
          return;
        }

        auto databases = notify->databases();
        if (!databases) {
          ZS_LOG_TRACE("nothing new notified")
          mMasterDatase->peerLocationTable()->notifyDownloaded(mPeerLocationRecord.mIndex, notify->version());
          return;
        }

        bool added = false;
        bool changed = false;

        for (auto iter = databases->begin(); iter != databases->end(); ++iter) {
          auto databaseInfo = (*iter);

          ZS_LOG_TRACE(log("notified about database") + databaseInfo.toDebug())

          UseLocationDatabasePtr database;
          auto found = mLocationDatabases.find(databaseInfo.mDatabaseID);
          if (found != mLocationDatabases.end()) {
            database = (*found).second.lock();
            if (!database) {
              mLocationDatabases.erase(found);
              found = mLocationDatabases.end();
            }
          }

          switch (databaseInfo.mDisposition) {
            case DatabaseInfo::Disposition_None:    break;
            case DatabaseInfo::Disposition_Add:
            case DatabaseInfo::Disposition_Update:  {
              UseDatabase::DatabaseRecord record;
              record.mIndexPeerLocation = mPeerLocationRecord.mIndex;
              record.mDatabaseID = databaseInfo.mDatabaseID;
              record.mMetaData = databaseInfo.mMetaData;
              record.mExpires = databaseInfo.mExpires;
              record.mUpdateVersion = databaseInfo.mVersion;

              UseDatabase::DatabaseChangeRecord changeRecord;
              mMasterDatase->databaseTable()->addOrUpdate(record, changeRecord);
              if (UseDatabase::DatabaseChangeRecord::Disposition_None != changeRecord.mDisposition) {
                ZS_LOG_TRACE(log("add or updated database record") + record.toDebug() + changeRecord.toDebug())

                mMasterDatase->databaseChangeTable()->insert(changeRecord);
                added = added || (UseDatabase::DatabaseChangeRecord::Disposition_Add == changeRecord.mDisposition);
                changed = changed || (UseDatabase::DatabaseChangeRecord::Disposition_Add != changeRecord.mDisposition);
              }

              if (database) {
                database->notifyUpdated();
              }
              break;
            }
            case DatabaseInfo::Disposition_Remove:  {
              UseDatabase::DatabaseRecord record;
              record.mIndexPeerLocation = mPeerLocationRecord.mIndex;
              record.mDatabaseID = databaseInfo.mDatabaseID;
              UseDatabase::DatabaseChangeRecord changeRecord;
              mMasterDatase->databaseTable()->remove(record, changeRecord);
              if (UseDatabase::DatabaseChangeRecord::Disposition_None != changeRecord.mDisposition) {
                ZS_LOG_DEBUG(log("removed database record") + record.toDebug() + changeRecord.toDebug())

                mMasterDatase->databaseChangeTable()->insert(changeRecord);
                changed = true;
              }

              if (database) {
                database->notifyRemoved();
                mLocationDatabases.erase(found);
              }
              break;
            }
          }
        }

        mMasterDatase->peerLocationTable()->notifyDownloaded(mPeerLocationRecord.mIndex, notify->version());

        if (changed) {
          mPeerLocationRecord.mUpdateVersion = mMasterDatase->peerLocationTable()->updateVersion(mPeerLocationRecord.mIndex);
          ZS_LOG_DEBUG(log("registered peer location databases has changed") + ZS_PARAMIZE(mPeerLocationRecord.mUpdateVersion))
        } else if (added) {
          ZS_LOG_DEBUG(log("registered peer location databases has additions") + ZS_PARAMIZE(mPeerLocationRecord.mUpdateVersion) + ZS_PARAMIZE(mLastChangeUpdateNotified))
          mLastChangeUpdateNotified = "added";  // by setting to added it will notify delegates (but doesn't actually cause a version update change)
        } else {
          ZS_LOG_DEBUG(log("no changes to database were found"))
        }
      }

      //-----------------------------------------------------------------------
      LocationDatabases::DatabaseInfo::Dispositions LocationDatabases::toDisposition(UseDatabase::DatabaseChangeRecord::Dispositions disposition)
      {
        switch (disposition) {
          case UseDatabase::DatabaseChangeRecord::Disposition_None:   break;
          case UseDatabase::DatabaseChangeRecord::Disposition_Add:    return DatabaseInfo::Disposition_Add;
          case UseDatabase::DatabaseChangeRecord::Disposition_Update: return DatabaseInfo::Disposition_Update;
          case UseDatabase::DatabaseChangeRecord::Disposition_Remove: return DatabaseInfo::Disposition_Remove;
        }
        return DatabaseInfo::Disposition_None;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::validateVersionInfo(
                                                  const String &version,
                                                  VersionedData &outVersionData
                                                  ) const
      {
        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(version, split, '-');

        if ((0 != split.size()) ||
            (3 != split.size())) {
          ZS_LOG_WARNING(Detail, log("version is not understood") + ZS_PARAMIZE(version))
          return false;
        }

        String type;
        String updateVersion;
        String versionNumberStr;
        if (3 == split.size()) {
          type = split[0];
          updateVersion = split[1];
          versionNumberStr = split[2];
        }

        if ((type.hasData()) ||
            (updateVersion.hasData()) ||
            (versionNumberStr.hasData())) {

          if ((type.isEmpty()) ||
              (updateVersion.isEmpty()) ||
              (versionNumberStr.isEmpty())) {
            ZS_LOG_WARNING(Detail, log("version is not understood") + ZS_PARAMIZE(version) + ZS_PARAMIZE(type) + ZS_PARAMIZE(updateVersion) + ZS_PARAMIZE(versionNumberStr))
            return false;
          }
        }

        if (VersionedData::toString(VersionedData::VersionType_FullList) == type) {
          outVersionData.mType = VersionedData::VersionType_FullList;
        } else if (VersionedData::toString(VersionedData::VersionType_ChangeList) == type) {
          outVersionData.mType = VersionedData::VersionType_ChangeList;
        } else if (String() != type) {
          ZS_LOG_WARNING(Detail, log("version type not understood") + ZS_PARAMIZE(version) + ZS_PARAMIZE(type))
          return false;
        }

        if (versionNumberStr.hasData()) {
          try {
            outVersionData.mAfterIndex = Numeric<decltype(outVersionData.mAfterIndex)>(versionNumberStr);
          } catch(Numeric<decltype(outVersionData.mAfterIndex)>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, log("failed to convert database version (thus must be conflict)") + ZS_PARAMIZE(versionNumberStr) + ZS_PARAMIZE(version))
            return false;
          }
        } else {
          outVersionData.mAfterIndex = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN;
        }

        switch (outVersionData.mType) {
          case VersionedData::VersionType_FullList: {
            UseDatabase::DatabaseRecord record;

            if (OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN != outVersionData.mAfterIndex) {
              auto result = mMasterDatase->databaseTable()->getByIndex(outVersionData.mAfterIndex, record);
              if (!result) {
                ZS_LOG_WARNING(Detail, log("database entry record was not found (thus must be conflict)") + outVersionData.toDebug())
                return false;
              }
            }

            if (updateVersion.hasData()) {
              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(record.mDatabaseID + ":" + mPeerLocationRecord.mUpdateVersion));

              if (hash != updateVersion) {
                ZS_LOG_WARNING(Detail, log("version does not match current version") + ZS_PARAMIZE(version) + record.toDebug() + mPeerLocationRecord.toDebug() + ZS_PARAMIZE(hash))
                return false;
              }
            }

            break;
          }
          case VersionedData::VersionType_ChangeList: {
            UseDatabase::DatabaseChangeRecord record;
            auto result = mMasterDatase->databaseChangeTable()->getByIndex(outVersionData.mAfterIndex, record);
            if (!result) {
              ZS_LOG_WARNING(Detail, log("database entry record was not found (thus must be conflict)"))
              return false;
            }

            String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(record.mDisposition) + ":" + string(record.mIndexPeerLocation) + ":" + record.mDatabaseID));
            if (hash != updateVersion) {
              ZS_LOG_WARNING(Detail, log("request version does not match current version") + ZS_PARAMIZE(version) + record.toDebug() + ZS_PARAMIZE(hash))
              return false;
            }
            break;
          }
        }

        ZS_LOG_TRACE(log("version information validated") + ZS_PARAMIZE(version) + outVersionData.toDebug())
        return true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(
                                             IMessageIncomingPtr message,
                                             ListSubscribeRequestPtr request
                                             )
      {
        ZS_LOG_DEBUG(log("notified of incoming list subscription request") + IMessageIncoming::toDebug(message) + Message::toDebug(request))

        IHTTP::HTTPStatusCodes errorCode = IHTTP::HTTPStatusCode_Conflict;
        String errorReason;

        IncomingListSubscriptionMap::iterator found = mIncomingListSubscription.end();

        {
          UseLocationPtr location = Location::convert(message->getLocation());
          if (!location) {
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            goto fail_incoming_subscribe;
          }

          IncomingListSubscriptionPtr subscription;
          found = mIncomingListSubscription.find(location->getID());
          if (found == mIncomingListSubscription.end()) {
            if ((Time() == request->expires()) ||
                (request->expires() < zsLib::now())) {
              // immediate expiry, nothing to do...
              ListSubscribeResultPtr result = ListSubscribeResult::create(request);
              message->sendResponse(result);
              return;
            }
            subscription = IncomingListSubscriptionPtr(new IncomingListSubscription);
            subscription->mLocation = location;

            mIncomingListSubscription[location->getID()] = subscription;
            found = mIncomingListSubscription.find(location->getID());
          } else {
            subscription = (*found).second;
          }

          subscription->mExpires = request->expires();
          subscription->mLastVersion = request->version();

          if (!mMasterDatase) {
            ZS_LOG_WARNING(Detail, log("master database is gone"))
            errorCode = IHTTP::HTTPStatusCode_Gone;
            errorReason = "Database is not currently available.";
            goto fail_incoming_subscribe;
          }

          if (!validateVersionInfo(request->version(), *subscription)) {
            ZS_LOG_WARNING(Detail, log("request version mismatch") + ZS_PARAM("version", request->version()))
            goto fail_incoming_subscribe;
          }

          ZS_LOG_DEBUG(log("accepting incoming list subscription") + subscription->toDebug())

          ListSubscribeResultPtr result = ListSubscribeResult::create(request);
          message->sendResponse(result);

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

      fail_incoming_subscribe:
        {
          if (found != mIncomingListSubscription.end()) {
            ZS_LOG_WARNING(Detail, log("removing incoming subscription"))
            mIncomingListSubscription.erase(found);
          }
          MessageResultPtr result = MessageResult::create(request);
          result->errorCode(errorCode);
          result->errorReason(errorReason.isEmpty() ? String(IHTTP::toString(errorCode)) : errorReason);
          message->sendResponse(result);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases::VersionedData
      #pragma mark

      //-----------------------------------------------------------------------
      const char *LocationDatabases::VersionedData::toString(VersionTypes type)
      {
        switch (type) {
          case VersionType_FullList:   return OPENPEER_STACK_P2P_DATABASE_TYPE_FULL_LIST_VERSION;
          case VersionType_ChangeList: return OPENPEER_STACK_P2P_DATABASE_TYPE_CHANGE_LIST_VERSION;
        }
        return "unknown";
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabases::VersionedData::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabases::VersionedData");

        UseServicesHelper::debugAppend(resultEl, "type", toString(mType));
        UseServicesHelper::debugAppend(resultEl, "after index", mAfterIndex);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases::IncomingListSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabases::IncomingListSubscription::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabases::IncomingListSubscription");

        UseServicesHelper::debugAppend(resultEl, "location", UseLocation::toDebug(mLocation));

        UseServicesHelper::debugAppend(resultEl, "type", toString(mType));
        UseServicesHelper::debugAppend(resultEl, "after index", mAfterIndex);

        UseServicesHelper::debugAppend(resultEl, "lst version", mLastVersion);

        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        UseServicesHelper::debugAppend(resultEl, "notified", mNotified);

        UseServicesHelper::debugAppend(resultEl, "pending send promise", mPendingSendPromise ? mPendingSendPromise->getID() : 0);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabasesFactory &ILocationDatabasesFactory::singleton()
      {
        return ILocationDatabasesFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ILocationDatabasesPtr ILocationDatabasesFactory::open(
                                                            ILocationPtr inLocation,
                                                            ILocationDatabasesDelegatePtr inDelegate
                                                            )
      {
        if (this) {}
        return LocationDatabases::open(inLocation, inDelegate);
      }

    }

    //-------------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabasesTypes::DatabaseInfo
    #pragma mark

    //-----------------------------------------------------------------------
    const char *ILocationDatabasesTypes::DatabaseInfo::toString(Dispositions disposition)
    {
      switch (disposition) {
        case Disposition_None:    return "none";
        case Disposition_Add:     return "add";
        case Disposition_Update:  return "update";
        case Disposition_Remove:  return "remove";
      }
      return "unknown";
    }

    //-----------------------------------------------------------------------
    ILocationDatabasesTypes::DatabaseInfo::Dispositions ILocationDatabasesTypes::DatabaseInfo::toDisposition(const char *inStr)
    {
      String str(inStr);

      static Dispositions dispositions[] = {
        Disposition_Add,
        Disposition_Update,
        Disposition_Remove,
        Disposition_None,
      };

      for (int index = 0; Disposition_None != dispositions[index]; ++index) {
        if (str != toString(dispositions[index])) continue;
        return dispositions[index];
      }
      return Disposition_None;
    }

    //-----------------------------------------------------------------------
    bool ILocationDatabasesTypes::DatabaseInfo::hasData() const
    {
      return ((Disposition_None != mDisposition) ||
              (mDatabaseID.hasData()) ||
              (mVersion.hasData()) ||
              ((bool)mMetaData) ||
              (Time() != mCreated) ||
              (Time() != mExpires));
    }

    //-----------------------------------------------------------------------
    ElementPtr ILocationDatabasesTypes::DatabaseInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("ILocationDatabases::::DatabaseInfo");

      UseServicesHelper::debugAppend(resultEl, "disposition", toString(mDisposition));
      UseServicesHelper::debugAppend(resultEl, "database id", mDatabaseID);
      UseServicesHelper::debugAppend(resultEl, "version", mVersion);
      UseServicesHelper::debugAppend(resultEl, "meta data", UseServicesHelper::toString(mMetaData));
      UseServicesHelper::debugAppend(resultEl, "created", mCreated);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

      return resultEl;
    }

    //-----------------------------------------------------------------------
    ILocationDatabasesTypes::DatabaseInfo ILocationDatabasesTypes::DatabaseInfo::create(ElementPtr elem)
    {
      DatabaseInfo info;

      if (!elem) return info;

      info.mDisposition = DatabaseInfo::toDisposition(elem->getAttributeValue("disposition"));
      info.mDatabaseID = UseMessageHelper::getAttributeID(elem);
      info.mVersion = UseMessageHelper::getAttribute(elem, "version");

      info.mMetaData = elem->findFirstChildElement("metaData");
      if (info.mMetaData) {
        info.mMetaData = info.mMetaData->clone()->toElement();
      }

      info.mCreated = UseServicesHelper::stringToTime(UseMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("created")));
      info.mExpires = UseServicesHelper::stringToTime(UseMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("expires")));

      return info;
    }

    //-----------------------------------------------------------------------
    ElementPtr ILocationDatabasesTypes::DatabaseInfo::createElement() const
    {
      ElementPtr databaseEl = UseMessageHelper::createElementWithTextID("database", mDatabaseID);

      if (Disposition_None != mDisposition) {
        UseMessageHelper::setAttributeWithText(databaseEl, "disposition", toString(mDisposition));
      }
      if (mVersion.hasData()) {
        UseMessageHelper::setAttributeWithText(databaseEl, "version", mVersion);
      }

      if (mMetaData) {
        if (mMetaData->getValue() != "metaData") {
          ElementPtr metaDataEl = Element::create("metaData");
          metaDataEl->adoptAsLastChild(mMetaData->clone());
          databaseEl->adoptAsLastChild(metaDataEl);
        } else {
          databaseEl->adoptAsLastChild(mMetaData->clone());
        }
      }

      if (Time() != mCreated) {
        databaseEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("created", UseServicesHelper::timeToString(mCreated)));
      }
      if (Time() != mExpires) {
        databaseEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
      }

      return databaseEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabases
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr ILocationDatabases::toDebug(ILocationDatabasesPtr object)
    {
      return internal::LocationDatabases::toDebug(object);
    }

    //-------------------------------------------------------------------------
    ILocationDatabasesPtr ILocationDatabases::open(
                                                   ILocationPtr inLocation,
                                                   ILocationDatabasesDelegatePtr inDelegate
                                                   )
    {
      return internal::ILocationDatabasesFactory::singleton().open(inLocation, inDelegate);
    }

  }
}
