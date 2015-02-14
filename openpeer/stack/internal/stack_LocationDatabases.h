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

#include <openpeer/stack/ILocationDatabases.h>
#include <openpeer/stack/ILocationSubscription.h>
#include <openpeer/stack/IServiceLockbox.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IBackOffTimer.h>
#include <openpeer/services/IWakeDelegate.h>

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
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabasesManagerForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabaseForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(IServiceLockboxSessionForLocationDatabases)

      //-----------------------------------------------------------------------
      interaction ILocationDatabasesForLocationDatabase
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabase, ForLocationDatabase)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, UseLocationDatabase)

        static ElementPtr toDebug(ForLocationDatabasePtr object);

        virtual void notifyDestroyed(UseLocationDatabase &database) = 0;
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
                                public zsLib::ITimerDelegate,
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
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesManagerForLocationDatabases, UseLocationDatabasesManager)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, UseLocationDatabase)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForLocationDatabases, UseLockbox)

        ZS_DECLARE_TYPEDEF_PTR(services::IBackOffTimer, IBackOffTimer)

        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeRequest, ListSubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeResult, ListSubscribeResult)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::ListSubscribeNotify, ListSubscribeNotify)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeRequest, SubscribeRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeResult, SubscribeResult)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::SubscribeNotify, SubscribeNotify)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetRequest, DataGetRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::p2p_database::DataGetResult, DataGetResult)

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction, UseDatabase)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseAbstraction::PeerLocationRecord, PeerLocationRecord)

        ZS_DECLARE_STRUCT_PTR(IncomingListSubscription)

        typedef String DatabaseID;
        typedef std::map<DatabaseID, UseLocationDatabaseWeakPtr> LocationDatabaseMap;

        struct IncomingListSubscription
        {
          enum SubscriptionTypes
          {
            SubscriptionType_FullList,
            SubscriptionType_ChangeList,
          };
          static const char *toString(SubscriptionTypes type);

          UseLocationPtr mLocation;

          SubscriptionTypes mType {SubscriptionType_FullList};
          UseDatabase::index mAfterIndex = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN;

          String mLastVersion;

          Time mExpires;

          bool mNotified {false};

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

        virtual LocationDatabaseListPtr getUpdates(
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

        virtual void notifyDestroyed(UseLocationDatabase &database);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => IWakeDelegate
        #pragma mark

        virtual void onWake();

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
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<ListSubscribeResult>
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

        bool isLocal() const {return mIsLocal;}
        bool isReady() const {return mReady;}
        bool isShutdown() const {return mShutdown;}

        void cancel();

        void step();

        bool stepPrepareMasterDatabase();
        bool stepSubscribeLocationAll();
        bool stepSubscribeLocationRemote();

        bool stepRemoteSubscribeList();
        bool stepRemoteResubscribeTimer();

        bool stepLocalIncomingSubscriptionsTimer();
        bool stepLocalIncomingSubscriptionsNotify();

        bool stepChangeNotify();

        void notifyIncoming(ListSubscribeNotifyPtr notify);
        void notifyIncoming(
                            IMessageIncomingPtr message,
                            ListSubscribeRequestPtr request
                            );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabasesWeakPtr mThisWeak;

        bool mReady {false};
        bool mShutdown {false};

        bool mIsLocal {false};

        UseLocationPtr mLocation;
        ILocationSubscriptionPtr mLocationSubscription;

        ILocationDatabasesDelegateSubscriptions mSubscriptions;

        ILocationDatabaseAbstractionPtr mMasterDatase;

        IServiceLockboxSessionSubscriptionPtr mLockboxSubscription;
        UseLockboxPtr mLockbox;

        PeerLocationRecord mPeerLocationRecord;

        LocationDatabaseMap mLocationDatabases;

        IBackOffTimerPtr mRemoteSubscriptionBackOffTimer;
        IMessageMonitorPtr mRemoteSubscribeListMonitor;
        TimerPtr mRemoteSubscribeTimeout;

        String mLastChangeUpdateNotified;

        IncomingListSubscriptionMap mIncomingListSubscription;
        TimerPtr mIncomingListSubscriptionTimer;
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
