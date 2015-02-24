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
#include <openpeer/stack/ILocationDatabaseLocal.h>
#include <openpeer/stack/internal/stack_ILocationDatabaseAbstraction.h>

#include <openpeer/stack/message/p2p-database/SubscribeResult.h>
#include <openpeer/stack/message/p2p-database/DataGetResult.h>

#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/ILocationSubscription.h>

#include <openpeer/services/IBackOffTimer.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/Promise.h>
#include <zsLib/Timer.h>

#define OPENPEER_STACK_SETTING_LOCATION_DATABASE_WAKE_TIME_AFTER_INCOMING_DATABASE_REQUEST_IN_SECONDS "openpeer/stack/location-database-wake-time-after-incoming-database-request-in-seconds"

#define OPENPEER_STACK_SETTING_P2P_DATABASE_SUBSCRIPTION_EXPIRES_TIMEOUT_IN_SECONDS "openpeer/stack/p2p-database-subscription-expires-timeout-in-seconds"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_INTERACTION_PTR(ILocationForLocationDatabase)
      ZS_DECLARE_INTERACTION_PTR(ILocationSubscriptionForLocationDatabase)
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabasesForLocationDatabase)
      ZS_DECLARE_INTERACTION_PTR(IMessageIncomingForLocationDatabase)

      //-----------------------------------------------------------------------
      interaction ILocationDatabaseForLocationDatabases
      {
        typedef ILocationDatabaseLocalTypes::PeerList PeerList;

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, ForLocationDatabases)

        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeRequest, SubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetRequest, DataGetRequest)
        ZS_DECLARE_TYPEDEF_PTR(IMessageIncomingForLocationDatabase, UseMessageIncoming)

        static ForLocationDatabasesPtr openRemote(
                                                  LocationDatabasesPtr databases,
                                                  const String &databaseID,
                                                  bool downloadAllEntries
                                                  );

        static ForLocationDatabasesPtr openLocal(
                                                 LocationDatabasesPtr databases,
                                                 const String &databaseID
                                                 );

        static ForLocationDatabasesPtr createLocal(
                                                   LocationDatabasesPtr databases,
                                                   const String &databaseID,
                                                   ElementPtr inMetaData,
                                                   const PeerList &inPeerAccessList,
                                                   Time expires
                                                   );

        virtual PUID getID() const = 0;

        virtual String getDatabaseID() const = 0;

        virtual void notifyConflict() = 0;

        virtual void notifyUpdated() = 0;

        virtual void notifyRemoved() = 0;

        virtual void notifyShutdown() = 0;

        virtual void notifyIncoming(
                                    UseMessageIncomingPtr message,
                                    SubscribeRequestPtr request
                                    ) = 0;
        virtual void notifyIncoming(
                                    UseMessageIncomingPtr message,
                                    DataGetRequestPtr request
                                    ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase
      #pragma mark

      class LocationDatabase : public MessageQueueAssociator,
                               public SharedRecursiveLock,
                               public Noop,
                               public ILocationDatabaseLocal,
                               public ILocationDatabaseForLocationDatabases,
                               public IWakeDelegate,
                               public ITimerDelegate,
                               public IBackOffTimerDelegate,
                               public IPromiseSettledDelegate,
                               public ILocationSubscriptionDelegate,
                               public IMessageMonitorResultDelegate<message::p2p_database::SubscribeResult>,
                               public IMessageMonitorResultDelegate<message::p2p_database::DataGetResult>
      {
      public:
        friend interaction ILocationDatabaseFactory;
        friend interaction ILocationDatabase;
        friend interaction ILocationDatabaseLocal;
        friend interaction ILocationDatabaseForLocationDatabases;

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabase, UseLocationDatabases)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationDatabase, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(ILocationSubscriptionForLocationDatabase, UseLocationSubscription)
        ZS_DECLARE_TYPEDEF_PTR(IMessageIncomingForLocationDatabase, UseMessageIncoming)

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction, UseDatabase)

        ZS_DECLARE_TYPEDEF_PTR(services::IBackOffTimer, IBackOffTimer)

        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeRequest, SubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeResult, SubscribeResult)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeNotify, SubscribeNotify)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetRequest, DataGetRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetResult, DataGetResult)

        typedef ILocationDatabaseLocalTypes::PeerList PeerList;

        typedef LocationDatabasePtr SelfReferencePtr;

        ZS_DECLARE_STRUCT_PTR(VersionedData)
        ZS_DECLARE_STRUCT_PTR(IncomingSubscription)

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase::VersionedData
        #pragma mark

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

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase::IncomingSubscription
        #pragma mark

        struct IncomingSubscription : public VersionedData
        {
          SelfReferencePtr mSelf;
          UseLocationSubscriptionPtr mLocationSubscription;

          UseLocationPtr mLocation;
          String mSubscribeMessageID;

          String mLastVersion;

          Time mExpires;

          bool mDownloadData {false};
          bool mNotified {false};
          PromisePtr mPendingSendPromise;

          IncomingSubscription();
          virtual ~IncomingSubscription();

          ElementPtr toDebug() const;
        };

        typedef PUID LocationID;
        typedef std::map<LocationID, IncomingSubscriptionPtr> IncomingSubscriptionMap;

        typedef std::map<TimerPtr, SelfReferencePtr> TimedReferenceMap;

        typedef String EntryID;
        typedef std::map<EntryID, Promise::PromiseList> PendingEntryMap;

        typedef std::list<EntryID> EntryList;

      protected:
        LocationDatabase(
                         const String &databaseID,
                         UseLocationDatabasesPtr databases
                         );
        
        LocationDatabase(
                         Noop,
                         IMessageQueuePtr queue = IMessageQueuePtr()
                         ) :
          MessageQueueAssociator(queue),
          SharedRecursiveLock(SharedRecursiveLock::create()),
          Noop(true) {}

        void init();

      public:
        ~LocationDatabase();

        static LocationDatabasePtr convert(ILocationDatabasePtr object);
        static LocationDatabasePtr convert(ForLocationDatabasesPtr object);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabase
        #pragma mark

        static ElementPtr toDebug(ILocationDatabasePtr object);

        static ILocationDatabasePtr open(
                                         ILocationDatabaseDelegatePtr inDelegate,
                                         ILocationPtr location,
                                         const char *databaseID,
                                         bool inAutomaticallyDownloadDatabaseData
                                         );

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getLocation() const;

        virtual String getDatabaseID() const;

        virtual ElementPtr getMetaData() const;

        virtual Time getExpires() const;

        virtual EntryInfoListPtr getUpdates(
                                            const String &inExistingVersion,
                                            String &outNewVersion
                                            ) const;

        virtual EntryInfoPtr getEntry(const char *inUniqueID) const;

        virtual PromisePtr notifyWhenDataReady(const UniqueIDList &needingEntryData);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabaseLocation
        #pragma mark


        static ILocationDatabaseLocalPtr toLocal(ILocationDatabasePtr location);

        static ILocationDatabaseLocalPtr create(
                                                ILocationDatabaseLocalDelegatePtr inDelegate,
                                                IAccountPtr account,
                                                const char *inDatabaseID,
                                                ElementPtr inMetaData,
                                                const PeerList &inPeerAccessList,
                                                Time expires = Time()
                                                );

        virtual PeerListPtr getPeerAccessList() const;

        virtual void setExpires(const Time &time);

        virtual bool add(
                         const char *uniqueID,
                         ElementPtr inMetaData,
                         ElementPtr inData
                         );

        virtual bool update(
                            const char *uniqueID,
                            ElementPtr inData
                            );

        virtual bool remove(const char *uniqueID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabaseForLocationDatabases
        #pragma mark

        static LocationDatabasePtr openRemote(
                                              LocationDatabasesPtr databases,
                                              const String &databaseID,
                                              bool downloadAllEntries
                                              );

        static LocationDatabasePtr openLocal(
                                             LocationDatabasesPtr databases,
                                             const String &databaseID
                                             );

        static LocationDatabasePtr createLocal(
                                               LocationDatabasesPtr databases,
                                               const String &databaseID,
                                               ElementPtr inMetaData,
                                               const PeerList &inPeerAccessList,
                                               Time expires
                                               );

        // (duplicate) virtual PUID getID() const = 0;

        // (duplicate) virtual String getDatabaseID() const = 0;

        virtual void notifyConflict();

        virtual void notifyUpdated();

        virtual void notifyRemoved();

        virtual void notifyShutdown();

        // (duplicate) virtual void notifyIncoming(
        //                                         UseMessageIncomingPtr message,
        //                                         SubscribeRequestPtr request
        //                                         ) = 0;

        // (duplicate) virtual void notifyIncoming(
        //                                         UseMessageIncomingPtr message,
        //                                         DataGetRequestPtr request
        //                                         ) = 0;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => IBackOffTimerDelegate
        #pragma mark

        virtual void onBackOffTimerAttemptAgainNow(IBackOffTimerPtr timer);

        virtual void onBackOffTimerAttemptTimeout(IBackOffTimerPtr timer);

        virtual void onBackOffTimerAllAttemptsFailed(IBackOffTimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => IPromiseSettledDelegate
        #pragma mark
        
        virtual void onPromiseSettled(PromisePtr promise);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationSubscriptionDelegate
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
        #pragma mark LocationDatabases => IMessageMonitorResultDelegate<SubscribeResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        SubscribeResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             SubscribeResultPtr ignore,         // will always be NULL
                                                             message::MessageResultPtr result
                                                             );
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IMessageMonitorResultDelegate<DataGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        DataGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             DataGetResultPtr ignore,         // will always be NULL
                                                             message::MessageResultPtr result
                                                             );
      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        bool isPending() const {return DatabaseState_Pending == mState;}
        bool isReady() const {return DatabaseState_Ready == mState;}
        bool isShutdown() const {return DatabaseState_Shutdown == mState;}
        bool isLocal() const {return mIsLocal;}

        ILocationDatabaseSubscriptionPtr subscribe(ILocationDatabaseDelegatePtr originalDelegate);

        void cancel();

        void setState(DatabaseStates state);

        void step();

        bool stepConflict();
        bool stepShutdown();

        bool stepMasterPromise();
        bool stepRemoved();
        bool stepOpenDB();
        bool stepUpdate();

        bool stepSubscribeLocationRemote();
        bool stepRemoteSubscribe();
        bool stepRemoteResubscribeTimer();

        bool stepLocalIncomingSubscriptionsTimer();
        bool stepLocalIncomingSubscriptionsNotify();

        bool stepRemoteDownloadPendingRequestedData();
        bool stepRemoteAutomaticDownloadAllData();

        bool stepChangeNotify();

        void notifyIncoming(SubscribeNotifyPtr notify);
        virtual void notifyIncoming(
                                    UseMessageIncomingPtr message,
                                    SubscribeRequestPtr request
                                    );
        virtual void notifyIncoming(
                                    UseMessageIncomingPtr message,
                                    DataGetRequestPtr request
                                    );

        void processEntries(const EntryInfoList &entries);
        void downloadAgain();

        static EntryInfo::Dispositions toDisposition(UseDatabase::EntryChangeRecord::Dispositions disposition);

        EntryInfoListPtr getUpdates(
                                    VersionedData &ioVersionData,
                                    String &outLastVersion,
                                    bool includeData
                                    ) const;

        bool validateVersionInfo(
                                 const String &version,
                                 VersionedData &outVersionData
                                 ) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabaseWeakPtr mThisWeak;

        UseLocationPtr mLocation;
        UseLocationSubscriptionPtr mLocationSubscription;

        UseLocationDatabasesPtr mDatabases;

        ILocationDatabaseDelegateSubscriptions mSubscriptions;

        DatabaseStates mState {DatabaseState_Pending};

        bool mIsLocal;

        String mDatabaseID;

        bool mAutomaticallyDownloadAllEntries {false};

        bool mOpenOnly {false};
        ElementPtr mMetaData;
        PeerList mPeerAccessList;

        Time mExpires;

        PromisePtr mMasterReady;

        UseDatabase::DatabaseRecord mDatabaseRecord;
        UseDatabasePtr mEntryDatabase;

        String mLastChangeUpdateNotified;

        TimedReferenceMap mTimedReferences;

        IncomingSubscriptionMap mIncomingSubscriptions;
        TimerPtr mIncomingSubscriptionTimer;

        IBackOffTimerPtr mRemoteSubscriptionBackOffTimer;
        IMessageMonitorPtr mRemoteSubscribeMonitor;
        TimerPtr mRemoteSubscribeTimeout;

        PendingEntryMap mPendingWhenDataReady;
        IBackOffTimerPtr mRemoteDataGetBackOffTimer;
        IMessageMonitorPtr mDataGetRequestMonitor;

        bool mDownloadedAllData {false};
        EntryList mRequestedEntries;
        UseDatabase::index mLastRequestedIndex {OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN};
        UseDatabase::index mLastRequestedIndexUponSuccess {OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN};

        // no lock needed
        std::atomic<bool> mNotifiedConflict {false};
        std::atomic<bool> mNotifiedUpdated {false};
        std::atomic<bool> mNotifiedRemoved {false};
        std::atomic<bool> mNotifiedShutdown {false};
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseFactory
      #pragma mark

      interaction ILocationDatabaseFactory
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseLocal::PeerList, PeerList)

        static ILocationDatabaseFactory &singleton();

        virtual ILocationDatabasePtr open(
                                          ILocationDatabaseDelegatePtr inDelegate,
                                          ILocationPtr location,
                                          const char *databaseID,
                                          bool inAutomaticallyDownloadDatabaseData
                                          );

        virtual ILocationDatabaseLocalPtr create(
                                                 ILocationDatabaseLocalDelegatePtr inDelegate,
                                                 IAccountPtr account,
                                                 const char *inDatabaseID,
                                                 ElementPtr inMetaData,
                                                 const PeerList &inPeerAccessList,
                                                 Time expires = Time()
                                                 );

        virtual LocationDatabasePtr openRemote(
                                               LocationDatabasesPtr databases,
                                               const String &databaseID,
                                               bool downloadAllEntries
                                               );

        virtual LocationDatabasePtr openLocal(
                                               LocationDatabasesPtr databases,
                                               const String &databaseID
                                               );

        virtual LocationDatabasePtr createLocal(
                                                LocationDatabasesPtr databases,
                                                const String &databaseID,
                                                ElementPtr inMetaData,
                                                const PeerList &inPeerAccessList,
                                                Time expires
                                                );
      };

      class LocationDatabaseFactory : public IFactory<ILocationDatabaseFactory> {};

    }
  }
}
