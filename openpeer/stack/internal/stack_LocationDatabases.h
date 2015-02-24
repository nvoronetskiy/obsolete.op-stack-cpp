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

#pragma once

#include <openpeer/stack/internal/types.h>
#include <openpeer/stack/internal/stack_ILocationDatabaseAbstraction.h>

#include <openpeer/stack/message/p2p-database/ListSubscribeResult.h>

#include <openpeer/stack/ILocationDatabaseLocal.h>
#include <openpeer/stack/ILocationDatabases.h>
#include <openpeer/stack/ILocationSubscription.h>
#include <openpeer/stack/IServiceLockbox.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IBackOffTimer.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/Promise.h>
#include <zsLib/Timer.h>

#define OPENPEER_STACK_P2P_DATABASE_TYPE_FULL_LIST_VERSION "flv"
#define OPENPEER_STACK_P2P_DATABASE_TYPE_CHANGE_LIST_VERSION "clv"

#define OPENPEER_STACK_SETTING_P2P_DATABASE_REQUEST_TIMEOUT_IN_SECONDS "openpeer/stack/p2p-database-request-timeout-in-seconds"
#define OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS "openpeer/stack/p2p-database-list-subscription-expires-timeout-in-seconds"
#define OPENPEER_STACK_SETTING_P2P_DATABASE_LIST_SUBSCRIPTION_BACK_OFF_TIMER_IN_SECONDS "openpeer/stack/p2p-database-list-subscription-back-off-timer-in-seconds"

