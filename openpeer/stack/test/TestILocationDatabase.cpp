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

#include "TestILocationDatabase.h"
#include "TestSetup.h"

#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Proxy.h>
#include <zsLib/Promise.h>
#include <zsLib/XML.h>

#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>

#include <openpeer/stack/IStack.h>
#include <openpeer/stack/ICache.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/ISettings.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IMessageQueueManager.h>

#include "config.h"
#include "testing.h"

using zsLib::ULONG;
using zsLib::Seconds;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack, ICache)
ZS_DECLARE_USING_PTR(openpeer::stack, ISettings)

ZS_DECLARE_TYPEDEF_PTR(openpeer::services::IHelper, UseServicesHelper)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IHelper, UseStackHelper)

ZS_DECLARE_TYPEDEF_PTR(openpeer::services::IMessageQueueManager, UseMessageQueueManager)

ZS_DECLARE_USING_PTR(openpeer::stack::test::location_database, Tester)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IAccountFactory, IAccountFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::AccountFactory, AccountFactory)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Location, Location)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationFactory, ILocationFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationFactory, LocationFactory)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationSubscriptionFactory, ILocationSubscriptionFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationSubscriptionFactory, LocationSubscriptionFactory)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IMessageIncomingFactory, IMessageIncomingFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::MessageIncomingFactory, MessageIncomingFactory)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::LockboxInfo, LockboxInfo)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IServiceLockboxSessionFactory, IServiceLockboxSessionFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServiceLockboxSessionFactory, ServiceLockboxSessionFactory)

