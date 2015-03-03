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

#include <openpeer/stack/internal/stack_LocationDatabase.h>

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationSubscription.h>
#include <openpeer/stack/internal/stack_LocationDatabaseTearAway.h>
#include <openpeer/stack/internal/stack_LocationDatabaseLocalTearAway.h>
#include <openpeer/stack/internal/stack_LocationDatabases.h>
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/SubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/DataGetRequest.h>

#include <openpeer/stack/message/IMessageHelper.h>


#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Numeric.h>
#include <zsLib/Log.h>
#include <zsLib/XML.h>


#define OPENPEER_STACK_LOCATION_DATABASE_MAXIMUM_BATCH_DOWNLOAD_DATA_AT_ONCE (10)


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    using zsLib::Numeric;

    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    ZS_DECLARE_TYPEDEF_PTR(stack::message::IMessageHelper, UseMessageHelper)

    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)
      ZS_DECLARE_TYPEDEF_PTR(stack::internal::IStackForInternal, UseStack)

      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::EntryInfoList, EntryInfoList)
      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::EntryInfo, EntryInfo)
      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::PeerList, PeerList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabaseForLocationDatabases::ForLocationDatabasesPtr ILocationDatabaseForLocationDatabases::openRemote(
                                                                                                                       LocationDatabasesPtr databases,
                                                                                                                       const String &databaseID,
                                                                                                                       bool downloadAllEntries
                                                                                                                       )
      {
        return ILocationDatabaseFactory::singleton().openRemote(databases, databaseID, downloadAllEntries);
      }

      //-----------------------------------------------------------------------
      ILocationDatabaseForLocationDatabases::ForLocationDatabasesPtr ILocationDatabaseForLocationDatabases::openLocal(
                                                                                                                      LocationDatabasesPtr databases,
                                                                                                                      const String &databaseID
                                                                                                                      )
      {
        return ILocationDatabaseFactory::singleton().openLocal(databases, databaseID);
      }

      //-----------------------------------------------------------------------
      ILocationDatabaseForLocationDatabases::ForLocationDatabasesPtr ILocationDatabaseForLocationDatabases::createLocal(
                                                                                                                        LocationDatabasesPtr databases,
                                                                                                                        const String &databaseID,
                                                                                                                        ElementPtr inMetaData,
                                                                                                                        const PeerList &inPeerAccessList,
                                                                                                                        Time expires
                                                                                                                        )
      {
        return ILocationDatabaseFactory::singleton().createLocal(databases, databaseID, inMetaData, inPeerAccessList, expires);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabase::LocationDatabase(
                                         const String &databaseID,
                                         UseLocationDatabasesPtr databases
                                         ) :
        MessageQueueAssociator(UseStack::queueStack()),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDatabaseID(databaseID),
        mDatabases(databases)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!databases)

        ZS_LOG_DEBUG(debug("created"))

        mLocation = Location::convert(databases->getLocation());
        mIsLocal = mDatabases->isLocal();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::init()
      {
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      LocationDatabase::~LocationDatabase()
      {
        if (isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))

        cancel();

        mDatabases->notifyDestroyed(*this);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabase::convert(ILocationDatabasePtr object)
      {
        ILocationDatabasePtr original = ILocationDatabaseTearAway::original(object);
        return ZS_DYNAMIC_PTR_CAST(LocationDatabase, original);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabase::convert(ForLocationDatabasesPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationDatabase, object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ILocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::toDebug(ILocationDatabasePtr object)
      {
        if (!object) return ElementPtr();
        return LocationDatabase::convert(object)->toDebug();
      }

      //-----------------------------------------------------------------------
      ILocationDatabasePtr LocationDatabase::open(
                                                  ILocationDatabaseDelegatePtr inDelegate,
                                                  ILocationPtr location,
                                                  const char *inDatabaseID,
                                                  bool inAutomaticallyDownloadDatabaseData
                                                  )
      {
        String databaseID(inDatabaseID);

        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        ILocationDatabasesPtr databasesForLocation = ILocationDatabases::open(location, ILocationDatabasesDelegatePtr());

        if (!databasesForLocation) {
          ZS_LOG_WARNING(Detail, slog("failed to open location databaess") + ILocation::toDebug(location))
          return ILocationDatabasePtr();
        }

        UseLocationDatabasesPtr databases = LocationDatabases::convert(databasesForLocation);

        LocationDatabasePtr pThis = databases->getOrCreate(String(inDatabaseID), inAutomaticallyDownloadDatabaseData);
        if (!pThis) {
          ZS_LOG_WARNING(Detail, slog("failed to create database") + ZS_PARAMIZE(inDatabaseID))
          return ILocationDatabasePtr();
        }

        auto subscription = pThis->subscribe(inDelegate);
        if (inAutomaticallyDownloadDatabaseData) {
          AutoRecursiveLock lock(*pThis);
          pThis->mAutomaticallyDownloadAllEntries = true;
          IWakeDelegateProxy::create(pThis)->onWake();
        }

        LocationDatabaseTearAwayDataPtr data(new LocationDatabaseTearAwayData);
        data->mSubscription = subscription;

        return ILocationDatabaseTearAway::create(pThis, data);
      }

      //-----------------------------------------------------------------------
      ILocationPtr LocationDatabase::getLocation() const
      {
        return Location::convert(mLocation);
      }

      //-----------------------------------------------------------------------
      String LocationDatabase::getDatabaseID() const
      {
        return mDatabaseID;
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::getMetaData() const
      {
        AutoRecursiveLock lock(*this);
        return mDatabaseRecord.mMetaData ? mDatabaseRecord.mMetaData->clone()->toElement() : ElementPtr();
      }

      //-----------------------------------------------------------------------
      Time LocationDatabase::getExpires() const
      {
        AutoRecursiveLock lock(*this);
        return mDatabaseRecord.mExpires;
      }

      //-----------------------------------------------------------------------
      LocationDatabase::EntryInfoListPtr LocationDatabase::getUpdates(
                                                                      const String &inExistingVersion,
                                                                      String &outNewVersion
                                                                      ) const
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          outNewVersion = inExistingVersion;
          return EntryInfoListPtr(new EntryInfoList);
        }

        VersionedData versionData;
        if (!validateVersionInfo(inExistingVersion, versionData)) {
          ZS_LOG_WARNING(Detail, log("version information failed to resolve") + ZS_PARAMIZE(inExistingVersion))
          return EntryInfoListPtr();
        }

        return getUpdates(versionData, outNewVersion, true);
      }

      //-----------------------------------------------------------------------
      LocationDatabase::EntryInfoPtr LocationDatabase::getEntry(const char *inUniqueID) const
      {
        UseDatabase::EntryRecord entryRecord;
        entryRecord.mEntryID = String(inUniqueID);
        auto result = mEntryDatabase->entryTable()->getEntry(entryRecord, true);

        if (!result) {
          ZS_LOG_WARNING(Detail, log("no entry found") + ZS_PARAMIZE(inUniqueID))
          return EntryInfoPtr();
        }

        EntryInfoPtr info(new EntryInfo);
        info->mDisposition = EntryInfo::Disposition_Add;
        info->mEntryID = entryRecord.mEntryID;
        info->mVersion = entryRecord.mVersion;
        info->mCreated = entryRecord.mCreated;
        info->mUpdated = entryRecord.mUpdated;
        info->mMetaData = entryRecord.mMetaData;
        info->mData = entryRecord.mData;
        info->mDataSize = entryRecord.mDataLength;

        ZS_LOG_TRACE(log("found record") + info->toDebug())
        return info;
      }

      //-----------------------------------------------------------------------
      PromisePtr LocationDatabase::notifyWhenDataReady(const UniqueIDList &needingEntryData)
      {
        AutoRecursiveLock lock(*this);

        Promise::PromiseList pendingPromises;

        for (auto iter = needingEntryData.begin(); iter != needingEntryData.end(); ++iter) {

          auto entryID = (*iter);

          {
            if (isShutdown()) {
              ZS_LOG_WARNING(Debug, log("no entry found") + ZS_PARAMIZE(entryID))
              goto reject_entry;
            }

            UseDatabase::EntryRecord entryRecord;
            entryRecord.mEntryID = entryID;
            auto result = mEntryDatabase->entryTable()->getEntry(entryRecord, true);

            if (!result) {
              ZS_LOG_WARNING(Debug, log("no entry found") + ZS_PARAMIZE(entryID))
              goto reject_entry;
            }

            if ((0 == entryRecord.mDataLength) ||
                (entryRecord.mData) ||
                (isLocal())) {
              EntryInfoPtr info(new EntryInfo);
              info->mDisposition = EntryInfo::Disposition_Add;
              info->mCreated = entryRecord.mCreated;
              info->mVersion = entryRecord.mVersion;
              info->mUpdated = entryRecord.mUpdated;
              info->mMetaData = entryRecord.mMetaData;
              info->mData = entryRecord.mData;
              info->mDataSize = entryRecord.mDataLength;

              pendingPromises.push_back(Promise::createResolved(info));

              ZS_LOG_TRACE(log("found record") + info->toDebug())
              continue;
            }

            // record is still pending...
            PromisePtr pendingPromise = Promise::create();
            pendingPromises.push_back(pendingPromise);

            auto found = mPendingWhenDataReady.find(entryID);
            if (found == mPendingWhenDataReady.end()) {
              mPendingWhenDataReady[entryID] = Promise::PromiseList();
              found = mPendingWhenDataReady.find(entryID);
              ZS_THROW_BAD_STATE_IF(found == mPendingWhenDataReady.end())
            }

            ZS_LOG_TRACE(log("entry is not availabe yet (complete later)") + ZS_PARAMIZE(entryID))

            Promise::PromiseList &pendingPromiseEntries = (*found).second;
            pendingPromiseEntries.push_back(pendingPromise);
            continue;
          }

        reject_entry:
          {
            EntryInfoPtr info(new EntryInfo);
            info->mEntryID = entryID;
            pendingPromises.push_back(Promise::createRejected(info));
            continue;
          }
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        PromisePtr allPromise = Promise::allSettled(pendingPromises, UseStack::queueDelegate());
        return allPromise;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ILocationDatabaseLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabaseLocalPtr LocationDatabase::toLocal(ILocationDatabasePtr inLocation)
      {
        LocationDatabasePtr location = convert(inLocation);

        if (!location->isLocal()) return ILocationDatabaseLocalPtr();

        return ZS_DYNAMIC_PTR_CAST(ILocationDatabaseLocal, location);
      }

      //-----------------------------------------------------------------------
      ILocationDatabaseLocalPtr LocationDatabase::create(
                                                         ILocationDatabaseLocalDelegatePtr inDelegate,
                                                         IAccountPtr account,
                                                         const char *inDatabaseID,
                                                         ElementPtr inMetaData,
                                                         const PeerList &inPeerAccessList,
                                                         Time expires
                                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)

        String databaseID(inDatabaseID);

        UseLocationPtr location = UseLocation::getForLocal(Account::convert(account));
        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        ILocationDatabasesPtr databasesForLocation = ILocationDatabases::open(Location::convert(location), ILocationDatabasesDelegatePtr());

        if (!databasesForLocation) {
          ZS_LOG_WARNING(Detail, slog("failed to open location databaess") + UseLocation::toDebug(location))
          return ILocationDatabaseLocalPtr();
        }

        UseLocationDatabasesPtr databases = LocationDatabases::convert(databasesForLocation);

        LocationDatabasePtr pThis = databases->getOrCreate(String(inDatabaseID), inMetaData, inPeerAccessList, expires);
        if (!pThis) {
          ZS_LOG_WARNING(Detail, slog("failed to create database") + ZS_PARAMIZE(inDatabaseID))
          return ILocationDatabaseLocalPtr();
        }

        auto subscription = pThis->subscribe(inDelegate);

        LocationDatabaseLocalTearAwayDataPtr data(new LocationDatabaseLocalTearAwayData);
        data->mSubscription = subscription;

        return ILocationDatabaseLocalTearAway::create(pThis, data);
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::remove(
                                    IAccountPtr account,
                                    const char *inDatabaseID
                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)

        String databaseID(inDatabaseID);

        UseLocationPtr location = UseLocation::getForLocal(Account::convert(account));
        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        ILocationDatabasesPtr databasesForLocation = ILocationDatabases::open(Location::convert(location), ILocationDatabasesDelegatePtr());

        if (!databasesForLocation) {
          ZS_LOG_WARNING(Detail, slog("failed to open location databaess") + UseLocation::toDebug(location))
          return false;
        }

        UseLocationDatabasesPtr databases = LocationDatabases::convert(databasesForLocation);

        return databases->remove(String(inDatabaseID));
      }

      //-----------------------------------------------------------------------
      PeerListPtr LocationDatabase::getPeerAccessList() const
      {
        ZS_THROW_INVALID_USAGE_IF(!isLocal())
        PeerListPtr peerList(new PeerList);
        (*peerList) = mPeerAccessList;
        return peerList;
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::setExpires(const Time &time)
      {
        ZS_THROW_INVALID_USAGE_IF(!isLocal())

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("unable to set expires due to database shutdown"))
          return;
        }
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::add(
                                 const char *uniqueID,
                                 ElementPtr inMetaData,
                                 ElementPtr inData
                                 )
      {
        ZS_THROW_INVALID_USAGE_IF(!isLocal())

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("unable to add record due to database shutdown"))
          return false;
        }
        if (!mEntryDatabase) {
          ZS_LOG_WARNING(Detail, log("unable to add record due to database not being ready"))
          return false;
        }

        UseDatabase::EntryRecord record;
        record.mEntryID = String(uniqueID);
        record.mMetaData = inMetaData;
        record.mData = inData;
        record.mDataLength = 0;
        record.mCreated = zsLib::now();
        record.mUpdated = record.mCreated;

        UseDatabase::EntryChangeRecord changeRecord;
        bool result = mEntryDatabase->entryTable()->add(record, changeRecord);

        if ((UseDatabase::EntryChangeRecord::Disposition_None != changeRecord.mDisposition) &&
            (result)) {
          ZS_LOG_DEBUG(log("added database record") + record.toDebug() + changeRecord.toDebug())

          mEntryDatabase->entryChangeTable()->insert(changeRecord);

          // cause any incoming subscriptions to be notified about new entry
          for (auto iter = mIncomingSubscriptions.begin(); iter != mIncomingSubscriptions.end(); ++iter) {
            auto subscription = (*iter).second;

            ZS_LOG_TRACE(log("incoming subscription will need notification about new entry") + subscription->toDebug());

            subscription->mNotified = false;
          }

          // cause local notification about new database
          mLastChangeUpdateNotified = "added";
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_WARNING(Detail, log("failed to add record") + record.toDebug())
        return false;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::update(
                                    const char *uniqueID,
                                    ElementPtr inData
                                    )
      {
        ZS_THROW_INVALID_USAGE_IF(!isLocal())

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("unable to update record due to database shutdown"))
          return false;
        }
        if (!mEntryDatabase) {
          ZS_LOG_WARNING(Detail, log("unable to update record due to database not being ready"))
          return false;
        }

        UseDatabase::EntryRecord record;
        record.mEntryID = String(uniqueID);
        record.mData = inData;
        record.mDataLength = 0;
        record.mUpdated = zsLib::now();

        UseDatabase::EntryChangeRecord changeRecord;
        bool result = mEntryDatabase->entryTable()->update(record, changeRecord);

        if ((UseDatabase::EntryChangeRecord::Disposition_None != changeRecord.mDisposition) &&
            (result)) {
          ZS_LOG_DEBUG(log("updated database record") + record.toDebug() + changeRecord.toDebug())

          mDatabaseRecord.mUpdateVersion = mDatabases->updateVersion(mDatabaseID);

          mEntryDatabase->entryChangeTable()->insert(changeRecord);

          // cause any incoming subscriptions to be notified about updated entry
          for (auto iter = mIncomingSubscriptions.begin(); iter != mIncomingSubscriptions.end(); ++iter) {
            auto subscription = (*iter).second;

            ZS_LOG_TRACE(log("incoming subscription will need notification about updated entry") + subscription->toDebug());

            subscription->mNotified = false;
          }

          // cause local notification about new database
          mLastChangeUpdateNotified = "updated";
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_WARNING(Detail, log("failed to update record") + record.toDebug())
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabase::remove(const char *uniqueID)
      {
        ZS_THROW_INVALID_USAGE_IF(!isLocal())

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("unable to remove record due to database shutdown"))
          return false;
        }
        if (!mEntryDatabase) {
          ZS_LOG_WARNING(Detail, log("unable to remove record due to database not being ready"))
          return false;
        }

        mLastChangeUpdateNotified = "remove";
        UseDatabase::EntryRecord record;
        record.mEntryID = String(uniqueID);

        UseDatabase::EntryChangeRecord changeRecord;
        mEntryDatabase->entryTable()->remove(record, changeRecord);

        if (UseDatabase::EntryChangeRecord::Disposition_None != changeRecord.mDisposition) {
          ZS_LOG_DEBUG(log("removed entry record") + record.toDebug() + changeRecord.toDebug())

          mDatabaseRecord.mUpdateVersion = mDatabases->updateVersion(mDatabaseID);

          mEntryDatabase->entryChangeTable()->insert(changeRecord);

          // cause any incoming subscriptions to be notified about removed entry
          for (auto iter_doNotUse = mIncomingSubscriptions.begin(); iter_doNotUse != mIncomingSubscriptions.end(); ) {

            auto current = iter_doNotUse;
            ++iter_doNotUse;

            auto subscription = (*current).second;

            if (subscription->mType == VersionedData::VersionType_ChangeList) {
              ZS_LOG_TRACE(log("incoming subscription will need notification about removed entry") + subscription->toDebug());

              subscription->mNotified = false;
              continue;
            }

            ZS_LOG_WARNING(Detail, log("incoming database subscription must be expired because it does not match version"))

            SubscribeNotifyPtr notify = SubscribeNotify::create();
            notify->messageID(subscription->mSubscribeMessageID);
            notify->databaseID(mDatabaseID);
            notify->expires(Time());

            subscription->mLocation->send(notify);

            mIncomingSubscriptions.erase(current);
          }

          // cause local notification about new database
          mLastChangeUpdateNotified = "removed";
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_WARNING(Detail, log("failed to remove record") + record.toDebug())
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ILocationDatabaseForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabase::openRemote(
                                                       LocationDatabasesPtr databases,
                                                       const String &databaseID,
                                                       bool downloadAllEntries
                                                       )
      {
        LocationDatabasePtr pThis(new LocationDatabase(databaseID, databases));
        pThis->mThisWeak = pThis;
        pThis->mAutomaticallyDownloadAllEntries = downloadAllEntries;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabase::openLocal(
                                                      LocationDatabasesPtr databases,
                                                      const String &databaseID
                                                      )
      {
        LocationDatabasePtr pThis(new LocationDatabase(databaseID, databases));
        pThis->mThisWeak = pThis;
        pThis->mOpenOnly = true;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr LocationDatabase::createLocal(
                                                        LocationDatabasesPtr databases,
                                                        const String &databaseID,
                                                        ElementPtr inMetaData,
                                                        const PeerList &inPeerAccessList,
                                                        Time expires
                                                        )
      {
        LocationDatabasePtr pThis(new LocationDatabase(databaseID, databases));
        pThis->mThisWeak = pThis;
        pThis->mOpenOnly = false;
        pThis->mMetaData = inMetaData ? inMetaData->clone()->toElement() : ElementPtr();
        pThis->mPeerAccessList = inPeerAccessList;
        pThis->mExpires = expires;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyConflict()
      {
        ZS_LOG_WARNING(Detail, log("notified of conflict"))

        mNotifiedConflict = true;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyUpdated()
      {
        ZS_LOG_DEBUG(log("notified updated"))
        mNotifiedUpdated = true;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyRemoved()
      {
        ZS_LOG_WARNING(Detail, log("notified removed"))

        mNotifiedRemoved = true;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyShutdown()
      {
        ZS_LOG_WARNING(Detail, log("notified shutdown"))
        mNotifiedShutdown = true;
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabase::onWake()
      {
        ZS_LOG_TRACE(log("on wake"))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabase::onTimer(TimerPtr timer)
      {
        ZS_LOG_TRACE(log("on timer") + ZS_PARAM("timer id", timer->getID()))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return;
        }

        auto found = mTimedReferences.find(timer);
        if (found != mTimedReferences.end()) {
          ZS_LOG_TRACE(log("removed timed reference"))
          mTimedReferences.erase(found);
          return;
        }

        if (timer == mRemoteSubscribeTimeout) {
          ZS_LOG_DEBUG(log("remote subscription has expired (thus must re-subscribe"))
          mRemoteSubscribeTimeout->cancel();
          mRemoteSubscribeTimeout.reset();

          if (mRemoteSubscribeMonitor) {
            mRemoteSubscribeMonitor->cancel();
            mRemoteSubscribeMonitor.reset();
          }
          step();
          return;
        }

        if (timer == mIncomingSubscriptionTimer) {
          ZS_LOG_TRACE(log("incoming subscription timer fired"))

          auto now = zsLib::now();

          for (auto iter_doNotUse = mIncomingSubscriptions.begin(); iter_doNotUse != mIncomingSubscriptions.end();)
          {
            auto current = iter_doNotUse;
            ++iter_doNotUse;

            auto subscription = (*current).second;

            if (subscription->mExpires > now) continue;

            ZS_LOG_WARNING(Debug, log("incoming subscription has now expired") + subscription->toDebug() + ZS_PARAMIZE(now))
            mIncomingSubscriptions.erase(current);
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
      void LocationDatabase::onBackOffTimerAttemptAgainNow(IBackOffTimerPtr timer)
      {
        ZS_LOG_DEBUG(log("attempt a retry again now") + IBackOffTimer::toDebug(timer))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::onBackOffTimerAttemptTimeout(IBackOffTimerPtr timer)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::onBackOffTimerAllAttemptsFailed(IBackOffTimerPtr timer)
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
      #pragma mark LocationDatabase => IPromiseSettledDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabase::onPromiseSettled(PromisePtr promise)
      {
        ZS_LOG_TRACE(log("on promise settled") + ZS_PARAM("promise", promise->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ILocationSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabase::onLocationSubscriptionShutdown(ILocationSubscriptionPtr inSubscription)
      {
        UseLocationSubscriptionPtr subscription = LocationSubscription::convert(inSubscription);

        ZS_LOG_TRACE(log("notified location subscription shutdown (thus account must be closed)") + UseLocationSubscription::toDebug(subscription))

        AutoRecursiveLock lock(*this);

        mLocationSubscription.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::onLocationSubscriptionLocationConnectionStateChanged(
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
              if (mRemoteSubscriptionBackOffTimer) {
                mRemoteSubscriptionBackOffTimer->cancel();
                mRemoteSubscriptionBackOffTimer.reset();
              }
              auto found = mIncomingSubscriptions.find(location->getID());
              if (found != mIncomingSubscriptions.end()) {
                auto subscription = (*found).second;
                mIncomingSubscriptions.erase(found);
              }
            } else {
              if (mRemoteSubscribeMonitor) {
                mRemoteSubscribeMonitor->cancel();
                mRemoteSubscribeMonitor.reset();
              }
            }
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::onLocationSubscriptionMessageIncoming(
                                                                   ILocationSubscriptionPtr subscription,
                                                                   IMessageIncomingPtr inMessage
                                                                   )
      {
        UseMessageIncomingPtr message = MessageIncoming::convert(inMessage);

        ZS_LOG_TRACE(log("notified location message") + ILocationSubscription::toDebug(subscription) + UseMessageIncoming::toDebug(message))

        AutoRecursiveLock lock(*this);
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown (thus cannot handle incoming requests)"))
          cancel();
          return;
        }

        if (isLocal()) {
          // nothing to do
        } else {

          auto actualMessage = message->getMessage();

          {
            auto notify = SubscribeNotify::convert(actualMessage);
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
      #pragma mark LocationDatabase => IMessageMonitorResultDelegate<SubscribeResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool LocationDatabase::handleMessageMonitorResultReceived(
                                                                IMessageMonitorPtr monitor,
                                                                SubscribeResultPtr result
                                                                )
      {
        AutoRecursiveLock lock(*this);

        if (monitor != mRemoteSubscribeMonitor) {
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
      bool LocationDatabase::handleMessageMonitorErrorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      SubscribeResultPtr ignore,         // will always be NULL
                                                                      message::MessageResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);

        if (monitor != mRemoteSubscribeMonitor) {
          ZS_LOG_WARNING(Debug, log("received subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("failed to subscribe to remote database") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))

        mRemoteSubscribeMonitor.reset();

        if (IHTTP::HTTPStatusCode_Conflict == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("remote version conflict detected (all remote databases must be purged)"))

          if (mEntryDatabase) {
            mEntryDatabase.reset();
          }

          UseDatabasePtr masterDatabase = mDatabases->getMasterDatabase();

          if (!masterDatabase) {
            ZS_LOG_WARNING(Detail, log("entry datbase is gone"))
            cancel();
            return false;
          }

          UseDatabase::deleteDatabase(masterDatabase, mLocation->getPeerURI(), mLocation->getLocationID(), mDatabaseID);
          mEntryDatabase = UseDatabase::openDatabase(masterDatabase, mLocation->getPeerURI(), mLocation->getLocationID(), mDatabaseID);

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
      #pragma mark LocationDatabase => IMessageMonitorResultDelegate<DataGetResultPtr>
      #pragma mark

      //-----------------------------------------------------------------------
      bool LocationDatabase::handleMessageMonitorResultReceived(
                                                                 IMessageMonitorPtr monitor,
                                                                 DataGetResultPtr result
                                                                 )
      {
        AutoRecursiveLock lock(*this);

        if (monitor != mDataGetRequestMonitor) {
          ZS_LOG_WARNING(Debug, log("received list subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        mLastRequestedIndex = mLastRequestedIndexUponSuccess;

        EntryInfoListPtr entries = result->entries();

        if (entries) {
          if (entries->size() > 0) {
            processEntries(*entries);
          }
        }

        for (auto iter = mRequestedEntries.begin(); iter != mRequestedEntries.end(); ++iter) {
          auto entryID = (*iter);

          auto found = mPendingWhenDataReady.find(entryID);
          if (found == mPendingWhenDataReady.end()) continue;

          auto promiseList = (*found).second;

          EntryInfoPtr promiseInfo(new EntryInfo);
          promiseInfo->mEntryID = entryID;

          PromisePtr broadcast = Promise::broadcast(promiseList);
          broadcast->resolve(promiseInfo);
          mPendingWhenDataReady.erase(found);
        }

        mRequestedEntries.clear();

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::handleMessageMonitorErrorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     DataGetResultPtr ignore,         // will always be NULL
                                                                     message::MessageResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);

        if (monitor != mDataGetRequestMonitor) {
          ZS_LOG_WARNING(Debug, log("received list subscribe result on obsolete monitor") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("failed to get remote data") + IMessageMonitor::toDebug(monitor) + Message::toDebug(result))

        mDataGetRequestMonitor.reset();

        mRequestedEntries.clear();

        mLastRequestedIndexUponSuccess = mLastRequestedIndex; // roll back to previous batch

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
      #pragma mark LocationDatabase => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params LocationDatabase::slog(const char *message)
      {
        return Log::Params(message, "LocationDatabase");
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabase::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("LocationDatabase");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabase::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabase");

        UseServicesHelper::debugAppend(resultEl, "id", mID);


        UseServicesHelper::debugAppend(resultEl, UseLocationDatabases::toDebug(mDatabases));
        UseServicesHelper::debugAppend(resultEl, "location subscription", mLocationSubscription ? mLocationSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "databases", mDatabases ? mDatabases->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());

        UseServicesHelper::debugAppend(resultEl, "state", toString(mState));

        UseServicesHelper::debugAppend(resultEl, "is local", mIsLocal);

        UseServicesHelper::debugAppend(resultEl, "database id", mDatabaseID);

        UseServicesHelper::debugAppend(resultEl, "automatically download all entries", mAutomaticallyDownloadAllEntries);

        UseServicesHelper::debugAppend(resultEl, "open only", mOpenOnly);
        UseServicesHelper::debugAppend(resultEl, "meta data", UseServicesHelper::toString(mMetaData));
        UseServicesHelper::debugAppend(resultEl, "peer access list", mPeerAccessList.size());

        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        UseServicesHelper::debugAppend(resultEl, "master ready", mMasterReady ? mMasterReady->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, mDatabaseRecord.toDebug());

        UseServicesHelper::debugAppend(resultEl, "last change update notified", mLastChangeUpdateNotified);

        UseServicesHelper::debugAppend(resultEl, "timed references", mTimedReferences.size());

        UseServicesHelper::debugAppend(resultEl, "incoming subscriptions", mIncomingSubscriptions.size());
        UseServicesHelper::debugAppend(resultEl, "incoming subscription timer", mIncomingSubscriptionTimer ? mIncomingSubscriptionTimer->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "remote subscription back-off timer", mRemoteSubscriptionBackOffTimer ? mRemoteSubscriptionBackOffTimer->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "remote subscription monitor", mRemoteSubscribeMonitor ? mRemoteSubscribeMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "remote subscribe timeout", mRemoteSubscribeTimeout ? mRemoteSubscribeTimeout->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "pending when data ready", mPendingWhenDataReady.size());
        UseServicesHelper::debugAppend(resultEl, "remote data get back-off timer", mRemoteSubscriptionBackOffTimer ? mRemoteSubscriptionBackOffTimer->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "remote data get monitor", mRemoteSubscribeMonitor ? mRemoteSubscribeMonitor->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "downloaded all data", mDownloadedAllData);
        UseServicesHelper::debugAppend(resultEl, "requested entries", mRequestedEntries.size());
        UseServicesHelper::debugAppend(resultEl, "last requested index", mLastRequestedIndex);
        UseServicesHelper::debugAppend(resultEl, "last requested index upon success", mLastRequestedIndexUponSuccess);
        return resultEl;
      }

      //-----------------------------------------------------------------------
      ILocationDatabaseSubscriptionPtr LocationDatabase::subscribe(ILocationDatabaseDelegatePtr originalDelegate)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("subscribe called") + ZS_PARAM("delegate", (bool)originalDelegate))

        if (!originalDelegate) return ILocationDatabaseSubscriptionPtr();

        ILocationDatabaseSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate);

        ILocationDatabaseDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          auto pThis = mThisWeak.lock();
          if (!isPending()) {
            delegate->onLocationDatabaseStateChanged(pThis, mState);
          }

          if (!isShutdown()) {
            if (mLastChangeUpdateNotified.hasData()) {
              delegate->onLocationDatabaseUpdated(pThis);
            }
          }
        }

        if (isShutdown()) {
          mSubscriptions.clear();
        }
        return subscription;
      }
      
      //-----------------------------------------------------------------------
      void LocationDatabase::cancel()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        if (mLocationSubscription) {
          mLocationSubscription->cancel();
          mLocationSubscription.reset();
        }

        mMasterReady.reset();

        mEntryDatabase.reset();

        // clear out timed references
        {
          for (auto iter = mTimedReferences.begin(); iter != mTimedReferences.end(); ++iter) {
            auto timer = (*iter).first;

            timer->cancel();
            timer.reset();
          }

          mTimedReferences.clear();
        }

        mIncomingSubscriptions.clear();

        if (mIncomingSubscriptionTimer) {
          mIncomingSubscriptionTimer->cancel();
          mIncomingSubscriptionTimer.reset();
        }

        if (mRemoteSubscriptionBackOffTimer) {
          mRemoteSubscriptionBackOffTimer->cancel();
          mRemoteSubscriptionBackOffTimer.reset();
        }

        if (mRemoteSubscribeMonitor) {
          mRemoteSubscribeMonitor->cancel();
          mRemoteSubscribeMonitor.reset();
        }

        if (mRemoteSubscribeTimeout) {
          mRemoteSubscribeTimeout->cancel();
          mRemoteSubscribeTimeout.reset();
        }

        // clear out all pending data promises (as they cannot be filled)
        {
          for (auto iter = mPendingWhenDataReady.begin(); iter != mPendingWhenDataReady.end(); ++iter) {
            auto entryID = (*iter).first;
            auto pendingPromiseList = (*iter).second;

            EntryInfoPtr info(new EntryInfo);
            info->mEntryID = entryID;

            PromisePtr broadcast = Promise::broadcast(pendingPromiseList);
            broadcast->reject(info);
          }
          mPendingWhenDataReady.clear();
        }

        if (mRemoteDataGetBackOffTimer) {
          mRemoteDataGetBackOffTimer->cancel();
          mRemoteDataGetBackOffTimer.reset();
        }
        if (mDataGetRequestMonitor) {
          mDataGetRequestMonitor->cancel();
          mDataGetRequestMonitor.reset();
        }

        mRequestedEntries.clear();

        setState(DatabaseState_Shutdown);

        mSubscriptions.clear();

        mDatabases->notifyShutdown(*this);
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::setState(DatabaseStates state)
      {
        if (mState == state) {
          ZS_LOG_TRACE(log("state has not change") + ZS_PARAM("current state", toString(mState)) + ZS_PARAM("new state", toString(state)))
          return;
        }

        ZS_LOG_TRACE(log("state has changed") + ZS_PARAM("old state", toString(mState)) + ZS_PARAM("new state", toString(state)))

        mState = state;
        auto pThis = mThisWeak.lock();
        if (pThis) {
          mSubscriptions.delegate()->onLocationDatabaseStateChanged(pThis, mState);
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::step()
      {
        ZS_LOG_DEBUG(log("step called") + toDebug())

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step calling cancel"))
          return;
        }

        if (!stepConflict()) goto not_ready;
        if (!stepShutdown()) goto not_ready;

        if (!stepMasterPromise()) goto not_ready;
        if (!stepRemoved()) goto not_ready;
        if (!stepOpenDB()) goto not_ready;
        if (!stepUpdate()) goto not_ready;

        if (!stepSubscribeLocationRemote()) goto not_ready;
        if (!stepRemoteSubscribe()) goto not_ready;
        if (!stepRemoteResubscribeTimer()) goto not_ready;

        if (!stepLocalIncomingSubscriptionsTimer()) goto not_ready;
        if (!stepLocalIncomingSubscriptionsNotify()) goto not_ready;

        setState(DatabaseState_Ready);

        if (!stepRemoteDownloadPendingRequestedData()) goto not_ready;
        if (!stepRemoteAutomaticDownloadAllData()) goto not_ready;

        if (!stepChangeNotify()) goto not_ready;

        ZS_LOG_TRACE(log("step complete") + toDebug())

      not_ready:
        {
        }
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepConflict()
      {
        if (!mNotifiedConflict) {
          ZS_LOG_TRACE(log("no conflict notified"))
          return true;
        }

        ZS_LOG_WARNING(Detail, log("database was told of conflict (thus must shutdown database)"))
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepShutdown()
      {
        if (!mNotifiedShutdown) {
          ZS_LOG_TRACE(log("no shutdown notified"))
          return true;
        }

        ZS_LOG_WARNING(Detail, log("database was told of databases shutdown (thus must shutdown this disconnected database)"))

        // NOTE: Database will be deleted by manager since all related databases are being purged from the system

        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepMasterPromise()
      {
        if (!mMasterReady) {
          ZS_LOG_DEBUG(log("creating promise"))
          mMasterReady = mDatabases->notifyWhenReady();
          mMasterReady->then(mThisWeak.lock());
        }

        if (!mMasterReady->isSettled()) {
          ZS_LOG_TRACE(log("waiting for master to be ready"))
          return false;
        }

        if (mMasterReady->isRejected()) {
          ZS_LOG_WARNING(Detail, log("master is shutdown (thus must shutdown)"))
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("promise is ready"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepRemoved()
      {
        if (!mNotifiedRemoved) {
          ZS_LOG_TRACE(log("no removal notified"))
          return true;
        }

        UseDatabasePtr master = mDatabases->getMasterDatabase();
        if (!master) {
          ZS_LOG_WARNING(Detail, log("could not obtain master database") + toDebug())
          cancel();
          return false;
        }

        mEntryDatabase.reset();

        UseDatabase::deleteDatabase(master, mLocation->getPeerURI(), mLocation->getLocationID(), mDatabaseID);

        ZS_LOG_WARNING(Detail, log("database was told that it needs to be removed (thus must shutdown database)"))
        cancel();
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabase::stepOpenDB()
      {
        if (mEntryDatabase) {
          ZS_LOG_TRACE(log("already openned entry database"))
          return true;
        }

        bool result = false;
        if (isLocal()) {
          if (mOpenOnly) {
            result = mDatabases->openLocalDB(mDatabaseID, mDatabaseRecord, mPeerAccessList);
          } else {
            result = mDatabases->createLocalDB(mDatabaseID, mMetaData, mPeerAccessList, mExpires, mDatabaseRecord);
          }
        } else {
          result = mDatabases->openRemoteDB(mDatabaseID, mDatabaseRecord);
        }

        if (!result) {
          ZS_LOG_WARNING(Detail, log("could not open or create database") + toDebug())
          cancel();
          return false;
        }

        UseDatabasePtr master = mDatabases->getMasterDatabase();
        if (!master) {
          ZS_LOG_WARNING(Detail, log("could not obtain master database") + toDebug())
          return false;
        }

        ZS_LOG_DEBUG(log("opening entry database") + toDebug())

        mEntryDatabase = UseDatabase::openDatabase(master, mLocation->getPeerURI(), mLocation->getLocationID(), mDatabaseID);
        if (!mEntryDatabase) {
          ZS_LOG_WARNING(Detail, log("could not open entry database"))
          return false;
        }
        
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepUpdate()
      {
        if (!mNotifiedRemoved) {
          ZS_LOG_TRACE(log("no update notified"))
          return true;
        }

        ZS_LOG_DEBUG(log("notified to update database record"))

        mDatabases->getDatabaseRecordUpdate(mDatabaseRecord);

        mNotifiedRemoved = false;

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepSubscribeLocationRemote()
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
      bool LocationDatabase::stepRemoteSubscribe()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping remote subscribe (is a local location)"))
          return true;
        }

        if (mRemoteSubscribeMonitor) {
          if (!mRemoteSubscribeMonitor->isComplete()) {
            ZS_LOG_TRACE(log("waiting for subscription to remote to complete"))
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

        ZS_LOG_DEBUG(log("subscribing to remote database"))

        Seconds expiresTimeout(UseSettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS));
        Time expires = zsLib::now() + expiresTimeout;

        SubscribeRequestPtr request = SubscribeRequest::create();
        request->databaseID(mDatabaseID);
        request->version(mDatabaseRecord.mLastDownloadedVersion);
        request->expires(expires);

        mRemoteSubscribeMonitor = IMessageMonitor::monitorAndSendToLocation(IMessageMonitorResultDelegate<SubscribeResult>::convert(mThisWeak.lock()), Location::convert(mLocation), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS)));
        
        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepRemoteResubscribeTimer()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("skipping create remote re-subscribe timer (is a local location)"))
          return true;
        }

        if (mRemoteSubscribeTimeout) {
          ZS_LOG_TRACE(log("already have remote subscribe list timeout"))
          return true;
        }

        Seconds expiresTimeout(UseSettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS));
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
      bool LocationDatabase::stepLocalIncomingSubscriptionsTimer()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping create of incoming subscription timer (is NOT a local location)"))
          return true;
        }

        if (mIncomingSubscriptionTimer) {
          ZS_LOG_TRACE(log("already have incoming subscription timer"))
          return true;
        }

        ZS_LOG_DEBUG(log("creating incoming subscription timer"))

        mIncomingSubscriptionTimer = Timer::create(mThisWeak.lock(), Seconds(60));
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabase::stepLocalIncomingSubscriptionsNotify()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping notification for incoming list subscriptions (is NOT a local location)"))
          return true;
        }

        for (auto iter = mIncomingSubscriptions.begin(); iter != mIncomingSubscriptions.end(); ++iter) {
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

          SubscribeNotifyPtr notify = SubscribeNotify::create();

          notify->messageID(subscription->mSubscribeMessageID);
          notify->before(subscription->mLastVersion);
          notify->expires(subscription->mExpires);

          EntryInfoListPtr notifyList = getUpdates(*subscription, subscription->mLastVersion, subscription->mDownloadData);
          if (!notifyList) {
            ZS_LOG_WARNING(Detail, log("did not find any database change records (despite having database records)"))
            goto notify_failure;
          }

          if (notifyList->size() > 0) {
            notify->entries(notifyList);
          } else {
            subscription->mNotified = true;
          }

          notify->version(subscription->mLastVersion);

          if (notify->version() == notify->before()) {
            ZS_LOG_DEBUG(log("nothing more to notify for subscription") + subscription->toDebug())
            subscription->mNotified = true;
            continue;
          }

          ZS_LOG_DEBUG(log("sending subscribe notifiction") + Message::toDebug(notify))

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
      bool LocationDatabase::stepRemoteDownloadPendingRequestedData()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("only remote databases can be downloaded"))
          return true;
        }

        if (mPendingWhenDataReady.size() < 1) {
          ZS_LOG_TRACE(log("no data is pending download at this time"))
          return true;
        }
        if (mDataGetRequestMonitor) {
          if (!mDataGetRequestMonitor->isComplete()) {
            ZS_LOG_TRACE(log("data get is still pending (try downlaoding again later)"))
            return true;
          }
          mDataGetRequestMonitor.reset();
        }

        if (mRemoteDataGetBackOffTimer) {
          Time nextRetry = mRemoteDataGetBackOffTimer->getNextRetryAfterTime();
          if (nextRetry > zsLib::now()) {
            ZS_LOG_TRACE(log("waiting to download data later"))
            return true;
          }
        }

        DataGetRequest::EntryIDList entries;

        DataGetRequestPtr request = DataGetRequest::create();
        request->databaseID(mDatabaseID);

        size_t loop = 0;
        for (auto iter = mPendingWhenDataReady.begin();
             (iter != mPendingWhenDataReady.end()) && (loop < OPENPEER_STACK_LOCATION_DATABASE_MAXIMUM_BATCH_DOWNLOAD_DATA_AT_ONCE);
             ++iter, ++loop) {
          auto entryID = (*iter).first;

          ZS_LOG_TRACE(log("will fetch data for") + ZS_PARAMIZE(entryID))

          mRequestedEntries.push_back(entryID);

          entries.push_back(entryID);
        }

        request->entries(entries);

        mDataGetRequestMonitor = IMessageMonitor::monitorAndSendToLocation(IMessageMonitorResultDelegate<DataGetResult>::convert(mThisWeak.lock()), Location::convert(mLocation), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepRemoteAutomaticDownloadAllData()
      {
        if (isLocal()) {
          ZS_LOG_TRACE(log("only remote databases can be downloaded"))
          return true;
        }

        if (!mAutomaticallyDownloadAllEntries) {
          ZS_LOG_TRACE(log("not told to automatically download all entries (skipping step)"))
          return true;
        }
        if (mDataGetRequestMonitor) {
          ZS_LOG_TRACE(log("data get is still pending (try downlaoding again later)"))
          return true;
        }

        if (mRemoteDataGetBackOffTimer) {
          Time nextRetry = mRemoteDataGetBackOffTimer->getNextRetryAfterTime();
          if (nextRetry > zsLib::now()) {
            ZS_LOG_TRACE(log("waiting to download data later"))
            return true;
          }
        }

        UseDatabase::EntryRecordListPtr batch = mEntryDatabase->entryTable()->getBatchMissingData(mLastRequestedIndex);
        if (!batch)  batch = UseDatabase::EntryRecordListPtr(new UseDatabase::EntryRecordList);

        if (batch->size() < 1) {
          ZS_LOG_TRACE(log("no more entries needing to be fetched at this time"))

          mDownloadedAllData = true;
          return true;
        }

        DataGetRequest::EntryIDList entries;

        DataGetRequestPtr request = DataGetRequest::create();
        request->databaseID(mDatabaseID);

        for (auto iter = batch->begin(); iter != batch->end(); ++iter) {
          auto record = (*iter);

          ZS_LOG_TRACE(log("will fetch data for") + ZS_PARAMIZE(record.mEntryID))

          entries.push_back(record.mEntryID);

          mRequestedEntries.push_back(record.mEntryID);

          mLastRequestedIndexUponSuccess = record.mIndex;
        }

        request->entries(entries);

        mDataGetRequestMonitor = IMessageMonitor::monitorAndSendToLocation(IMessageMonitorResultDelegate<DataGetResult>::convert(mThisWeak.lock()), Location::convert(mLocation), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool LocationDatabase::stepChangeNotify()
      {
        if (mDatabaseRecord.mUpdateVersion == mLastChangeUpdateNotified) {
          ZS_LOG_TRACE(log("no change notification required"))
          return true;
        }

        ZS_LOG_DEBUG(log("notified of database changed event") + ZS_PARAMIZE(mDatabaseRecord.mUpdateVersion) + ZS_PARAMIZE(mLastChangeUpdateNotified))

        mSubscriptions.delegate()->onLocationDatabaseUpdated(mThisWeak.lock());
        mLastChangeUpdateNotified = mDatabaseRecord.mUpdateVersion;
        return true;
      }
      
      //-----------------------------------------------------------------------
      void LocationDatabase::notifyIncoming(SubscribeNotifyPtr notify)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notified of incoming subscription notification") + Message::toDebug(notify))

        {
          if (!mEntryDatabase) {
            ZS_LOG_WARNING(Detail, log("entry database is not ready thus cannot accept list subscribe notification"))
            return;
          }

          if (notify->before() != mDatabaseRecord.mLastDownloadedVersion) {
            ZS_LOG_WARNING(Detail, log("cannot accept list subscribe notification due to conflicting version") + ZS_PARAM("version", notify->before()) + ZS_PARAM("expecting", mDatabaseRecord.mLastDownloadedVersion))
            return;
          }

          auto entries = notify->entries();
          if (!entries) {
            ZS_LOG_TRACE("nothing new notified")
            mDatabaseRecord.mLastDownloadedVersion = notify->version();
            mDatabases->notifyDownloaded(mDatabaseID, notify->version());
            goto check_subscription_must_terminate;
          }

          processEntries(*entries);

          mDatabaseRecord.mLastDownloadedVersion = notify->version();
          mDatabases->notifyDownloaded(mDatabaseID, notify->version());
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
      void LocationDatabase::processEntries(const EntryInfoList &entries)
      {
        bool added = false;
        bool changed = false;

        for (auto iter = entries.begin(); iter != entries.end(); ++iter) {
          auto entryInfo = (*iter);

          ZS_LOG_TRACE(log("notified about entry") + entryInfo.toDebug())

          switch (entryInfo.mDisposition) {
            case EntryInfo::Disposition_None:
            case EntryInfo::Disposition_Add:
            case EntryInfo::Disposition_Update:  {
              UseDatabase::EntryRecord record;
              record.mEntryID = entryInfo.mEntryID;
              record.mVersion = entryInfo.mVersion;
              record.mMetaData = entryInfo.mMetaData;
              record.mData = entryInfo.mData;
              record.mDataLength = entryInfo.mDataSize;
              record.mCreated = entryInfo.mCreated;
              record.mUpdated = entryInfo.mUpdated;

              bool result = false;
              UseDatabase::EntryChangeRecord changeRecord;
              if (EntryInfo::Disposition_Add == entryInfo.mDisposition) {
                result = mEntryDatabase->entryTable()->add(record, changeRecord);
              } else {
                result = mEntryDatabase->entryTable()->update(record, changeRecord);
              }

              if ((record.mData) ||
                  (0 == record.mDataLength)) {
                auto found = mPendingWhenDataReady.find(record.mEntryID);
                if (found != mPendingWhenDataReady.end()) {
                  auto promiseList = (*found).second;

                  EntryInfoPtr promiseInfo(new EntryInfo);
                  promiseInfo->mEntryID = record.mEntryID;
                  promiseInfo->mVersion = record.mVersion;
                  promiseInfo->mMetaData = record.mMetaData;
                  promiseInfo->mData = record.mData;
                  promiseInfo->mDataSize = record.mDataLength;
                  promiseInfo->mCreated = record.mCreated;
                  promiseInfo->mUpdated = record.mUpdated;

                  PromisePtr broadcast = Promise::broadcast(promiseList);
                  broadcast->resolve(promiseInfo);
                  mPendingWhenDataReady.erase(found);
                }
              }

              if ((UseDatabase::EntryChangeRecord::Disposition_None != changeRecord.mDisposition) &&
                  (result)) {
                ZS_LOG_TRACE(log("add or updated database record") + record.toDebug() + changeRecord.toDebug())

                downloadAgain();

                mEntryDatabase->entryChangeTable()->insert(changeRecord);
                added = added || (UseDatabase::EntryChangeRecord::Disposition_Add == changeRecord.mDisposition);
                changed = changed || (UseDatabase::EntryChangeRecord::Disposition_Add != changeRecord.mDisposition);
              }
              break;
            }
            case EntryInfo::Disposition_Remove:  {
              UseDatabase::EntryRecord record;
              record.mEntryID = entryInfo.mEntryID;
              UseDatabase::EntryChangeRecord changeRecord;
              mEntryDatabase->entryTable()->remove(record, changeRecord);
              if (UseDatabase::EntryChangeRecord::Disposition_None != changeRecord.mDisposition) {
                ZS_LOG_DEBUG(log("removed entry record") + record.toDebug() + changeRecord.toDebug())

                downloadAgain();

                mEntryDatabase->entryChangeTable()->insert(changeRecord);
                changed = true;
              }
              break;
            }
          }
        }

        if (changed) {
          mDatabaseRecord.mUpdateVersion = mDatabases->updateVersion(mDatabaseID);
          ZS_LOG_DEBUG(log("registered peer location databases has changed") + ZS_PARAMIZE(mDatabaseRecord.mUpdateVersion))
        } else if (added) {
          ZS_LOG_DEBUG(log("registered peer location databases has additions") + ZS_PARAMIZE(mDatabaseRecord.mUpdateVersion) + ZS_PARAMIZE(mLastChangeUpdateNotified))
          mLastChangeUpdateNotified = "added";  // by setting to added it will notify delegates (but doesn't actually cause a version update change)
        } else {
          ZS_LOG_DEBUG(log("no changes to database were found"))
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::downloadAgain()
      {
        if (!mDownloadedAllData) {
          ZS_LOG_TRACE(log("still downloading data"))
          return;
        }

        ZS_LOG_DEBUG(log("will require downloading again (potentially)"))

        mDownloadedAllData = false;
        mLastRequestedIndex = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN;
        mLastRequestedIndexUponSuccess = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyIncoming(
                                            UseMessageIncomingPtr message,
                                            SubscribeRequestPtr request
                                            )
      {
        ZS_LOG_DEBUG(log("notified of incoming subscription") + UseMessageIncoming::toDebug(message) + Message::toDebug(request))

        AutoRecursiveLock lock(*this);

        if (!mEntryDatabase) {
          step();
        }

        ZS_LOG_DEBUG(log("notified of incoming list subscription request") + UseMessageIncoming::toDebug(message) + Message::toDebug(request))

        IHTTP::HTTPStatusCodes errorCode = IHTTP::HTTPStatusCode_Conflict;
        String errorReason;

        IncomingSubscriptionMap::iterator found = mIncomingSubscriptions.end();

        {
          if (isShutdown()) {
            errorCode = IHTTP::HTTPStatusCode_Gone;
            goto fail_incoming_subscribe;
          }

          UseLocationPtr location = message->getLocation();
          if (!location) {
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            goto fail_incoming_subscribe;
          }

          if (mIncomingSubscriptions.size() < 1) {
            ZS_LOG_TRACE(log("creating temporary reference to self to keep it alive for a period of time (just in case)"))
            Seconds wakeUpTime(UseSettings::getUInt(OPENPEER_STACK_SETTING_LOCATION_DATABASE_WAKE_TIME_AFTER_INCOMING_DATABASE_REQUEST_IN_SECONDS));

            TimerPtr timer = Timer::create(mThisWeak.lock(), zsLib::now() + wakeUpTime);
            mTimedReferences[timer] = mThisWeak.lock();
          }

          IncomingSubscriptionPtr subscription;
          found = mIncomingSubscriptions.find(location->getID());
          if (found == mIncomingSubscriptions.end()) {
            if ((Time() == request->expires()) ||
                (request->expires() < zsLib::now())) {
              // immediate expiry, nothing to do...
              SubscribeResultPtr result = SubscribeResult::create(request);
              message->sendResponse(result);
              return;
            }
            subscription = IncomingSubscriptionPtr(new IncomingSubscription);
            subscription->mSelf = mThisWeak.lock();
            subscription->mLocation = location;
            subscription->mLocationSubscription = UseLocationSubscription::subscribe(Location::convert(location), mThisWeak.lock());

            mIncomingSubscriptions[location->getID()] = subscription;
            found = mIncomingSubscriptions.find(location->getID());
          } else {
            subscription = (*found).second;
          }

          subscription->mSubscribeMessageID = request->messageID();
          subscription->mDownloadData = request->data();
          subscription->mExpires = request->expires();
          subscription->mLastVersion = request->version();

          if (!mEntryDatabase) {
            ZS_LOG_WARNING(Detail, log("entry database is gone"))
            errorCode = IHTTP::HTTPStatusCode_Gone;
            errorReason = "Database is not currently available.";
            goto fail_incoming_subscribe;
          }

          if (!validateVersionInfo(request->version(), *subscription)) {
            ZS_LOG_WARNING(Detail, log("request version mismatch") + ZS_PARAM("version", request->version()))
            goto fail_incoming_subscribe;
          }

          ZS_LOG_DEBUG(log("accepting incoming subscription") + subscription->toDebug())

          SubscribeResultPtr result = SubscribeResult::create(request);
          message->sendResponse(result);

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

      fail_incoming_subscribe:
        {
          if (found != mIncomingSubscriptions.end()) {
            ZS_LOG_WARNING(Detail, log("removing incoming subscription"))
            mIncomingSubscriptions.erase(found);
          }
          MessageResultPtr result = MessageResult::create(request);
          result->errorCode(errorCode);
          result->errorReason(errorReason.isEmpty() ? String(IHTTP::toString(errorCode)) : errorReason);
          message->sendResponse(result);
        }
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyIncoming(
                                            UseMessageIncomingPtr message,
                                            DataGetRequestPtr request
                                            )
      {
        ZS_LOG_DEBUG(log("notified of incoming subscription") + UseMessageIncoming::toDebug(message) + Message::toDebug(request))

        AutoRecursiveLock lock(*this);

        if (!mEntryDatabase) {
          step();
        }

        ZS_LOG_DEBUG(log("notified of incoming list subscription request") + UseMessageIncoming::toDebug(message) + Message::toDebug(request))

        IHTTP::HTTPStatusCodes errorCode = IHTTP::HTTPStatusCode_Conflict;
        String errorReason;

        {
          if (isShutdown()) {
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            goto fail_incoming_dataget;
          }

          if (!mEntryDatabase) {
            errorCode = IHTTP::HTTPStatusCode_InternalServerError;
            goto fail_incoming_dataget;
          }

          EntryInfoListPtr entries(new EntryInfoList);

          for (auto iter = request->entries().begin(); iter != request->entries().end(); ++iter)
          {
            auto entryID = (*iter);

            UseDatabase::EntryRecord entryRecord;
            entryRecord.mEntryID = entryID;
            if (!mEntryDatabase->entryTable()->getEntry(entryRecord, true)) {
              ZS_LOG_WARNING(Detail, log("entry id was not found") + entryRecord.toDebug())
              continue;
            }

            EntryInfo info;
            info.mDisposition = EntryInfo::Disposition_Add;
            info.mEntryID = entryRecord.mEntryID;
            info.mVersion = entryRecord.mVersion;
            info.mCreated = entryRecord.mCreated;
            info.mUpdated = entryRecord.mUpdated;
            info.mMetaData = entryRecord.mMetaData;
            info.mData = entryRecord.mData;

            entries->push_back(info);
          }


          ZS_LOG_DEBUG(log("responding incoming data get"))

          DataGetResultPtr result = DataGetResult::create(request);
          result->entries(entries);
          message->sendResponse(result);

          if (mIncomingSubscriptions.size() > 0) {
            ZS_LOG_TRACE(log("already have incoming subscription thus no need for a data-get keep alive mechanism"))
            return;
          }

          Seconds wakeUpTime(UseSettings::getUInt(OPENPEER_STACK_SETTING_LOCATION_DATABASE_WAKE_TIME_AFTER_INCOMING_DATABASE_REQUEST_IN_SECONDS));

          TimerPtr timer = Timer::create(mThisWeak.lock(), zsLib::now() + wakeUpTime);
          mTimedReferences[timer] = mThisWeak.lock();
          return;
        }

      fail_incoming_dataget:
        {
          MessageResultPtr result = MessageResult::create(request);
          result->errorCode(errorCode);
          result->errorReason(errorReason.isEmpty() ? String(IHTTP::toString(errorCode)) : errorReason);
          message->sendResponse(result);
        }
      }
      
      //-----------------------------------------------------------------------
      EntryInfo::Dispositions LocationDatabase::toDisposition(UseDatabase::EntryChangeRecord::Dispositions disposition)
      {
        switch (disposition) {
          case UseDatabase::EntryChangeRecord::Disposition_None:   break;
          case UseDatabase::EntryChangeRecord::Disposition_Add:    return EntryInfo::Disposition_Add;
          case UseDatabase::EntryChangeRecord::Disposition_Update: return EntryInfo::Disposition_Update;
          case UseDatabase::EntryChangeRecord::Disposition_Remove: return EntryInfo::Disposition_Remove;
        }
        return EntryInfo::Disposition_None;
      }

      //-----------------------------------------------------------------------
      EntryInfoListPtr LocationDatabase::getUpdates(
                                                    VersionedData &ioVersionData,
                                                    String &outLastVersion,
                                                    bool includeData
                                                    ) const
      {
        EntryInfoListPtr result(new EntryInfoList);

        switch (ioVersionData.mType) {
          case VersionedData::VersionType_FullList: {
            UseDatabase::EntryRecordListPtr batch = mEntryDatabase->entryTable()->getBatch(includeData, ioVersionData.mAfterIndex);
            if (!batch) batch = UseDatabase::EntryRecordListPtr(new UseDatabase::EntryRecordList);

            ZS_LOG_TRACE(log("getting batch of entries to notify") + ZS_PARAM("size", batch->size()))

            for (auto batchIter = batch->begin(); batchIter != batch->end(); ++batchIter) {
              auto entryRecord = (*batchIter);

              EntryInfo info;
              info.mDisposition = EntryInfo::Disposition_Add;
              info.mEntryID = entryRecord.mEntryID;
              info.mVersion = entryRecord.mVersion;
              info.mCreated = entryRecord.mCreated;
              info.mUpdated = entryRecord.mUpdated;
              info.mMetaData = entryRecord.mMetaData;
              info.mData = entryRecord.mData;
              info.mDataSize = entryRecord.mDataLength;

              ZS_LOG_TRACE(log("found additional updated record") + info.toDebug())

              result->push_back(info);

              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(entryRecord.mEntryID + ":" + mDatabaseRecord.mUpdateVersion));
              outLastVersion = String(VersionedData::toString(VersionedData::VersionType_FullList)) + "-" + hash + "-" + string(entryRecord.mIndex);
            }

            if (batch->size() < 1) {
              ZS_LOG_DEBUG(log("no additional database records found"))
              // nothing more to notify

              if (OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN != ioVersionData.mAfterIndex) {
                ZS_LOG_TRACE(log("switching to change list subscription type"))
                UseDatabase::EntryChangeRecord changeRecord;
                auto result = mEntryDatabase->entryChangeTable()->getLast(changeRecord);
                if (!result) {
                  ZS_LOG_WARNING(Detail, log("did not find any database change records (despite having database records)"))
                  return EntryInfoListPtr();
                }

                ioVersionData.mType = VersionedData::VersionType_ChangeList;
                ioVersionData.mAfterIndex = changeRecord.mIndex;

                String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(changeRecord.mDisposition) + ":" + mDatabaseRecord.mDatabaseID + ":" + changeRecord.mEntryID));
                outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);
              } else {
                outLastVersion = String();
              }
            }
            break;
          }
          case VersionedData::VersionType_ChangeList: {
            UseDatabase::EntryChangeRecordListPtr batch;
            batch = mEntryDatabase->entryChangeTable()->getBatch(ioVersionData.mAfterIndex);
            if (!batch) batch = UseDatabase::EntryChangeRecordListPtr(new UseDatabase::EntryChangeRecordList);

            ZS_LOG_TRACE(log("getting batch of database changes to notify") + ZS_PARAM("size", batch->size()))

            for (auto batchIter = batch->begin(); batchIter != batch->end(); ++batchIter) {
              auto changeRecord = (*batchIter);

              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(changeRecord.mDisposition) + ":" + mDatabaseRecord.mDatabaseID + ":" + changeRecord.mEntryID));
              outLastVersion = String(VersionedData::toString(VersionedData::VersionType_ChangeList)) + "-" + hash + "-" + string(changeRecord.mIndex);

              EntryInfo info;
              info.mDisposition = toDisposition(changeRecord.mDisposition);
              info.mEntryID = changeRecord.mEntryID;

              if (EntryInfo::Disposition_Remove != info.mDisposition) {
                UseDatabase::EntryRecord entryRecord;
                entryRecord.mEntryID = changeRecord.mEntryID;
                auto result = mEntryDatabase->entryTable()->getEntry(entryRecord, includeData);
                if (result) {
                  if (EntryInfo::Disposition_Update == info.mDisposition) {
                  } else {
                    info.mMetaData = entryRecord.mMetaData;
                    info.mCreated = entryRecord.mCreated;
                  }
                  info.mVersion = entryRecord.mVersion;
                  info.mUpdated = entryRecord.mUpdated;
                  info.mData = entryRecord.mData;
                  if (!includeData) {
                    info.mDataSize = entryRecord.mDataLength;
                  }
                } else {
                  ZS_LOG_WARNING(Detail, log("did not find any related database record (thus must have been removed later)"))
                  info.mDisposition = EntryInfo::Disposition_Remove;
                }
              }

              ZS_LOG_TRACE(log("found additional updated record") + info.toDebug())
              result->push_back(info);
            }
            break;
          }
        }
        
        ZS_LOG_TRACE(log("found changes") + ioVersionData.toDebug() + ZS_PARAMIZE(outLastVersion) + ZS_PARAM("size", result->size()))
        return result;
      }
      
      //-----------------------------------------------------------------------
      bool LocationDatabase::validateVersionInfo(
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
            UseDatabase::EntryRecord record;

            record.mIndex = outVersionData.mAfterIndex;

            if (OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN != outVersionData.mAfterIndex) {
              auto result = mEntryDatabase->entryTable()->getEntry(record, false);
              if (!result) {
                ZS_LOG_WARNING(Detail, log("database entry record was not found (thus must be conflict)") + outVersionData.toDebug())
                return false;
              }
            }

            if (updateVersion.hasData()) {
              String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(mDatabaseRecord.mDatabaseID + ":" + mDatabaseRecord.mUpdateVersion));

              if (hash != updateVersion) {
                ZS_LOG_WARNING(Detail, log("version does not match current version") + ZS_PARAMIZE(version) + record.toDebug() + mDatabaseRecord.toDebug() + ZS_PARAMIZE(hash))
                return false;
              }
            }

            break;
          }
          case VersionedData::VersionType_ChangeList: {
            UseDatabase::EntryChangeRecord record;
            auto result = mEntryDatabase->entryChangeTable()->getByIndex(outVersionData.mAfterIndex, record);
            if (!result) {
              ZS_LOG_WARNING(Detail, log("database entry record was not found (thus must be conflict)"))
              return false;
            }

            String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(string(record.mDisposition) + ":" + mDatabaseRecord.mDatabaseID + ":" + record.mEntryID));
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
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase::VersionedData
      #pragma mark

      //-----------------------------------------------------------------------
      const char *LocationDatabase::VersionedData::toString(VersionTypes type)
      {
        switch (type) {
          case VersionType_FullList:   return OPENPEER_STACK_P2P_DATABASE_TYPE_FULL_LIST_VERSION;
          case VersionType_ChangeList: return OPENPEER_STACK_P2P_DATABASE_TYPE_CHANGE_LIST_VERSION;
        }
        return "unknown";
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::VersionedData::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabase::VersionedData");

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
      LocationDatabase::IncomingSubscription::IncomingSubscription()
      {
      }

      //-----------------------------------------------------------------------
      LocationDatabase::IncomingSubscription::~IncomingSubscription()
      {
        if (mLocationSubscription) {
          mLocationSubscription->cancel();
          mLocationSubscription.reset();
        }
        if (mPendingSendPromise) {
          mPendingSendPromise->reject();
          mPendingSendPromise.reset();
        }
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::IncomingSubscription::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabase::IncomingSubscription");

        UseServicesHelper::debugAppend(resultEl, "self", (bool)mSelf);

        UseServicesHelper::debugAppend(resultEl, "location subscription", mLocationSubscription ? mLocationSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "location", UseLocation::toDebug(mLocation));

        UseServicesHelper::debugAppend(resultEl, "subscribe message id", mSubscribeMessageID);

        UseServicesHelper::debugAppend(resultEl, "type", toString(mType));
        UseServicesHelper::debugAppend(resultEl, "after index", mAfterIndex);

        UseServicesHelper::debugAppend(resultEl, "last version", mLastVersion);

        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        UseServicesHelper::debugAppend(resultEl, "download data", mDownloadData);
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
      ILocationDatabaseFactory &ILocationDatabaseFactory::singleton()
      {
        return LocationDatabaseFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ILocationDatabasePtr ILocationDatabaseFactory::open(
                                                          ILocationDatabaseDelegatePtr inDelegate,
                                                          ILocationPtr location,
                                                          const char *databaseID,
                                                          bool inAutomaticallyDownloadDatabaseData
                                                          )
      {
        if (this) {}
        return LocationDatabase::open(inDelegate, location, databaseID, inAutomaticallyDownloadDatabaseData);
      }

      //-----------------------------------------------------------------------
      ILocationDatabaseLocalPtr ILocationDatabaseFactory::create(
                                                                 ILocationDatabaseLocalDelegatePtr inDelegate,
                                                                 IAccountPtr account,
                                                                 const char *inDatabaseID,
                                                                 ElementPtr inMetaData,
                                                                 const PeerList &inPeerAccessList,
                                                                 Time expires
                                                                 )
      {
        if (this) {}
        return LocationDatabase::create(inDelegate, account, inDatabaseID, inMetaData, inPeerAccessList, expires);
      }

      //-----------------------------------------------------------------------
      bool ILocationDatabaseFactory::remove(
                                            IAccountPtr account,
                                            const char *inDatabaseID
                                            )
      {
        if (this) {}
        return LocationDatabase::remove(account, inDatabaseID);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr ILocationDatabaseFactory::openRemote(
                                                               LocationDatabasesPtr databases,
                                                               const String &databaseID,
                                                               bool downloadAllEntries
                                                               )
      {
        if (this) {}
        return LocationDatabase::openRemote(databases, databaseID, downloadAllEntries);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr ILocationDatabaseFactory::openLocal(
                                                              LocationDatabasesPtr databases,
                                                              const String &databaseID
                                                              )
      {
        if (this) {}
        return LocationDatabase::openLocal(databases, databaseID);
      }

      //-----------------------------------------------------------------------
      LocationDatabasePtr ILocationDatabaseFactory::createLocal(
                                                                LocationDatabasesPtr databases,
                                                                const String &databaseID,
                                                                ElementPtr inMetaData,
                                                                const PeerList &inPeerAccessList,
                                                                Time expires
                                                                )
      {
        if (this) {}
        return LocationDatabase::createLocal(databases, databaseID, inMetaData, inPeerAccessList, expires);
      }
    }

    //-------------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseTypes
    #pragma mark

    //-----------------------------------------------------------------------
    const char *ILocationDatabaseTypes::toString(DatabaseStates states)
    {
      switch (states) {
        case DatabaseState_Pending:    return "Pending";
        case DatabaseState_Ready:      return "Ready";
        case DatabaseState_Shutdown:   return "Shutdown";
      }
      return "Unknown";
    }

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseTypes::EntryInfo
    #pragma mark

    //---------------------------------------------------------------------
    static Log::Params slog_DatabaseEntryInfo(const char *message)
    {
      return Log::Params(message, "DatabaseEntryInfo");
    }

    //-----------------------------------------------------------------------
    const char *ILocationDatabaseTypes::EntryInfo::toString(Dispositions disposition)
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
    ILocationDatabaseTypes::EntryInfo::Dispositions ILocationDatabaseTypes::EntryInfo::toDisposition(const char *inStr)
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
    bool ILocationDatabaseTypes::EntryInfo::hasData() const
    {
      return ((Disposition_None != mDisposition) ||
              (mEntryID.hasData()) ||
              (0 != mVersion) ||
              ((bool)mMetaData) ||
              ((bool)mData) ||
              (0 != mDataSize) ||
              (Time() != mCreated) ||
              (Time() != mUpdated));
    }

    //-----------------------------------------------------------------------
    ElementPtr ILocationDatabaseTypes::EntryInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("ILocationDatabaseTypes::EntryInfo");

      UseServicesHelper::debugAppend(resultEl, "disposition", toString(mDisposition));
      UseServicesHelper::debugAppend(resultEl, "entry id", mEntryID);
      UseServicesHelper::debugAppend(resultEl, "version", mVersion);
      UseServicesHelper::debugAppend(resultEl, "meta data", UseServicesHelper::toString(mMetaData));
      UseServicesHelper::debugAppend(resultEl, "data", (bool)mData);
      UseServicesHelper::debugAppend(resultEl, "size", mDataSize);
      UseServicesHelper::debugAppend(resultEl, "created", mCreated);
      UseServicesHelper::debugAppend(resultEl, "updated", mUpdated);

      return resultEl;
    }

    //-----------------------------------------------------------------------
    ILocationDatabaseTypes::EntryInfo ILocationDatabaseTypes::EntryInfo::create(ElementPtr elem)
    {
      EntryInfo info;

      if (!elem) return info;

      info.mDisposition = EntryInfo::toDisposition(elem->getAttributeValue("disposition"));
      info.mEntryID = UseMessageHelper::getAttributeID(elem);

      String versionStr = UseMessageHelper::getAttribute(elem, "version");
      try {
        info.mVersion = Numeric<decltype(info.mVersion)>(versionStr);
      } catch(Numeric<decltype(info.mVersion)>::ValueOutOfRange &) {
        ZS_LOG_WARNING(Detail, slog_DatabaseEntryInfo("failed to convert") + ZS_PARAMIZE(versionStr))
      }

      info.mMetaData = elem->findFirstChildElement("metaData");
      if (info.mMetaData) {
        info.mMetaData = info.mMetaData->clone()->toElement();
      }

      info.mData = elem->findFirstChildElement("data");
      if (info.mData) {
        info.mData = info.mData->clone()->toElement();
      }

      String dataSizeStr = UseMessageHelper::getElementText(elem->findFirstChildElement("size"));
      try {
        info.mDataSize = Numeric<decltype(info.mDataSize)>(dataSizeStr);
      } catch(Numeric<decltype(info.mDataSize)>::ValueOutOfRange &) {
        ZS_LOG_WARNING(Detail, slog_DatabaseEntryInfo("failed to convert") + ZS_PARAMIZE(dataSizeStr))
      }

      info.mCreated = UseServicesHelper::stringToTime(UseMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("created")));
      info.mUpdated = UseServicesHelper::stringToTime(UseMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));

      return info;
    }

    //-----------------------------------------------------------------------
    ElementPtr ILocationDatabaseTypes::EntryInfo::createElement() const
    {
      ElementPtr entryEl = UseMessageHelper::createElementWithTextID("entry", mEntryID);

      if (Disposition_None != mDisposition) {
        UseMessageHelper::setAttributeWithText(entryEl, "disposition", toString(mDisposition));
      }

      if (0 != mVersion) {
        UseMessageHelper::setAttributeWithNumber(entryEl, "version", internal::string(mVersion));
      }

      if (mMetaData) {
        if (mMetaData->getValue() != "metaData") {
          ElementPtr metaDataEl = Element::create("metaData");
          metaDataEl->adoptAsLastChild(mMetaData->clone());
          entryEl->adoptAsLastChild(metaDataEl);
        } else {
          entryEl->adoptAsLastChild(mMetaData->clone());
        }
      }
      if (mData) {
        if (mData->getValue() != "data") {
          ElementPtr dataEl = Element::create("data");
          dataEl->adoptAsLastChild(mData->clone());
          entryEl->adoptAsLastChild(dataEl);
        } else {
          entryEl->adoptAsLastChild(mData->clone());
        }
      }

      if (0 != mDataSize) {
        entryEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("size", internal::string(mDataSize)));
      }

      if (Time() != mCreated) {
        entryEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("created", UseServicesHelper::timeToString(mCreated)));
      }
      if (Time() != mUpdated) {
        entryEl->adoptAsLastChild(UseMessageHelper::createElementWithNumber("updated", UseServicesHelper::timeToString(mUpdated)));
      }

      return entryEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabase
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr ILocationDatabase::toDebug(ILocationDatabasePtr object)
    {
      return internal::LocationDatabase::toDebug(object);
    }

    //-------------------------------------------------------------------------
    ILocationDatabasePtr ILocationDatabase::open(
                                                 ILocationDatabaseDelegatePtr inDelegate,
                                                 ILocationPtr location,
                                                 const char *databaseID,
                                                 bool inAutomaticallyDownloadDatabaseData
                                                 )
    {
      return internal::ILocationDatabaseFactory::singleton().open(inDelegate, location, databaseID, inAutomaticallyDownloadDatabaseData);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseLocal
    #pragma mark

    //-------------------------------------------------------------------------
    ILocationDatabaseLocalPtr ILocationDatabaseLocal::toLocal(ILocationDatabasePtr object)
    {
      return internal::LocationDatabase::toLocal(object);
    }

    //-------------------------------------------------------------------------
    ILocationDatabaseLocalPtr ILocationDatabaseLocal::create(
                                                             ILocationDatabaseLocalDelegatePtr inDelegate,
                                                             IAccountPtr account,
                                                             const char *inDatabaseID,
                                                             ElementPtr inMetaData,
                                                             const PeerList &inPeerAccessList,
                                                             Time expires
                                                             )
    {
      return internal::ILocationDatabaseFactory::singleton().create(inDelegate, account, inDatabaseID, inMetaData, inPeerAccessList, expires);
    }

    //-------------------------------------------------------------------------
    bool ILocationDatabaseLocal::remove(
                                        IAccountPtr account,
                                        const char *inDatabaseID
                                        )
    {
      return internal::ILocationDatabaseFactory::singleton().remove(account, inDatabaseID);
    }
  }
}
