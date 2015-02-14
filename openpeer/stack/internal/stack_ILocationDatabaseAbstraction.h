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

#define OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN (-1)


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseAbstractionTypes
      #pragma mark

      interaction ILocationDatabaseAbstractionTypes
      {
        typedef int index;

        ZS_DECLARE_STRUCT_PTR(PeerLocationRecord)
        ZS_DECLARE_STRUCT_PTR(DatabaseRecord)
        ZS_DECLARE_STRUCT_PTR(DatabaseChangeRecord)
        ZS_DECLARE_STRUCT_PTR(EntryRecord)
        ZS_DECLARE_STRUCT_PTR(EntryChangeRecord)

        typedef std::list<PeerLocationRecord> PeerLocationRecordList;
        ZS_DECLARE_PTR(PeerLocationRecordList)

        typedef std::list<DatabaseRecord> DatabaseRecordList;
        ZS_DECLARE_PTR(DatabaseRecordList)

        typedef std::list<DatabaseChangeRecord> DatabaseChangeRecordList;
        ZS_DECLARE_PTR(DatabaseChangeRecordList)

        typedef std::list<EntryRecord> EntryRecordList;
        ZS_DECLARE_PTR(EntryRecordList)

        typedef std::list<EntryChangeRecord> EntryChangeRecordList;
        ZS_DECLARE_PTR(EntryChangeRecordList)

        typedef String PeerURI;
        typedef std::list<PeerURI> PeerURIList;
        ZS_DECLARE_PTR(PeerURIList)

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerLocationRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct PeerLocationRecord
        {
          index     mIndex {-1};              // [auto, unique]
          String    mPeerURI;
          String    mLocationID;
          String    mLastDownloadedVersion;
          Time      mLastAccessed;
          String    mUpdateVersion;

          ElementPtr toDebug() const;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark DatabaseRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct DatabaseRecord
        {
          index   mIndex {-1};              // [auto, unique]
          index   mIndexPeerLocation {-1};
          String  mDatabaseID;
          String  mLastDownloadedVersion;
          ElementPtr mMetaData;
          Time    mCreated;
          Time    mExpires;
          String  mUpdateVersion;

          ElementPtr toDebug() const;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark DatabaseChangeRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct DatabaseChangeRecord
        {
          enum Dispositions
          {
            Disposition_None    = 0,
            Disposition_Add     = 1,
            Disposition_Update  = 2,
            Disposition_Remove  = 3,
          };

          static const char *toString(Dispositions disposition);

          index         mIndex {-1};              // [auto, unique]
          index         mIndexPeerLocation {-1};
          Dispositions  mDisposition {Disposition_None};
          index         mIndexDatabase;

          ElementPtr toDebug() const;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark EntryRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct EntryRecord
        {
          index       mIndex {-1};              // [auto, unique]
          String      mEntryID;
          int         mVersion {0};
          ElementPtr  mMetaData;
          ElementPtr  mData;
          ULONGEST    mDataLength;
          bool        mDataFetched {false};
          Time        mCreated;
          Time        mUpdated;

          ElementPtr toDebug() const;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark EntryChangeRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct EntryChangeRecord
        {
          enum Dispositions
          {
            Disposition_None    = 0,
            Disposition_Add     = 1,
            Disposition_Update  = 2,
            Disposition_Remove  = 3,
          };

          static const char *toString(Dispositions disposition);

          index         mIndex {-1};              // [auto, unique]
          Dispositions  mDisposition {Disposition_Add};
          String        mEntryID;

          ElementPtr toDebug() const;
        };
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseAbstraction
      #pragma mark

      interaction ILocationDatabaseAbstraction : public ILocationDatabaseAbstractionTypes
      {
        ZS_DECLARE_INTERACTION_PTR(IPeerLocationTable)
        ZS_DECLARE_INTERACTION_PTR(IDatabaseTable)
        ZS_DECLARE_INTERACTION_PTR(IDatabaseChangeTable)
        ZS_DECLARE_INTERACTION_PTR(IPermissionTable)
        ZS_DECLARE_INTERACTION_PTR(IEntryTable)
        ZS_DECLARE_INTERACTION_PTR(IEntryChangeTable)


        static ElementPtr toDebug(ILocationDatabaseAbstractionPtr session);

        static ILocationDatabaseAbstractionPtr createMaster(
                                                            const char *inHashRepresentingUser,
                                                            const char *inUserStorageFilePath
                                                            );

        static ILocationDatabaseAbstractionPtr openDatabase(
                                                            ILocationDatabaseAbstractionPtr masterDatabase,
                                                            const char *peerURI,
                                                            const char *locationID,
                                                            const char *databaseID
                                                            );

        static bool deleteDatabase(
                                   ILocationDatabaseAbstractionPtr masterDatabase,
                                   const char *peerURI,
                                   const char *locationID,
                                   const char *databaseID
                                   );

        virtual PUID getID() const = 0;

        virtual IPeerLocationTablePtr peerLocationTable() const = 0;
        virtual IDatabaseTablePtr databaseTable() const = 0;
        virtual IDatabaseChangeTablePtr databaseChangeTable() const = 0;
        virtual IPermissionTablePtr permissionTable() const = 0;
        virtual IEntryTablePtr entryTable() const = 0;
        virtual IEntryChangeTablePtr entryChangeTable() const = 0;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IPeerLocationTable
        #pragma mark

        interaction IPeerLocationTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: create an entry for the list of databases at a particular
          //          peer location
          virtual void createOrObtain(
                                      const char *peerURI,
                                      const char *locationID,
                                      PeerLocationRecord &outRecord
                                      ) = 0;

          virtual String updateVersion(index indexPeerLocationRecord) = 0;

          virtual void notifyDownloaded(
                                        index indexPeerLocationRecord,
                                        const char *downloadedToVersion
                                        ) = 0;

          virtual PeerLocationRecordListPtr getUnusedLocationsBatch(const Time &lastAccessedBefore) const = 0;

          virtual void remove(index indexPeerLocationRecord) = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDatabaseTable
        #pragma mark

        interaction IDatabaseTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: remove all database records associated to a peer location
          //          location
          virtual void flushAllForPeerLocation(index indexPeerLocationRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: add a database record to list of peer locations
          virtual void addOrUpdate(
                                   DatabaseRecord &ioRecord,
                                   DatabaseChangeRecord &outChangeRecord
                                   ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: update a database record to indicate it has changed
          //          since last checked
          virtual String updateVersion(index indexDatabase) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: remove a database from the database table
          // RETURNS: true if entry was removed
          //          ioRecord - last information about database
          //          outChangeRecord - information to insert into databaes
          //                            changed record table
          virtual bool remove(
                              const DatabaseRecord &inRecord,
                              DatabaseChangeRecord &outChangeRecord
                              ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a database record given the index
          virtual bool getByIndex(
                                  index indexDatabaseRecord,
                                  DatabaseRecord &outRecord
                                  ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of databases related to a peer location
          virtual DatabaseRecordListPtr getBatchByPeerLocationIndex(
                                                                    index indexPeerLocationRecord,
                                                                    index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                    ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of databases related to a peer location
          virtual DatabaseRecordListPtr getBatchByPeerLocationIndexForPeerURI(
                                                                              const char *peerURIWithPermission,
                                                                              index indexPeerLocationRecord,
                                                                              index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                              ) const = 0;
          
          //-------------------------------------------------------------------
          // PURPOSE: get a batch of expired database records
          virtual DatabaseRecordListPtr getBatchExpired(
                                                        index indexPeerLocationRecord,
                                                        const Time &now
                                                        ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Notify that database entries have been downloaded for a
          //          given database.
          virtual void notifyDownloaded(
                                        index indexDatabaseRecord,
                                        const char *downloadedToVersion
                                        ) = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IDatabaseChangeTable
        #pragma mark

        interaction IDatabaseChangeTable
        {
          virtual void flushAllForPeerLocation(index indexPeerLocationRecord) = 0;

          virtual void flushAllForDatabase(index indexDatabaseRecord) = 0;

          virtual void insert(const DatabaseChangeRecord &record) = 0;

          virtual bool getByIndex(
                                  index indexDatabaseChangeRecord,
                                  DatabaseChangeRecord &outRecord
                                  ) const = 0;

          virtual DatabaseChangeRecordListPtr getChangeBatch(
                                                             index indexPeerLocationRecord,
                                                             index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                             ) const = 0;

          virtual DatabaseChangeRecordListPtr getChangeBatchForPeerURI(
                                                                       const char *peerURIWithPermission,
                                                                       index indexPeerLocationRecord,
                                                                       index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                       ) const = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IPermissionTable
        #pragma mark

        interaction IPermissionTable
        {
          virtual void flushAllForPeerLocation(index indexPeerLocationRecord) = 0;

          virtual void flushAllForDatabase(index indexDatabaseRecord) = 0;

          virtual void insert(
                              index indexPeerLocation,
                              index indexDatabaseRecord,
                              const PeerURIList &uris
                              ) = 0;

          virtual void getByDatabaseIndex(
                                          index indexDatabaseRecord,
                                          PeerURIList &outURIs
                                          ) const = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IEntryTable
        #pragma mark

        interaction IEntryTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: add an entry record for a database
          // RETURNS: true if entry was added
          virtual bool add(
                           const EntryRecord &entry,
                           EntryChangeRecord &outChangeRecord
                           ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: update an entry record for a database
          // RETURNS: true if entry was updated
          virtual bool update(
                              const EntryRecord &entry,
                              EntryChangeRecord &outChangeRecord
                              ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: remove an entry record for a database
          // RETURNS: true if entry was removed
          //          outChangeRecord - information to insert into databaes
          //                            changed record table
          virtual bool remove(
                              const EntryRecord &entry,
                              EntryChangeRecord &outChangeRecord
                              ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of entries after an index record
          virtual EntryRecordListPtr getBatch(
                                              bool includeData,
                                              index afterIndexEntry = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                              ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get an individual entry record
          virtual bool getEntry(
                                EntryRecord &ioRecord,
                                bool includeData
                                ) const = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IEntryChangeTable
        #pragma mark

        interaction IEntryChangeTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: insert a entry change record
          virtual void insert(const EntryChangeRecord &record) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of entry changes after a particular change
          //          index
          virtual EntryChangeRecordListPtr getBatch(index afterIndexEntryChanged = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN) const = 0;
        };

      };

    }
  }
}
