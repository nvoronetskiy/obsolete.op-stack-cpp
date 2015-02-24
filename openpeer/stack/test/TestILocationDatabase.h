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

#pragma once

#include <zsLib/types.h>
#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

#include <openpeer/stack/ILocationDatabase.h>
#include <openpeer/stack/ILocationDatabases.h>

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationSubscription.h>
#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      namespace location_database
      {
        using zsLib::AutoPUID;
        using zsLib::MessageQueueAssociator;

        ZS_DECLARE_USING_PTR(zsLib, Timer)
        ZS_DECLARE_USING_PTR(zsLib, ITimerDelegate)

        ZS_DECLARE_CLASS_PTR(OverrideAccountFactory)
        ZS_DECLARE_CLASS_PTR(OverrideAccount)

        ZS_DECLARE_CLASS_PTR(OverrideLocationFactory)
        ZS_DECLARE_CLASS_PTR(OverrideLocation)

        ZS_DECLARE_CLASS_PTR(OverrideLocationSubscriptionFactory)
        ZS_DECLARE_CLASS_PTR(OverrideLocationSubscription)

        ZS_DECLARE_CLASS_PTR(OverrideMessageIncomingFactory)
        ZS_DECLARE_CLASS_PTR(OverrideMessageIncoming)

        ZS_DECLARE_CLASS_PTR(OverrideServiceLockboxSessionFactory)
        ZS_DECLARE_CLASS_PTR(OverrideServiceLockboxSession)


        ZS_DECLARE_CLASS_PTR(Tester)

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideAccount
        #pragma mark

        class OverrideAccount : public openpeer::stack::internal::Account
        {
        public:

          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Account, Account)

          typedef ILocation::LocationConnectionStates LocationConnectionStates;

        public:
          OverrideAccount(OverrideServiceLockboxSessionPtr session);
          ~OverrideAccount();

          static OverrideAccountPtr convert(AccountPtr account);

          static OverrideAccountPtr create(OverrideServiceLockboxSessionPtr session);

          void init();

          Log::Params log(const char *message) const;

        public:
          void testAddLocation(OverrideLocationPtr location);
          void testAddSubcribeAll(OverrideLocationSubscriptionPtr subscription);
          void testAddSubcribeLocation(
                                       OverrideLocationPtr location,
                                       OverrideLocationSubscriptionPtr subscription
                                       );
          void testNotifySubscriptions(
                                       OverrideLocationPtr location,
                                       LocationConnectionStates state
                                       );

        public:
          virtual IServiceLockboxSessionPtr getLockboxSession() const;

        private:
          OverrideServiceLockboxSessionPtr mLockboxSession;

          typedef String LocationHash;
          typedef std::map<LocationHash, OverrideLocationPtr> LocationMap;

          typedef std::list<OverrideLocationSubscriptionPtr> LocationSubscriptionList;
          ZS_DECLARE_PTR(LocationSubscriptionList)

          typedef std::map<LocationHash, LocationSubscriptionListPtr> LocationSubscriptionMap;


          LocationMap mLocations;

          LocationSubscriptionList mSubscribeAllLocations;
          LocationSubscriptionMap mSubscribeLocations;
        };

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocation
        #pragma mark

        class OverrideLocation : public openpeer::stack::internal::Location
        {
        public:

          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Account, Account)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Location, Location)

          typedef ILocation::LocationConnectionStates LocationConnectionStates;

        public:
          OverrideLocation(
                           OverrideAccountPtr account,
                           ILocation::LocationTypes type,
                           const char *peerURI,
                           const char *locationID
                           );
          ~OverrideLocation();

          static OverrideLocationPtr convert(LocationPtr location);

          static OverrideLocationPtr create(
                                            OverrideAccountPtr account,
                                            ILocation::LocationTypes type,
                                            const char *peerURI,
                                            const char *locationID
                                            );

          void init();

          Log::Params log(const char *message) const;

        public:
          String testGetLocationHash() const;

          void testSetConnected(bool isConnected) {mIsConnected = isConnected;}
          void testSetSendFailure(bool failure) {mSendFailure = failure;}
          void testSetConnectionState(LocationConnectionStates state);
          LocationConnectionStates testGetConnectionState() const {return mConnectionState;}

        public:
          virtual PUID getID() const;

          virtual ElementPtr toDebug() const;

          virtual String getPeerURI() const;
          virtual String getLocationID() const;

          virtual bool isConnected() const;

          virtual PromisePtr send(message::MessagePtr message) const;

          virtual LocationTypes getLocationType() const;

          virtual AccountPtr getAccount() const;

        private:
          OverrideAccountWeakPtr mAccount;
          ILocation::LocationTypes mType;
          String mPeerURI;
          String mLocationID;

          std::atomic<bool> mIsConnected {false};
          std::atomic<bool> mSendFailure {false};
          std::atomic<LocationConnectionStates> mConnectionState {ILocation::LocationConnectionState_Pending};
        };

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationSubscription
        #pragma mark

        class OverrideLocationSubscription : public openpeer::stack::internal::LocationSubscription
        {
        public:

          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationSubscription, LocationSubscription)

          typedef ILocation::LocationConnectionStates LocationConnectionStates;

        public:
          OverrideLocationSubscription(ILocationSubscriptionDelegatePtr delegate);
          ~OverrideLocationSubscription();

          static OverrideLocationSubscriptionPtr convert(LocationSubscriptionPtr location);

          static OverrideLocationSubscriptionPtr create(ILocationSubscriptionDelegatePtr delegate);

          void init();

          Log::Params log(const char *message) const;

        public:
          void testNotifyState(
                               OverrideLocationPtr location,
                               LocationConnectionStates state
                               );

        public:
          virtual PUID getID() const;

          virtual ElementPtr toDebug() const;

          virtual void cancel();

        private:
          ILocationSubscriptionDelegatePtr mDelegate;
        };


        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideMessageIncoming
        #pragma mark

        class OverrideMessageIncoming : public openpeer::stack::internal::MessageIncoming
        {
        public:

          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::MessageIncoming, MessageIncoming)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Location, Location)

        public:
          OverrideMessageIncoming(
                                  OverrideLocationPtr location,
                                  message::MessagePtr message
                                  );
          ~OverrideMessageIncoming();

          static OverrideMessageIncomingPtr convert(MessageIncomingPtr messageIncoming);

          static OverrideMessageIncomingPtr create(
                                                   OverrideLocationPtr location,
                                                   message::MessagePtr message
                                                   );

          void init();

        public:
          virtual LocationPtr getLocation(bool internal = true) const;
          virtual message::MessagePtr getMessage() const;

          virtual ElementPtr toDebug() const;

          virtual PromisePtr sendResponse(message::MessagePtr message);

        private:
          OverrideLocationPtr mLocation;
          message::MessagePtr mMessage;
        };

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideServiceLockboxSession
        #pragma mark

        class OverrideServiceLockboxSession : public openpeer::stack::internal::ServiceLockboxSession
        {
        public:

          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServiceLockboxSession, ServiceLockboxSession)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::LockboxInfo, LockboxInfo)

          typedef IServiceLockboxSession::SessionStates SessionStates;

        public:
          OverrideServiceLockboxSession();
          ~OverrideServiceLockboxSession();

          static OverrideServiceLockboxSessionPtr convert(ServiceLockboxSessionPtr session);

          static OverrideServiceLockboxSessionPtr create();

          void init();

          Log::Params log(const char *message) const;

        public:

          void testSetLockboxInfo(const LockboxInfo &info);
          void testSetState(
                            SessionStates state,
                            WORD errorCode = 0,
                            const char *errorReason = NULL
                            );

        public:
          virtual PUID getID() const;

          virtual SessionStates getState(
                                         WORD *lastErrorCode = NULL,
                                         String *lastErrorReason = NULL
                                         ) const;

          virtual LockboxInfo getLockboxInfo() const;

          virtual IServiceLockboxSessionSubscriptionPtr subscribe(IServiceLockboxSessionDelegatePtr delegate);

          virtual ElementPtr toDebug() const;

        private:
          LockboxInfo mLockboxInfo;

          SessionStates mState {IServiceLockboxSession::SessionState_Pending};
          WORD mErrorCode {};
          String mReason;

          IServiceLockboxSessionDelegateSubscriptions mLockboxSubscriptions;
        };
        
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark Tester
        #pragma mark

        class Tester : public MessageQueueAssociator,
                       public SharedRecursiveLock,
                       public ILocationDatabasesDelegate,
                       public ITimerDelegate
        {
        public:
          ZS_DECLARE_TYPEDEF_PTR(zsLib::RecursiveLock, RecursiveLock)

        private:
          Tester();

          void init();

          Log::Params log(const char *message) const;

        public:
          ~Tester();

          static TesterPtr create();

          void test();

          bool testIsDone();

          void testFinalCheck();

        protected:

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Tester => ILocationDatabasesDelegate
          #pragma mark

          virtual void onLocationDatabasesStateChanged(
                                                       ILocationDatabasesPtr inDatabases,
                                                       DatabasesStates state
                                                       );

          virtual void onLocationDatabasesUpdated(ILocationDatabasesPtr inDatabases);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Tester => ITimerDelegate
          #pragma mark

          void onTimer(TimerPtr timer);

        public:
          AutoPUID mID;

          mutable RecursiveLock mLock;
          TesterWeakPtr mThisWeak;

          TesterPtr mSelfReference;

          OverrideAccountFactoryPtr mAccountFactory;
          OverrideLocationFactoryPtr mLocationFactory;
          OverrideLocationSubscriptionFactoryPtr mLocationSubscriptionFactory;
          OverrideMessageIncomingFactoryPtr mMessageIncomingFactory;

          OverrideAccountPtr mAccount;
          OverrideServiceLockboxSessionPtr mLockboxSession;

          OverrideLocationPtr mLocal;

          ILocationDatabasesPtr mLocalDatabases;

          TimerPtr mLockboxPendingWithLockboxAccessTimer;
          TimerPtr mLockboxReadyTimer;
          TimerPtr mShuttingDownTimer;
          TimerPtr mShutdownTimer;
        };
      }
    }
  }
}

