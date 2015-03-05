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
#include <openpeer/stack/internal/stack_LocationSubscription.h>
#include <openpeer/stack/internal/stack_LocationDatabase.h>
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/p2p-database/ListSubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/DataGetRequest.h>

#include <openpeer/stack/message/IMessageHelper.h>


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

        mIsLocal = ILocation::LocationType_Local == mLocation->getLocationType();

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

        cancel();

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
      ILocationDatabases::DatabasesStates LocationDatabases::getState() const
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("current state") + ZS_PARAM("state", toString(mState)))
        return mState;
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
          return DatabaseInfoListPtr(new DatabaseInfoList);
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
      LocationDatabasePtr LocationDatabases::getOrCreate(
                                                         const String &databaseID,
                                                         bool downloadAllEntries
                                                         )
      {
        ZS_LOG_TRACE(log("creating or obtaining remote database") + ZS_PARAMIZE(databaseID) + ZS_PARAMIZE(downloadAllEntries))

        AutoRecursiveLock lock(*this);

        auto found = mLocationDatabases.find(databaseID);
        if (found != mLocationDatabases.end()) {
          auto database = (*found).second.lock();

          if (database) {
            ZS_LOG_TRACE(log("found existing database") + ZS_PARAMIZE(databaseID))
            return LocationDatabase::convert(database);
          }

          mLocationDatabases.erase(found);
          found = mLocationDatabases.end();
        }

        UseLocationDatabasePtr database;
        if (isLocal()) {
          database = UseLocationDatabase::openLocal(mThisWeak.lock(), databaseID);
        } else {
          database = UseLocationDatabase::openRemote(mThisWeak.lock(), databaseID, downloadAllEntries);
        }

        if (!isShutdown()) {
          mLocationDatabases[databaseID] = database;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return LocationDatabase::convert(database);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabases::getOrCreate(
                                                         const String &databaseID,
                                                         ElementPtr inMetaData,
                                                         const PeerList &inPeerAccessList,
                                                         Time expires
                                                         )
      {
        ZS_LOG_TRACE(log("creating or obtaining local database") + ZS_PARAMIZE(databaseID) + ZS_PARAM("meta data", (bool)inMetaData) + ZS_PARAM("peer access list", inPeerAccessList.size()) + ZS_PARAMIZE(expires))

        AutoRecursiveLock lock(*this);

        auto found = mLocationDatabases.find(databaseID);
        if (found != mLocationDatabases.end()) {
          auto database = (*found).second.lock();

          if (database) {
            ZS_LOG_TRACE(log("found existing database") + ZS_PARAMIZE(databaseID))
            return LocationDatabase::convert(database);
          }

          mLocationDatabases.erase(found);
          found = mLocationDatabases.end();
        }

        UseLocationDatabasePtr database = UseLocationDatabase::createLocal(mThisWeak.lock(), databaseID, inMetaData, inPeerAccessList, expires);

        if (!isShutdown()) {
          mLocationDatabases[databaseID] = database;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return LocationDatabase::convert(database);
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::remove(const String &databaseID)
      {
        ZS_LOG_DEBUG(log("removing local database") + ZS_PARAMIZE(databaseID))

        ZS_THROW_INVALID_ASSUMPTION_IF(!isLocal())

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot remove database when shutdown"))
          return false;
        }

        if (!mMasterDatase) {
          step();
        }

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("cannot remove database when master database is not ready"))
          return false;
        }

        UseLocationDatabasePtr database;
        auto found = mLocationDatabases.find(databaseID);
        if (found != mLocationDatabases.end()) {
          database = (*found).second.lock();
          if (!database) {
            mLocationDatabases.erase(found);
            found = mLocationDatabases.end();
          }
        }

        UseDatabase::DatabaseRecord record;
        record.mIndexPeerLocation = mPeerLocationRecord.mIndex;
        record.mDatabaseID = databaseID;

        UseDatabase::DatabaseChangeRecord changeRecord;
        mMasterDatase->databaseTable()->remove(record, changeRecord);
        if (UseDatabase::DatabaseChangeRecord::Disposition_None == changeRecord.mDisposition) {
          ZS_LOG_WARNING(Detail, log("database was not found"))
          return false;
        }

        ZS_LOG_DEBUG(log("removed database record") + record.toDebug() + changeRecord.toDebug())

        mMasterDatase->databaseChangeTable()->insert(changeRecord);

        mMasterDatase->permissionTable()->flushAllForDatabase(mPeerLocationRecord.mIndex, record.mDatabaseID);

        mPeerLocationRecord.mUpdateVersion = mMasterDatase->peerLocationTable()->updateVersion(mPeerLocationRecord.mIndex);

        if (database) {
          database->notifyRemoved();
          mExpectingShutdownLocationDatabases[databaseID] = UseLocationDatabasePtr();
        } else {
          UseDatabase::deleteDatabase(mMasterDatase, mLocation->getPeerURI(), mLocation->getLocationID(), databaseID);
        }

        for (auto iter_doNotUse = mIncomingListSubscriptions.begin(); iter_doNotUse != mIncomingListSubscriptions.end(); ) {
          auto current = iter_doNotUse;
          ++iter_doNotUse;

          auto subscription = (*current).second;
          if (VersionedData::VersionType_ChangeList == subscription->mType) {
            ZS_LOG_TRACE(log("removal will be notified") + subscription->toDebug())
            subscription->mNotified = false;
            continue;
          }

          ZS_LOG_WARNING(Debug, log("must shutdown incoming subscription because it cannot understand this version") + subscription->toDebug())

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();
          notify->messageID(subscription->mSubscribeMessageID);
          notify->expires(Time());
          mLocation->send(notify);

          mIncomingListSubscriptions.erase(current);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyShutdown(UseLocationDatabase &database)
      {
        String databaseID = database.getDatabaseID();

        ZS_LOG_DEBUG(log("notify database shutdown") + ZS_PARAMIZE(databaseID))

        AutoRecursiveLock lock(*this);

        {
          auto found = mExpectingShutdownLocationDatabases.find(database.getDatabaseID());
          if (found != mExpectingShutdownLocationDatabases.end()) {
            ZS_LOG_DEBUG(log("notified of database expected to shutdown"))
            mLocationDatabases.erase(found);
          }
        }

        {
          auto found = mLocationDatabases.find(database.getDatabaseID());
          if (found == mLocationDatabases.end()) {
            ZS_LOG_WARNING(Debug, log("notified of database that is already removed (probably okay)"))
            return;
          }
          mLocationDatabases.erase(found);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyDestroyed(UseLocationDatabase &database)
      {
        ZS_LOG_DEBUG(log("notify database destroyed"))
        notifyShutdown(database);
      }

      //-----------------------------------------------------------------------
      PromisePtr LocationDatabases::notifyWhenReady()
      {
        AutoRecursiveLock lock(*this);
        PromisePtr promise = Promise::create(UseStack::queueDelegate());
        ZS_LOG_TRACE(log("notify when ready promise created") + ZS_PARAM("promise id", promise->getID()))

        if (isShutdown()) {
          promise->reject();
          return promise;
        }

        if (mExpectingShutdownLocationDatabases.size() > 0) {
          ZS_LOG_WARNING(Detail, log("cannot notify ready just yet because some locations must be shutdown first") + ZS_PARAMIZE(mExpectingShutdownLocationDatabases.size()))
        } else {
          if (isReady()) {
            promise->resolve();
            return promise;
          }
        }

        mPromiseWhenReadyList.push_back(promise);
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return promise;
      }

      //-----------------------------------------------------------------------
      LocationDatabases::UseDatabasePtr LocationDatabases::getMasterDatabase()
      {
        AutoRecursiveLock lock(*this);
        return  mMasterDatase;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::openRemoteDB(
                                           const String &databaseID,
                                           UseDatabase::DatabaseRecord &outRecord
                                           )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("opening existing database") + ZS_PARAMIZE(databaseID) + ZS_PARAM("local", isLocal()))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown (thus cannot open database)") + ZS_PARAMIZE(databaseID))
          return false;
        }

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("master database is not available (thus cannot open database)") + ZS_PARAMIZE(databaseID))
          return false;
        }

        if (!mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, databaseID, outRecord)) {
          ZS_LOG_WARNING(Detail, log("database is not found (thus cannot open database)") + ZS_PARAMIZE(databaseID))
          return false;
        }

        ZS_LOG_DEBUG(log("found existing database") + ZS_PARAMIZE(databaseID) + outRecord.toDebug())
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::openLocalDB(
                                          const String &databaseID,
                                          UseDatabase::DatabaseRecord &outRecord,
                                          PeerList &outPeerAccessList
                                          )
      {
        outPeerAccessList.clear();

        auto result = openRemoteDB(databaseID, outRecord);
        if (!result) {
          ZS_LOG_WARNING(Detail, log("could not open local database") + ZS_PARAMIZE(databaseID))
          return false;
        }

        UseDatabase::PeerURIList uris;
        mMasterDatase->permissionTable()->getByDatabaseID(mPeerLocationRecord.mIndex, databaseID, uris);

        IAccountPtr account = mLocation->getAccount();
        if (account) {
          for (auto iter = uris.begin(); iter != uris.end(); ++iter) {
            auto uri = (*iter);
            ZS_LOG_INSANE(log("found uri with permission") + ZS_PARAM("uri", uri))

            UsePeerPtr peer = UsePeer::create(Account::convert(account), uri);
            if (peer) {
              outPeerAccessList.push_back(Peer::convert(peer));
            }
          }
        }

        ZS_LOG_DEBUG(log("existing local database open") + ZS_PARAMIZE(databaseID) + outRecord.toDebug() + ZS_PARAM("permissions", outPeerAccessList.size()))
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabases::createLocalDB(
                                            const String &databaseID,
                                            ElementPtr inMetaData,
                                            const PeerList &inPeerAccessList,
                                            Time expires,
                                            UseDatabase::DatabaseRecord &outRecord
                                           )
      {
        AutoRecursiveLock lock(*this);

        if (!isLocal()) {
          ZS_LOG_WARNING(Detail, log("not a local database location (thus cannot open database)") + ZS_PARAMIZE(databaseID))
          return false;
        }

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown (thus cannot open database)") + ZS_PARAMIZE(databaseID))
          return false;
        }

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("master database is not available thus cannot open database") + ZS_PARAMIZE(databaseID))
          return false;
        }

        if (mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, databaseID, outRecord)) {
          ZS_LOG_WARNING(Detail, log("found existing local database (thus opening and not creating)") + ZS_PARAMIZE(databaseID) + outRecord.toDebug())
          return true;
        }

        outRecord.mDatabaseID = databaseID;
        outRecord.mIndexPeerLocation = mPeerLocationRecord.mIndex;
        outRecord.mMetaData = inMetaData;
        outRecord.mCreated = zsLib::now();
        outRecord.mExpires = expires;

        UseDatabase::DatabaseChangeRecord changeRecord;
        mMasterDatase->databaseTable()->addOrUpdate(outRecord, changeRecord);

        if (UseDatabase::DatabaseChangeRecord::Disposition_None == changeRecord.mDisposition) {
          ZS_LOG_WARNING(Detail, log("unable to create database") + ZS_PARAMIZE(databaseID) + outRecord.toDebug() + mPeerLocationRecord.toDebug())
          return false;
        }

        mMasterDatase->databaseChangeTable()->insert(changeRecord);

        UseDatabase::PeerURIList uris;
        for (auto iter = inPeerAccessList.begin(); iter != inPeerAccessList.end(); ++iter) {
          UsePeerPtr peer = Peer::convert(*iter);
          if (!peer) {
            ZS_LOG_WARNING(Detail, log("found null peer in list") + ZS_PARAMIZE(databaseID) + ZS_PARAM("peers", inPeerAccessList.size()))
            continue;
          }
          uris.push_back(peer->getPeerURI());
        }

        mMasterDatase->permissionTable()->insert(mPeerLocationRecord.mIndex, databaseID, uris);

        ZS_LOG_DEBUG(log("created new local database") + ZS_PARAMIZE(databaseID) + outRecord.toDebug())

        // cause any incoming subscriptions to be notified about new database
        for (auto iter = mIncomingListSubscriptions.begin(); iter != mIncomingListSubscriptions.end(); ++iter) {
          auto subscription = (*iter).second;

          ZS_LOG_TRACE(log("incoming subscription will need notification about new database") + subscription->toDebug());

          subscription->mNotified = false;
        }

        // cause local notification about new database
        mLastChangeUpdateNotified = "added";
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyDownloaded(
                                               const String &databaseID,
                                               const String &downloadedVersion
                                               )
      {
        ZS_LOG_TRACE(log("notify downloaded") + ZS_PARAMIZE(databaseID) + ZS_PARAMIZE(downloadedVersion))

        AutoRecursiveLock lock(*this);

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("cannot update download version for database (master database is gone)") + ZS_PARAMIZE(databaseID) + ZS_PARAMIZE(downloadedVersion))
          return;
        }

        UseDatabase::DatabaseRecord record;
        if (!mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, databaseID, record)) {
          ZS_LOG_WARNING(Detail, log("cannot update download version for database (database not found)") + ZS_PARAMIZE(databaseID) + ZS_PARAMIZE(downloadedVersion))
          return;
        }

        mMasterDatase->databaseTable()->notifyDownloaded(record.mIndex, downloadedVersion);
      }

      //-----------------------------------------------------------------------
      String LocationDatabases::updateVersion(const String &databaseID)
      {
        ZS_LOG_TRACE(log("update version") + ZS_PARAMIZE(databaseID))

        AutoRecursiveLock lock(*this);
        UseDatabase::DatabaseRecord record;
        if (!mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, databaseID, record)) {
          ZS_LOG_WARNING(Detail, log("cannot update version for database (database not found)") + ZS_PARAMIZE(databaseID))
          return String();
        }

        return mMasterDatase->databaseTable()->updateVersion(record.mIndex);
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::getDatabaseRecordUpdate(UseDatabase::DatabaseRecord &ioRecord) const
      {
        ZS_LOG_TRACE(log("get database record update") + ioRecord.toDebug())

        AutoRecursiveLock lock(*this);
        UseDatabase::DatabaseRecord record;
        if (!mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, ioRecord.mDatabaseID, record)) {
          ZS_LOG_WARNING(Detail, log("cannot update version for database (database not found)") + ioRecord.toDebug())
          return;
        }

        ZS_LOG_TRACE(log("updating database record") + ZS_PARAM("from", ioRecord.toDebug()) + ZS_PARAM("to", record.toDebug()))

        ioRecord.mIndex = record.mIndex;
        ioRecord.mCreated = record.mCreated;
        ioRecord.mExpires = record.mExpires;
        ioRecord.mIndexPeerLocation = mPeerLocationRecord.mIndex;
        ioRecord.mMetaData = record.mMetaData;
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

          if (mRemoteListSubscribeMonitor) {
            mRemoteListSubscribeMonitor->cancel();
            mRemoteListSubscribeMonitor.reset();
          }
          step();
          return;
        }

        if (timer == mIncomingListSubscriptionTimer) {
          ZS_LOG_TRACE(log("incoming list subscription timer fired"))

          auto now = zsLib::now();

          for (auto iter_doNotUse = mIncomingListSubscriptions.begin(); iter_doNotUse != mIncomingListSubscriptions.end();)
          {
            auto current = iter_doNotUse;
            ++iter_doNotUse;

            auto subscription = (*current).second;

            if (subscription->mExpires > now) continue;

            ZS_LOG_WARNING(Debug, log("incoming list subscription has now expired") + subscription->toDebug() + ZS_PARAMIZE(now))
            mIncomingListSubscriptions.erase(current);
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
                                                                  IServiceLockboxSessionPtr inSession,
                                                                  IServiceLockboxSession::SessionStates state
                                                                  )
      {
        UseLockboxPtr session = ServiceLockboxSession::convert(inSession);

        ZS_LOG_DEBUG(log("lockbox state changed") + ZS_PARAM("lockbox", session->getID()) + ZS_PARAM("state", IServiceLockboxSession::toString(state)))

        AutoRecursiveLock lock(*this);
        if (mLockbox) {
          if ((IServiceLockboxSession::SessionState_Shutdown == state) &&
              (session->getID() == mLockbox->getID())) {
            ZS_LOG_WARNING(Detail, log("lockbox is shutdown (thus databases needs to shutdown)"))
            cancel();
            return;
          }
        }
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
      void LocationDatabases::onLocationSubscriptionShutdown(ILocationSubscriptionPtr inSubscription)
      {
        UseLocationSubscriptionPtr subscription = LocationSubscription::convert(inSubscription);

        ZS_LOG_TRACE(log("notified location subscription shutdown (thus account must be closed)") + UseLocationSubscription::toDebug(subscription))

        AutoRecursiveLock lock(*this);

        mLocationSubscription.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onLocationSubscriptionLocationConnectionStateChanged(
                                                                                   ILocationSubscriptionPtr inSubscription,
                                                                                   ILocationPtr inLocation,
                                                                                   LocationConnectionStates state
                                                                                   )
      {
        UseLocationSubscriptionPtr subscription = LocationSubscription::convert(inSubscription);
        UseLocationPtr location = Location::convert(inLocation);

        ZS_LOG_TRACE(log("notified location state changed") + UseLocationSubscription::toDebug(subscription) + UseLocation::toDebug(location) + ZS_PARAM("state", ILocation::toString(state)))

        AutoRecursiveLock lock(*this);

        switch (state) {
          case ILocation::LocationConnectionState_Pending:
          case ILocation::LocationConnectionState_Connected: {
            break;
          }
          case ILocation::LocationConnectionState_Disconnecting:
          case ILocation::LocationConnectionState_Disconnected: {
            ZS_LOG_WARNING(Debug, log("location has disconnected (thus subscription is gone)"))
            if (isLocal()) {
              auto found = mIncomingListSubscriptions.find(location->getID());
              if (found != mIncomingListSubscriptions.end()) {
                auto subscription = (*found).second;
                mIncomingListSubscriptions.erase(found);
              }
            } else {
              if (mRemoteListSubscribeMonitor) {
                mRemoteListSubscribeMonitor->cancel();
                mRemoteListSubscribeMonitor.reset();
              }
            }
          }
        }

        if (!isLocal()) {
          step();
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::onLocationSubscriptionMessageIncoming(
                                                                    ILocationSubscriptionPtr subscription,
                                                                    IMessageIncomingPtr inMessage
                                                                    )
      {
        UseMessageIncomingPtr message = MessageIncoming::convert(inMessage);
        ZS_LOG_TRACE(log("notified location message") + ILocationSubscription::toDebug(subscription) + UseMessageIncoming::toDebug(message))

        {
          AutoRecursiveLock lock(*this);
          if (isShutdown()) {
            ZS_LOG_WARNING(Detail, log("already shutdown (thus cannot handle incoming requests)"))
            cancel();
            return;
          }
        }

        auto actualMessage = message->getMessage();

        if (isLocal()) {
          {
            auto request = DataGetRequest::convert(actualMessage);
            if (request) {
              notifyIncoming(message, request);
              return;
            }
          }
          {
            auto request = SubscribeRequest::convert(actualMessage);
            if (request) {
              notifyIncoming(message, request);
              return;
            }
          }
          {
            auto request = ListSubscribeRequest::convert(actualMessage);
            if (request) {
              notifyIncoming(message, request);
              return;
            }
          }
        } else {
          {
            auto notify = ListSubscribeNotify::convert(actualMessage);
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
        AutoRecursiveLock lock(*this);

        if (monitor != mRemoteListSubscribeMonitor) {
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
        AutoRecursiveLock lock(*this);

        if (monitor != mRemoteListSubscribeMonitor) {
          ZS_LOG_WARNING(Debug, log("received list subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("failed to subscribe to remote list") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))

        mRemoteListSubscribeMonitor.reset();

        if (IHTTP::HTTPStatusCode_Conflict == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("remote list version conflict detected (all remote databases must be purged)"))

          mDatabaseListConflict = true;

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


        UseServicesHelper::debugAppend(resultEl, "state", toString(mState));

        UseServicesHelper::debugAppend(resultEl, "is local", mIsLocal);

        UseServicesHelper::debugAppend(resultEl, UseLocation::toDebug(mLocation));
        UseServicesHelper::debugAppend(resultEl, "location subscription", mLocationSubscription ? mLocationSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());

        UseServicesHelper::debugAppend(resultEl, "master database", mMasterDatase ? mMasterDatase->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "lockbox subscription", (bool)mLockboxSubscription);
        UseServicesHelper::debugAppend(resultEl, "lockbox", mLockbox ? mLockbox->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "peer location record", mPeerLocationRecord.toDebug());

        UseServicesHelper::debugAppend(resultEl, "location databases", mLocationDatabases.size());
        UseServicesHelper::debugAppend(resultEl, "expecting shutdown locations", mExpectingShutdownLocationDatabases.size());

        UseServicesHelper::debugAppend(resultEl, "back-off timer", mRemoteSubscriptionBackOffTimer ? mRemoteSubscriptionBackOffTimer->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "remote list subscribe monitor", mRemoteListSubscribeMonitor ? mRemoteListSubscribeMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "remote subscribe timeout", mRemoteSubscribeTimeout ? mRemoteSubscribeTimeout->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "last changed update notified", mLastChangeUpdateNotified);
        UseServicesHelper::debugAppend(resultEl, "changed update notified", mChangeUpdateNotified);

        UseServicesHelper::debugAppend(resultEl, "incoming subscriptions", mIncomingListSubscriptions.size());
        UseServicesHelper::debugAppend(resultEl, "incoming subscription timer", mIncomingListSubscriptionTimer ? mIncomingListSubscriptionTimer->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "promise when ready list", mPromiseWhenReadyList.size());

        UseServicesHelper::debugAppend(resultEl, "database list conflict", mDatabaseListConflict);


        return resultEl;
      }

      //-----------------------------------------------------------------------
      ILocationDatabasesSubscriptionPtr LocationDatabases::subscribe(ILocationDatabasesDelegatePtr originalDelegate)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("subscribe called") + ZS_PARAM("delegate", (bool)originalDelegate))

        if (!originalDelegate) return ILocationDatabasesSubscriptionPtr();

        ILocationDatabasesSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate, UseStack::queueDelegate());

        ILocationDatabasesDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          LocationDatabasesPtr pThis = mThisWeak.lock();
          if (!isPending()) {
            delegate->onLocationDatabasesStateChanged(pThis, mState);
          }

          if (!isShutdown()) {
            if (mChangeUpdateNotified) {
              delegate->onLocationDatabasesUpdated(pThis);
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
        if (isShutdown()) {
          ZS_LOG_TRACE(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("cancel called") + toDebug())

        mMasterDatase.reset();

        if (mLocationSubscription) {
          mLocationSubscription->cancel();
          mLocationSubscription.reset();
        }

        if (mLockboxSubscription) {
          mLockboxSubscription->cancel();
          mLockboxSubscription.reset();
        }
        mLockbox.reset();

        {
          for (auto iter = mLocationDatabases.begin(); iter != mLocationDatabases.end(); ++iter) {
            auto database = (*iter).second.lock();
            if (database) {
              database->notifyShutdown();
            }
          }
          mLocationDatabases.clear();
        }

        mExpectingShutdownLocationDatabases.clear();

        if (mRemoteSubscriptionBackOffTimer) {
          mRemoteSubscriptionBackOffTimer->cancel();
          mRemoteSubscriptionBackOffTimer.reset();
        }
        if (mRemoteListSubscribeMonitor) {
          mRemoteListSubscribeMonitor->cancel();
          mRemoteListSubscribeMonitor.reset();
        }
        if  (mRemoteSubscribeTimeout) {
          mRemoteSubscribeTimeout->cancel();
          mRemoteSubscribeTimeout.reset();
        }

        mIncomingListSubscriptions.clear();
        if (mIncomingListSubscriptionTimer) {
          mIncomingListSubscriptionTimer->cancel();
          mIncomingListSubscriptionTimer.reset();
        }

        setState(DatabasesState_Shutdown);

        if (mPromiseWhenReadyList.size() > 0) {
          PromisePtr broadcast = Promise::broadcast(mPromiseWhenReadyList);
          broadcast->reject();
          mPromiseWhenReadyList.clear();
        }

        mSubscriptions.clear();
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

        {
          if ((mExpectingShutdownLocationDatabases.size() < 1) &&
              (mPromiseWhenReadyList.size() > 0)) {
            PromisePtr broadcast = Promise::broadcast(mPromiseWhenReadyList);
            broadcast->resolve();
            mPromiseWhenReadyList.clear();
          }
        }

        if (!stepSubscribeLocationAll()) goto not_ready;
        if (!stepSubscribeLocationRemote()) goto not_ready;

        if (!stepPurgeConflict()) goto not_ready;

        if (!stepRemoteSubscribeList()) goto not_ready;
        if (!stepRemoteResubscribeTimer()) goto not_ready;

        if (!stepLocalIncomingSubscriptionsTimer()) goto not_ready;
        if (!stepLocalIncomingSubscriptionsNotify()) goto not_ready;

        setState(DatabasesState_Ready);

        if (!stepChangeNotify()) goto not_ready;

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

        UseLockboxPtr lockbox = mLockbox;

        if (!lockbox) {
          UseAccountPtr account = mLocation->getAccount();
          if (!account) {
            ZS_LOG_WARNING(Detail, log("account was not found for location"))
            cancel();
            return false;
          }

          lockbox = ServiceLockboxSession::convert(account->getLockboxSession());
          if (!lockbox) {
            ZS_LOG_WARNING(Detail, log("lockbox session was not found for location"))
            cancel();
            return false;
          }
        }

        IServiceLockboxSession::SessionStates state = lockbox->getState();
        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingWithLockboxAccessReady:
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

          if (!mLockboxSubscription) {
            mLockboxSubscription = lockbox->subscribe(mThisWeak.lock());
            mLockbox = lockbox;
          }
          return false;
        }

        String prehash = lockboxInfo.mDomain + ":" + lockboxInfo.mAccountID;
        String userHash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));

        ZS_LOG_DEBUG(log("creating master database for location") + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(userHash))

        mMasterDatase = UseLocationDatabasesManager::getOrCreateForUserHash(Location::convert(mLocation), userHash);

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("unable to create master database") + ZS_PARAMIZE(userHash) + lockboxInfo.toDebug())
          cancel();
          return false;
        }

        // obtain the DB record information about this database location
        mMasterDatase->peerLocationTable()->createOrObtain(mLocation->getPeerURI(), mLocation->getLocationID(), mPeerLocationRecord);

        mLastChangeUpdateNotified = mPeerLocationRecord.mUpdateVersion;

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
        mLocationSubscription = UseLocationSubscription::subscribeAll(mLocation->getAccount(), mThisWeak.lock());

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
        mLocationSubscription = UseLocationSubscription::subscribe(Location::convert(mLocation), mThisWeak.lock());

        return true;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabases::stepPurgeConflict()
      {
        if (!mDatabaseListConflict) {
          ZS_LOG_TRACE(log("not purging any conflict"))
          return true;
        }

        for (auto iter_doNotUse = mLocationDatabases.begin(); iter_doNotUse != mLocationDatabases.end();)
        {
          auto current = iter_doNotUse;
          ++iter_doNotUse;

          auto databaseID = (*current).first;
          UseLocationDatabasePtr database = (*current).second.lock();

          if (database) {
            ZS_LOG_WARNING(Detail, log("notify open database of conflict") + ZS_PARAM("database id", database->getDatabaseID()))
            database->notifyConflict();
            mExpectingShutdownLocationDatabases[databaseID] = UseLocationDatabasePtr();
          }
        }

        if (mLocationDatabases.size() > 0) {
          ZS_LOG_TRACE(log("still waiting on some database locations to shutdown from being purged"))
          return false;
        }

        ZS_LOG_WARNING(Debug, log("purging all exisitng database locations"))

        UseLocationDatabasesManager::purgePeerLocationDatabases(Location::convert(mLocation), mMasterDatase, mPeerLocationRecord.mIndex);

        // everything about the current peer location has been flushed
        mPeerLocationRecord = PeerLocationRecord();

        // obtain the DB record information about this database location
        mMasterDatase->peerLocationTable()->createOrObtain(mLocation->getPeerURI(), mLocation->getLocationID(), mPeerLocationRecord);

        mDatabaseListConflict = false;

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabases::stepRemoteSubscribeList()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping remote subscribe list (is a local location)"))
          return true;
        }

        if (mRemoteListSubscribeMonitor) {
          if (!mRemoteListSubscribeMonitor->isComplete()) {
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

        mRemoteListSubscribeMonitor = IMessageMonitor::monitorAndSendToLocation(IMessageMonitorResultDelegate<ListSubscribeResult>::convert(mThisWeak.lock()), Location::convert(mLocation), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS)));

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
      bool LocationDatabases::stepLocalIncomingSubscriptionsNotify()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping notification for incoming list subscriptions (is NOT a local location)"))
          return true;
        }

        for (auto iter = mIncomingListSubscriptions.begin(); iter != mIncomingListSubscriptions.end(); ++iter) {
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

          notify->messageID(subscription->mSubscribeMessageID);
          notify->before(subscription->mLastVersion);
          notify->expires(subscription->mExpires);

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
          continue;

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

        mChangeUpdateNotified = true;

        mSubscriptions.delegate()->onLocationDatabasesUpdated(mThisWeak.lock());
        mLastChangeUpdateNotified = mPeerLocationRecord.mUpdateVersion;
        return true;
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::setState(DatabasesStates state)
      {
        if (mState == state) {
          ZS_LOG_TRACE(log("state has not change") + ZS_PARAM("current state", toString(mState)) + ZS_PARAM("new state", toString(state)))
          return;
        }

        ZS_LOG_TRACE(log("state has changed") + ZS_PARAM("old state", toString(mState)) + ZS_PARAM("new state", toString(state)))

        mState = state;
        auto pThis = mThisWeak.lock();
        if (pThis) {
          mSubscriptions.delegate()->onLocationDatabasesStateChanged(pThis, mState);
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
      DatabaseInfoListPtr LocationDatabases::getUpdates(
                                                        const String &peerURI,
                                                        VersionedData &ioVersionData,
                                                        String &outLastVersion
                                                        ) const
      {
        DatabaseInfoListPtr result(new DatabaseInfoList);

        switch (ioVersionData.mType) {
          case VersionedData::VersionType_FullList: {
            for (int count = 0; count < 2; ++count) {
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

                ioVersionData.mAfterIndex = databaseRecord.mIndex;

                result->push_back(info);

                String prehash = databaseRecord.mDatabaseID + ":" + mPeerLocationRecord.mUpdateVersion;
                String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));
                outLastVersion = String(VersionedData::toString(VersionedData::VersionType_FullList)) + "-" + hash + "-" + string(databaseRecord.mIndex);

                ZS_LOG_TRACE(log("found additional updated record") + info.toDebug() + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(hash) + ZS_PARAMIZE(outLastVersion))
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

                  String prehash = string(changeRecord.mDisposition) + ":" + string(changeRecord.mIndexPeerLocation) + ":" + changeRecord.mDatabaseID;
                  String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));
                  outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);

                  ZS_LOG_TRACE(log("switched to change list view") + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(hash) + ZS_PARAMIZE(outLastVersion))
                } else {
                  outLastVersion = String();
                  break;
                }
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

              String prehash = string(changeRecord.mDisposition) + ":" + string(changeRecord.mIndexPeerLocation) + ":" + changeRecord.mDatabaseID;
              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));
              outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);
              ioVersionData.mAfterIndex = changeRecord.mIndex;

              DatabaseInfo info;
              info.mDisposition = toDisposition(changeRecord.mDisposition);
              info.mDatabaseID = changeRecord.mDatabaseID;

              if (DatabaseInfo::Disposition_Remove != info.mDisposition) {
                UseDatabase::DatabaseRecord databaseRecord;
                auto result = mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, changeRecord.mDatabaseID, databaseRecord);
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

              ZS_LOG_TRACE(log("found additional updated record") + info.toDebug() + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(hash) + ZS_PARAMIZE(outLastVersion))
              result->push_back(info);
            }
            break;
          }
        }
        
        ZS_LOG_TRACE(log("found changes") + ZS_PARAMIZE(peerURI) + ioVersionData.toDebug() + ZS_PARAMIZE(outLastVersion) + ZS_PARAM("size", result->size()))
        return result;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabases::validateVersionInfo(
                                                  const String &version,
                                                  VersionedData &outVersionData
                                                  ) const
      {
        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(version, split, '-');

        if ((0 != split.size()) &&
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
              String prehash = record.mDatabaseID + ":" + mPeerLocationRecord.mUpdateVersion;
              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));

              if (hash != updateVersion) {
                ZS_LOG_WARNING(Detail, log("version does not match current version") + ZS_PARAMIZE(version) + record.toDebug() + mPeerLocationRecord.toDebug() + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(hash))
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

            String prehash = string(record.mDisposition) + ":" + string(record.mIndexPeerLocation) + ":" + record.mDatabaseID;
            String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(prehash));
            if (hash != updateVersion) {
              ZS_LOG_WARNING(Detail, log("request version does not match current version") + ZS_PARAMIZE(version) + record.toDebug() + ZS_PARAMIZE(prehash) + ZS_PARAMIZE(hash))
              return false;
            }
            break;
          }
        }

        ZS_LOG_TRACE(log("version information validated") + ZS_PARAMIZE(version) + outVersionData.toDebug())
        return true;
      }
      
      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(ListSubscribeNotifyPtr notify)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notified of incoming list subscription notification") + Message::toDebug(notify))

        if (!mMasterDatase) {
          ZS_LOG_WARNING(Detail, log("master database is not ready thus cannot accept list subscribe notification"))
          return;
        }

        {
          if (notify->before() != mPeerLocationRecord.mLastDownloadedVersion) {
            ZS_LOG_WARNING(Detail, log("cannot accept list subscribe notification due to conflicting version") + ZS_PARAM("version", notify->before()) + ZS_PARAM("expecting", mPeerLocationRecord.mLastDownloadedVersion))
            goto subscription_must_terminate;
          }

          auto databases = notify->databases();
          if (!databases) {
            ZS_LOG_TRACE("nothing new notified")
            mPeerLocationRecord.mLastDownloadedVersion = notify->version();
            mMasterDatase->peerLocationTable()->notifyDownloaded(mPeerLocationRecord.mIndex, notify->version());
            goto check_subscription_must_terminate;
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

                mMasterDatase->permissionTable()->flushAllForDatabase(mPeerLocationRecord.mIndex, record.mDatabaseID);

                if (database) {
                  database->notifyRemoved();
                  mExpectingShutdownLocationDatabases[record.mDatabaseID] = UseLocationDatabasePtr();
                } else {
                  UseDatabase::deleteDatabase(mMasterDatase, mLocation->getPeerURI(), mLocation->getLocationID(), record.mDatabaseID);
                }
                break;
              }
            }
          }

          mPeerLocationRecord.mLastDownloadedVersion = notify->version();
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

          goto check_subscription_must_terminate;
        }

      check_subscription_must_terminate:
        {
          if (Time() == notify->expires()) {
            ZS_LOG_WARNING(Detail, log("notified that subscription is being terminated"))
            goto subscription_must_terminate;
          }
          return;
        }

      subscription_must_terminate:
        {
          if (mRemoteSubscribeTimeout) {
            mRemoteSubscribeTimeout->cancel();
            mRemoteSubscribeTimeout.reset();
          }

          // re-subscribe immediately
          mRemoteSubscribeTimeout = Timer::create(mThisWeak.lock(), Milliseconds(1), false);
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(
                                             UseMessageIncomingPtr message,
                                             ListSubscribeRequestPtr request
                                             )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notified of incoming list subscription request") + UseMessageIncoming::toDebug(message) + Message::toDebug(request))

        IHTTP::HTTPStatusCodes errorCode = IHTTP::HTTPStatusCode_Conflict;
        String errorReason;

        IncomingListSubscriptionMap::iterator found = mIncomingListSubscriptions.end();

        {
          UseLocationPtr location = message->getLocation();
          if (!location) {
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            goto fail_incoming_subscribe;
          }

          IncomingListSubscriptionPtr subscription;
          found = mIncomingListSubscriptions.find(location->getID());
          if (found == mIncomingListSubscriptions.end()) {
            if ((Time() == request->expires()) ||
                (request->expires() < zsLib::now())) {
              // immediate expiry, nothing to do...
              ListSubscribeResultPtr result = ListSubscribeResult::create(request);
              message->sendResponse(result);
              return;
            }
            subscription = IncomingListSubscriptionPtr(new IncomingListSubscription);
            subscription->mLocation = location;

            mIncomingListSubscriptions[location->getID()] = subscription;
            found = mIncomingListSubscriptions.find(location->getID());
          } else {
            subscription = (*found).second;
          }

          subscription->mSubscribeMessageID = request->messageID();
          subscription->mExpires = request->expires();
          subscription->mLastVersion = request->version();
          subscription->mNotified = false;

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
          if (found != mIncomingListSubscriptions.end()) {
            ZS_LOG_WARNING(Detail, log("removing incoming subscription"))
            mIncomingListSubscriptions.erase(found);
          }
          MessageResultPtr result = MessageResult::create(request);
          result->errorCode(errorCode);
          result->errorReason(errorReason.isEmpty() ? String(IHTTP::toString(errorCode)) : errorReason);
          message->sendResponse(result);
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(
                                             UseMessageIncomingPtr message,
                                             SubscribeRequestPtr request
                                             )
      {
        UseLocationPtr location = message->getLocation();
        String databaseID = request->databaseID();

        UseLocationDatabasePtr database = prepareDatabaseLocationForMessage(request, message, location, databaseID);

        if (!database) {
          ZS_LOG_WARNING(Detail, log("could not subscribe to database") + ZS_PARAMIZE(databaseID) + location->toDebug())
          return;
        }
        database->notifyIncoming(MessageIncoming::convert(message), request);
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::notifyIncoming(
                                             UseMessageIncomingPtr message,
                                             DataGetRequestPtr request
                                             )
      {
        UseLocationPtr location = message->getLocation();
        String databaseID = request->databaseID();

        UseLocationDatabasePtr database = prepareDatabaseLocationForMessage(request, message, location, databaseID);

        if (!database) {
          ZS_LOG_WARNING(Detail, log("could not subscribe to database") + ZS_PARAMIZE(databaseID) + location->toDebug())
          return;
        }
        database->notifyIncoming(MessageIncoming::convert(message), request);
      }

      //-----------------------------------------------------------------------
      LocationDatabases::UseLocationDatabasePtr LocationDatabases::prepareDatabaseLocationForMessage(
                                                                                                     MessageRequestPtr request,
                                                                                                     UseMessageIncomingPtr message,
                                                                                                     UseLocationPtr fromLocation,
                                                                                                     const String &requestedDatabaseID
                                                                                                     )
      {

        IHTTP::HTTPStatusCodes errorCode = IHTTP::HTTPStatusCode_Gone;
        String errorReason;

        {
          AutoRecursiveLock lock(*this);
          if (!mMasterDatase) {
            ZS_LOG_WARNING(Detail, log("master database is not ready"))
            return UseLocationDatabasePtr();
          }

          auto result = mMasterDatase->permissionTable()->hasPermission(mPeerLocationRecord.mIndex, requestedDatabaseID, fromLocation->getPeerURI());
          if (!result) {
            ZS_LOG_WARNING(Detail, log("does not have permission to access database") + ZS_PARAMIZE(requestedDatabaseID) + fromLocation->toDebug())
            errorCode = IHTTP::HTTPStatusCode_Forbidden;
            goto not_available;
          }

          auto found = mLocationDatabases.find(requestedDatabaseID);
          if (found != mLocationDatabases.end()) {
            auto database = (*found).second.lock();
            if (database) {
              ZS_LOG_TRACE(log("found existing database") + ZS_PARAMIZE(requestedDatabaseID) + ZS_PARAM("location database id", database->getID()))
              return database;
            }
            ZS_LOG_WARNING(Debug, log("previously found location database is now gone"))
            mLocationDatabases.erase(found);
            found = mLocationDatabases.end();
          }

          UseDatabase::DatabaseRecord record;
          if (!mMasterDatase->databaseTable()->getByDatabaseID(mPeerLocationRecord.mIndex, requestedDatabaseID, record)) {
            ZS_LOG_WARNING(Detail, log("database is not found (thus cannot open database)") + ZS_PARAMIZE(requestedDatabaseID))
            errorCode = IHTTP::HTTPStatusCode_NotFound;
            return UseLocationDatabasePtr();
          }

          UseLocationDatabasePtr database = UseLocationDatabase::openLocal(mThisWeak.lock(), requestedDatabaseID);
          if (!database) {
            ZS_LOG_WARNING(Detail, log("could not open local database") + ZS_PARAMIZE(requestedDatabaseID))
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            return database;
          }

          ZS_LOG_DEBUG(log("opened new local database to handle message") + ZS_PARAMIZE(requestedDatabaseID) + ZS_PARAM("location database id", database->getID()))

          mLocationDatabases[requestedDatabaseID] = database;
          return database;
        }

      not_available:
        {
          MessageResultPtr result = MessageResult::create(request);
          result->errorCode(errorCode);
          result->errorReason(errorReason.isEmpty() ? String(IHTTP::toString(errorCode)) : errorReason);
          message->sendResponse(result);
        }
        return UseLocationDatabasePtr();
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
      LocationDatabases::IncomingListSubscription::IncomingListSubscription()
      {
      }

      //-----------------------------------------------------------------------
      LocationDatabases::IncomingListSubscription::~IncomingListSubscription()
      {
        if (mPendingSendPromise) {
          mPendingSendPromise->reject();
          mPendingSendPromise.reset();
        }
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabases::IncomingListSubscription::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabases::IncomingListSubscription");

        UseServicesHelper::debugAppend(resultEl, "location", UseLocation::toDebug(mLocation));

        UseServicesHelper::debugAppend(resultEl, "subscribe message id", mSubscribeMessageID);

        UseServicesHelper::debugAppend(resultEl, "type", toString(mType));
        UseServicesHelper::debugAppend(resultEl, "after index", mAfterIndex);

        UseServicesHelper::debugAppend(resultEl, "last version", mLastVersion);

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
        return LocationDatabasesFactory::singleton();
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
    #pragma mark ILocationDatabasesTypes
    #pragma mark

    //-----------------------------------------------------------------------
    const char *ILocationDatabasesTypes::toString(DatabasesStates states)
    {
      switch (states) {
        case DatabasesState_Pending:    return "Pending";
        case DatabasesState_Ready:      return "Ready";
        case DatabasesState_Shutdown:   return "Shutdown";
      }
      return "Unknown";
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
