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
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(stack::internal::IStackForInternal, UseStack)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(ILocationDatabases::LocationDatabaseList, LocationDatabaseList)

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
      LocationDatabaseListPtr LocationDatabases::getUpdates(
                                                            const String &inExistingVersion,
                                                            String &outNewVersion
                                                            ) const
      {
        return LocationDatabaseListPtr();
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
      void LocationDatabases::notifyDestroyed(UseLocationDatabase &databases)
      {
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
      #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabase
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
      bool LocationDatabases::stepOpenLocal()
      {
        if (!isLocal()) {
          ZS_LOG_TRACE(log("skipping open local - not a local location"))
          return true;
        }

        return true;
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