namespace openpeer { namespace stack { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_stack_test) } } }

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      namespace location_database
      {
        using namespace zsLib::XML;
        using zsLib::Promise;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideAccountFactory
        #pragma mark

        interaction OverrideAccountFactory : public IAccountFactory
        {
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IAccount, IAccount)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Account, Account)

          //-------------------------------------------------------------------
          static OverrideAccountFactoryPtr create()
          {
            return OverrideAccountFactoryPtr(new OverrideAccountFactory);
          }
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationAbstraction
        #pragma mark

        //---------------------------------------------------------------------
        OverrideAccount::OverrideAccount(OverrideServiceLockboxSessionPtr session) :
          Account(zsLib::Noop(true)),
          mLockboxSession(session)
        {
          ZS_LOG_BASIC(log("created"))
        }

        //---------------------------------------------------------------------
        OverrideAccount::~OverrideAccount()
        {
          mThisWeak.reset();
          ZS_LOG_BASIC(log("destroyed"))
        }

        //---------------------------------------------------------------------
        OverrideAccountPtr OverrideAccount::convert(AccountPtr account)
        {
          return ZS_DYNAMIC_PTR_CAST(OverrideAccount, account);
        }

        //---------------------------------------------------------------------
        OverrideAccountPtr OverrideAccount::create(OverrideServiceLockboxSessionPtr session)
        {
          OverrideAccountPtr pThis(new OverrideAccount(session));
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void OverrideAccount::init()
        {
        }

        //---------------------------------------------------------------------
        Log::Params OverrideAccount::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("OverrideAccount");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void OverrideAccount::testAddLocation(OverrideLocationPtr location)
        {
          {
            AutoRecursiveLock lock(*this);

            auto hash = location->testGetLocationHash();

            ZS_LOG_BASIC(log("adding location") + ZS_PARAMIZE(hash) + location->toDebug())
            mLocations[hash] = location;
          }

          auto state = location->testGetConnectionState();

          if (ILocation::LocationConnectionState_Pending == state) return;

          testNotifySubscriptions(location, state);
        }

        //---------------------------------------------------------------------
        void OverrideAccount::testAddSubcribeAll(OverrideLocationSubscriptionPtr subscription)
        {
          LocationMap tempLocations;

          ZS_LOG_BASIC(log("adding subscribe all") + ZS_PARAMIZE(subscription->getID()))

          {
            AutoRecursiveLock lock(*this);
            mSubscribeAllLocations.push_back(subscription);

            tempLocations = mLocations;
          }

          for (auto iter = tempLocations.begin(); iter != tempLocations.end(); ++iter) {
            auto location = (*iter).second;
            auto state = location->testGetConnectionState();
            if (ILocation::LocationConnectionState_Pending != state) {
              subscription->testNotifyState(location, state);
            }
          }
        }

        //---------------------------------------------------------------------
        void OverrideAccount::testAddSubcribeLocation(
                                                      OverrideLocationPtr location,
                                                      OverrideLocationSubscriptionPtr subscription
                                                      )
        {
          ZS_LOG_BASIC(log("adding subscribe location") + location->toDebug() + ZS_PARAMIZE(subscription->getID()))

          {
            AutoRecursiveLock lock(*this);
            auto hash = location->testGetLocationHash();

            LocationSubscriptionListPtr foundList;

            auto found = mSubscribeLocations.find(hash);
            if (found == mSubscribeLocations.end()) {
              foundList = LocationSubscriptionListPtr(new LocationSubscriptionList);
              mSubscribeLocations[hash] = foundList;
            } else {
              foundList = (*found).second;
            }

            foundList->push_back(subscription);
          }

          auto state = location->testGetConnectionState();
          if (ILocation::LocationConnectionState_Pending != state) {
            subscription->testNotifyState(location, state);
          }
        }

        //---------------------------------------------------------------------
        void OverrideAccount::testNotifySubscriptions(
                                                      OverrideLocationPtr location,
                                                      LocationConnectionStates state
                                                      )
        {
          ZS_LOG_BASIC(log("notifying subscriptions") + location->toDebug() + ZS_PARAM("state", ILocation::toString(state)))

          LocationSubscriptionList tempAll;
          LocationSubscriptionList tempLocations;

          {
            AutoRecursiveLock lock(*this);
            tempAll = mSubscribeAllLocations;

            auto found = mSubscribeLocations.find(location->testGetLocationHash());
            if (found != mSubscribeLocations.end()) {
              tempLocations = *((*found).second);
            }
          }

          {
            for (auto iter = tempAll.begin(); iter != tempAll.end(); ++iter) {
              auto subscription = (*iter);
              subscription->testNotifyState(location, state);
            }
          }

          {
            for (auto iter = tempLocations.begin(); iter != tempLocations.end(); ++iter) {
              auto subscription = (*iter);
              subscription->testNotifyState(location, state);
            }
          }
        }

        //---------------------------------------------------------------------
        IServiceLockboxSessionPtr OverrideAccount::getLockboxSession() const
        {
          return mLockboxSession;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationFactory
        #pragma mark

        interaction OverrideLocationFactory : public ILocationFactory
        {
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::ILocation, ILocation)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Location, Location)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Account, Account)

          //-------------------------------------------------------------------
          static OverrideLocationFactoryPtr create()
          {
            return OverrideLocationFactoryPtr(new OverrideLocationFactory);
          }

          //-------------------------------------------------------------------
          virtual LocationPtr getForLocal(IAccountPtr account)
          {
            return OverrideLocation::create(OverrideAccount::convert(Account::convert(account)), ILocation::LocationType_Local, "peer:://domain.com/2bd806c97f0e00af1a1fc3328fa763a9269723c8db8fac4f93af71db186d6e90", "foo");
          }

          //-------------------------------------------------------------------
          virtual LocationPtr getForFinder(IAccountPtr account)
          {
            ZS_THROW_NOT_IMPLEMENTED("NA")
            return LocationPtr();
          }

          //-------------------------------------------------------------------
          virtual LocationPtr getForPeer(
                                         IPeerPtr peer,
                                         const char *locationID
                                         )
          {
            ZS_THROW_NOT_IMPLEMENTED("NA")
            return LocationPtr();
          }

          //-------------------------------------------------------------------
          virtual LocationPtr create(
                                     IMessageSourcePtr messageSource,
                                     const char *peerURI,
                                     const char *locationID
                                     )
          {
            ZS_THROW_NOT_IMPLEMENTED("NA")
            return LocationPtr();
          }
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationAbstraction
        #pragma mark

        //---------------------------------------------------------------------
        OverrideLocation::OverrideLocation(
                                           OverrideAccountPtr account,
                                           ILocation::LocationTypes type,
                                           const char *peerURI,
                                           const char *locationID
                                           ) :
          Location(zsLib::Noop(true)),
          mAccount(account),
          mType(type),
          mPeerURI(peerURI),
          mLocationID(locationID)
        {
          ZS_LOG_BASIC(log("location created") + toDebug())
        }

        //---------------------------------------------------------------------
        OverrideLocation::~OverrideLocation()
        {
          mThisWeak.reset();
          ZS_LOG_BASIC(log("location destroyed") + toDebug())
        }

        //---------------------------------------------------------------------
        OverrideLocationPtr OverrideLocation::convert(LocationPtr location)
        {
          return ZS_DYNAMIC_PTR_CAST(OverrideLocation, location);
        }

        //---------------------------------------------------------------------
        OverrideLocationPtr OverrideLocation::create(
                                                     OverrideAccountPtr account,
                                                     ILocation::LocationTypes type,
                                                     const char *peerURI,
                                                     const char *locationID
                                                     )
        {
          OverrideLocationPtr pThis(new OverrideLocation(account, type, peerURI, locationID));
          pThis->mThisWeak = pThis;

          account->testAddLocation(pThis);

          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void OverrideLocation::init()
        {
        }

        //---------------------------------------------------------------------
        Log::Params OverrideLocation::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("OverrideLocation");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        String OverrideLocation::testGetLocationHash() const
        {
          String preHash = String(ILocation::toString(mType)) + ":" + mPeerURI + ":" + mLocationID;
          String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(preHash));
          return hash;
        }

        //---------------------------------------------------------------------
        void OverrideLocation::testSetConnectionState(LocationConnectionStates state)
        {
          if (state == mConnectionState) return;

          ZS_LOG_BASIC(log("location connection state changed") + ZS_PARAM("new state", ILocation::toString(state)) + ZS_PARAM("new state", ILocation::toString(mConnectionState)))

          mConnectionState = state;

          auto account = mAccount.lock();
          if (account) account->testNotifySubscriptions(OverrideLocation::convert(mThisWeak.lock()), state);
        }

        //---------------------------------------------------------------------
        PUID OverrideLocation::getID() const
        {
          return mID;
        }

        //---------------------------------------------------------------------
        ElementPtr OverrideLocation::toDebug() const
        {
          ElementPtr resultEl = Element::create("OverrideLocation");

          UseServicesHelper::debugAppend(resultEl, "id", mID);
          UseServicesHelper::debugAppend(resultEl, "type", ILocation::toString(mType));
          UseServicesHelper::debugAppend(resultEl, "peer uri", mPeerURI);
          UseServicesHelper::debugAppend(resultEl, "location id", mLocationID);

          return resultEl;
        }

        //---------------------------------------------------------------------
        String OverrideLocation::getPeerURI() const
        {
          return mPeerURI;
        }

        //---------------------------------------------------------------------
        String OverrideLocation::getLocationID() const
        {
          return mLocationID;
        }

        //---------------------------------------------------------------------
        bool OverrideLocation::isConnected() const
        {
          return mIsConnected;
        }

        //---------------------------------------------------------------------
        PromisePtr OverrideLocation::send(message::MessagePtr message) const
        {
          if ((!mIsConnected) ||
              (mSendFailure)) {
            ZS_LOG_BASIC(log("message send successfully (simulation)") + message::Message::toDebug(message))
            return Promise::createRejected(IStack::getStackQueue());
          }
          ZS_LOG_BASIC(log("message send failure (simulation)") + message::Message::toDebug(message))
          return Promise::createResolved(IStack::getStackQueue());
        }

        //---------------------------------------------------------------------
        ILocation::LocationTypes OverrideLocation::getLocationType() const
        {
          return mType;
        }


        //---------------------------------------------------------------------
        OverrideLocation::AccountPtr OverrideLocation::getAccount() const
        {
          return mAccount.lock();
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationSubscriptionFactory
        #pragma mark

        interaction OverrideLocationSubscriptionFactory : public ILocationSubscriptionFactory
        {
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::ILocationSubscription, ILocationSubscription)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Account, Account)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::Location, Location)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationSubscription, LocationSubscription)

          //-------------------------------------------------------------------
          static OverrideLocationSubscriptionFactoryPtr create()
          {
            return OverrideLocationSubscriptionFactoryPtr(new OverrideLocationSubscriptionFactory);
          }

          //-------------------------------------------------------------------
          virtual LocationSubscriptionPtr subscribeAll(
                                                       AccountPtr account,
                                                       ILocationSubscriptionDelegatePtr delegate
                                                       )
          {
            auto overrideAccount = OverrideAccount::convert(account);

            auto subscription = OverrideLocationSubscription::create(delegate);
            overrideAccount->testAddSubcribeAll(subscription);
            return subscription;
          }

          //-------------------------------------------------------------------
          virtual LocationSubscriptionPtr subscribe(
                                                    ILocationPtr location,
                                                    ILocationSubscriptionDelegatePtr delegate
                                                    )
          {
            auto overrideLocation = OverrideLocation::convert(Location::convert(location));

            auto subscription = OverrideLocationSubscription::create(delegate);
            OverrideAccount::convert(overrideLocation->getAccount())->testAddSubcribeLocation(overrideLocation, subscription);
            return subscription;
          }
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideLocationSubscription
        #pragma mark

        //---------------------------------------------------------------------
        OverrideLocationSubscription::OverrideLocationSubscription(ILocationSubscriptionDelegatePtr delegate) :
          LocationSubscription(zsLib::Noop(true)),
          mDelegate(ILocationSubscriptionDelegateProxy::createWeak(IStack::getDefaultQueue(), delegate))
        {
          ZS_LOG_BASIC(log("created"))
        }

        //---------------------------------------------------------------------
        OverrideLocationSubscription::~OverrideLocationSubscription()
        {
          mThisWeak.reset();
          ZS_LOG_BASIC(log("destroyed"))
        }

        //---------------------------------------------------------------------
        OverrideLocationSubscriptionPtr OverrideLocationSubscription::convert(LocationSubscriptionPtr subscription)
        {
          return ZS_DYNAMIC_PTR_CAST(OverrideLocationSubscription, subscription);
        }

        //---------------------------------------------------------------------
        OverrideLocationSubscriptionPtr OverrideLocationSubscription::create(ILocationSubscriptionDelegatePtr delegate)
        {
          OverrideLocationSubscriptionPtr pThis(new OverrideLocationSubscription(delegate));
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void OverrideLocationSubscription::init()
        {
        }

        //---------------------------------------------------------------------
        Log::Params OverrideLocationSubscription::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("OverrideLocationSubscription");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void OverrideLocationSubscription::testNotifyState(
                                                           OverrideLocationPtr location,
                                                           LocationConnectionStates state
                                                           )
        {
          ZS_LOG_BASIC(log("notify location state") + location->toDebug() + ZS_PARAM("state", ILocation::toString(state)))

          ILocationSubscriptionDelegatePtr delegate;

          {
            AutoRecursiveLock lock(*this);
            if (!mDelegate) return;

            delegate = mDelegate;
          }

          try {
            ZS_LOG_BASIC(log("notifying state") + location->toDebug() + ZS_PARAM("state", ILocation::toString(state)))
            mDelegate->onLocationSubscriptionLocationConnectionStateChanged(mThisWeak.lock(), location, state);
          } catch (ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Basic, log("delegate gone") + location->toDebug() + ZS_PARAM("state", ILocation::toString(state)))
          }
        }

        //---------------------------------------------------------------------
        PUID OverrideLocationSubscription::getID() const
        {
          return mID;
        }

        //---------------------------------------------------------------------
        ElementPtr OverrideLocationSubscription::toDebug() const
        {
          ElementPtr resultEl = Element::create("OverrideLocationSubscription");

          UseServicesHelper::debugAppend(resultEl, "id", mID);

          return resultEl;
        }

        //---------------------------------------------------------------------
        void OverrideLocationSubscription::cancel()
        {
          ILocationSubscriptionDelegatePtr delegate;

          {
            AutoRecursiveLock lock(*this);
            delegate = mDelegate;
            mDelegate.reset();
          }

          if (delegate) {
            try {
              delegate->onLocationSubscriptionShutdown(mThisWeak.lock());
            } catch(ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Basic, log("delegate gone"))
            }
          }
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideMessageIncomingFactory
        #pragma mark

        interaction OverrideMessageIncomingFactory : public IMessageIncomingFactory
        {
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IMessageIncoming, IMessageIncoming)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::MessageIncoming, MessageIncoming)

          static OverrideMessageIncomingFactoryPtr create()
          {
            return OverrideMessageIncomingFactoryPtr(new OverrideMessageIncomingFactory);
          }
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideServiceLockboxSession
        #pragma mark

        //---------------------------------------------------------------------
        OverrideMessageIncoming::OverrideMessageIncoming(
                                                         OverrideLocationPtr location,
                                                         message::MessagePtr message
                                                         ) :
          MessageIncoming(zsLib::Noop(true)),
          mLocation(location),
          mMessage(message)
        {
          ZS_LOG_BASIC(log("created"))
        }

        //---------------------------------------------------------------------
        OverrideMessageIncoming::~OverrideMessageIncoming()
        {
          mThisWeak.reset();
          ZS_LOG_BASIC(log("destroyed"))
        }

        //---------------------------------------------------------------------
        OverrideMessageIncomingPtr OverrideMessageIncoming::convert(MessageIncomingPtr messageIncoming)
        {
          return ZS_DYNAMIC_PTR_CAST(OverrideMessageIncoming, messageIncoming);
        }

        //---------------------------------------------------------------------
        OverrideMessageIncomingPtr OverrideMessageIncoming::create(
                                                                   OverrideLocationPtr location,
                                                                   message::MessagePtr message
                                                                   )
        {
          OverrideMessageIncomingPtr pThis(new OverrideMessageIncoming(location, message));
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void OverrideMessageIncoming::init()
        {
        }

        //---------------------------------------------------------------------
        LocationPtr OverrideMessageIncoming::getLocation(bool internal) const
        {
          return mLocation;
        }

        //---------------------------------------------------------------------
        message::MessagePtr OverrideMessageIncoming::getMessage() const
        {
          return mMessage;
        }

        //---------------------------------------------------------------------
        ElementPtr OverrideMessageIncoming::toDebug() const
        {
          ElementPtr resultEl = Element::create("OverrideMessageIncoming");

          UseServicesHelper::debugAppend(resultEl, "id", mID);

          UseServicesHelper::debugAppend(resultEl, "location", mLocation->toDebug());
          UseServicesHelper::debugAppend(resultEl, "message", message::Message::toDebug(mMessage));

          return resultEl;
        }

        //---------------------------------------------------------------------
        PromisePtr OverrideMessageIncoming::sendResponse(message::MessagePtr message)
        {
          return mLocation->send(message);
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideServiceLockboxSessionFactory
        #pragma mark

        interaction OverrideServiceLockboxSessionFactory : public IServiceLockboxSessionFactory
        {
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IServiceLockboxSession, IServiceLockboxSession)
          ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServiceLockboxSession, ServiceLockboxSession)

          static OverrideServiceLockboxSessionFactoryPtr create()
          {
            return OverrideServiceLockboxSessionFactoryPtr(new OverrideServiceLockboxSessionFactory);
          }
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark OverrideServiceLockboxSession
        #pragma mark

        //---------------------------------------------------------------------
        OverrideServiceLockboxSession::OverrideServiceLockboxSession() :
          ServiceLockboxSession(zsLib::Noop(true))
        {
          ZS_LOG_BASIC(log("created"))
        }

        //---------------------------------------------------------------------
        OverrideServiceLockboxSession::~OverrideServiceLockboxSession()
        {
          mThisWeak.reset();
          ZS_LOG_BASIC(log("destroyed"))
        }

        //---------------------------------------------------------------------
        OverrideServiceLockboxSessionPtr OverrideServiceLockboxSession::convert(ServiceLockboxSessionPtr session)
        {
          return ZS_DYNAMIC_PTR_CAST(OverrideServiceLockboxSession, session);
        }

        //---------------------------------------------------------------------
        OverrideServiceLockboxSessionPtr OverrideServiceLockboxSession::create()
        {
          OverrideServiceLockboxSessionPtr pThis(new OverrideServiceLockboxSession());
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void OverrideServiceLockboxSession::init()
        {
        }

        //---------------------------------------------------------------------
        Log::Params OverrideServiceLockboxSession::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("OverrideServiceLockboxSession");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void OverrideServiceLockboxSession::testSetLockboxInfo(const LockboxInfo &info)
        {
          AutoRecursiveLock lock(*this);
          mLockboxInfo = info;
        }

        //---------------------------------------------------------------------
        void OverrideServiceLockboxSession::testSetState(
                                                         SessionStates state,
                                                         WORD errorCode,
                                                         const char *errorReason
                                                         )
        {
          AutoRecursiveLock lock(*this);

          ZS_LOG_BASIC(log("state changed") + ZS_PARAM("state", IServiceLockboxSession::toString(state)) + ZS_PARAMIZE(errorCode) + ZS_PARAMIZE(errorReason))

          auto oldState = mState;

          mState = state;
          mErrorCode = errorCode;
          mReason = String(errorReason);

          if (oldState != mState) {
            mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(mThisWeak.lock(), mState);
          }
        }

        //---------------------------------------------------------------------
        PUID OverrideServiceLockboxSession::getID() const
        {
          return mID;
        }

        //---------------------------------------------------------------------
        IServiceLockboxSession::SessionStates OverrideServiceLockboxSession::getState(
                                                                                      WORD *lastErrorCode,
                                                                                      String *lastErrorReason
                                                                                      ) const
        {
          AutoRecursiveLock lock(*this);
          if (lastErrorCode) *lastErrorCode = mErrorCode;
          if (lastErrorReason) *lastErrorReason = mReason;
          return mState;
        }

        //---------------------------------------------------------------------
        LockboxInfo OverrideServiceLockboxSession::getLockboxInfo() const
        {
          AutoRecursiveLock lock(*this);
          return mLockboxInfo;
        }

        //---------------------------------------------------------------------
        IServiceLockboxSessionSubscriptionPtr OverrideServiceLockboxSession::subscribe(IServiceLockboxSessionDelegatePtr inDelegate)
        {
          auto subscription = mLockboxSubscriptions.subscribe(inDelegate, IStack::getDefaultQueue());

          auto pThis = mThisWeak.lock();

          IServiceLockboxSessionDelegatePtr delegate = mLockboxSubscriptions.delegate(subscription, true);

          if (delegate) {
            if (pThis) {
              if (IServiceLockboxSession::SessionState_Pending != mState) {
                delegate->onServiceLockboxSessionStateChanged(mThisWeak.lock(), mState);
              }
            }
          }

          if (IServiceLockboxSession::SessionState_Shutdown == mState) {
            mLockboxSubscriptions.clear();
          }

          return subscription;
        }

        //---------------------------------------------------------------------
        ElementPtr OverrideServiceLockboxSession::toDebug() const
        {
          ElementPtr resultEl = Element::create("OverrideServiceLockboxSession");

          UseServicesHelper::debugAppend(resultEl, "id", mID);

          UseServicesHelper::debugAppend(resultEl, mLockboxInfo.toDebug());

          UseServicesHelper::debugAppend(resultEl, "state", IServiceLockboxSession::toString(mState));
          UseServicesHelper::debugAppend(resultEl, "error code", mErrorCode);
          UseServicesHelper::debugAppend(resultEl, "error reason", mReason);

          return resultEl;
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Tester
        #pragma mark

        //---------------------------------------------------------------------
        Tester::Tester() :
          MessageQueueAssociator(IStack::getDefaultQueue()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {
          mAccountFactory = OverrideAccountFactory::create();
          mLocationFactory = OverrideLocationFactory::create();
          mLocationSubscriptionFactory = OverrideLocationSubscriptionFactory::create();
          mMessageIncomingFactory = OverrideMessageIncomingFactory::create();

          AccountFactory::override(mAccountFactory);
          LocationFactory::override(mLocationFactory);
          LocationSubscriptionFactory::override(mLocationSubscriptionFactory);
          MessageIncomingFactory::override(mMessageIncomingFactory);

          mLockboxSession = OverrideServiceLockboxSession::create();
          mAccount = OverrideAccount::create(mLockboxSession);
          mLocal = OverrideLocation::convert(mLocationFactory->getForLocal(mAccount));
        }

        //---------------------------------------------------------------------
        Tester::~Tester()
        {
          mThisWeak.reset();

          AccountFactory::override(AccountFactoryPtr());
          LocationFactory::override(LocationFactoryPtr());
          LocationSubscriptionFactory::override(LocationSubscriptionFactoryPtr());
          MessageIncomingFactory::override(MessageIncomingFactoryPtr());
        }

        //---------------------------------------------------------------------
        TesterPtr Tester::create()
        {
          TesterPtr pThis(new Tester());
          pThis->mThisWeak = pThis;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        void Tester::init()
        {
          ISettings::applyDefaults();
          UseStackHelper::mkdir("/tmp/test_location_databases", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
          ISettings::setString(OPENPEER_STACK_SETTING_LOCATION_DATABASE_PATH, "/tmp/test_location_databases");

          AutoRecursiveLock lock(*this);

          mSelfReference = mThisWeak.lock();
        }

        //---------------------------------------------------------------------
        Log::Params Tester::log(const char *message) const
        {
          ElementPtr objectEl = Element::create("Tester");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void Tester::test()
        {
          mLocalDatabases = ILocationDatabases::open(mLocal, mThisWeak.lock());

          mLockboxPendingWithLockboxAccessTimer = Timer::create(mThisWeak.lock(), zsLib::now() + Seconds(1));
        }

        //---------------------------------------------------------------------
        bool Tester::testIsDone()
        {
          AutoRecursiveLock lock(*this);
          return !((bool)mSelfReference);
        }

        //---------------------------------------------------------------------
        void Tester::testFinalCheck()
        {
          TESTING_CHECK(!mSelfReference)
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Tester => ILocationDatabasesDelegate
        #pragma mark

        //---------------------------------------------------------------------
        void Tester::onLocationDatabasesStateChanged(
                                                     ILocationDatabasesPtr inDatabases,
                                                     DatabasesStates state
                                                     )
        {
          ZS_LOG_BASIC(log("on location databases state changed") + ILocationDatabases::toDebug(inDatabases) + ZS_PARAM("state", ILocationDatabases::toString(state)))
        }

        //---------------------------------------------------------------------
        void Tester::onLocationDatabasesUpdated(ILocationDatabasesPtr inDatabases)
        {
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Tester => ITimerDelegate
        #pragma mark

        //---------------------------------------------------------------------
        void Tester::onTimer(TimerPtr timer)
        {
          if (timer == mLockboxPendingWithLockboxAccessTimer) {
            ZS_LOG_BASIC(log("lockbox is now pending with lockbox access"))
            LockboxInfo info;
            info.mAccountID = "act1234";
            info.mDomain = "domain.com";
            mLockboxSession->testSetLockboxInfo(info);
            mLockboxSession->testSetState(IServiceLockboxSession::SessionState_PendingWithLockboxAccessReady);
            mLockboxPendingWithLockboxAccessTimer.reset();

            mLockboxReadyTimer = Timer::create(mThisWeak.lock(), zsLib::now() + Seconds(3));
            return;
          }
          if (timer == mLockboxReadyTimer) {
            ZS_LOG_BASIC(log("lockbox is now ready"))
            mLockboxSession->testSetState(IServiceLockboxSession::SessionState_Ready);
            mLockboxReadyTimer.reset();

            mShuttingDownTimer = Timer::create(mThisWeak.lock(), zsLib::now() + Seconds(30));
            return;
          }
          if (timer == mShuttingDownTimer) {
            ZS_LOG_BASIC(log("shutting down"))
            mLockboxSession->testSetState(IServiceLockboxSession::SessionState_Shutdown);
            mShuttingDownTimer.reset();

            mShutdownTimer = Timer::create(mThisWeak.lock(), zsLib::now() + Seconds(1));
            return;
          }
          if (timer == mShutdownTimer) {
            ZS_LOG_BASIC(log("shutting down"))
            mShutdownTimer.reset();

            mSelfReference.reset();
            return;
          }
        }
      }
    }
  }
}

void doTestILocationDatabase()
{
  if (!OPENPEER_STACK_TEST_DO_ILOCATIONDATABASE_TEST) return;

  TESTING_INSTALL_LOGGER();

  {
    TestSetupPtr setup = TestSetup::singleton();

    {
      TesterPtr testObject = Tester::create();

      testObject->test();

      int count = 0;
      for (; count < 120; ++count)
      {
        std::cout << "TESTING:      ILocationDatabase...\n";

        std::this_thread::sleep_for(Seconds(1));

        auto total = UseMessageQueueManager::getTotalUnprocessedMessages();
        if (0 == total) {
          if (testObject->testIsDone()) {
            testObject->testFinalCheck();
            break;
          }
        }
      }

      TESTING_CHECK(count < 120)
    }

    std::cout << "COMPLETE:     ILocationDatabase(s) complete.\n";
  }

  TESTING_UNINSTALL_LOGGER()
  zsLib::proxyDump();
  TESTING_EQUAL(zsLib::proxyGetTotalConstructed(), 0);
}
