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
    #pragma mark ILocationDatabaseTypes
    #pragma mark

    interaction ILocationDatabaseTypes
    {
      struct Entry
      {
        typedef String UniqueID;

        enum Dispositions
        {
          Disposition_Add,
          Disposition_Update,
          Disposition_Remove,
        };

        static const char *toString(Dispositions disposition);

        Dispositions mDisposition;
        UniqueID mUniqueID;
        UINT mVersion;
        ElementPtr mMetaData;
        ElementPtr mData;

        Time mCreation;
        Time mLastUpdated;

        bool hasData() const;
        ElementPtr toDebug() const;
      };

      ZS_DECLARE_PTR(Entry)

      typedef std::list<Entry> EntryList;
      ZS_DECLARE_PTR(EntryList)

      typedef std::list<Entry::UniqueID> UniqueIDList;
      ZS_DECLARE_PTR(UniqueIDList)
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabase
    #pragma mark

    interaction ILocationDatabase : public ILocationDatabaseTypes
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Get debug information about the location database object.
      static ElementPtr toDebug(ILocationDatabasePtr database);

      //-----------------------------------------------------------------------
      // PURPOSE: Subscribe to database changes for a particular database from
      //          a particular peer's location.
      virtual ILocationDatabasePtr open(
                                        ILocationDatabaseDelegatePtr inDelegate,
                                        ILocationPtr location,
                                        const char *databaseID,
                                        bool inAutomaticallyDownloadDatabaseData
                                        ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get a unique object instance identifier.
      virtual PUID getID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the Location object associated with this database.
      virtual ILocationPtr getLocation() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the unique database ID for this database at this
      //          location.
      virtual String getDatabaseID() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the meta data associated with the database.
      virtual ElementPtr getMetaData() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the time when this database will expire.
      virtual Time getExpires() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get latest batch of updates available after notification
      //          that updates are available.
      // RETURNS: If a NULL EntryListPtr() is returned then a conflict has
      //          occured and the entire entry database list must be flushed
      //          and redownloaded from scratch.
      //
      //          An empty return result indicates there is no more data
      //          available at this time.
      virtual EntryListPtr getUpdates(
                                      const String &inExistingVersion,  // pass in String() when no data has been fetched before
                                      String &outNewVersion             // the version to which the database has now been updated
                                      ) const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get a database entry data based on a unique entry identifier.
      virtual EntryPtr getEntry(const char *inUniqueID) const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: When database information is only downloaded on demand this
      //          method will
      virtual void notifyWhenDataReady(
                                       const UniqueIDList &needingEntryData,
                                       ILocationDatabaseDataReadyDelegatePtr inDelegate
                                       ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Flag this database to have all the content removed.
      virtual void remove() = 0;
    };
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseDelegate
    #pragma mark

    interaction ILocationDatabaseDelegate
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Notification event when the database data has changed.
      virtual void onLocationDatabaseChanged(ILocationDatabasePtr inDatabase) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseDataReadyDelegate
    #pragma mark

    interaction ILocationDatabaseDataReadyDelegate
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Notification event when the all the requested data is now
      //          available for the location.
      virtual void onLocationDatabaseDataReady(ILocationDatabasePtr inDatabase) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseSubscription
    #pragma mark

    interaction ILocationDatabaseSubscription
    {
      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual void background() = 0;
    };

  }
}


ZS_DECLARE_PROXY_BEGIN(openpeer::stack::ILocationDatabaseDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationDatabasePtr, ILocationDatabasePtr)
ZS_DECLARE_PROXY_METHOD_1(onLocationDatabaseChanged, ILocationDatabasePtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::ILocationDatabaseDelegate, openpeer::stack::ILocationDatabaseSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::ILocationDatabasePtr, ILocationDatabasePtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onLocationDatabaseChanged, ILocationDatabasePtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::ILocationDatabaseDataReadyDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationDatabasePtr, ILocationDatabasePtr)
ZS_DECLARE_PROXY_METHOD_1(onLocationDatabaseDataReady, ILocationDatabasePtr)
ZS_DECLARE_PROXY_END()
