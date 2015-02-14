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

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationDatabaseTearAway.h>
#include <openpeer/stack/internal/stack_LocationDatabases.h>
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

      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::EntryList, EntryList)
      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::Entry, Entry)
      ZS_DECLARE_TYPEDEF_PTR(LocationDatabase::PeerList, PeerList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase
      #pragma mark

      //-----------------------------------------------------------------------
      LocationDatabase::LocationDatabase(UseLocationDatabasesPtr databases) :
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDatabases(databases)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!databases)

        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::init()
      {
      }

      //-----------------------------------------------------------------------
      LocationDatabase::~LocationDatabase()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))

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
      #pragma mark LocationDatabases => ILocationDatabases
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
                                                  const char *databaseID,
                                                  bool inAutomaticallyDownloadDatabaseData
                                                  )
      {
        return ILocationDatabasePtr();
      }

      //-----------------------------------------------------------------------
      ILocationPtr LocationDatabase::getLocation() const
      {
        return ILocationPtr();
      }

      //-----------------------------------------------------------------------
      String LocationDatabase::getDatabaseID() const
      {
        return String();
      }

      //-----------------------------------------------------------------------
      ElementPtr LocationDatabase::getMetaData() const
      {
        return ElementPtr();
      }

      //-----------------------------------------------------------------------
      Time LocationDatabase::getExpires() const
      {
        return Time();
      }

      //-----------------------------------------------------------------------
      EntryListPtr LocationDatabase::getUpdates(
                                                const String &inExistingVersion,
                                                String &outNewVersion
                                                ) const
      {
        return EntryListPtr();
      }

      //-----------------------------------------------------------------------
      EntryPtr LocationDatabase::getEntry(const char *inUniqueID) const
      {
        return EntryPtr();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyWhenDataReady(
                                                 const UniqueIDList &needingEntryData,
                                                 ILocationDatabaseDataReadyDelegatePtr inDelegate
                                                 )
      {
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase => ILocationDatabaseLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationDatabaseLocalPtr LocationDatabase::toLocal(ILocationDatabasePtr location)
      {
        return ILocationDatabaseLocalPtr();
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
        return ILocationDatabaseLocalPtr();
      }

      //-----------------------------------------------------------------------
      PeerListPtr LocationDatabase::getPeerAccessList() const
      {
        return PeerListPtr();
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::setExpires(const Time &time)
      {
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::add(
                                 const char *uniqueID,
                                 ElementPtr inMetaData,
                                 ElementPtr inData
                                 )
      {
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::update(
                                    const char *uniqueID,
                                    ElementPtr inData
                                    )
      {
      }
      
      //-----------------------------------------------------------------------
      void LocationDatabase::remove(const char *uniqueID)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => ILocationDatabaseForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyConflict()
      {
        ZS_LOG_WARNING(Detail, log("notified of conflict"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyUpdated()
      {
        ZS_LOG_DEBUG(log("notified updated"))
      }

      //-----------------------------------------------------------------------
      void LocationDatabase::notifyRemoved()
      {
        ZS_LOG_WARNING(Detail, log("notified removed"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases => (internal)
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

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabases
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
    #pragma mark ILocationDatabases
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
  }
}
