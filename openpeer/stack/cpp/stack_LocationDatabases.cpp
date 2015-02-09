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

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
//#include <openpeer/stack/internal/stack_Peer.h>
//#include <openpeer/stack/internal/stack_Account.h>
//#include <openpeer/stack/internal/stack_Peer.h>
//#include <openpeer/stack/internal/stack_Helper.h>
//
#include <openpeer/services/IHelper.h>
//
#include <zsLib/Log.h>
#include <zsLib/XML.h>
//#include <zsLib/helpers.h>
//#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
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
        mLocation(Location::convert(location))
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!mLocation)

        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabases::init()
      {
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

        UseServicesHelper::debugAppend(resultEl, UseLocation::toDebug(mLocation));

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

#define WARNING_CHECK_IF_SHUTDOWN 1
#define WARNING_CHECK_IF_SHUTDOWN 2
//        if (isShutdown()) {
//          mSubscriptions.clear();
//        }

        return subscription;
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
