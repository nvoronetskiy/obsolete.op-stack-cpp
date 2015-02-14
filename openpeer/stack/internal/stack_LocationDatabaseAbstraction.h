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

#include <openpeer/stack/internal/stack_ILocationDatabaseAbstraction.h>

#include <easySQLite/SqlDatabase.h>

#define OPENPEER_STACK_SETTING_LOCATION_MASTER_DATABASE_FILE "openpeer/stack/location-database-master-filename"
#define OPENPEER_STACK_SETTING_LOCATION_DATABASE_FILE_PREFIX "openpeer/stack/location-database-filename-prefix"
#define OPENPEER_STACK_SETTING_LOCATION_DATABASE_FILE_POSTFIX "openpeer/stack/location-database-filename-postfix"

#define OPENPEER_STACK_SETTING_LOCATION_ANALYZE_DATABASE_RANDOMLY_EVERY "openpeer/stack/location-analyze-database-randomly-every"


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabaseAbstraction
      #pragma mark

      class LocationDatabaseAbstraction : public Noop,
                                          public SharedRecursiveLock,
                                          public ILocationDatabaseAbstraction,
                                          public sql::Database::Trace
      {
      public:
        friend interaction ILocationDatabaseAbstractionFactory;
        friend interaction ILocationDatabaseAbstraction;

        ZS_DECLARE_CLASS_PTR(PeerLocationTable)
        ZS_DECLARE_CLASS_PTR(DatabaseTable)
        ZS_DECLARE_CLASS_PTR(DatabaseChangeTable)
        ZS_DECLARE_CLASS_PTR(PermissionTable)
        ZS_DECLARE_CLASS_PTR(EntryTable)
        ZS_DECLARE_CLASS_PTR(EntryChangeTable)

        friend class PeerLocationTable;
        friend class DatabaseTable;
        friend class DatabaseChangeTable;
        friend class PermissionTable;
        friend class EntryTable;
        friend class EntryChangeTable;

        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)
        ZS_DECLARE_TYPEDEF_PTR(sql::Exception, SqlException)

      protected:
        LocationDatabaseAbstraction(
                                    const char *inHashRepresentingUser,
                                    const char *inUserStorageFilePath
                                    );

        LocationDatabaseAbstraction(
                                    ILocationDatabaseAbstractionPtr masterDatabase,
                                    const char *peerURI,
                                    const char *locationID,
                                    const char *databaseID,
                                    bool deleteSelf
                                    );

        LocationDatabaseAbstraction(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create()) {}

        void init();

      public:
        ~LocationDatabaseAbstraction();

        static LocationDatabaseAbstractionPtr convert(ILocationDatabaseAbstractionPtr session);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => ILocationDatabaseAbstraction
        #pragma mark

        static ElementPtr toDebug(ILocationDatabaseAbstractionPtr session);

        static LocationDatabaseAbstractionPtr createMaster(
                                                           const char *inHashRepresentingUser,
                                                           const char *inUserStorageFilePath
                                                           );

        static LocationDatabaseAbstractionPtr openDatabase(
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

        virtual PUID getID() const;

        virtual IPeerLocationTablePtr peerLocationTable() const;
        virtual IDatabaseTablePtr databaseTable() const;
        virtual IDatabaseChangeTablePtr databaseChangeTable() const;
        virtual IPermissionTablePtr permissionTable() const;
        virtual IEntryTablePtr entryTable() const;
        virtual IEntryChangeTablePtr entryChangeTable() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => (friend location)
        #pragma mark

        virtual String getUserHash() const;
        virtual String getUserStoragePath() const;

      public:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::PeerLocationTable
        #pragma mark

        class PeerLocationTable : public IPeerLocationTable
        {
          PeerLocationTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static PeerLocationTablePtr create(LocationDatabaseAbstractionPtr outer)   {return PeerLocationTablePtr(new PeerLocationTable(outer));}

          virtual void createOrObtain(
                                      const char *peerURI,
                                      const char *locationID,
                                      PeerLocationRecord &outRecord
                                      )                                           {mOuter->IPeerLocationTable_createOrObtain(peerURI, locationID, outRecord);}

          virtual String updateVersion(index indexPeerLocationRecord)             {return mOuter->IPeerLocationTable_updateVersion(indexPeerLocationRecord);}

          virtual void notifyDownloaded(
                                        index indexPeerLocationRecord,
                                        const char *downloadedToVersion
                                        )                                         {mOuter->IPeerLocationTable_notifyDownloaded(indexPeerLocationRecord, downloadedToVersion);}

          virtual PeerLocationRecordListPtr getUnusedLocationsBatch(const Time &lastAccessedBefore) const  {return mOuter->IPeerLocationTable_getUnusedLocationsBatch(lastAccessedBefore);}

          virtual void remove(index indexPeerLocationRecord)                         {mOuter->IPeerLocationTable_remove(indexPeerLocationRecord);}

        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::DatabaseTable
        #pragma mark

        class DatabaseTable : public IDatabaseTable
        {
          DatabaseTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static DatabaseTablePtr create(LocationDatabaseAbstractionPtr outer)  {return DatabaseTablePtr(new DatabaseTable(outer));}

          virtual void flushAllForPeerLocation(index indexPeerLocationRecord)   {mOuter->IDatabaseTable_flushAllForPeerLocation(indexPeerLocationRecord);}

          virtual void addOrUpdate(
                                   DatabaseRecord &ioRecord,
                                   DatabaseChangeRecord &outChangeRecord
                                   )                                            {mOuter->IDatabaseTable_addOrUpdate(ioRecord, outChangeRecord);}

          virtual String updateVersion(index indexDatabase)                     {return mOuter->IDatabaseTable_updateVersion(indexDatabase);}

          virtual bool remove(
                              const DatabaseRecord &inRecord,
                              DatabaseChangeRecord &outChangeRecord
                              )                                                 {return mOuter->IDatabaseTable_remove(inRecord, outChangeRecord);}

          virtual bool getByIndex(
                                  index indexDatabaseRecord,
                                  DatabaseRecord &outRecord
                                  ) const                                       {return mOuter->IDatabaseTable_getByIndex(indexDatabaseRecord, outRecord);}

          virtual DatabaseRecordListPtr getBatchByPeerLocationIndex(
                                                                 index indexPeerLocationRecord,
                                                                 index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                 ) const        {return mOuter->IDatabaseTable_getBatchByPeerLocationIndex(indexPeerLocationRecord, afterIndexDatabase);}

          virtual DatabaseRecordListPtr getBatchByPeerLocationIndexForPeerURI(
                                                                           const char *peerURIWithPermission,
                                                                           index indexPeerLocationRecord,
                                                                           index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                           ) const  {return mOuter->IDatabaseTable_getBatchByPeerLocationIndexForPeerURI(peerURIWithPermission, indexPeerLocationRecord, afterIndexDatabase);}

          virtual DatabaseRecordListPtr getBatchExpired(
                                                        index indexPeerLocationRecord,
                                                        const Time &now
                                                        ) const  {return mOuter->IDatabaseTable_getBatchExpired(indexPeerLocationRecord, now);}

          virtual void notifyDownloaded(
                                        index indexDatabaseRecord,
                                        const char *downloadedToVersion
                                        )                                       {return mOuter->IDatabaseTable_notifyDownloaded(indexDatabaseRecord, downloadedToVersion);}

        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::DatabaseChangeTable
        #pragma mark

        class DatabaseChangeTable : public IDatabaseChangeTable
        {
          DatabaseChangeTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static DatabaseChangeTablePtr create(LocationDatabaseAbstractionPtr outer)      {return DatabaseChangeTablePtr(new DatabaseChangeTable(outer));}

          virtual void flushAllForPeerLocation(index indexPeerLocationRecord)   {mOuter->IDatabaseChangeTable_flushAllForPeerLocation(indexPeerLocationRecord);}

          virtual void flushAllForDatabase(index indexDatabaseRecord)           {mOuter->IDatabaseChangeTable_flushAllForDatabase(indexDatabaseRecord);}

          virtual void insert(const DatabaseChangeRecord &record)               {mOuter->IDatabaseChangeTable_insert(record);}

          virtual bool getByIndex(
                                  index indexDatabaseChangeRecord,
                                  DatabaseChangeRecord &outRecord
                                  ) const                                       {return mOuter->IDatabaseChangeTable_getByIndex(indexDatabaseChangeRecord, outRecord);}

          virtual DatabaseChangeRecordListPtr getChangeBatch(
                                                             index indexPeerLocationRecord,
                                                             index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                             ) const            {return mOuter->IDatabaseChangeTable_getChangeBatch(indexPeerLocationRecord, afterIndexDatabaseChange);}

          virtual DatabaseChangeRecordListPtr getChangeBatchForPeerURI(
                                                                       const char *peerURIWithPermission,
                                                                       index indexPeerLocationRecord,
                                                                       index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                       ) const  {return mOuter->IDatabaseChangeTable_getChangeBatchForPeerURI(peerURIWithPermission, indexPeerLocationRecord, afterIndexDatabaseChange);}

        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::PermissionTable
        #pragma mark

        class PermissionTable : public IPermissionTable
        {
          PermissionTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static PermissionTablePtr create(LocationDatabaseAbstractionPtr outer)  {return PermissionTablePtr(new PermissionTable(outer));}

          virtual void flushAllForPeerLocation(index indexPeerLocationRecord)   {mOuter->IPermissionTable_flushAllForPeerLocation(indexPeerLocationRecord);}

          virtual void flushAllForDatabase(index indexDatabaseRecord)           {mOuter->IPermissionTable_flushAllForDatabase(indexDatabaseRecord);}

          virtual void insert(
                              index indexPeerLocation,
                              index indexDatabaseRecord,
                              const PeerURIList &uris
                              )                                                 {mOuter->IPermissionTable_insert(indexPeerLocation, indexDatabaseRecord, uris);}

          virtual void getByDatabaseIndex(
                                          index indexDatabaseRecord,
                                          PeerURIList &outURIs
                                          ) const                               {mOuter->IPermissionTable_getByDatabaseIndex(indexDatabaseRecord, outURIs);}

        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::EntryTable
        #pragma mark

        class EntryTable : public IEntryTable
        {
          EntryTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static EntryTablePtr create(LocationDatabaseAbstractionPtr outer)     {return EntryTablePtr(new EntryTable(outer));}

          virtual bool add(
                           const EntryRecord &entry,
                           EntryChangeRecord &outChangeRecord
                           )                                                    {return mOuter->IEntryTable_add(entry, outChangeRecord);}

          virtual bool update(
                              const EntryRecord &entry,
                              EntryChangeRecord &outChangeRecord
                              )                                                 {return mOuter->IEntryTable_update(entry, outChangeRecord);}

          virtual bool remove(
                              const EntryRecord &entry,
                              EntryChangeRecord &outChangeRecord
                              )                                                 {return mOuter->IEntryTable_remove(entry, outChangeRecord);}

          virtual EntryRecordListPtr getBatch(
                                              bool includeData,
                                              index afterIndexEntry = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                              ) const                           {return mOuter->IEntryTable_getBatch(includeData, afterIndexEntry);}

          virtual bool getEntry(
                                EntryRecord &ioRecord,
                                bool includeData
                                ) const                                         {return mOuter->IEntryTable_getEntry(ioRecord, includeData);}
        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction::EntryChangeTable
        #pragma mark

        class EntryChangeTable : public IEntryChangeTable
        {
          EntryChangeTable(LocationDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static EntryChangeTablePtr create(LocationDatabaseAbstractionPtr outer) {return EntryChangeTablePtr(new EntryChangeTable(outer));}

          virtual void insert(const EntryChangeRecord &record)                  {mOuter->IEntryChangeTable_insert(record);}

          virtual EntryChangeRecordListPtr getBatch(index afterIndexEntryChanged = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN) const {return mOuter->IEntryChangeTable_getBatch(afterIndexEntryChanged);}

        protected:
          LocationDatabaseAbstractionPtr mOuter;
        };


      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IPeerLocationTable
        #pragma mark

        void IPeerLocationTable_createOrObtain(
                                               const char *peerURI,
                                               const char *locationID,
                                               PeerLocationRecord &outRecord
                                               );

        String IPeerLocationTable_updateVersion(index indexPeerLocationRecord);

        void IPeerLocationTable_notifyDownloaded(
                                                 index indexPeerLocationRecord,
                                                 const char *downloadedToVersion
                                                 );

        PeerLocationRecordListPtr IPeerLocationTable_getUnusedLocationsBatch(const Time &lastAccessedBefore) const;

        void IPeerLocationTable_remove(index indexPeerLocationRecord);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IDatabaseTable
        #pragma mark

        void IDatabaseTable_flushAllForPeerLocation(index indexPeerLocationRecord);

        void IDatabaseTable_addOrUpdate(
                                        DatabaseRecord &ioRecord,
                                        DatabaseChangeRecord &outChangeRecord
                                        );

        String IDatabaseTable_updateVersion(index indexDatabase);

        bool IDatabaseTable_remove(
                                   const DatabaseRecord &inRecord,
                                   DatabaseChangeRecord &outChangeRecord
                                   );

        bool IDatabaseTable_getByIndex(
                                       index indexDatabaseRecord,
                                       DatabaseRecord &outRecord
                                       ) const;

        DatabaseRecordListPtr IDatabaseTable_getBatchByPeerLocationIndex(
                                                                         index indexPeerLocationRecord,
                                                                         index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                         ) const;

        DatabaseRecordListPtr IDatabaseTable_getBatchByPeerLocationIndexForPeerURI(
                                                                                   const char *peerURIWithPermission,
                                                                                   index indexPeerLocationRecord,
                                                                                   index afterIndexDatabase = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                                   ) const;

        DatabaseRecordListPtr IDatabaseTable_getBatchExpired(
                                                             index indexPeerLocationRecord,
                                                             const Time &now
                                                             ) const;

        void IDatabaseTable_notifyDownloaded(
                                             index indexDatabaseRecord,
                                             const char *downloadedToVersion
                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IDatabaseChangeTable
        #pragma mark

        void IDatabaseChangeTable_flushAllForPeerLocation(index indexPeerLocationRecord);

        void IDatabaseChangeTable_flushAllForDatabase(index indexDatabase);

        void IDatabaseChangeTable_insert(const DatabaseChangeRecord &record);

        bool IDatabaseChangeTable_getByIndex(
                                             index indexDatabaseChangeRecord,
                                             DatabaseChangeRecord &outRecord
                                             ) const;

        DatabaseChangeRecordListPtr IDatabaseChangeTable_getChangeBatch(
                                                                        index indexPeerLocationRecord,
                                                                        index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                        ) const;

        DatabaseChangeRecordListPtr IDatabaseChangeTable_getChangeBatchForPeerURI(
                                                                                  const char *peerURIWithPermission,
                                                                                  index indexPeerLocationRecord,
                                                                                  index afterIndexDatabaseChange = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                                                  ) const;
        
        
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IPermissionTable
        #pragma mark

        void IPermissionTable_flushAllForPeerLocation(index indexPeerLocationRecord);

        void IPermissionTable_flushAllForDatabase(index indexDatabaseRecord);

        void IPermissionTable_insert(
                                     index indexPeerLocation,
                                     index indexDatabaseRecord,
                                     const PeerURIList &uris
                                     );

        void IPermissionTable_getByDatabaseIndex(
                                                 index indexDatabaseRecord,
                                                 PeerURIList &outURIs
                                                 ) const;
        

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IEntryTable
        #pragma mark

        bool IEntryTable_add(
                             const EntryRecord &entry,
                             EntryChangeRecord &outChangeRecord
                             );

        bool IEntryTable_update(
                                const EntryRecord &entry,
                                EntryChangeRecord &outChangeRecord
                                );

        bool IEntryTable_remove(
                                const EntryRecord &entry,
                                EntryChangeRecord &outChangeRecord
                                );

        EntryRecordListPtr IEntryTable_getBatch(
                                                bool includeData,
                                                index afterIndexEntry = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN
                                                ) const;

        bool IEntryTable_getEntry(
                                  EntryRecord &ioRecord,
                                  bool includeData
                                  ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => IEntryChangeTable
        #pragma mark

        void IEntryChangeTable_insert(const EntryChangeRecord &record);

        EntryChangeRecordListPtr IEntryChangeTable_getBatch(index afterIndexEntryChanged = OPENPEER_STACK_LOCATION_DATABASE_INDEX_UNKNOWN) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => SqlDatabase::Trace
        #pragma mark

        virtual void notifyDatabaseTrace(
                                         SqlDatabase::Trace::Severity severity,
                                         const char *message
                                         ) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();

        void prepareDB();
        void constructDBTables();

        bool deleteDatabase();

        static String SqlEscape(const String &input);
        static String SqlQuote(const String &input);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabaseAbstraction => (data)
        #pragma mark

        LocationDatabaseAbstractionWeakPtr mThisWeak;

        AutoPUID mID;

        bool mDeleteSelf {false};

        String mUserHash;
        String mStoragePath;
        String mDBFileName;
        String mDBFilePath;

        mutable SqlDatabasePtr mDB;

        LocationDatabaseAbstractionPtr mMaster;
        String mPeerURI;
        String mLocationID;
        String mDatabaseID;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseAbstractionFactory
      #pragma mark

      interaction ILocationDatabaseAbstractionFactory
      {
        static ILocationDatabaseAbstractionFactory &singleton();

        virtual LocationDatabaseAbstractionPtr createMaster(
                                                            const char *inHashRepresentingUser,
                                                            const char *inUserStorageFilePath
                                                            );

        virtual LocationDatabaseAbstractionPtr openDatabase(
                                                            ILocationDatabaseAbstractionPtr masterDatabase,
                                                            const char *peerURI,
                                                            const char *locationID,
                                                            const char *databaseID
                                                            );
        virtual bool deleteDatabase(
                                    ILocationDatabaseAbstractionPtr masterDatabase,
                                    const char *peerURI,
                                    const char *locationID,
                                    const char *databaseID
                                    );
      };

      class LocationDatabaseAbstractionFactory : public IFactory<ILocationDatabaseAbstractionFactory> {};
      
    }
  }
}
