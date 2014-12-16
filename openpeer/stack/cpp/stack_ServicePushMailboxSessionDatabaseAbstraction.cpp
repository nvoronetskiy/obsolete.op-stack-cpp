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

#include <openpeer/stack/internal/stack_ServicePushMailboxSessionDatabaseAbstraction.h>

#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstractionTables.h>

#include <openpeer/stack/IHelper.h>

#include <openpeer/services/ISettings.h>
#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_STACK_PUSH_MAILBOX_DATABASE_VERSION 1
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDERS_NEEDING_UPDATE_LIMIT 5
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_UNIQUE_ID_LENGTH 16

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(stack::IHelper, UseStackHelper)

      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstractionTables, UseTables)

      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::MessageDeliveryStateRecordList, MessageDeliveryStateRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::PendingDeliveryMessageRecord, PendingDeliveryMessageRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::PendingDeliveryMessageRecordList, PendingDeliveryMessageRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ListRecordList, ListRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::KeyDomainRecord, KeyDomainRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::SendingKeyRecord, SendingKeyRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ReceivingKeyRecord, ReceivingKeyRecord)

      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ISettingsTable, ISettingsTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IFolderTable, IFolderTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable, IFolderMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable, IFolderVersionedMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IMessageTable, IMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IMessageDeliveryStateTable, IMessageDeliveryStateTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IPendingDeliveryMessageTable, IPendingDeliveryMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IListTable, IListTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IListURITable, IListURITable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IKeyDomainTable, IKeyDomainTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ISendingKeyTable, ISendingKeyTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IReceivingKeyTable, IReceivingKeyTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IStorage, IStorage)

      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::FolderRecord, FolderRecord)
      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::FolderRecordList, FolderRecordList)
      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageRecordList, FolderVersionedMessageRecordList)
      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::MessageRecord, MessageRecord)
      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::MessageRecordList, MessageRecordList)

      ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
      ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
      ZS_DECLARE_TYPEDEF_PTR(sql::RecordSet, SqlRecordSet)
      ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)

      typedef ServicePushMailboxSessionDatabaseAbstraction::index index;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstraction::ServicePushMailboxSessionDatabaseAbstraction(
                                                                                                 const char *inHashRepresentingUser,
                                                                                                 const char *inUserTemporaryFilePath,
                                                                                                 const char *inUserStorageFilePath
                                                                                                 ) :
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mUserHash(inHashRepresentingUser),
        mTempPath(inUserTemporaryFilePath),
        mStoragePath(inUserStorageFilePath)
      {
        ZS_THROW_INVALID_ARGUMENT_IF((mUserHash.isEmpty()) && (mStoragePath.isEmpty()))
        ZS_THROW_INVALID_ARGUMENT_IF(mTempPath.isEmpty())

        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstraction::~ServicePushMailboxSessionDatabaseAbstraction()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::init()
      {
        AutoRecursiveLock lock(*this);
        prepareDB();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstractionPtr ServicePushMailboxSessionDatabaseAbstraction::convert(IServicePushMailboxSessionDatabaseAbstractionPtr session)
      {
        return ZS_DYNAMIC_PTR_CAST(ServicePushMailboxSessionDatabaseAbstraction, session);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSessionDatabaseAbstraction::toDebug(IServicePushMailboxSessionDatabaseAbstractionPtr session)
      {
        if (!session) return ElementPtr();
        return ServicePushMailboxSessionDatabaseAbstraction::convert(session)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstractionPtr ServicePushMailboxSessionDatabaseAbstraction::create(
                                                                                                           const char *inHashRepresentingUser,
                                                                                                           const char *inUserTemporaryFilePath,
                                                                                                           const char *inUserStorageFilePath
                                                                                                           )
      {
        ServicePushMailboxSessionDatabaseAbstractionPtr pThis(new ServicePushMailboxSessionDatabaseAbstraction(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PUID ServicePushMailboxSessionDatabaseAbstraction::getID() const
      {
        return mID;
      }

      //-----------------------------------------------------------------------
      ISettingsTablePtr ServicePushMailboxSessionDatabaseAbstraction::settingsTable() const
      {
        return SettingsTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IFolderTablePtr ServicePushMailboxSessionDatabaseAbstraction::folderTable() const
      {
        return FolderTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IFolderVersionedMessageTablePtr ServicePushMailboxSessionDatabaseAbstraction::folderVersionedMessageTable() const
      {
        return FolderVersionedMessageTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IFolderMessageTablePtr ServicePushMailboxSessionDatabaseAbstraction::folderMessageTable() const
      {
        return FolderMessageTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IMessageTablePtr ServicePushMailboxSessionDatabaseAbstraction::messageTable() const
      {
        return MessageTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IMessageDeliveryStateTablePtr ServicePushMailboxSessionDatabaseAbstraction::messageDeliveryStateTable() const
      {
        return MessageDeliveryStateTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IPendingDeliveryMessageTablePtr ServicePushMailboxSessionDatabaseAbstraction::pendingDeliveryMessageTable() const
      {
        return PendingDeliveryMessageTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IListTablePtr ServicePushMailboxSessionDatabaseAbstraction::listTable() const
      {
        return ListTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IListURITablePtr ServicePushMailboxSessionDatabaseAbstraction::listURITable() const
      {
        return ListURITable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IKeyDomainTablePtr ServicePushMailboxSessionDatabaseAbstraction::keyDomainTable() const
      {
        return KeyDomainTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ISendingKeyTablePtr ServicePushMailboxSessionDatabaseAbstraction::sendingKeyTable() const
      {
        return SendingKeyTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IReceivingKeyTablePtr ServicePushMailboxSessionDatabaseAbstraction::receivingKeyTable() const
      {
        return ReceivingKeyTable::create(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IStoragePtr ServicePushMailboxSessionDatabaseAbstraction::storage() const
      {
        return Storage::create(mThisWeak.lock());
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => ISettingsTable
      #pragma mark

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::ISettingsTable_getLastDownloadedVersionForFolders() const
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Settings_name(), UseTables::Settings());

          table.open();

          SqlRecord *settingsRecord = table.getTopRecord();

          return settingsRecord->getValue(UseTables::lastDownloadedVersionForFolders)->asString();
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return String();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::ISettingsTable_setLastDownloadedVersionForFolders(const char *version)
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Settings_name(), UseTables::Settings());

          table.open();

          SqlRecord *settingsRecord = table.getTopRecord();

          settingsRecord->setString(UseTables::lastDownloadedVersionForFolders, String(version));

          table.updateRecord(settingsRecord);
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_flushAll()
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.deleteRecords(String());
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_removeByIndex(index indexFolderRecord)
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.deleteRecords(String(SqlField::id) + " = " + string(indexFolderRecord));
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getIndex(const char *inFolderName) const
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.open(String(UseTables::folderName) + " = " + SqlQuote(inFolderName));

          SqlRecord *record = table.getTopRecord();
          if (record) {
            auto result = record->getKeyIdValue()->asInteger();
            ZS_LOG_TRACE(log("folder found") + ZS_PARAM("folder name", inFolderName) + ZS_PARAM("index", result))
            return static_cast<index>(result);
          }

          ZS_LOG_TRACE(log("folder does not exist") + ZS_PARAM("folder name", inFolderName))
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      static FolderRecordPtr convertFolderRecord(SqlRecord *record)
      {
        FolderRecordPtr result(new FolderRecord);

        result->mIndex = static_cast<decltype(result->mIndex)>(record->getKeyIdValue()->asInteger());
        result->mUniqueID = record->getValue(UseTables::uniqueID)->asString();
        result->mFolderName = record->getValue(UseTables::folderName)->asString();
        result->mServerVersion = record->getValue(UseTables::serverVersion)->asString();
        result->mDownloadedVersion = record->getValue(UseTables::downloadedVersion)->asString();
        result->mTotalUnreadMessages = static_cast<decltype(result->mTotalUnreadMessages)>(record->getValue(UseTables::totalUnreadMessages)->asInteger());
        result->mTotalMessages = static_cast<decltype(result->mTotalMessages)>(record->getValue(UseTables::totalMessages)->asInteger());
        result->mUpdateNext = zsLib::timeSinceEpoch(Seconds(record->getValue(UseTables::updateNext)->asInteger()));

        return result;
      }

      //-----------------------------------------------------------------------
      FolderRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getIndexAndUniqueID(const char *inFolderName) const
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.open(String(UseTables::folderName) + " = " + SqlQuote(inFolderName));

          SqlRecord *record = table.getTopRecord();
          if (record) {
            FolderRecordPtr result = convertFolderRecord(record);

            ZS_LOG_TRACE(log("folder found") + ZS_PARAM("folder name", result->mFolderName) + ZS_PARAM("index", result->mIndex))
            return result;
          }

          ZS_LOG_TRACE(log("folder does not exist") + ZS_PARAM("folder name", inFolderName))
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return FolderRecordPtr();
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getName(index indexFolderRecord)
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.open(String(SqlField::id) + " = " + string(indexFolderRecord));

          SqlRecord *record = table.getTopRecord();
          if (record) {
            auto result = record->getValue(UseTables::folderName)->asString();
            ZS_LOG_TRACE(log("folder found") + ZS_PARAM("folder name", result) + ZS_PARAM("index", indexFolderRecord))
            return result;
          }

          ZS_LOG_TRACE(log("folder does not exist") + ZS_PARAM("index", indexFolderRecord))
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return String();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_addOrUpdate(const FolderRecord &folder)
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          table.open(String(UseTables::folderName) + " = " + SqlQuote(folder.mFolderName));

          SqlRecord *found = table.getTopRecord();
          SqlRecord addRecord(table.fields());

          SqlRecord *useRecord = found ? found : &addRecord;

          if (found) {
            if (folder.mUniqueID.hasData()) {
              if (folder.mUniqueID == useRecord->getValue(UseTables::uniqueID)->asString()) {
                useRecord->setIgnored(UseTables::uniqueID);
              }
            } else {
              useRecord->setIgnored(UseTables::uniqueID);
            }
            if (folder.mFolderName.hasData()) {
              if (folder.mFolderName == useRecord->getValue(UseTables::folderName)->asString()) {
                useRecord->setIgnored(UseTables::folderName);
              }
            } else {
              useRecord->setIgnored(UseTables::folderName);
            }
            useRecord->setIgnored(UseTables::downloadedVersion);
          } else {
            useRecord->setString(UseTables::uniqueID, UseServicesHelper::randomString(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_UNIQUE_ID_LENGTH));
            useRecord->setString(UseTables::folderName, folder.mFolderName);
            useRecord->setString(UseTables::downloadedVersion, String());
          }

          useRecord->setString(UseTables::serverVersion, folder.mServerVersion);
          useRecord->setInteger(UseTables::totalUnreadMessages, folder.mTotalUnreadMessages);
          useRecord->setInteger(UseTables::totalMessages, folder.mTotalMessages);
          useRecord->setInteger(UseTables::updateNext, Time() == folder.mUpdateNext ? 0 : zsLib::timeSinceEpoch<Seconds>(folder.mUpdateNext).count());

          if (found) {
            table.updateRecord(useRecord);
          } else {
            table.addRecord(useRecord);
          }

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateFolderName(
                                                                                       index indexFolderRecord,
                                                                                       const char *newfolderName
                                                                                       )
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::folderName + "=" + SqlQuote(newfolderName) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateServerVersionIfFolderExists(
                                                                                                        const char *inFolderName,
                                                                                                        const char *inServerVersion
                                                                                                        )
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::serverVersion + "=" + SqlQuote(inServerVersion) + " WHERE " + UseTables::folderName + " = " + SqlQuote(inFolderName));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      FolderRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getNeedingUpdate(const Time &now) const
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());

          FolderRecordListPtr result(new FolderRecordList);

          table.open(String(UseTables::updateNext) + " < " + string(zsLib::timeSinceEpoch<Seconds>(now).count()) + " OR " + UseTables::downloadedVersion + " != " + UseTables::serverVersion + " LIMIT " + string(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDERS_NEEDING_UPDATE_LIMIT));

          for (int row = 0; row < table.recordCount(); ++row) {
            SqlRecord *record = table.getRecord(row);
            FolderRecordPtr folder = convertFolderRecord(record);
            result->push_back(*folder);
          }
          return result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        // mUpdateNext or downloadVersion != serverVersion
        return FolderRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateDownloadInfo(
                                                                                         index indexFolderRecord,
                                                                                         const char *inDownloadedVersion,
                                                                                         const Time &inUpdateNext
                                                                                         )
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::downloadedVersion + "=" + SqlQuote(inDownloadedVersion) + ", " + UseTables::updateNext + "=" + string(zsLib::timeSinceEpoch<Seconds>(inUpdateNext).count()) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_resetUniqueID(index indexFolderRecord)
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::uniqueID + "=" + SqlQuote(UseServicesHelper::randomString(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_UNIQUE_ID_LENGTH)) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderMessageTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_flushAll()
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());
          
          table.deleteRecords(String());

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_addToFolderIfNotPresent(const FolderMessageRecord &folderMessage)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());
          
          table.deleteRecords(String());
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_removeFromFolder(const FolderMessageRecord &folderMessage)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_removeAllFromFolder(index folderIndex)
      {
      }

      //-----------------------------------------------------------------------
      IFolderMessageTable::IndexListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_getWithMessageID(const char *messageID) const
      {
        ZS_DECLARE_TYPEDEF_PTR(IFolderMessageTable::IndexList, IndexList)
        return IndexListPtr();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderVersionedMessageTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_flushAll()
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_add(const FolderVersionedMessageRecord &message)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_addRemovedEntryIfNotAlreadyRemoved(const FolderVersionedMessageRecord &message)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_removeAllRelatedToFolder(index indexFolderRecord)
      {
      }

      //-----------------------------------------------------------------------
      FolderVersionedMessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_getBatchAfterIndex(
                                                                                                                                                                                  index indexFolderVersionMessageRecord,    // if < 0 then return all 0 or >
                                                                                                                                                                                  index indexFolderRecord
                                                                                                                                                                                  ) const
      {
        return FolderVersionedMessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_getLastIndexForFolder(index indexFolderRecord) const
      {
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IMessageTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_addOrUpdate(
                                                                                   const char *messageID,
                                                                                   const char *serverVersion
                                                                                   )
      {
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getIndex(const char *messageID) const
      {
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_remove(index indexMessageRecord)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_update(const MessageRecord &message)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncodingAndEncryptedDataLength(
                                                                                                            index indexMessageRecord,
                                                                                                            const char *encoding,
                                                                                                            ULONGEST encryptedDataLength
                                                                                                            )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncodingAndFileNames(
                                                                                                  index indexMessageRecord,
                                                                                                  const char *encoding,
                                                                                                  const char *encryptedFileName,
                                                                                                  const char *decryptedFileName,
                                                                                                  ULONGEST encryptedDataLength
                                                                                                  )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptionFileName(
                                                                                                index indexMessageRecord,
                                                                                                const char *encryptedFileName
                                                                                                )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptionStorage(
                                                                                               index indexMessageRecord,
                                                                                               const char *encryptedFileName,
                                                                                               ULONGEST encryptedDataLength
                                                                                               )
      {
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_insert(const MessageRecord &message)
      {
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      MessageRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getByIndex(index indexMessageRecord) const
      {
        return MessageRecordPtr();
      }

      //-----------------------------------------------------------------------
      MessageRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getByMessageID(const char *messageID)
      {
        return MessageRecordPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateServerVersionIfExists(
                                                                                                   const char *messageID,
                                                                                                   const char *serverVersion
                                                                                                   )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptedData(
                                                                                           index indexMessageRecord,
                                                                                           const SecureByteBlock &encryptedData
                                                                                           )
      {
      }

      //-----------------------------------------------------------------------
      IMessageTable::MessageNeedingUpdateListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingUpdate()
      {
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateList, MessageNeedingUpdateList)
        return MessageNeedingUpdateListPtr();
      }

      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingData() const
      {
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateList, MessageNeedingUpdateList)
        return MessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDownload(
                                                                                      index indexMessageRecord,
                                                                                      bool downloaded
                                                                                      )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDownloadFailure(index indexMessageRecord)
      {
      }

      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingKeysProcessing(
                                                                                                                                                                   index indexFolderRecord,
                                                                                                                                                                   const char *whereMessageTypeIs,
                                                                                                                                                                   const char *whereMimeType
                                                                                                                                                                   ) const
      {
        return MessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyKeyingProcessed(index indexMessageRecord)
      {
      }

      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingDecrypting(const char *excludeWhereMessageTypeIs) const
      {
        return MessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDecryptLater(
                                                                                          index indexMessageRecord,
                                                                                          const char *decryptKeyID
                                                                                          )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDecryptionFailure(index indexMessageRecord)
      {
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDecryptNowForKeys(const char *whereDecryptKeyIDIs)
      {
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDecrypted(
                                                                                       index indexMessageRecord,
                                                                                       SecureByteBlockPtr decryptedData,
                                                                                       bool needsNotification
                                                                                       )
      {
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDecrypted(
                                                                                       index indexMessageRecord,
                                                                                       const char *encrytpedFileName,
                                                                                       const char *decryptedFileName,
                                                                                       bool needsNotification
                                                                                       )
      {
      }
      
      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingNotification() const
      {
        return MessageRecordListPtr();
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_clearNeedsNotification(index indexMessageRecord)
      {
      }
      
      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingExpiry(Time now) const
      {
        return MessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IMessageDeliveryStateTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageDeliveryStateTable_removeForMessage(index indexMessageRecord)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageDeliveryStateTable_updateForMessage(
                                                                                                     index indexMessageRecord,
                                                                                                     const MessageDeliveryStateRecordList &uris
                                                                                                     )
      {
      }

      //-----------------------------------------------------------------------
      MessageDeliveryStateRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageDeliveryStateTable_getForMessage(index indexMessageRecord) const
      {
        return MessageDeliveryStateRecordListPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IPendingDeliveryMessageTable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IPendingDeliveryMessageTable_insert(const PendingDeliveryMessageRecord &message)
      {
      }

      //-----------------------------------------------------------------------
      PendingDeliveryMessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IPendingDeliveryMessageTable_getBatchToDeliver()
      {
        return PendingDeliveryMessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IPendingDeliveryMessageTable_removeByMessageIndex(index indexMessageRecord)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IPendingDeliveryMessageTable_remove(index indexPendingDeliveryRecord)
      {
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IListTable
      #pragma mark

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IListTable_addOrUpdateListID(const char *listID)
      {
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSessionDatabaseAbstraction::IListTable_hasListID(const char *listID) const
      {
        return false;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IListTable_notifyDownloaded(index indexListRecord)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IListTable_notifyFailedToDownload(index indexListRecord)
      {
      }

      //-----------------------------------------------------------------------
      ListRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IListTable_getBatchNeedingDownload() const
      {
        return ListRecordListPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IListURITable
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IListURITable_update(
                                                                              index indexListRecord,
                                                                              const IListURITable::URIList &uris
                                                                              )
      {
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstraction::IListURITable::URIListPtr ServicePushMailboxSessionDatabaseAbstraction::IListURITable_getURIs(const char *listID)
      {
        ZS_DECLARE_TYPEDEF_PTR(IListURITable::URIList, URIList)

        return URIListPtr();
      }

      //-----------------------------------------------------------------------
      int ServicePushMailboxSessionDatabaseAbstraction::IListURITable_getOrder(
                                                                               const char *listID,
                                                                               const char *uri
                                                                               )
      {
        return OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN;
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IKeyDomainTable
      #pragma mark

      //-----------------------------------------------------------------------
      KeyDomainRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IKeyDomainTable_getByKeyDomain(int keyDomain) const
      {
        return KeyDomainRecordPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IKeyDomainTable_add(const KeyDomainRecord &keyDomain)
      {
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => ISendingKeyTable
      #pragma mark

      //-----------------------------------------------------------------------
      SendingKeyRecordPtr ServicePushMailboxSessionDatabaseAbstraction::ISendingKeyTable_getByKeyID(const char *keyID) const
      {
        return SendingKeyRecordPtr();
      }

      //-----------------------------------------------------------------------
      SendingKeyRecordPtr ServicePushMailboxSessionDatabaseAbstraction::ISendingKeyTable_getActive(
                                                                                                   const char *uri,
                                                                                                   Time now
                                                                                                   ) const
      {
        return SendingKeyRecordPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::ISendingKeyTable_addOrUpdate(const SendingKeyRecord &key)
      {
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IReceivingKeyTable
      #pragma mark

      //-----------------------------------------------------------------------
      ReceivingKeyRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IReceivingKeyTable_getByKeyID(const char *keyID) const
      {
        return ReceivingKeyRecordPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IReceivingKeyTable_addOrUpdate(const ReceivingKeyRecord &key)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IStorage
      #pragma mark

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::IStorage_storeToTemporaryFile(const SecureByteBlock &buffer)
      {
        return String();
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::IStorage_getStorageFileName() const
      {
        return String();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => SqlDatabase::Trace
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::notifyDatabaseTrace(
                                                                             SqlDatabase::Trace::Severity severity,
                                                                             const char *message
                                                                             ) const
      {
        switch (severity) {
          case SqlDatabase::Trace::Informational: ZS_INTERNAL_LOG_TRACE_WITH_SEVERITY(UseStackHelper::toSeverity(severity), log(message)) break;
          case SqlDatabase::Trace::Warning:       ZS_INTERNAL_LOG_DETAIL_WITH_SEVERITY(UseStackHelper::toSeverity(severity), log(message)) break;
          case SqlDatabase::Trace::Error:         ZS_INTERNAL_LOG_DETAIL_WITH_SEVERITY(UseStackHelper::toSeverity(severity), log(message)) break;
          case SqlDatabase::Trace::Fatal:         ZS_INTERNAL_LOG_BASIC_WITH_SEVERITY(UseStackHelper::toSeverity(severity), log(message)) break;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSessionDatabaseAbstraction::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSessionDatabaseAbstraction");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSessionDatabaseAbstraction::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSessionDatabaseAbstraction::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePushMailboxSessionDatabaseAbstraction");

        UseServicesHelper::debugAppend(resultEl, "id", mID);

        UseServicesHelper::debugAppend(resultEl, "user hash", mUserHash);
        UseServicesHelper::debugAppend(resultEl, "temp path", mTempPath);
        UseServicesHelper::debugAppend(resultEl, "storage path", mStoragePath);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::cancel()
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::prepareDB()
      {
        if (mDB) return;

        bool alreadyDeleted = false;

        String postFix = UseSettings::getString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DATABASE_FILE_POSTFIX);

        String path = UseStackHelper::appendPath(mStoragePath, (mUserHash + postFix).c_str());

        ZS_THROW_BAD_STATE_IF(path.isEmpty())

        try {
          while (true) {
            ZS_LOG_DETAIL(log("loading push mailbox database") + ZS_PARAM("path", path))

            mDB = SqlDatabasePtr(new SqlDatabase(ZS_IS_LOGGING(Trace) ? this : NULL));

            mDB->open(path);

            SqlTable versionTable(*mDB, UseTables::Version_name(), UseTables::Version());

            if (!versionTable.exists()) {
              versionTable.create();

              SqlRecord record(versionTable.fields());
              record.setInteger(UseTables::version, OPENPEER_STACK_PUSH_MAILBOX_DATABASE_VERSION);
              versionTable.addRecord(&record);

              constructDBTables();
            }

            versionTable.open();

            SqlRecord *versionRecord = versionTable.getTopRecord();

            auto databaseVersion = versionRecord->getValue(UseTables::version)->asInteger();

            switch (databaseVersion) {
              // "upgrades" are possible here...
              case OPENPEER_STACK_PUSH_MAILBOX_DATABASE_VERSION: {
                ZS_LOG_DEBUG(log("push mailbox database loaded"))
                goto database_open_complete;
              }
              default: {
                ZS_LOG_DEBUG(log("push mailbox database version is not not compatible (thus will delete database)"))

                // close the database
                mDB.reset();

                if (alreadyDeleted) {
                  // already attempted delete once
                  ZS_LOG_FATAL(Basic, log("failed to load database"))
                  goto database_open_complete;
                }

                auto status = remove(path);
                if (0 != status) {
                  ZS_LOG_ERROR(Detail, log("attempt to remove incompatible push mailbox database file failed") + ZS_PARAM("path", path) + ZS_PARAM("result", status) + ZS_PARAM("errno", errno))
                }
                alreadyDeleted = true;
                break;
              }
            }
          }

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database preparation failure") + ZS_PARAM("message", e.msg()))
          mDB.reset();
        }

      database_open_complete:
        {
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::constructDBTables()
      {
        ZS_LOG_DEBUG(log("constructing push mailbox database tables"))

        {
          SqlTable table(*mDB, UseTables::Settings_name(), UseTables::Settings());
          table.create();

          SqlRecord record(table.fields());
          record.setString(UseTables::lastDownloadedVersionForFolders, String());
          table.addRecord(&record);
        }
        {
          SqlTable table(*mDB, UseTables::Folder_name(), UseTables::Folder());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::MessageDeliveryState_name(), UseTables::MessageDeliveryState());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::PendingDeliveryMessage_name(), UseTables::PendingDeliveryMessage());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::List_name(), UseTables::List());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::ListURI_name(), UseTables::ListURI());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::KeyDomain_name(), UseTables::KeyDomain());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::SendingKey_name(), UseTables::SendingKey());
          table.create();
        }
        {
          SqlTable table(*mDB, UseTables::ReceivingKey_name(), UseTables::ReceivingKey());
          table.create();
        }
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::SqlEscape(const String &input)
      {
        String result(input);
        result.replaceAll("\'", "\'\'");
        return result;
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::SqlQuote(const String &input)
      {
        String result(input);
        result.replaceAll("\'", "\'\'");
        return "\'" + result + "\'";
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionDatabaseAbstractionFactory &IServicePushMailboxSessionDatabaseAbstractionFactory::singleton()
      {
        return ServicePushMailboxSessionDatabaseAbstractionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionDatabaseAbstractionPtr IServicePushMailboxSessionDatabaseAbstractionFactory::create(
                                                                                                                   const char *inHashRepresentingUser,
                                                                                                                   const char *inUserTemporaryFilePath,
                                                                                                                   const char *inUserStorageFilePath
                                                                                                                   )
      {
        if (this) {}
        return ServicePushMailboxSessionDatabaseAbstraction::create(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstraction::toDebug(IServicePushMailboxSessionDatabaseAbstractionPtr session)
      {
        return ServicePushMailboxSessionDatabaseAbstraction::toDebug(session);
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionDatabaseAbstractionPtr IServicePushMailboxSessionDatabaseAbstraction::create(
                                                                                                             const char *inHashRepresentingUser,
                                                                                                             const char *inUserTemporaryFilePath,
                                                                                                             const char *inUserStorageFilePath
                                                                                                             )
      {
        return IServicePushMailboxSessionDatabaseAbstractionFactory::singleton().create(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath);
      }

    }

  }
}
