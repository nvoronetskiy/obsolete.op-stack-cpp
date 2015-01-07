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

#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstraction.h>

#include <easySQLite/SqlDatabase.h>

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_DATABASE_FILE_POSTFIX "openpeer/stack/push-mailbox-database-filename-postfix"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_ANALYZE_DATABASE_RANDOMLY_EVERY "openpeer/stack/push-mailbox-analyze-database-randomly-every"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_SIZE_IN_BYTES "openpeer/stack/push-mailbox-max-message-download-size-in-bytes"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_MESSAGE_DOWNLOAD_FAILURE_RETRY_PATTERN_IN_SECONDS "openpeer/stack/push-mailbox-message-download-failure-retry-pattern-in-seconds"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_LIST_DOWNLOAD_FAILURE_RETRY_PATTERN_IN_SECONDS "openpeer/stack/push-mailbox-list-download-failure-retry-pattern-in-seconds"

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
      #pragma mark ServicePushMailboxSession
      #pragma mark

      class ServicePushMailboxSessionDatabaseAbstraction : public Noop,
                                                           public SharedRecursiveLock,
                                                           public IServicePushMailboxSessionDatabaseAbstraction,
                                                           public sql::Database::Trace
      {
      public:
        friend interaction IServicePushMailboxSessionDatabaseAbstractionFactory;
        friend interaction IServicePushMailboxSessionDatabaseAbstraction;

        ZS_DECLARE_CLASS_PTR(SettingsTable)
        ZS_DECLARE_CLASS_PTR(FolderTable)
        ZS_DECLARE_CLASS_PTR(FolderMessageTable)
        ZS_DECLARE_CLASS_PTR(FolderVersionedMessageTable)
        ZS_DECLARE_CLASS_PTR(MessageTable)
        ZS_DECLARE_CLASS_PTR(MessageDeliveryStateTable)
        ZS_DECLARE_CLASS_PTR(PendingDeliveryMessageTable)
        ZS_DECLARE_CLASS_PTR(ListTable)
        ZS_DECLARE_CLASS_PTR(ListURITable)
        ZS_DECLARE_CLASS_PTR(KeyDomainTable)
        ZS_DECLARE_CLASS_PTR(SendingKeyTable)
        ZS_DECLARE_CLASS_PTR(ReceivingKeyTable)
        ZS_DECLARE_CLASS_PTR(Storage)

        friend class SettingsTable;
        friend class FolderTable;
        friend class FolderMessageTable;
        friend class FolderVersionedMessageTable;
        friend class MessageTable;
        friend class MessageDeliveryStateTable;
        friend class PendingDeliveryMessageTable;
        friend class ListTable;
        friend class ListURITable;
        friend class KeyDomainTable;
        friend class SendingKeyTable;
        friend class ReceivingKeyTable;
        friend class Storage;

        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)
        ZS_DECLARE_TYPEDEF_PTR(sql::Exception, SqlException)

      protected:
        ServicePushMailboxSessionDatabaseAbstraction(
                                                     const char *inHashRepresentingUser,
                                                     const char *inUserTemporaryFilePath,
                                                     const char *inUserStorageFilePath
                                                     );

        ServicePushMailboxSessionDatabaseAbstraction(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create()) {}

        void init();

      public:
        ~ServicePushMailboxSessionDatabaseAbstraction();

        static ServicePushMailboxSessionDatabaseAbstractionPtr convert(IServicePushMailboxSessionDatabaseAbstractionPtr session);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IServicePushMailboxSessionDatabaseAbstraction
        #pragma mark

        static ElementPtr toDebug(IServicePushMailboxSessionDatabaseAbstractionPtr session);

        static ServicePushMailboxSessionDatabaseAbstractionPtr create(
                                                                      const char *inHashRepresentingUser,
                                                                      const char *inUserTemporaryFilePath,
                                                                      const char *inUserStorageFilePath
                                                                      );

        virtual PUID getID() const;

        virtual ISettingsTablePtr settingsTable() const;
        virtual IFolderTablePtr folderTable() const;
        virtual IFolderVersionedMessageTablePtr folderVersionedMessageTable() const;
        virtual IFolderMessageTablePtr folderMessageTable() const;
        virtual IMessageTablePtr messageTable() const;
        virtual IMessageDeliveryStateTablePtr messageDeliveryStateTable() const;
        virtual IPendingDeliveryMessageTablePtr pendingDeliveryMessageTable() const;
        virtual IListTablePtr listTable() const;
        virtual IListURITablePtr listURITable() const;
        virtual IKeyDomainTablePtr keyDomainTable() const;
        virtual ISendingKeyTablePtr sendingKeyTable() const;
        virtual IReceivingKeyTablePtr receivingKeyTable() const;
        virtual IStoragePtr storage() const;

      public:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::SettingsTable
        #pragma mark

        class SettingsTable : public ISettingsTable
        {
          SettingsTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static SettingsTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return SettingsTablePtr(new SettingsTable(outer));}

          virtual String getLastDownloadedVersionForFolders() const             {return mOuter->ISettingsTable_getLastDownloadedVersionForFolders();}
          virtual void setLastDownloadedVersionForFolders(const char *version)  {mOuter->ISettingsTable_setLastDownloadedVersionForFolders(version);}
        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::FolderTable
        #pragma mark

        class FolderTable : public IFolderTable
        {
          FolderTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static FolderTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return FolderTablePtr(new FolderTable(outer));}

          virtual void flushAll()                                                     {mOuter->IFolderTable_flushAll();}
          virtual void removeByIndex(index indexFolderRecord)                         {mOuter->IFolderTable_removeByIndex(indexFolderRecord);}
          virtual index getIndex(const char *inFolderName) const                      {return mOuter->IFolderTable_getIndex(inFolderName);}
          virtual FolderRecordPtr getIndexAndUniqueID(const char *inFolderName) const {return mOuter->IFolderTable_getIndexAndUniqueID(inFolderName);}
          virtual String getName(index indexFolderRecord)                             {return mOuter->IFolderTable_getName(indexFolderRecord);}
          virtual void addOrUpdate(const FolderRecord &folder)                        {mOuter->IFolderTable_addOrUpdate(folder);}
          virtual void updateFolderName(
                                        index indexFolderRecord,
                                        const char *newfolderName
                                        )                                             {mOuter->IFolderTable_updateFolderName(indexFolderRecord, newfolderName);}
          virtual void updateServerVersionIfFolderExists(
                                                         const char *inFolderName,
                                                         const char *inServerVersion
                                                         )                            {mOuter->IFolderTable_updateServerVersionIfFolderExists(inFolderName, inServerVersion);}
          virtual FolderRecordListPtr getNeedingUpdate(const Time &now) const         {return mOuter->IFolderTable_getNeedingUpdate(now);}
          virtual void updateDownloadInfo(
                                          index indexFolderRecord,
                                          const char *inDownloadedVersion,
                                          const Time &inUpdateNext
                                          )                                           {mOuter->IFolderTable_updateDownloadInfo(indexFolderRecord, inDownloadedVersion, inUpdateNext);}
          virtual void resetUniqueID(index indexFolderRecord)                         {mOuter->IFolderTable_resetUniqueID(indexFolderRecord);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::FolderMessageTable
        #pragma mark

        class FolderMessageTable : public IFolderMessageTable
        {
          FolderMessageTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static FolderMessageTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return FolderMessageTablePtr(new FolderMessageTable(outer));}

          virtual void flushAll()                                                         {mOuter->IFolderMessageTable_flushAll();}
          virtual void addToFolderIfNotPresent(const FolderMessageRecord &folderMessage)  {mOuter->IFolderMessageTable_addToFolderIfNotPresent(folderMessage);}
          virtual void removeFromFolder(const FolderMessageRecord &folderMessage)         {mOuter->IFolderMessageTable_removeFromFolder(folderMessage);}
          virtual void removeAllFromFolder(index indexFolderRecord)                       {mOuter->IFolderMessageTable_removeAllFromFolder(indexFolderRecord);}
          virtual IndexListPtr getWithMessageID(const char *messageID) const              {return mOuter->IFolderMessageTable_getWithMessageID(messageID);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageTable
        #pragma mark

        class FolderVersionedMessageTable : public IFolderVersionedMessageTable
        {
          FolderVersionedMessageTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static FolderVersionedMessageTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return FolderVersionedMessageTablePtr(new FolderVersionedMessageTable(outer));}

          virtual void flushAll()                                                         {mOuter->IFolderVersionedMessageTable_flushAll();}
          virtual void add(const FolderVersionedMessageRecord &message)                   {mOuter->IFolderVersionedMessageTable_add(message);}
          virtual void addRemovedEntryIfNotAlreadyRemoved(const FolderVersionedMessageRecord &message)  {mOuter->IFolderVersionedMessageTable_addRemovedEntryIfNotAlreadyRemoved(message);}
          virtual void removeAllRelatedToFolder(index indexFolderRecord)                  {mOuter->IFolderVersionedMessageTable_removeAllRelatedToFolder(indexFolderRecord);}
          virtual FolderVersionedMessageRecordListPtr getBatchAfterIndex(
                                                                         index indexFolderVersionMessageRecord,    // if < 0 then return all 0 or >
                                                                         index indexFolderRecord
                                                                         ) const          {return mOuter->IFolderVersionedMessageTable_getBatchAfterIndex(indexFolderVersionMessageRecord, indexFolderRecord);}
          virtual index getLastIndexForFolder(index indexFolderRecord) const              {return mOuter->IFolderVersionedMessageTable_getLastIndexForFolder(indexFolderRecord);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::MessageTable
        #pragma mark

        class MessageTable : public IMessageTable
        {
          MessageTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static MessageTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return MessageTablePtr(new MessageTable(outer));}

          virtual void addOrUpdate(
                                   const char *messageID,
                                   const char *serverVersion
                                   )                                                        {mOuter->IMessageTable_addOrUpdate(messageID, serverVersion);}
          virtual index getIndex(const char *messageID) const                               {return mOuter->IMessageTable_getIndex(messageID);}
          virtual void remove(index indexMessageRecord)                                     {mOuter->IMessageTable_remove(indexMessageRecord);}
          virtual void update(const MessageRecord &message)                                 {mOuter->IMessageTable_update(message);}
          virtual void updateEncodingAndEncryptedDataLength(
                                                            index indexMessageRecord,
                                                            const char *encoding,
                                                            ULONGEST encryptedDataLength
                                                            )                               {mOuter->IMessageTable_updateEncodingAndEncryptedDataLength(indexMessageRecord, encoding, encryptedDataLength);}
          virtual void updateEncodingAndFileNames(
                                                  index indexMessageRecord,
                                                  const char *encoding,
                                                  const char *encryptedFileName,
                                                  const char *decryptedFileName,
                                                  ULONGEST encryptedDataLength
                                                  )                                         {mOuter->IMessageTable_updateEncodingAndFileNames(indexMessageRecord, encoding, encryptedFileName, decryptedFileName, encryptedDataLength);}
          virtual void updateEncryptionFileName(
                                                index indexMessageRecord,
                                                const char *encryptedFileName
                                                )                                           {mOuter->IMessageTable_updateEncryptionFileName(indexMessageRecord, encryptedFileName);}
          virtual void updateEncryptionStorage(
                                               index indexMessageRecord,
                                               const char *encryptedFileName,
                                               ULONGEST encryptedDataLength
                                               )                                            {mOuter->IMessageTable_updateEncryptionStorage(indexMessageRecord, encryptedFileName, encryptedDataLength);}
          virtual index insert(const MessageRecord &message)                                {return mOuter->IMessageTable_insert(message);}
          virtual MessageRecordPtr getByIndex(index indexMessageRecord) const               {return mOuter->IMessageTable_getByIndex(indexMessageRecord);}
          virtual MessageRecordPtr getByMessageID(const char *messageID)                    {return mOuter->IMessageTable_getByMessageID(messageID);}
          virtual void updateServerVersionIfExists(
                                                   const char *messageID,
                                                   const char *serverVersion
                                                   )                                        {mOuter->IMessageTable_updateServerVersionIfExists(messageID, serverVersion);}
          virtual void updateEncryptedData(
                                                  index indexMessageRecord,
                                                  const SecureByteBlock &encryptedData
                                                  )                                         {mOuter->IMessageTable_updateEncryptedData(indexMessageRecord, encryptedData);}
          virtual MessageNeedingUpdateListPtr getBatchNeedingUpdate()                       {return mOuter->IMessageTable_getBatchNeedingUpdate();}
          virtual MessageRecordListPtr getBatchNeedingData(const Time &now) const           {return mOuter->IMessageTable_getBatchNeedingData(now);}
          virtual void notifyDownload(
                                      index indexMessageRecord,
                                      bool downloaded
                                      )                                                     {mOuter->IMessageTable_notifyDownload(indexMessageRecord, downloaded);}
          virtual MessageRecordListPtr getBatchNeedingKeysProcessing(
                                                                     index indexFolderRecord,
                                                                     const char *whereMessageTypeIs,
                                                                     const char *whereMimeType
                                                                     ) const                {return mOuter->IMessageTable_getBatchNeedingKeysProcessing(indexFolderRecord, whereMessageTypeIs, whereMimeType);}
          virtual void notifyKeyingProcessed(index indexMessageRecord)                      {mOuter->IMessageTable_notifyKeyingProcessed(indexMessageRecord);}
          virtual MessageRecordListPtr getBatchNeedingDecrypting(const char *excludeWhereMessageTypeIs) const {return mOuter->IMessageTable_getBatchNeedingDecrypting(excludeWhereMessageTypeIs);}
          virtual void notifyDecryptLater(
                                          index indexMessageRecord,
                                          const char *decryptKeyID
                                          )                                                 {mOuter->IMessageTable_notifyDecryptLater(indexMessageRecord, decryptKeyID);}
          virtual void notifyDecryptionFailure(index indexMessageRecord)                    {mOuter->IMessageTable_notifyDecryptionFailure(indexMessageRecord);}
          virtual void notifyDecryptNowForKeys(const char *whereDecryptKeyIDIs)             {mOuter->IMessageTable_notifyDecryptNowForKeys(whereDecryptKeyIDIs);}
          virtual void notifyDecrypted(
                                       index indexMessageRecord,
                                       SecureByteBlockPtr decryptedData,
                                       bool needsNotification
                                       )                                                    {mOuter->IMessageTable_notifyDecrypted(indexMessageRecord, decryptedData, needsNotification);}
          virtual void notifyDecrypted(
                                       index indexMessageRecord,
                                       const char *encrytpedFileName,
                                       const char *decryptedFileName,
                                       bool needsNotification
                                       )                                                    {mOuter->IMessageTable_notifyDecrypted(indexMessageRecord, encrytpedFileName, decryptedFileName, needsNotification);}
          virtual MessageRecordListPtr getBatchNeedingNotification() const                  {return mOuter->IMessageTable_getBatchNeedingNotification();}
          virtual void clearNeedsNotification(index indexMessageRecord)                     {mOuter->IMessageTable_clearNeedsNotification(indexMessageRecord);}
          virtual MessageRecordListPtr getBatchNeedingExpiry(Time now) const                {return mOuter->IMessageTable_getBatchNeedingExpiry(now);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::MessageDeliveryStateTable
        #pragma mark

        class MessageDeliveryStateTable : public IMessageDeliveryStateTable
        {
          MessageDeliveryStateTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static MessageDeliveryStateTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return MessageDeliveryStateTablePtr(new MessageDeliveryStateTable(outer));}

          virtual void removeForMessage(index indexMessageRecord)                           {mOuter->IMessageDeliveryStateTable_removeForMessage(indexMessageRecord);}
          virtual void updateForMessage(
                                        index indexMessageRecord,
                                        const MessageDeliveryStateRecordList &states
                                        )                                                   {mOuter->IMessageDeliveryStateTable_updateForMessage(indexMessageRecord, states);}
          virtual MessageDeliveryStateRecordListPtr getForMessage(index indexMessageRecord) const {return mOuter->IMessageDeliveryStateTable_getForMessage(indexMessageRecord);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::PendingDeliveryMessageTable
        #pragma mark

        class PendingDeliveryMessageTable : public IPendingDeliveryMessageTable
        {
          PendingDeliveryMessageTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static PendingDeliveryMessageTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return PendingDeliveryMessageTablePtr(new PendingDeliveryMessageTable(outer));}

          virtual void insert(const PendingDeliveryMessageRecord &message)                  {mOuter->IPendingDeliveryMessageTable_insert(message);}
          virtual PendingDeliveryMessageRecordListPtr getBatchToDeliver()                   {return mOuter->IPendingDeliveryMessageTable_getBatchToDeliver();}
          virtual void removeByMessageIndex(index indexMessageRecord)                       {mOuter->IPendingDeliveryMessageTable_removeByMessageIndex(indexMessageRecord);}
          virtual void remove(index indexPendingDeliveryRecord)                             {mOuter->IPendingDeliveryMessageTable_remove(indexPendingDeliveryRecord);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::ListTable
        #pragma mark

        class ListTable : public IListTable
        {
          ListTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static ListTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return ListTablePtr(new ListTable(outer));}

          virtual index addOrUpdateListID(const char *listID)                               {return mOuter->IListTable_addOrUpdateListID(listID);}
          virtual bool hasListID(const char *listID) const                                  {return mOuter->IListTable_hasListID(listID);}
          virtual void notifyDownloaded(index indexListRecord)                              {mOuter->IListTable_notifyDownloaded(indexListRecord);}
          virtual void notifyFailedToDownload(index indexListRecord)                        {mOuter->IListTable_notifyFailedToDownload(indexListRecord);}
          virtual ListRecordListPtr getBatchNeedingDownload(const Time &now) const          {return mOuter->IListTable_getBatchNeedingDownload(now);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::ListURITable
        #pragma mark

        class ListURITable : public IListURITable
        {
          ListURITable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static ListURITablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return ListURITablePtr(new ListURITable(outer));}

          virtual void update(
                              index indexListRecord,
                              const URIList &uris
                              )                                                             {mOuter->IListURITable_update(indexListRecord, uris);}
          virtual URIListPtr getURIs(const char *listID)                                    {return mOuter->IListURITable_getURIs(listID);}
          virtual int getOrder(
                               const char *listID,
                               const char *uri
                               )                                                            {return mOuter->IListURITable_getOrder(listID, uri);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::KeyDomainTable
        #pragma mark

        class KeyDomainTable : public IKeyDomainTable
        {
          KeyDomainTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static KeyDomainTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return KeyDomainTablePtr(new KeyDomainTable(outer));}

          virtual KeyDomainRecordPtr getByKeyDomain(int keyDomain) const                    {return mOuter->IKeyDomainTable_getByKeyDomain(keyDomain);}
          virtual void add(const KeyDomainRecord &keyDomain)                                {return mOuter->IKeyDomainTable_add(keyDomain);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::SendingKeyTable
        #pragma mark

        class SendingKeyTable : public ISendingKeyTable
        {
          SendingKeyTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static SendingKeyTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return SendingKeyTablePtr(new SendingKeyTable(outer));}

          virtual SendingKeyRecordPtr getByKeyID(const char *keyID) const                   {return mOuter->ISendingKeyTable_getByKeyID(keyID);}
          virtual SendingKeyRecordPtr getActive(
                                                const char *uri,
                                                Time now
                                                ) const                                     {return mOuter->ISendingKeyTable_getActive(uri, now);}
          virtual void addOrUpdate(const SendingKeyRecord &key)                             {mOuter->ISendingKeyTable_addOrUpdate(key);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::ReceivingKeyTable
        #pragma mark

        class ReceivingKeyTable : public IReceivingKeyTable
        {
          ReceivingKeyTable(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static ReceivingKeyTablePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return ReceivingKeyTablePtr(new ReceivingKeyTable(outer));}

          virtual ReceivingKeyRecordPtr getByKeyID(const char *keyID) const                 {return mOuter->IReceivingKeyTable_getByKeyID(keyID);}
          virtual void addOrUpdate(const ReceivingKeyRecord &key)                           {mOuter->IReceivingKeyTable_addOrUpdate(key);}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction::Storage
        #pragma mark

        class Storage : public IStorage
        {
          Storage(ServicePushMailboxSessionDatabaseAbstractionPtr outer) : mOuter(outer) {}

        public:
          static StoragePtr create(ServicePushMailboxSessionDatabaseAbstractionPtr outer) {return StoragePtr(new Storage(outer));}

          virtual String storeToTemporaryFile(const SecureByteBlock &buffer)                {return mOuter->IStorage_storeToTemporaryFile(buffer);}
          virtual String getStorageFileName() const                                         {return mOuter->IStorage_getStorageFileName();}

        protected:
          ServicePushMailboxSessionDatabaseAbstractionPtr mOuter;
        };


      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => ISettingsTable
        #pragma mark

        String ISettingsTable_getLastDownloadedVersionForFolders() const;
        void ISettingsTable_setLastDownloadedVersionForFolders(const char *version);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderTable
        #pragma mark

        void IFolderTable_flushAll();
        void IFolderTable_removeByIndex(index indexFolderRecord);
        index IFolderTable_getIndex(const char *inFolderName) const;
        FolderRecordPtr IFolderTable_getIndexAndUniqueID(const char *inFolderName) const;
        String IFolderTable_getName(index indexFolderRecord);
        void IFolderTable_addOrUpdate(const FolderRecord &folder);
        void IFolderTable_updateFolderName(
                                           index indexFolderRecord,
                                           const char *newfolderName
                                           );
        void IFolderTable_updateServerVersionIfFolderExists(
                                                            const char *inFolderName,
                                                            const char *inServerVersion
                                                            );
        FolderRecordListPtr IFolderTable_getNeedingUpdate(const Time &now) const;
        void IFolderTable_updateDownloadInfo(
                                             index indexFolderRecord,
                                             const char *inDownloadedVersion,
                                             const Time &inUpdateNext
                                             );
        void IFolderTable_resetUniqueID(index indexFolderRecord);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderMessageTable
        #pragma mark

        void IFolderMessageTable_flushAll();
        void IFolderMessageTable_addToFolderIfNotPresent(const FolderMessageRecord &folderMessage);
        void IFolderMessageTable_removeFromFolder(const FolderMessageRecord &folderMessage);
        void IFolderMessageTable_removeAllFromFolder(index indexFolderRecord);
        IFolderMessageTable::IndexListPtr IFolderMessageTable_getWithMessageID(const char *messageID) const;


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IFolderVersionedMessageTable
        #pragma mark

        void IFolderVersionedMessageTable_flushAll();
        void IFolderVersionedMessageTable_add(const FolderVersionedMessageRecord &message);
        void IFolderVersionedMessageTable_addRemovedEntryIfNotAlreadyRemoved(const FolderVersionedMessageRecord &message);
        void IFolderVersionedMessageTable_removeAllRelatedToFolder(index indexFolderRecord);
        FolderVersionedMessageRecordListPtr IFolderVersionedMessageTable_getBatchAfterIndex(
                                                                                            index indexFolderVersionMessageRecord,    // if < 0 then return all 0 or >
                                                                                            index indexFolderRecord
                                                                                            ) const;
        index IFolderVersionedMessageTable_getLastIndexForFolder(index indexFolderRecord) const;


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IMessageTable
        #pragma mark

        void IMessageTable_addOrUpdate(
                                       const char *messageID,
                                       const char *serverVersion
                                       );
        index IMessageTable_getIndex(const char *messageID) const;
        void IMessageTable_remove(index indexMessageRecord);
        void IMessageTable_update(const MessageRecord &message);
        void IMessageTable_updateEncodingAndEncryptedDataLength(
                                                                index indexMessageRecord,
                                                                const char *encoding,
                                                                ULONGEST encryptedDataLength
                                                                );
        void IMessageTable_updateEncodingAndFileNames(
                                                      index indexMessageRecord,
                                                      const char *encoding,
                                                      const char *encryptedFileName,
                                                      const char *decryptedFileName,
                                                      ULONGEST encryptedDataLength
                                                      );
        void IMessageTable_updateEncryptionFileName(
                                                    index indexMessageRecord,
                                                    const char *encryptedFileName
                                                    );
        void IMessageTable_updateEncryptionStorage(
                                                   index indexMessageRecord,
                                                   const char *encryptedFileName,
                                                   ULONGEST encryptedDataLength
                                                   );
        index IMessageTable_insert(const MessageRecord &message);
        MessageRecordPtr IMessageTable_getByIndex(index indexMessageRecord) const;
        MessageRecordPtr IMessageTable_getByMessageID(const char *messageID);
        void IMessageTable_updateServerVersionIfExists(
                                                       const char *messageID,
                                                       const char *serverVersion
                                                       );
        void IMessageTable_updateEncryptedData(
                                               index indexMessageRecord,
                                               const SecureByteBlock &encryptedData
                                               );
        IMessageTable::MessageNeedingUpdateListPtr IMessageTable_getBatchNeedingUpdate();
        MessageRecordListPtr IMessageTable_getBatchNeedingData(const Time &now) const;
        void IMessageTable_notifyDownload(
                                          index indexMessageRecord,
                                          bool downloaded
                                          );
        MessageRecordListPtr IMessageTable_getBatchNeedingKeysProcessing(
                                                                         index indexFolderRecord,
                                                                         const char *whereMessageTypeIs,
                                                                         const char *whereMimeType
                                                                         ) const;
        void IMessageTable_notifyKeyingProcessed(index indexMessageRecord);
        MessageRecordListPtr IMessageTable_getBatchNeedingDecrypting(const char *excludeWhereMessageTypeIs) const;
        void IMessageTable_notifyDecryptLater(
                                              index indexMessageRecord,
                                              const char *decryptKeyID
                                              );
        void IMessageTable_notifyDecryptionFailure(index indexMessageRecord);
        void IMessageTable_notifyDecryptNowForKeys(const char *whereDecryptKeyIDIs);
        void IMessageTable_notifyDecrypted(
                                           index indexMessageRecord,
                                           SecureByteBlockPtr decryptedData,
                                           bool needsNotification
                                           );
        void IMessageTable_notifyDecrypted(
                                           index indexMessageRecord,
                                           const char *encrytpedFileName,
                                           const char *decryptedFileName,
                                           bool needsNotification
                                           );
        MessageRecordListPtr IMessageTable_getBatchNeedingNotification() const;
        void IMessageTable_clearNeedsNotification(index indexMessageRecord);
        MessageRecordListPtr IMessageTable_getBatchNeedingExpiry(Time now) const;


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IMessageDeliveryStateTable
        #pragma mark

        void IMessageDeliveryStateTable_removeForMessage(index indexMessageRecord);
        void IMessageDeliveryStateTable_updateForMessage(
                                                         index indexMessageRecord,
                                                         const MessageDeliveryStateRecordList &states
                                                         );
        MessageDeliveryStateRecordListPtr IMessageDeliveryStateTable_getForMessage(index indexMessageRecord) const;


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IPendingDeliveryMessageTable
        #pragma mark

        void IPendingDeliveryMessageTable_insert(const PendingDeliveryMessageRecord &message);
        PendingDeliveryMessageRecordListPtr IPendingDeliveryMessageTable_getBatchToDeliver();
        void IPendingDeliveryMessageTable_removeByMessageIndex(index indexMessageRecord);
        void IPendingDeliveryMessageTable_remove(index indexPendingDeliveryRecord);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IListTable
        #pragma mark

        index IListTable_addOrUpdateListID(const char *listID);
        bool IListTable_hasListID(const char *listID) const;
        void IListTable_notifyDownloaded(index indexListRecord);
        void IListTable_notifyFailedToDownload(index indexListRecord);
        ListRecordListPtr IListTable_getBatchNeedingDownload(const Time &now) const;


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IListURITable
        #pragma mark

        void IListURITable_update(
                                  index indexListRecord,
                                  const IListURITable::URIList &uris
                                  );
        IListURITable::URIListPtr IListURITable_getURIs(const char *listID);
        int IListURITable_getOrder(
                                   const char *listID,
                                   const char *uri
                                   );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IKeyDomainTable
        #pragma mark

        KeyDomainRecordPtr IKeyDomainTable_getByKeyDomain(int keyDomain) const;
        void IKeyDomainTable_add(const KeyDomainRecord &keyDomain);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => ISendingKeyTable
        #pragma mark

        SendingKeyRecordPtr ISendingKeyTable_getByKeyID(const char *keyID) const;
        SendingKeyRecordPtr ISendingKeyTable_getActive(
                                                       const char *uri,
                                                       Time now
                                                       ) const;
        void ISendingKeyTable_addOrUpdate(const SendingKeyRecord &key);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IReceivingKeyTable
        #pragma mark

        ReceivingKeyRecordPtr IReceivingKeyTable_getByKeyID(const char *keyID) const;
        void IReceivingKeyTable_addOrUpdate(const ReceivingKeyRecord &key);


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => IStorage
        #pragma mark

        String IStorage_storeToTemporaryFile(const SecureByteBlock &buffer);
        String IStorage_getStorageFileName() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => SqlDatabase::Trace
        #pragma mark

        virtual void notifyDatabaseTrace(
                                         SqlDatabase::Trace::Severity severity,
                                         const char *message
                                         ) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();

        void prepareDB();
        void constructDBTables();

        static String SqlEscape(const String &input);
        static String SqlQuote(const String &input);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSessionDatabaseAbstraction => (data)
        #pragma mark

        ServicePushMailboxSessionDatabaseAbstractionWeakPtr mThisWeak;

        AutoPUID mID;

        String mUserHash;
        String mTempPath;
        String mStoragePath;

        mutable SqlDatabasePtr mDB;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSessionDatabaseAbstractionFactory
      #pragma mark

      interaction IServicePushMailboxSessionDatabaseAbstractionFactory
      {
        static IServicePushMailboxSessionDatabaseAbstractionFactory &singleton();

        virtual ServicePushMailboxSessionDatabaseAbstractionPtr create(
                                                                       const char *inHashRepresentingUser,
                                                                       const char *inUserTemporaryFilePath,
                                                                       const char *inUserStorageFilePath
                                                                       );
      };

      class ServicePushMailboxSessionDatabaseAbstractionFactory : public IFactory<IServicePushMailboxSessionDatabaseAbstractionFactory> {};
      
    }
  }
}
