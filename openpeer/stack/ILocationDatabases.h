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

#pragma once

#include <openpeer/stack/types.h>
#include <openpeer/stack/ILocation.h>

namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabasesTypes
    #pragma mark

    interaction ILocationDatabasesTypes
    {
      struct LocationDatabase
      {
        typedef String DatabaseID;

        enum Dispositions
        {
          Disposition_Add,
          Disposition_Update,
          Disposition_Remove,
        };

        static const char *toString(Dispositions disposition);

        Dispositions mDisposition;
        DatabaseID mDatabaseID;
        ElementPtr mMetaData;

        Time mCreation;
        Time mLastUpdated;

        String mLastUpdateVersion;

        bool hasData() const;
        ElementPtr toDebug() const;
      };

      ZS_DECLARE_PTR(LocationDatabase)

      typedef std::list<LocationDatabase> LocationDatabaseList;
      ZS_DECLARE_PTR(LocationDatabaseList)
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabases
    #pragma mark

    interaction ILocationDatabases : public ILocationDatabasesTypes
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Get debug information about the location databases object.
      static ElementPtr toDebug(ILocationDatabasesPtr databases);

      //-----------------------------------------------------------------------
      // PURPOSE: Open information about databases contained within a specific
      //          peer's location.
      static ILocationDatabasesPtr open(
                                        ILocationPtr inLocation,
                                        ILocationDatabasesDelegatePtr inDelegate
                                        );

      //-----------------------------------------------------------------------
      // PURPOSE: Get a unique object instance identifier.
      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the Location object associated with this set of
      //          databases.
      virtual ILocationPtr getLocation() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get latest batch of databases available for a location.
      // RETURNS: If a NULL LocationDatabaseListPtr() is returned then a
      //          conflict has occured and the entire list of databases must
      //          be obtained from scratch.
      //
      //          An empty return result indicates there are no more databases
      //          available at this time.
      virtual LocationDatabaseListPtr getUpdates(
                                                 const String &inExistingVersion,  // pass in String() when no data has been fetched before
                                                 String &outNewVersion             // the version to which the database has now been updated
                                                 ) const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabasesDelegate
    #pragma mark

    interaction ILocationDatabasesDelegate
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Notify that the available list of datbases has changed
      virtual void onLocationDatabasesChanged(ILocationDatabasesPtr inDatabases) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Notify that the available list of datbases has changed
      virtual void onLocationDatabasesShutdown(ILocationDatabasesPtr inDatabases) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabasesSubscription
    #pragma mark

    interaction ILocationDatabasesSubscription
    {
      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual void background() = 0;
    };
  }
}


ZS_DECLARE_PROXY_BEGIN(openpeer::stack::ILocationDatabasesDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationDatabasesPtr, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_METHOD_1(onLocationDatabasesChanged, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_METHOD_1(onLocationDatabasesShutdown, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::ILocationDatabasesDelegate, openpeer::stack::ILocationDatabasesSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::ILocationDatabasesPtr, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onLocationDatabasesChanged, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onLocationDatabasesShutdown, ILocationDatabasesPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()
