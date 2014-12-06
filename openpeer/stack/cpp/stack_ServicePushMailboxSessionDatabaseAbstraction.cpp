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

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

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
        return String();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::ISettingsTable_setLastDownloadedVersionForFolders(const char *version)
      {
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
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_removeByIndex(index indexFolderRecord)
      {
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getIndex(const char *inFolderName) const
      {
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      FolderRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getIndexAndUniqueID(const char *inFolderName) const
      {
        return FolderRecordPtr();
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getName(index indexFolderRecord)
      {
        return String();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_addOrUpdate(const FolderRecord &folder)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateFolderName(
                                                                                       index indexFolderRecord,
                                                                                       const char *newfolderName
                                                                                       )
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateServerVersionIfFolderExists(
                                                                                                        const char *inFolderName,
                                                                                                        const char *inServerVersion
                                                                                                        )
      {
      }

      //-----------------------------------------------------------------------
      FolderRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_getNeedingUpdate(const Time &now) const
      {
        return FolderRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_updateDownloadInfo(
                                                                                         index indexFolderRecord,
                                                                                         const char *inDownloadedVersion,
                                                                                         const Time &inUpdateNext
                                                                                         )
      {
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderTable_resetUniqueID(index indexFolderIndex)
      {
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
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_addToFolderIfNotPresent(const FolderMessageRecord &folderMessage)
      {
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


    }
  }
}
