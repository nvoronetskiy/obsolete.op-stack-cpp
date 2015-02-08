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

#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabases.h>

//#include <openpeer/stack/internal/stack_Location.h>
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

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesManagerForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabasesManagerForLocationDatabases::ForLocationDatabasesPtr ILocationDatabasesManagerForLocationDatabases::singleton()
      {
        return internal::ILocationDatabasesManagerFactory::singleton().singletonManager();
      }

      //-----------------------------------------------------------------------
      LocationDatabasesPtr ILocationDatabasesManagerForLocationDatabases::getOrCreateForLocation(ILocationPtr location)
      {
        ForLocationDatabasesPtr singleton = ILocationDatabasesManagerForLocationDatabases::singleton();
        if (!singleton) return LocationDatabasesPtr();

        return singleton->getOrCreateForLocation(location);
      }

      //-----------------------------------------------------------------------
      void ILocationDatabasesManagerForLocationDatabases::notifyDestroyed(LocationDatabases &databases)
      {
        ForLocationDatabasesPtr singleton = ILocationDatabasesManagerForLocationDatabases::singleton();
        if (!singleton) return;

        return singleton->notifyDestroyed(databases);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabasesManager
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabasesManager::LocationDatabasesManager() :
        SharedRecursiveLock(SharedRecursiveLock::create())
      {
        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabasesManager::init()
      {
      }

      //-----------------------------------------------------------------------
      LocationDatabasesManagerPtr LocationDatabasesManager::create()
      {
        LocationDatabasesManagerPtr pThis(new LocationDatabasesManager);
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationDatabasesManagerPtr LocationDatabasesManager::singleton()
      {
        static SingletonLazySharedPtr<LocationDatabasesManager> singleton(LocationDatabasesManager::create());
        LocationDatabasesManagerPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      LocationDatabasesManager::~LocationDatabasesManager()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      LocationDatabasesManagerPtr LocationDatabasesManager::convert(ForLocationDatabasesPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(LocationDatabasesManager, object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabasesManager => ILocationDatabasesManagerForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabasesPtr LocationDatabasesManager::getOrCreateForLocation(ILocationPtr location)
      {
        AutoRecursiveLock lock(*this);

        auto found = mDatabases.find(location->getID());
        if (found != mDatabases.end()) {
          UseLocationDatabasesPtr existing = (*found).second.lock();
          if (existing) {
            ZS_LOG_TRACE(log("found existing database for location") + ILocation::toDebug(location) + UseLocationDatabases::toDebug(existing))
            return LocationDatabases::convert(existing);
          }

          ZS_LOG_WARNING(Detail, log("existing location databases is now gone (thus pruning)") + ILocation::toDebug(location))
          mDatabases.erase(found);
          found = mDatabases.end();
        }

        UseLocationDatabasesPtr databases = UseLocationDatabases::create(location);

        mDatabases[location->getID()] = databases;

        ZS_LOG_DEBUG(log("remembering newly created database location") + UseLocationDatabases::toDebug(databases))

        return LocationDatabases::convert(databases);
      }

      //-----------------------------------------------------------------------
      void LocationDatabasesManager::notifyDestroyed(LocationDatabases &inDatabases)
      {
        UseLocationDatabases &databases = inDatabases;

        ILocationPtr location = databases.getLocation();

        AutoRecursiveLock lock(*this);
        auto found = mDatabases.find(location->getID());
        if (found == mDatabases.end()) {
          ZS_LOG_WARNING(Debug, log("location databases was not found in manager (probably okay)") + ZS_PARAM("databases", databases.getID()))
          return;
        }

        ZS_LOG_TRACE(log("removed location databases from manager") + ZS_PARAM("databases", databases.getID()))

        mDatabases.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabasesManager => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params LocationDatabasesManager::slog(const char *message)
      {
        return Log::Params(message, "LocationDatabasesManager");
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabasesManager::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("LocationDatabasesManager");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params LocationDatabasesManager::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabasesManager::toDebug() const
      {
        ElementPtr resultEl = Element::create("LocationDatabasesManager");

        UseServicesHelper::debugAppend(resultEl, "id", mID);

        UseServicesHelper::debugAppend(resultEl, "databases", mDatabases.size());

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabasesManagerFactory &ILocationDatabasesManagerFactory::singleton()
      {
        return ILocationDatabasesManagerFactory::singleton();
      }

      //-----------------------------------------------------------------------
      LocationDatabasesManagerPtr ILocationDatabasesManagerFactory::singletonManager()
      {
        if (this) {}
        return LocationDatabasesManager::singleton();
      }

    }

  }
}