#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_INTERACTION_PTR(IAccountForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationSubscriptionForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabasesManagerForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabaseForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(IMessageIncomingForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(IServiceLockboxSessionForLocationDatabases)

      //-----------------------------------------------------------------------
      interaction ILocationDatabasesForLocationDatabase
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabase, ForLocationDatabase)

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, UseLocationDatabase)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction, UseDatabase)

        typedef ILocationDatabaseLocalTypes::PeerList PeerList;

        static ElementPtr toDebug(ForLocationDatabasePtr object);

        virtual PUID getID() const = 0;

        virtual ILocationPtr getLocation() const = 0;

        virtual bool isLocal() const = 0;

        virtual LocationDatabasePtr getOrCreate(
                                                const String &databaseID,
                                                bool downloadAllEntries
                                                ) = 0;

        virtual LocationDatabasePtr getOrCreate(
                                                const String &databaseID,
                                                ElementPtr inMetaData,
                                                const PeerList &inPeerAccessList,
                                                Time expires
                                                ) = 0;

        virtual void notifyShutdown(UseLocationDatabase &database) = 0;
        virtual void notifyDestroyed(UseLocationDatabase &database) = 0;

        virtual PromisePtr notifyWhenReady() = 0;

        virtual UseDatabasePtr getMasterDatabase() = 0;

        virtual bool openRemoteDB(
                                  const String &databaseID,
                                  UseDatabase::DatabaseRecord &outRecord
                                  ) = 0;
        virtual bool openLocalDB(
                                 const String &databaseID,
                                 UseDatabase::DatabaseRecord &outRecord,
                                 PeerList &outPeerAccessList
                                 ) = 0;

        virtual bool createLocalDB(
                                   const String &databaseID,
                                   ElementPtr inMetaData,
                                   const PeerList &inPeerAccessList,
                                   Time expires,
                                   UseDatabase::DatabaseRecord &outRecord
                                   ) = 0;

        virtual void notifyDownloaded(
                                      const String &databaseID,
                                      const String &downloadedVersion
                                      ) = 0;

        virtual String updateVersion(const String &databaseID) = 0;

        virtual void getDatabaseRecordUpdate(UseDatabase::DatabaseRecord &ioRecord) const = 0;
      };
      
      //-----------------------------------------------------------------------
      interaction ILocationDatabasesForLocationDatabasesManager
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabasesManager, ForLocationDatabasesManager)

        static ElementPtr toDebug(ForLocationDatabasesManagerPtr object);

        static ForLocationDatabasesManagerPtr create(ILocationPtr location);

        virtual PUID getID() const = 0;

        virtual ILocationPtr getLocation() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases
      #pragma mark

      class LocationDatabases : public SharedRecursiveLock,
                                public MessageQueueAssociator,
                                public Noop,
                                public ILocationDatabases,
                                public ILocationDatabasesForLocationDatabase,
                                public ILocationDatabasesForLocationDatabasesManager,
                                public IWakeDelegate,
                                public IPromiseSettledDelegate,
                                public ITimerDelegate,
                                public IBackOffTimerDelegate,
                                public IServiceLockboxSessionDelegate,
                                public ILocationSubscriptionDelegate,
                                public IMessageMonitorResultDelegate<message::p2p_database::ListSubscribeResult>
      {
      public:
        friend interaction ILocationDatabasesFactory;
        friend interaction ILocationDatabases;
        friend interaction ILocationDatabasesForLocationDatabasesManager;
        friend interaction ILocationDatabasesForLocationDatabase;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocationDatabases, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationDatabases, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(ILocationSubscriptionForLocationDatabases, UseLocationSubscription)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesManagerForLocationDatabases, UseLocationDatabasesManager)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, UseLocationDatabase)
        ZS_DECLARE_TYPEDEF_PTR(IMessageIncomingForLocationDatabases, UseMessageIncoming)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForLocationDatabases, UseLockbox)

        ZS_DECLARE_TYPEDEF_PTR(services::IBackOffTimer, IBackOffTimer)

        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeRequest, ListSubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeResult, ListSubscribeResult)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeNotify, ListSubscribeNotify)

        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeRequest, SubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetRequest, DataGetRequest)

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction, UseDatabase)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction::PeerLocationRecord, PeerLocationRecord)

        ZS_DECLARE_STRUCT_PTR(IncomingListSubscription)

        typedef String DatabaseID;
        typedef std::map<DatabaseID, UseLocationDatabaseWeakPtr> LocationDatabaseMap;

        struct VersionedData
        {
          enum VersionTypes
          {
            VersionType_FullList,
            VersionType_ChangeList,
          };
          static const char *toString(VersionTypes type);

          VersionTypes mType {VersionType_FullList};
          UseDatabase::index mAfterIndex {OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN};

          ElementPtr toDebug() const;
        };

        struct IncomingListSubscription : public VersionedData
        {
          UseLocationPtr mLocation;

          String mSubscribeMessageID;

          String mLastVersion;

          Time mExpires;

          bool mNotified {false};
          PromisePtr mPendingSendPromise;

          IncomingListSubscription();
          virtual ~IncomingListSubscription();

          ElementPtr toDebug() const;
        };

        typedef PUID LocationID;
        typedef std::map<LocationID, IncomingListSubscriptionPtr> IncomingListSubscriptionMap;

      protected:
        LocationDatabases(ILocationPtr location);

        LocationDatabases(
                          Noop,
                          IMessageQueuePtr queue = IMessageQueuePtr()
                          ) :
          MessageQueueAssociator(queue),
          SharedRecursiveLock(SharedRecursiveLock::create()),
          Noop(true) {}

        void init();

      public:
        ~LocationDatabases();

        static LocationDatabasesPtr convert(ILocationDatabasesPtr object);
        static LocationDatabasesPtr convert(ForLocationDatabasesManagerPtr object);
        static LocationDatabasesPtr convert(ForLocationDatabasePtr object);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationDatabases
        #pragma mark

        static ElementPtr toDebug(ILocationDatabasesPtr object);

        static ILocationDatabasesPtr open(
                                          ILocationPtr inLocation,
                                          ILocationDatabasesDelegatePtr inDelegate
                                          );

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getLocation() const;

        virtual DatabasesStates getState() const;

        virtual DatabaseInfoListPtr getUpdates(
                                               const String &inExistingVersion,
                                               String &outNewVersion
                                               ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabasesManager
        #pragma mark

        // (duplicate) virtual ElementPtr toDebug() const;

        static LocationDatabasesPtr create(ILocationPtr location);

        // (duplicate) virtual PUID getID() const = 0;
        // (duplicate) virtual ILocationPtr getLocation() const = 0;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabase
        #pragma mark

        // (duplicate) virtual ElementPtr toDebug() const;

        // (duplicate) virtual ILocationPtr getLocation() const = 0;

        // (duplicate) virtual bool isLocal() const = 0;

        virtual LocationDatabasePtr getOrCreate(
                                                const String &databaseID,
                                                bool downloadAllEntries
                                                );
        virtual LocationDatabasePtr getOrCreate(
                                                const String &databaseID,
                                                ElementPtr inMetaData,
                                                const PeerList &inPeerAccessList,
                                                Time expires
                                                );
        virtual void notifyShutdown(UseLocationDatabase &database);
        virtual void notifyDestroyed(UseLocationDatabase &database);

        virtual PromisePtr notifyWhenReady();

        virtual UseDatabasePtr getMasterDatabase();

        virtual bool openRemoteDB(
                                  const String &databaseID,
                                  UseDatabase::DatabaseRecord &outRecord
                                  );

        virtual bool openLocalDB(
                                 const String &databaseID,
                                 UseDatabase::DatabaseRecord &outRecord,
                                 PeerList &outPeerAccessList
                                 );

        virtual bool createLocalDB(
                                   const String &databaseID,
                                   ElementPtr inMetaData,
                                   const PeerList &inPeerAccessList,
                                   Time expires,
                                   UseDatabase::DatabaseRecord &outRecord
                                   );

        virtual void notifyDownloaded(
                                      const String &databaseID,
                                      const String &downloadedVersion
                                      );

        virtual String updateVersion(const String &databaseID);

        virtual void getDatabaseRecordUpdate(UseDatabase::DatabaseRecord &ioRecord) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IPromiseSettledDelegate
        #pragma mark

        virtual void onPromiseSettled(PromisePtr promise);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IBackOffTimerDelegate
        #pragma mark

        virtual void onBackOffTimerAttemptAgainNow(IBackOffTimerPtr timer);

        virtual void onBackOffTimerAttemptTimeout(IBackOffTimerPtr timer);

        virtual void onBackOffTimerAllAttemptsFailed(IBackOffTimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IServiceLockboxSessionDelegate
        #pragma mark

        virtual void onServiceLockboxSessionStateChanged(
                                                         IServiceLockboxSessionPtr session,
                                                         IServiceLockboxSession::SessionStates state
                                                         );

        virtual void onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationSubscriptionDelegate
        #pragma mark

        virtual void onLocationSubscriptionShutdown(ILocationSubscriptionPtr subscription);

        virtual void onLocationSubscriptionLocationConnectionStateChanged(
                                                                          ILocationSubscriptionPtr subscription,
                                                                          ILocationPtr location,
                                                                          LocationConnectionStates state
                                                                          );

        virtual void onLocationSubscriptionMessageIncoming(
                                                           ILocationSubscriptionPtr subscription,
                                                           IMessageIncomingPtr message
                                                           );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IMessageMonitorResultDelegate<ListSubscribeResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ListSubscribeResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ListSubscribeResultPtr ignore,         // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        ILocationDatabasesSubscriptionPtr subscribe(ILocationDatabasesDelegatePtr originalDelegate);

        virtual bool isLocal() const {return mIsLocal;}
        bool isPending() const {return DatabasesState_Pending == mState;}
        bool isReady() const {return DatabasesState_Ready == mState;}
        bool isShutdown() const {return DatabasesState_Shutdown == mState;}

        void cancel();

        void step();

        bool stepPrepareMasterDatabase();
        bool stepSubscribeLocationAll();
        bool stepSubscribeLocationRemote();

        bool stepPurgeConflict();

        bool stepRemoteSubscribeList();
        bool stepRemoteResubscribeTimer();

        bool stepLocalIncomingSubscriptionsTimer();
        bool stepLocalIncomingSubscriptionsNotify();

        bool stepChangeNotify();

        void setState(DatabasesStates state);

        static DatabaseInfo::Dispositions toDisposition(UseDatabase::DatabaseChangeRecord::Dispositions disposition);

        DatabaseInfoListPtr getUpdates(
                                       const String &peerURI,
                                       VersionedData &ioVersionData,
                                       String &outLastVersion
                                       ) const;
        bool validateVersionInfo(
                                 const String &version,
                                 VersionedData &outVersionData
                                 ) const;

        void notifyIncoming(ListSubscribeNotifyPtr notify);
        void notifyIncoming(
                            UseMessageIncomingPtr message,
                            ListSubscribeRequestPtr request
                            );
        void notifyIncoming(
                            UseMessageIncomingPtr message,
                            SubscribeRequestPtr request
                            );
        void notifyIncoming(
                            UseMessageIncomingPtr message,
                            DataGetRequestPtr request
                            );

        UseLocationDatabasePtr prepareDatabaseLocationForMessage(
                                                                 MessageRequestPtr request,
                                                                 UseMessageIncomingPtr message,
                                                                 UseLocationPtr fromLocation,
                                                                 const String &requestedDatabaseID
                                                                 );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabasesWeakPtr mThisWeak;

        DatabasesStates mState {DatabasesState_Pending};

        bool mIsLocal {false};

        UseLocationPtr mLocation;
        UseLocationSubscriptionPtr mLocationSubscription;

        ILocationDatabasesDelegateSubscriptions mSubscriptions;

        ILocationDatabaseAbstractionPtr mMasterDatase;

        IServiceLockboxSessionSubscriptionPtr mLockboxSubscription;
        UseLockboxPtr mLockbox;

        PeerLocationRecord mPeerLocationRecord;

        LocationDatabaseMap mLocationDatabases;
        LocationDatabaseMap mExpectingShutdownLocationDatabases;

        IBackOffTimerPtr mRemoteSubscriptionBackOffTimer;
        IMessageMonitorPtr mRemoteListSubscribeMonitor;
        TimerPtr mRemoteSubscribeTimeout;

        String mLastChangeUpdateNotified;

        IncomingListSubscriptionMap mIncomingListSubscriptions;
        TimerPtr mIncomingListSubscriptionTimer;

        Promise::PromiseWeakList mPromiseWhenReadyList;

        bool mDatabaseListConflict {false};
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesFactory
      #pragma mark

      interaction ILocationDatabasesFactory
      {
        static ILocationDatabasesFactory &singleton();

        virtual ILocationDatabasesPtr open(
                                           ILocationPtr inLocation,
                                           ILocationDatabasesDelegatePtr inDelegate
                                           );
      };

      class LocationDatabasesFactory : public IFactory<ILocationDatabasesFactory> {};

    }
  }
}
