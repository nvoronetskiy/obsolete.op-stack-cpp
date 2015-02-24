/*

 Copyright (c) 2015, Hookflash Inc.
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

#include <openpeer/stack/internal/stack_LocationSubscription.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/stack/IMessageIncoming.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

//#include <algorithm>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;
      
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ILocationSubscriptionForAccount::toDebug(ForAccountPtr subscription)
      {
        return ILocationSubscription::toDebug(LocationSubscription::convert(subscription));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionForLocationDatabase
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationSubscriptionForLocationDatabase::ForLocationDatabasePtr ILocationSubscriptionForLocationDatabase::subscribe(
                                                                                                                           ILocationPtr location,
                                                                                                                           ILocationSubscriptionDelegatePtr delegate
                                                                                                                           )
      {
        return internal::ILocationSubscriptionFactory::singleton().subscribe(location, delegate);
      }

      //-----------------------------------------------------------------------
      ElementPtr ILocationSubscriptionForLocationDatabase::toDebug(ForLocationDatabasePtr subscription)
      {
        return ILocationSubscription::toDebug(LocationSubscription::convert(subscription));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationSubscriptionForLocationDatabases::ForLocationDatabasesPtr ILocationSubscriptionForLocationDatabases::subscribe(
                                                                                                                              ILocationPtr location,
                                                                                                                              ILocationSubscriptionDelegatePtr delegate
                                                                                                                              )
      {
        return internal::ILocationSubscriptionFactory::singleton().subscribe(location, delegate);
      }

      //-----------------------------------------------------------------------
      ILocationSubscriptionForLocationDatabases::ForLocationDatabasesPtr ILocationSubscriptionForLocationDatabases::subscribeAll(
                                                                                                                                 AccountPtr account,
                                                                                                                                 ILocationSubscriptionDelegatePtr delegate
                                                                                                                                 )
      {
        return internal::ILocationSubscriptionFactory::singleton().subscribeAll(account, delegate);
      }

      //-----------------------------------------------------------------------
      ElementPtr ILocationSubscriptionForLocationDatabases::toDebug(ForLocationDatabasesPtr subscription)
      {
        return ILocationSubscription::toDebug(LocationSubscription::convert(subscription));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      LocationSubscription::LocationSubscription(
                                                 AccountPtr account,
                                                 ILocationSubscriptionDelegatePtr delegate
                                                 ) :
        SharedRecursiveLock(account ? (*account) : SharedRecursiveLock::create()),
        mAccount(account),
        mDelegate(ILocationSubscriptionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate))
      {
        ZS_LOG_DEBUG(log("constructed"))
      }

      //-----------------------------------------------------------------------
      void LocationSubscription::init()
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("subscribing to a location attached to a dead account"))
          try {
            mDelegate->onLocationSubscriptionShutdown(mThisWeak.lock());
          } catch (ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          }
          mDelegate.reset();
          return;
        }
        account->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      LocationSubscription::~LocationSubscription()
      {
        if(isNoop()) return;
        
        ZS_LOG_DEBUG(log("destroyed"))
        mThisWeak.reset();

        cancel();
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::convert(ILocationSubscriptionPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::convert(ForAccountPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::convert(ForLocationDatabasePtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::convert(ForLocationDatabasesPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationSubscription => ILocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr LocationSubscription::toDebug(ILocationSubscriptionPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return LocationSubscription::convert(subscription)->toDebug();
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::subscribeAll(
                                                                 AccountPtr account,
                                                                 ILocationSubscriptionDelegatePtr delegate
                                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        LocationSubscriptionPtr pThis(new LocationSubscription(account, delegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr LocationSubscription::subscribe(
                                                              ILocationPtr inLocation,
                                                              ILocationSubscriptionDelegatePtr delegate
                                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inLocation)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        UseLocationPtr location = Location::convert(inLocation);
        AccountPtr account = location->getAccount();

        LocationSubscriptionPtr pThis(new LocationSubscription(account, delegate));
        pThis->mLocation = location;
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ILocationPtr LocationSubscription::getSubscribedToLocation() const
      {
        AutoRecursiveLock lock(*this);
        return Location::convert(mLocation);
      }

      //-----------------------------------------------------------------------
      bool LocationSubscription::LocationSubscription::isShutdown() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      void LocationSubscription::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_DEBUG(log("cancel called but already shutdown (probably okay)"))
          return;
        }

        LocationSubscriptionPtr subscription = mThisWeak.lock();

        if ((mDelegate) &&
            (subscription)) {
          try {
            mDelegate->onLocationSubscriptionShutdown(subscription);
          } catch(ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate already gone"))
          }
        }

        mDelegate.reset();

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_DEBUG(log("cancel called but account gone"))
          return;
        }

        account->notifyDestroyed(*this);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationSubscription => ILocationSubscriptionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationSubscription::notifyLocationConnectionStateChanged(
                                                                      LocationPtr inLocation,
                                                                      LocationConnectionStates state
                                                                      )
      {
        UseLocationPtr location = inLocation;

        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notify of location connection state changed after shutdown"))
          return;
        }

        if (mLocation) {
          if (mLocation->getID() != location->getID()) {
            ZS_LOG_TRACE(log("ignoring location connection state change from unrelated location") + ZS_PARAM("subscribing", mLocation->toDebug()) + ZS_PARAM("notified", location->toDebug()))
            return;
          }
        }

        try {
          ZS_LOG_DEBUG(log("notifying location subscription of state change") + ZS_PARAM("subscribing", UseLocation::toDebug(mLocation)) + ZS_PARAM("notified location", location->toDebug()) + ZS_PARAM("state", ILocation::toString(state)))
          mDelegate->onLocationSubscriptionLocationConnectionStateChanged(mThisWeak.lock(), Location::convert(location), state);
        } catch(ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void LocationSubscription::notifyMessageIncoming(IMessageIncomingPtr message)
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notify of incoming message after shutdown"))
          return;
        }

        UseLocationPtr location = Location::convert(message->getLocation());
        if (!location) {
          ZS_LOG_DEBUG(log("ignoring incoming message missing location"))
          return;
        }

        if (mLocation) {
          if (mLocation->getID() != location->getID()) {
            ZS_LOG_TRACE(log("ignoring incoming message from wrong location") + ZS_PARAM("subscribing", mLocation->toDebug()) + ZS_PARAM("incoming", IMessageIncoming::toDebug(message)))
            return;
          }
        }

        try {
          ZS_LOG_DEBUG(log("notifying peer subscription of messaging incoming") + ZS_PARAM("subscribing", UseLocation::toDebug(mLocation)) + ZS_PARAM("incoming", IMessageIncoming::toDebug(message)))
          mDelegate->onLocationSubscriptionMessageIncoming(mThisWeak.lock(), message);
        } catch(ILocationSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void LocationSubscription::notifyShutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params LocationSubscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("LocationSubscription");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationSubscription::toDebug() const
      {
        AutoRecursiveLock lock(*this);
        ElementPtr resultEl = Element::create("LocationSubscription");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "subscribing", mLocation ? "peer" : "all");
        IHelper::debugAppend(resultEl, UseLocation::toDebug(mLocation));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationSubscriptionFactory &ILocationSubscriptionFactory::singleton()
      {
        return LocationSubscriptionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr ILocationSubscriptionFactory::subscribeAll(
                                                                         AccountPtr account,
                                                                         ILocationSubscriptionDelegatePtr delegate
                                                                         )
      {
        if (this) {}
        return LocationSubscription::subscribeAll(account, delegate);
      }

      //-----------------------------------------------------------------------
      LocationSubscriptionPtr ILocationSubscriptionFactory::subscribe(
                                                                      ILocationPtr location,
                                                                      ILocationSubscriptionDelegatePtr delegate
                                                                      )
      {
        if (this) {}
        return LocationSubscription::subscribe(location, delegate);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationSubscription
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr ILocationSubscription::toDebug(ILocationSubscriptionPtr subscription)
    {
      return internal::LocationSubscription::toDebug(subscription);
    }

    //-------------------------------------------------------------------------
    ILocationSubscriptionPtr ILocationSubscription::subscribeAll(
                                                                 IAccountPtr account,
                                                                 ILocationSubscriptionDelegatePtr delegate
                                                                 )
    {
      return internal::ILocationSubscriptionFactory::singleton().subscribeAll(internal::Account::convert(account), delegate);
    }

    //-------------------------------------------------------------------------
    ILocationSubscriptionPtr ILocationSubscription::subscribe(
                                                              ILocationPtr location,
                                                              ILocationSubscriptionDelegatePtr delegate
                                                              )
    {
      return internal::ILocationSubscriptionFactory::singleton().subscribe(location, delegate);
    }
  }
}
