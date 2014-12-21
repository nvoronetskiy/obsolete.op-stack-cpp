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

#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/services/ISettings.h>
#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#include <easySQLite/SqlFieldSet.h>

#define OPENPEER_STACK_PUSH_MAILBOX_DATABASE_VERSION 1
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDERS_NEEDING_UPDATE_LIMIT 5
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_UNIQUE_ID_LENGTH 16

#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_VERSIONED_MESSAGES_BATCH_LIMIT 10
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_MESSAGES_NEEDING_UPDATE_BATCH_LIMIT 10
#define OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_MESSAGES_NEEDING_DATA_BATCH_LIMIT 10

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(Helper, UseStackHelper)

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

      ZS_DECLARE_TYPEDEF_PTR(ServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageRecord, FolderVersionedMessageRecord)
      
      ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
      ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
      ZS_DECLARE_TYPEDEF_PTR(sql::RecordSet, SqlRecordSet)
      ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
      ZS_DECLARE_TYPEDEF_PTR(sql::FieldSet, SqlFieldSet)
      ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

      typedef ServicePushMailboxSessionDatabaseAbstraction::index index;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static void ignoreAllFieldsInTable(
                                         SqlTable &table,
                                         bool ignored = true
                                         )
      {
        SqlFieldSet *fields = table.fields();
        
        for (int loop = 0; loop < fields->count(); ++loop) {
          SqlField *field = fields->getByIndex(loop);
          field->setIgnored(ignored);
        }
      }
      
      //-----------------------------------------------------------------------
      static void initializeFields(SqlRecord &record)
      {
        const SqlFieldSet *fields = record.fields();
        for (int loop = 0; loop < fields->count(); ++loop)
        {
          const SqlField *field = fields->getByIndex(loop);
          if (field->isIgnored()) continue;
          if (!field->isNotNull()) continue;

          SqlValue *value = record.getValue(loop);
          if (value->isIgnored()) continue;
          if (value->hasData()) continue;
          
          switch (field->getType()) {
            case sql::type_undefined: break;
            case sql::type_int:       value->setInteger(0);
            case sql::type_text:      value->setString(String());
            case sql::type_float:     value->setDouble(0.0);
            case sql::type_bool:      value->setBool(false);
            case sql::type_time:      value->setTime(0);
          }
        }
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

        auto updateNext = record->getValue(UseTables::updateNext)->asInteger();
        if (0 != updateNext) {
          result->mUpdateNext = zsLib::timeSinceEpoch(Seconds(updateNext));
        }

        return result;
      }

      //-----------------------------------------------------------------------
      static void convertFolderVersionedMessage(
                                                SqlRecord *record,
                                                FolderVersionedMessageRecord &output
                                                )
      {
        FolderVersionedMessageRecordPtr result(new FolderVersionedMessageRecord);
        
        output.mIndex = static_cast<decltype(result->mIndex)>(record->getValue(SqlField::id)->asInteger());
        output.mIndexFolderRecord = static_cast<decltype(result->mIndexFolderRecord)>(record->getValue(UseTables::indexFolderRecord)->asInteger());
        output.mMessageID = record->getValue(UseTables::messageID)->asString();
        output.mRemovedFlag = record->getValue(UseTables::removedFlag)->asBool();
      }

      //-----------------------------------------------------------------------
      static MessageRecordPtr convertMessageRecord(SqlRecord *record)
      {
        MessageRecordPtr result(new MessageRecord);

        result->mIndex = static_cast<decltype(result->mIndex)>(record->getKeyIdValue()->asInteger());
        result->mMessageID = record->getValue(UseTables::messageID)->asString();
        result->mServerVersion = record->getValue(UseTables::serverVersion)->asString();
        result->mDownloadedVersion = record->getValue(UseTables::downloadedVersion)->asString();
        result->mTo = record->getValue(UseTables::to)->asString();
        result->mFrom = record->getValue(UseTables::from)->asString();
        result->mCC = record->getValue(UseTables::cc)->asString();
        result->mBCC = record->getValue(UseTables::bcc)->asString();
        result->mType = record->getValue(UseTables::type)->asString();
        result->mMimeType = record->getValue(UseTables::mimeType)->asString();
        result->mEncoding = record->getValue(UseTables::encoding)->asString();
        result->mPushType = record->getValue(UseTables::pushType)->asString();

        String pushInfo = record->getValue(UseTables::pushInfo)->asString();
        if (pushInfo.hasData()) {
          IServicePushMailboxSession::PushInfoListPtr pushInfoResult = IServicePushMailboxSession::PushInfoList::create(UseServicesHelper::toJSON(pushInfo));
          if (pushInfoResult) {
            result->mPushInfos = *pushInfoResult;
          }
        }

        auto sent = record->getValue(UseTables::sent)->asInteger();
        if (0 != sent) {
          result->mSent = zsLib::timeSinceEpoch(Seconds(sent));
        }
        auto expires = record->getValue(UseTables::sent)->asInteger();
        if (0 != expires) {
          result->mExpires = zsLib::timeSinceEpoch(Seconds(expires));
        }

        result->mEncryptedDataLength = record->getValue(UseTables::encryptedDataLength)->asInteger();

        String encryptedData = record->getValue(UseTables::encryptedData)->asString();
        if (encryptedData.hasData()) {
          result->mEncryptedData = UseServicesHelper::convertFromBase64(encryptedData);
        }
        String decryptedData = record->getValue(UseTables::decryptedData)->asString();
        if (decryptedData.hasData()) {
          result->mDecryptedData = UseServicesHelper::convertFromBase64(decryptedData);
        }

        result->mEncryptedFileName = record->getValue(UseTables::encryptedFileName)->asString();
        result->mDecryptedFileName = record->getValue(UseTables::decryptedFileName)->asString();
        result->mHasEncryptedData = record->getValue(UseTables::hasEncryptedData)->asBool();
        result->mHasDecryptedData = record->getValue(UseTables::hasEncryptedData)->asBool();
        result->mDownloadedEncryptedData = record->getValue(UseTables::downloadedEncryptedData)->asBool();
        result->mDownloadFailures = record->getValue(UseTables::downloadFailures)->asInteger();
        auto downloadRetryAfter = record->getValue(UseTables::downloadRetryAfter)->asInteger();
        if (0 != downloadRetryAfter) {
          result->mDownloadRetryAfter = zsLib::timeSinceEpoch(Seconds(downloadRetryAfter));
        }

        result->mProcessedKey = record->getValue(UseTables::processedKey)->asBool();
        result->mDecryptKeyID = record->getValue(UseTables::decryptKeyID)->asString();
        result->mDecryptLater = record->getValue(UseTables::decryptLater)->asBool();
        result->mDecryptFailure = record->getValue(UseTables::decryptFailure)->asBool();
        result->mNeedsNotification = record->getValue(UseTables::needsNotification)->asBool();

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSessionDatabaseAbstraction
      #pragma mark
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::SettingsRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("SettingsRecord");
        UseServicesHelper::debugAppend(resultEl, "last downloaded version for folders", mLastDownloadedVersionForFolders);
        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::FolderRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("FolderRecord");

        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "unique id", mUniqueID);
        UseServicesHelper::debugAppend(resultEl, "folder name", mFolderName);
        UseServicesHelper::debugAppend(resultEl, "server version", mServerVersion);
        UseServicesHelper::debugAppend(resultEl, "downloaded version", mDownloadedVersion);
        UseServicesHelper::debugAppend(resultEl, "total unread messages", mTotalUnreadMessages);
        UseServicesHelper::debugAppend(resultEl, "total messages", mTotalMessages);
        UseServicesHelper::debugAppend(resultEl, "update next", mUpdateNext);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::FolderMessageRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("FolderMessageRecord");

        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "index folder record", mIndexFolderRecord);
        UseServicesHelper::debugAppend(resultEl, "message id", mMessageID);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::FolderVersionedMessageRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("FolderVersionedMessageRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "index folder record", mIndexFolderRecord);
        UseServicesHelper::debugAppend(resultEl, "message id", mMessageID);
        UseServicesHelper::debugAppend(resultEl, "removed flag", mRemovedFlag);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::MessageRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("MessageRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "message id", mMessageID);
        UseServicesHelper::debugAppend(resultEl, "server version", mServerVersion);
        UseServicesHelper::debugAppend(resultEl, "download version", mDownloadedVersion);
        UseServicesHelper::debugAppend(resultEl, "to", mTo);
        UseServicesHelper::debugAppend(resultEl, "from", mFrom);
        UseServicesHelper::debugAppend(resultEl, "cc", mCC);
        UseServicesHelper::debugAppend(resultEl, "bcc", mBCC);
        UseServicesHelper::debugAppend(resultEl, "type", mType);
        UseServicesHelper::debugAppend(resultEl, "mime type", mMimeType);
        UseServicesHelper::debugAppend(resultEl, "encoding", mEncoding);
        UseServicesHelper::debugAppend(resultEl, "push type", mPushType);
        UseServicesHelper::debugAppend(resultEl, mPushInfos.toDebug());
        UseServicesHelper::debugAppend(resultEl, "sent", mSent);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);
        UseServicesHelper::debugAppend(resultEl, "encrypted data length", mEncryptedDataLength);
        UseServicesHelper::debugAppend(resultEl, "encrypted data", (bool)mEncryptedData);
        UseServicesHelper::debugAppend(resultEl, "decrypted data", (bool)mDecryptedData);
        UseServicesHelper::debugAppend(resultEl, "encrypted file name", mEncryptedFileName);
        UseServicesHelper::debugAppend(resultEl, "decrypted file name", mDecryptedFileName);
        UseServicesHelper::debugAppend(resultEl, "has encrypted data", mHasEncryptedData);
        UseServicesHelper::debugAppend(resultEl, "has decrypted data", mHasDecryptedData);
        UseServicesHelper::debugAppend(resultEl, "downloaded encrypted data", mDownloadedEncryptedData);
        UseServicesHelper::debugAppend(resultEl, "processed key", mProcessedKey);
        UseServicesHelper::debugAppend(resultEl, "decrypt key id", mDecryptKeyID);
        UseServicesHelper::debugAppend(resultEl, "decrypt later", mDecryptLater);
        UseServicesHelper::debugAppend(resultEl, "decrypt failure", mDecryptFailure);
        UseServicesHelper::debugAppend(resultEl, "needs notification", mNeedsNotification);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::MessageDeliveryStateRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("MessageDeliveryStateRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "index message", mIndexMessage);
        UseServicesHelper::debugAppend(resultEl, "flag", mFlag);
        UseServicesHelper::debugAppend(resultEl, "uri", mURI);
        UseServicesHelper::debugAppend(resultEl, "error code", mErrorCode);
        UseServicesHelper::debugAppend(resultEl, "error reason", mErrorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::PendingDeliveryMessageRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("PendingDeliveryMessageRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "index message", mIndexMessage);
        UseServicesHelper::debugAppend(resultEl, "remote folder", mRemoteFolder);
        UseServicesHelper::debugAppend(resultEl, "copy to sent", mCopyToSent);
        UseServicesHelper::debugAppend(resultEl, "subscribe flags", mSubscribeFlags);
        UseServicesHelper::debugAppend(resultEl, "encrypted data length", mEncryptedDataLength);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::ListRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("ListRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "list id", mListID);
        UseServicesHelper::debugAppend(resultEl, "needs download", mNeedsDownload);
        UseServicesHelper::debugAppend(resultEl, "failed download", mFailedDownload);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::ListURIRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("ListURIRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "index list record", mIndexListRecord);
        UseServicesHelper::debugAppend(resultEl, "order", mOrder);
        UseServicesHelper::debugAppend(resultEl, "uri", mURI);
        
        return resultEl;
      }

      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::KeyDomainRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("KeyDomainRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "key domain", mKeyDomain);
        UseServicesHelper::debugAppend(resultEl, "static private key", mDHStaticPrivateKey);
        UseServicesHelper::debugAppend(resultEl, "static public key", mDHStaticPublicKey);
        
        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::SendingKeyRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("SendingKeyRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "key id", mKeyID);
        UseServicesHelper::debugAppend(resultEl, "uri", mURI);
        UseServicesHelper::debugAppend(resultEl, "rsa passphrase", mRSAPassphrase);
        UseServicesHelper::debugAppend(resultEl, "dh passphrase", mDHPassphrase);
        UseServicesHelper::debugAppend(resultEl, "key domain", mKeyDomain);
        UseServicesHelper::debugAppend(resultEl, "dh ephemeral private key", mDHEphemeralPrivateKey);
        UseServicesHelper::debugAppend(resultEl, "dh ephemeral public key", mDHEphemeralPublicKey);
        UseServicesHelper::debugAppend(resultEl, "list size", mListSize);
        UseServicesHelper::debugAppend(resultEl, "total with dh passphrase", mTotalWithDHPassphrase);
        UseServicesHelper::debugAppend(resultEl, "ack dh passphrase set", mAckDHPassphraseSet);
        UseServicesHelper::debugAppend(resultEl, "active until", mActiveUntil);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr IServicePushMailboxSessionDatabaseAbstractionTypes::ReceivingKeyRecord::toDebug() const
      {
        ElementPtr resultEl = Element::create("ReceivingKeyRecord");
        
        UseServicesHelper::debugAppend(resultEl, "index", mIndex);
        UseServicesHelper::debugAppend(resultEl, "key id", mKeyID);
        UseServicesHelper::debugAppend(resultEl, "uri", mURI);
        UseServicesHelper::debugAppend(resultEl, "passphrase", mPassphrase);
        UseServicesHelper::debugAppend(resultEl, "key domain", mKeyDomain);
        UseServicesHelper::debugAppend(resultEl, "dh ephemeral private key", mDHEphemeralPrivateKey);
        UseServicesHelper::debugAppend(resultEl, "dh ephemeral public key", mDHEphemeralPublicKey);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageTable::MessageNeedingUpdateInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("IMessageTable::MessageNeedingUpdateInfo");

        UseServicesHelper::debugAppend(resultEl, "index message record", mIndexMessageRecord);
        UseServicesHelper::debugAppend(resultEl, "message id", mMessageID);

        return resultEl;
      }
      
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

          ZS_LOG_TRACE(log("get last downloaded version") + UseStackHelper::toDebug(&table, settingsRecord))

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

          ZS_LOG_TRACE(log("set last downloaded version") + UseStackHelper::toDebug(&table, settingsRecord))

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
          ZS_LOG_TRACE(log("flushing all from folder table"))

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
          ZS_LOG_TRACE(log("removing folder by index") + ZS_PARAMIZE(indexFolderRecord))

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
            ZS_LOG_TRACE(log("folder found") + ZS_PARAMIZE(inFolderName) + UseStackHelper::toDebug(&table, record))
            return static_cast<index>(result);
          }

          ZS_LOG_TRACE(log("folder does not exist") + ZS_PARAMIZE(inFolderName))
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
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

            ZS_LOG_TRACE(log("folder found") + result->toDebug())
            return result;
          }

          ZS_LOG_TRACE(log("folder does not exist") + ZS_PARAMIZE(inFolderName))
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
            ZS_LOG_TRACE(log("folder found") + ZS_PARAM("folder name", result) + UseStackHelper::toDebug(&table, record))
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
            ZS_LOG_TRACE(log("updating existing folder record") + UseStackHelper::toDebug(&table, useRecord))
            table.updateRecord(useRecord);
          } else {
            ZS_LOG_TRACE(log("adding new folder record") + UseStackHelper::toDebug(&table, useRecord))
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

          ZS_LOG_TRACE(log("updating folder name") + ZS_PARAMIZE(indexFolderRecord) + ZS_PARAMIZE(newfolderName))

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::folderName + " = " + SqlQuote(newfolderName) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

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

          ZS_LOG_TRACE(log("update server version (if folder exists)") + ZS_PARAMIZE(inFolderName) + ZS_PARAMIZE(inServerVersion))

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::serverVersion + " = " + SqlQuote(inServerVersion) + " WHERE " + UseTables::folderName + " = " + SqlQuote(inFolderName));

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

            ZS_LOG_TRACE(log("found folder needing update") + folder->toDebug())

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

          ZS_LOG_TRACE(log("updating folder download info") + ZS_PARAMIZE(indexFolderRecord) + ZS_PARAMIZE(inDownloadedVersion) + ZS_PARAMIZE(inUpdateNext))

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::downloadedVersion + " = " + SqlQuote(inDownloadedVersion) + ", " + UseTables::updateNext + " = " + string(zsLib::timeSinceEpoch<Seconds>(inUpdateNext).count()) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

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

          ZS_LOG_TRACE(log("reseting folder unique id") + ZS_PARAMIZE(indexFolderRecord))

          query.query(String("UPDATE ") + UseTables::Folder_name() + " SET " + UseTables::uniqueID + " = " + SqlQuote(UseServicesHelper::randomString(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_UNIQUE_ID_LENGTH)) + " WHERE " + SqlField::id + " = " + string(indexFolderRecord));

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

          ZS_LOG_TRACE(log("flushing all folder messages"))
          
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
          
          table.open(String(UseTables::indexFolderRecord) + " = " + string(folderMessage.mIndexFolderRecord) + " AND " + UseTables::messageID + " = " + SqlQuote(folderMessage.mMessageID));
          
          if (table.recordCount() > 0) {
            ZS_LOG_TRACE(log("folder message record already present") + folderMessage.toDebug())
            return;
          }

          ZS_LOG_TRACE(log("adding new folder message record") + folderMessage.toDebug())
          
          SqlRecord record(table.fields());
          record.setInteger(UseTables::indexFolderRecord, folderMessage.mIndexFolderRecord);
          record.setString(UseTables::messageID, folderMessage.mMessageID);
          table.addRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_removeFromFolder(const FolderMessageRecord &folderMessage)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());

          ZS_LOG_TRACE(log("removing folder message record") + folderMessage.toDebug())
          
          table.deleteRecords(String(UseTables::indexFolderRecord) + " = " + string(folderMessage.mIndexFolderRecord) + " AND " + UseTables::messageID + " = " + SqlQuote(folderMessage.mMessageID));
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_removeAllFromFolder(index indexFolderRecord)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());

          ZS_LOG_TRACE(log("removing all folder messages from folder") + ZS_PARAMIZE(indexFolderRecord))
          
          table.deleteRecords(String(UseTables::indexFolderRecord) + " = " + string(indexFolderRecord));
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      IFolderMessageTable::IndexListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable_getWithMessageID(const char *messageID) const
      {
        ZS_DECLARE_TYPEDEF_PTR(IFolderMessageTable::IndexList, IndexList)
        
        IndexListPtr result(new IndexList);

        AutoRecursiveLock lock(*this);
        
        try {
          SqlTable table(*mDB, UseTables::FolderMessage_name(), UseTables::FolderMessage());
          
          table.open(String(UseTables::messageID) + " = " + SqlQuote(messageID));
          
          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no folder messages with message id") + ZS_PARAMIZE(messageID))
            return result;
          }
          
          for (int loop = 0; loop < table.recordCount(); ++loop) {
            SqlRecord *record = table.getRecord(loop);
            if (!record) {
              ZS_LOG_WARNING(Detail, log("null record found in folder message record table") + ZS_PARAMIZE(messageID))
              continue;
            }

            ZS_LOG_TRACE(log("found folder message record with message id") + ZS_PARAMIZE(messageID) + UseStackHelper::toDebug(&table, record))

            result->push_back(static_cast<index>(record->getValue(UseTables::indexFolderRecord)->asInteger()));
          }

          return result;

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

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
        AutoRecursiveLock lock(*this);
        
        try {
          ZS_LOG_TRACE(log("flushing all folder versioned messages"))

          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());

          table.deleteRecords(String());
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_add(const FolderVersionedMessageRecord &message)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          ZS_LOG_TRACE(log("adding folder versioned message") + message.toDebug())

          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          
          SqlRecord record(table.fields());
          record.setInteger(UseTables::indexFolderRecord, message.mIndexFolderRecord);
          record.setString(UseTables::messageID, message.mMessageID);
          record.setBool(UseTables::removedFlag, message.mRemovedFlag);
          table.addRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_addRemovedEntryIfNotAlreadyRemoved(const FolderVersionedMessageRecord &message)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          
          table.open(String(UseTables::indexFolderRecord) + " = " + string(message.mIndexFolderRecord) + " AND " + UseTables::messageID + " = " + SqlQuote(message.mMessageID) + " ORDER BY " + SqlField::id + " DESC LIMIT 1");
          
          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no need to add folder versioned message removal because no previous added record exists") + message.toDebug())
            return;
          }

          SqlRecord *lastRecord = table.getTopRecord();
          if (lastRecord->getValue(UseTables::removedFlag)->asBool()) {
            ZS_LOG_TRACE(log("folder versioned message record removal already present") + message.toDebug())
            return;
          }

          ZS_LOG_TRACE(log("adding folder versioned message record") + message.toDebug())

          SqlRecord record(table.fields());
          record.setInteger(UseTables::indexFolderRecord, message.mIndexFolderRecord);
          record.setString(UseTables::messageID, message.mMessageID);
          record.setBool(UseTables::removedFlag, true);
          table.addRecord(&record);
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_removeAllRelatedToFolder(index indexFolderRecord)
      {
        AutoRecursiveLock lock(*this);
        
        try {
          ZS_LOG_TRACE(log("deleting all folder versioned message record for folder") + ZS_PARAMIZE(indexFolderRecord))

          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          
          table.deleteRecords(String(UseTables::indexFolderRecord) + " = " + string(indexFolderRecord));
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      FolderVersionedMessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_getBatchAfterIndex(
                                                                                                                                        index indexFolderVersionMessageRecord,    // if < 0 then return all 0 or >
                                                                                                                                        index indexFolderRecord
                                                                                                                                        ) const
      {
        FolderVersionedMessageRecordListPtr result(new FolderVersionedMessageRecordList);

        try {
          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          
          String clauseIndexFolderVersionedMessageRecord;
          if (indexFolderVersionMessageRecord >= 0) {
            clauseIndexFolderVersionedMessageRecord = String(SqlField::id) + " > " + string(indexFolderVersionMessageRecord) + " AND ";
          }

          table.open(clauseIndexFolderVersionedMessageRecord + UseTables::indexFolderRecord + " = " + string(indexFolderRecord) +" ORDER BY " + SqlField::id + " ASC LIMIT " + string(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_FOLDER_VERSIONED_MESSAGES_BATCH_LIMIT));
          
          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no folder versioned messages found matching query") + ZS_PARAMIZE(indexFolderVersionMessageRecord) + ZS_PARAMIZE(indexFolderRecord))
            return result;
          }
          
          for (int loop = 0; loop < table.recordCount(); ++loop) {
            SqlRecord *record = table.getRecord(loop);
            FolderVersionedMessageRecord output;

            if (!record) {
              ZS_LOG_WARNING(Detail, log("null record found in folder versioned message record result") + ZS_PARAMIZE(indexFolderVersionMessageRecord) + ZS_PARAMIZE(indexFolderRecord))
              continue;
            }
            convertFolderVersionedMessage(record, output);

            ZS_LOG_TRACE(log("found folder versioned message record") + output.toDebug())

            result->push_back(output);
          }

          return result;
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return FolderVersionedMessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IFolderVersionedMessageTable_getLastIndexForFolder(index indexFolderRecord) const
      {
        AutoRecursiveLock lock(*this);
        
        try {

          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());

          table.open(String(UseTables::indexFolderRecord) + " = " + string(indexFolderRecord) + " ORDER BY " + SqlField::id + " DESC LIMIT 1");

          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no folder messaged versioned records exists for folder") + ZS_PARAMIZE(indexFolderRecord))
            return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
          }
          
          SqlRecord *record = table.getTopRecord();

          ZS_LOG_TRACE(log("found last folder versioned message record") + UseStackHelper::toDebug(&table, record))
          
          return static_cast<index>(record->getValue(SqlField::id)->asInteger());
          
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

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
        AutoRecursiveLock lock(*this);
        
        try {
          
          // update existing record (if exists)
          {
            SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());
            
            ignoreAllFieldsInTable(table);
            
            table.fields()->getByName(SqlField::id)->setIgnored(false);
            table.fields()->getByName(UseTables::messageID)->setIgnored(false);
            table.fields()->getByName(UseTables::serverVersion)->setIgnored(false);
            
            table.open(String(UseTables::messageID) + " = " + SqlQuote(messageID));
            SqlRecord *record = table.getTopRecord();
            
            if (record) {
              ZS_LOG_TRACE(log("updating existing message record") + ZS_PARAMIZE(messageID) + ZS_PARAMIZE(serverVersion))
              record->setString(UseTables::serverVersion, serverVersion);
              table.updateRecord(record);
              return;
            }
          }

          // add a new record
          {
            SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());
            
            SqlRecord record(table.fields());
            
            record.setString(UseTables::messageID, messageID);
            record.setString(UseTables::serverVersion, serverVersion);
            
            initializeFields(record);

            ZS_LOG_TRACE(log("adding new message record") + UseStackHelper::toDebug(&table, &record))

            table.addRecord(&record);
          }

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getIndex(const char *messageID) const
      {
        AutoRecursiveLock lock(*this);
        
        try {
          
          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());
          
          ignoreAllFieldsInTable(table);
          
          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::messageID)->setIgnored(false);
          
          table.open(String(UseTables::messageID) + " = " + SqlQuote(messageID));
          SqlRecord *record = table.getTopRecord();
          
          if (!record) {
            ZS_LOG_TRACE(log("message index not found for message") + ZS_PARAMIZE(messageID))
            return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
          }

          ZS_LOG_TRACE(log("found message index for message") + ZS_PARAMIZE(messageID) + UseStackHelper::toDebug(&table, record))

          return  static_cast<index>(record->getValue(SqlField::id)->asInteger());
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_remove(index indexMessageRecord)
      {
        AutoRecursiveLock lock(*this);

        try {
          ZS_LOG_TRACE(log("deleting message record") + ZS_PARAMIZE(indexMessageRecord))

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          table.deleteRecords(String(SqlField::id) + " = " + string(indexMessageRecord));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_update(const MessageRecord &message)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("updating message record") + message.toDebug())

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::serverVersion)->setIgnored(false);
          table.fields()->getByName(UseTables::to)->setIgnored(false);
          table.fields()->getByName(UseTables::from)->setIgnored(false);
          table.fields()->getByName(UseTables::cc)->setIgnored(false);
          table.fields()->getByName(UseTables::bcc)->setIgnored(false);
          table.fields()->getByName(UseTables::type)->setIgnored(false);
          table.fields()->getByName(UseTables::mimeType)->setIgnored(false);
          table.fields()->getByName(UseTables::encoding)->setIgnored(false);
          table.fields()->getByName(UseTables::pushType)->setIgnored(false);
          table.fields()->getByName(UseTables::pushInfo)->setIgnored(false);
          table.fields()->getByName(UseTables::sent)->setIgnored(false);
          table.fields()->getByName(UseTables::expires)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedDataLength)->setIgnored(false);
          table.fields()->getByName(UseTables::needsNotification)->setIgnored(false);

          SqlRecord record(table.fields());

          record.setInteger(SqlField::id, message.mIndex);
          record.setString(UseTables::serverVersion, message.mServerVersion);
          record.setString(UseTables::to, message.mTo);
          record.setString(UseTables::from, message.mFrom);
          record.setString(UseTables::cc, message.mCC);
          record.setString(UseTables::bcc, message.mBCC);
          record.setString(UseTables::type, message.mType);
          record.setString(UseTables::mimeType, message.mMimeType);
          record.setString(UseTables::encoding, message.mEncoding);
          record.setString(UseTables::pushType, message.mPushType);
          if (message.mPushInfos.size() > 0) {
            record.setString(UseTables::pushInfo, UseServicesHelper::toString(message.mPushInfos.createElement()));
          }
          if (Time() != message.mSent) {
            record.setInteger(UseTables::sent, zsLib::timeSinceEpoch<Seconds>(message.mSent).count());
          } else {
            record.setInteger(UseTables::sent, 0);
          }
          if (Time() != message.mExpires) {
            record.setInteger(UseTables::expires, zsLib::timeSinceEpoch<Seconds>(message.mExpires).count());
          } else {
            record.setInteger(UseTables::expires, 0);
          }
          record.setInteger(UseTables::encryptedDataLength, message.mEncryptedDataLength);
          record.setBool(UseTables::needsNotification, message.mNeedsNotification);

          table.updateRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncodingAndEncryptedDataLength(
                                                                                                            index indexMessageRecord,
                                                                                                            const char *encoding,
                                                                                                            ULONGEST encryptedDataLength
                                                                                                            )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("updating message encoding and encrypted data length") + ZS_PARAMIZE(indexMessageRecord) + ZS_PARAMIZE(encoding) + ZS_PARAMIZE(encryptedDataLength))

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::encoding)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedDataLength)->setIgnored(false);

          SqlRecord record(table.fields());

          record.setString(UseTables::encoding, encoding);
          record.setInteger(UseTables::encryptedDataLength, encryptedDataLength);
          record.setInteger(UseTables::encryptedDataLength, encryptedDataLength);

          table.updateRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
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
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("updating message encoding and file names") + ZS_PARAMIZE(indexMessageRecord) + ZS_PARAMIZE(encoding) + ZS_PARAMIZE(encryptedFileName) + ZS_PARAMIZE(decryptedFileName) + ZS_PARAMIZE(encryptedDataLength))

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::encoding)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedFileName)->setIgnored(false);
          table.fields()->getByName(UseTables::decryptedFileName)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedDataLength)->setIgnored(false);

          SqlRecord record(table.fields());

          record.setString(UseTables::encoding, encoding);
          record.setString(UseTables::encryptedFileName, encryptedFileName);
          record.setString(UseTables::decryptedFileName, decryptedFileName);
          record.setInteger(UseTables::encryptedDataLength, encryptedDataLength);

          table.updateRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptionFileName(
                                                                                                index indexMessageRecord,
                                                                                                const char *encryptedFileName
                                                                                                )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("updating message encrypted file name") + ZS_PARAMIZE(indexMessageRecord) + ZS_PARAMIZE(encryptedFileName))

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedFileName)->setIgnored(false);

          SqlRecord record(table.fields());

          record.setString(UseTables::encryptedFileName, encryptedFileName);

          table.updateRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptionStorage(
                                                                                               index indexMessageRecord,
                                                                                               const char *encryptedFileName,
                                                                                               ULONGEST encryptedDataLength
                                                                                               )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("updating message encrypted file information") + ZS_PARAMIZE(indexMessageRecord) + ZS_PARAMIZE(encryptedFileName) + ZS_PARAMIZE(encryptedDataLength))

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedFileName)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedDataLength)->setIgnored(false);

          SqlRecord record(table.fields());

          record.setString(UseTables::encryptedFileName, encryptedFileName);
          record.setInteger(UseTables::encryptedDataLength, encryptedDataLength);

          table.updateRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      index ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_insert(const MessageRecord &message)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("inserting message") + message.toDebug())

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          SqlRecord record(table.fields());

          record.setString(UseTables::messageID, message.mMessageID);
          record.setString(UseTables::to, message.mTo);
          record.setString(UseTables::from, message.mFrom);
          record.setString(UseTables::cc, message.mCC);
          record.setString(UseTables::bcc, message.mBCC);
          record.setString(UseTables::type, message.mType);
          record.setString(UseTables::mimeType, message.mMimeType);
          record.setString(UseTables::encoding, message.mEncoding);
          record.setString(UseTables::pushType, message.mPushType);
          if (message.mPushInfos.size() > 0) {
            record.setString(UseTables::pushType, UseServicesHelper::toString(message.mPushInfos.createElement()));
          }
          if (Time() != message.mSent) {
            record.setInteger(UseTables::sent, zsLib::timeSinceEpoch<Seconds>(message.mSent).count());
          }
          if (Time() != message.mExpires) {
            record.setInteger(UseTables::expires, zsLib::timeSinceEpoch<Seconds>(message.mExpires).count());
          }
          if (Time() != message.mExpires) {
            record.setInteger(UseTables::expires, zsLib::timeSinceEpoch<Seconds>(message.mExpires).count());
          }
          if (message.mEncryptedData) {
            String base64 = UseServicesHelper::convertToBase64(*message.mEncryptedData);
            record.setString(UseTables::encryptedData, base64);
            record.setBool(UseTables::hasEncryptedData, true);
            record.setInteger(UseTables::encryptedDataLength, message.mEncryptedData->SizeInBytes());
          }
          record.setString(UseTables::pushType, message.mEncryptedFileName);
          if (message.mDecryptedData) {
            String base64 = UseServicesHelper::convertToBase64(*message.mDecryptedData);
            record.setString(UseTables::decryptedData, base64);
            record.setBool(UseTables::hasDecryptedData, true);
          }
          record.setBool(UseTables::needsNotification, message.mNeedsNotification);

          initializeFields(record);

          table.addRecord(&record);

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
        return OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //-----------------------------------------------------------------------
      MessageRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getByIndex(index indexMessageRecord) const
      {
        AutoRecursiveLock lock(*this);

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::messageID)->setIgnored(false);
          table.fields()->getByName(UseTables::to)->setIgnored(false);
          table.fields()->getByName(UseTables::from)->setIgnored(false);
          table.fields()->getByName(UseTables::cc)->setIgnored(false);
          table.fields()->getByName(UseTables::bcc)->setIgnored(false);
          table.fields()->getByName(UseTables::type)->setIgnored(false);
          table.fields()->getByName(UseTables::mimeType)->setIgnored(false);
          table.fields()->getByName(UseTables::encoding)->setIgnored(false);
          table.fields()->getByName(UseTables::pushType)->setIgnored(false);
          table.fields()->getByName(UseTables::pushInfo)->setIgnored(false);
          table.fields()->getByName(UseTables::sent)->setIgnored(false);
          table.fields()->getByName(UseTables::expires)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedFileName)->setIgnored(false);
          table.fields()->getByName(UseTables::decryptedFileName)->setIgnored(false);

          table.open(String(SqlField::id) + " = " + string(indexMessageRecord));
          SqlRecord *record = table.getTopRecord();

          if (!record) {
            ZS_LOG_TRACE(log("message not found") + ZS_PARAMIZE(indexMessageRecord))
            return MessageRecordPtr();
          }

          MessageRecordPtr result = convertMessageRecord(record);

          ZS_LOG_TRACE(log("found message") + result->toDebug())

          return  result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return MessageRecordPtr();
      }

      //-----------------------------------------------------------------------
      MessageRecordPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getByMessageID(const char *messageID)
      {
        AutoRecursiveLock lock(*this);

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::messageID)->setIgnored(false);
          table.fields()->getByName(UseTables::to)->setIgnored(false);
          table.fields()->getByName(UseTables::from)->setIgnored(false);
          table.fields()->getByName(UseTables::cc)->setIgnored(false);
          table.fields()->getByName(UseTables::bcc)->setIgnored(false);
          table.fields()->getByName(UseTables::type)->setIgnored(false);
          table.fields()->getByName(UseTables::mimeType)->setIgnored(false);
          table.fields()->getByName(UseTables::decryptedData)->setIgnored(false);
          table.fields()->getByName(UseTables::decryptedFileName)->setIgnored(false);
          table.fields()->getByName(UseTables::pushType)->setIgnored(false);
          table.fields()->getByName(UseTables::pushInfo)->setIgnored(false);
          table.fields()->getByName(UseTables::sent)->setIgnored(false);
          table.fields()->getByName(UseTables::expires)->setIgnored(false);

          table.open(String(UseTables::messageID) + " = " + SqlQuote(messageID));
          SqlRecord *record = table.getTopRecord();

          if (!record) {
            ZS_LOG_TRACE(log("message not found") + ZS_PARAMIZE(messageID))
            return MessageRecordPtr();
          }

          MessageRecordPtr result = convertMessageRecord(record);

          ZS_LOG_TRACE(log("found message") + result->toDebug())

          return  result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }

        return MessageRecordPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateServerVersionIfExists(
                                                                                                   const char *messageID,
                                                                                                   const char *serverVersion
                                                                                                   )
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          ZS_LOG_TRACE(log("updating message server version") + ZS_PARAMIZE(messageID) + ZS_PARAMIZE(serverVersion))

          query.query(String("UPDATE ") + UseTables::Message_name() + " SET " + UseTables::serverVersion + " = " + SqlQuote(serverVersion) + " WHERE " + UseTables::messageID + " = " + SqlQuote(messageID));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_updateEncryptedData(
                                                                                           index indexMessageRecord,
                                                                                           const SecureByteBlock &encryptedData
                                                                                           )
      {
        AutoRecursiveLock lock(*this);

        try {
          SqlRecordSet query(*mDB);

          ZS_LOG_TRACE(log("updating message encrypted data") + ZS_PARAMIZE(indexMessageRecord) + ZS_PARAM("encrypted data", encryptedData.SizeInBytes()))

          String dataStr;
          if (encryptedData.SizeInBytes() > 0) {
            dataStr = UseServicesHelper::convertToBase64(encryptedData);
          }

          query.query(String("UPDATE ") + UseTables::Message_name() + " SET " + UseTables::encryptedData + " = " + SqlQuote(dataStr) + " WHERE " + SqlField::id + " = " + string(indexMessageRecord));

        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
      }

      //-----------------------------------------------------------------------
      IMessageTable::MessageNeedingUpdateListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingUpdate()
      {
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateInfo, MessageNeedingUpdateInfo)
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateList, MessageNeedingUpdateList)

        AutoRecursiveLock lock(*this);

        MessageNeedingUpdateListPtr result(new MessageNeedingUpdateList);

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::messageID)->setIgnored(false);

          table.open(String(UseTables::serverVersion) + " != " + UseTables::downloadedVersion + " LIMIT " + string(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_MESSAGES_NEEDING_UPDATE_BATCH_LIMIT));

          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no messages needing update"))
            return result;
          }

          for (int loop = 0; loop < table.recordCount(); ++loop)
          {
            SqlRecord *record = table.getRecord(loop);
            if (!record) {
              ZS_LOG_WARNING(Detail, log("record is null from batch of messages needing update"))
              continue;
            }

            MessageNeedingUpdateInfo needingUpdate;
            needingUpdate.mIndexMessageRecord = static_cast<decltype(needingUpdate.mIndexMessageRecord)>(record->getKeyIdValue()->asInteger());
            needingUpdate.mMessageID = record->getValue(UseTables::messageID)->asString();

            ZS_LOG_TRACE(log("found message needing update") + needingUpdate.toDebug())

            result->push_back(needingUpdate);
          }

          return  result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
        
        return MessageNeedingUpdateListPtr();
      }

      //-----------------------------------------------------------------------
      MessageRecordListPtr ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_getBatchNeedingData() const
      {
        AutoRecursiveLock lock(*this);

        MessageRecordListPtr result(new MessageRecordList);

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          auto maxDownloadSize = UseSettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_SIZE_IN_BYTES);
          auto maxDownloadRetries = UseSettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_MAX_DOWNLOAD_RETRIES);

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::messageID)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedDataLength)->setIgnored(false);
          table.fields()->getByName(UseTables::encryptedFileName)->setIgnored(false);

          String maxSizeClause;
          if (0 != maxDownloadSize) {
            maxSizeClause = String(" AND ") + UseTables::encryptedDataLength + " < " + string(maxDownloadSize);
          }

          auto retryAfter = zsLib::timeSinceEpoch<Seconds>(zsLib::now()).count();

          table.open(String(UseTables::hasDecryptedData) + " = 0 AND " + UseTables::decryptedFileName + " != '' AND " + UseTables::downloadedEncryptedData + " = 0 AND " + UseTables::downloadRetryAfter + " < " + string(retryAfter) + " AND " + UseTables::downloadFailures + " < " + string(maxDownloadRetries) + " LIMIT " + string(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_MESSAGES_NEEDING_DATA_BATCH_LIMIT));

          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no messages needing data"))
            return result;
          }

          for (int loop = 0; loop < table.recordCount(); ++loop)
          {
            SqlRecord *record = table.getRecord(loop);
            if (!record) {
              ZS_LOG_WARNING(Detail, log("record is null from batch of messages needing data"))
              continue;
            }

            MessageRecordPtr message = convertMessageRecord(record);

            ZS_LOG_TRACE(log("found message needing data") + message->toDebug())

            result->push_back(*message);
          }

          return  result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
        
        return MessageRecordListPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSessionDatabaseAbstraction::IMessageTable_notifyDownload(
                                                                                      index indexMessageRecord,
                                                                                      bool downloaded
                                                                                      )
      {
        AutoRecursiveLock lock(*this);

        try {

          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());

          ignoreAllFieldsInTable(table);

          table.fields()->getByName(SqlField::id)->setIgnored(false);
          table.fields()->getByName(UseTables::downloadedEncryptedData)->setIgnored(false);
          table.fields()->getByName(UseTables::downloadRetryAfter)->setIgnored(false);
          table.fields()->getByName(UseTables::downloadFailures)->setIgnored(false);

          table.open(String(SqlField::id) + " = " + string(indexMessageRecord));

          SqlRecord *record = table.getTopRecord();
          if (!record) {
            ZS_LOG_WARNING(Detail, log("message record not found") + ZS_PARAMIZE(indexMessageRecord))
            return;
          }

          record->setBool(UseTables::downloadedEncryptedData, downloaded);

          auto downloadFailures = record->getValue(UseTables::downloadFailures)->asInteger();
          ++downloadFailures;

          record->setInteger(UseTables::downloadRetryAfter, 0);
          record->setInteger(UseTables::downloadFailures, 0);

          String maxSizeClause;
          if (0 != maxDownloadSize) {
            maxSizeClause = String(" AND ") + UseTables::encryptedDataLength + " < " + string(maxDownloadSize);
          }

          auto retryAfter = zsLib::timeSinceEpoch<Seconds>(zsLib::now()).count();

          table.open(String(UseTables::hasDecryptedData) + " = 0 AND " + UseTables::decryptedFileName + " != '' AND " + UseTables::downloadedEncryptedData + " = 0 AND " + UseTables::downloadRetryAfter + " < " + string(retryAfter) + " AND " + UseTables::downloadFailures + " < " + string(maxDownloadRetries) + " LIMIT " + string(OPENPEER_STACK_SERVICES_PUSH_MAILBOX_DATABASE_ABSTRACTION_MESSAGES_NEEDING_DATA_BATCH_LIMIT));

          if (table.recordCount() < 1) {
            ZS_LOG_TRACE(log("no messages needing data"))
            return result;
          }

          for (int loop = 0; loop < table.recordCount(); ++loop)
          {
            SqlRecord *record = table.getRecord(loop);
            if (!record) {
              ZS_LOG_WARNING(Detail, log("record is null from batch of messages needing data"))
              continue;
            }

            MessageRecordPtr message = convertMessageRecord(record);

            ZS_LOG_TRACE(log("found message needing data") + message->toDebug())

            result->push_back(*message);
          }

          return  result;
        } catch (SqlException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
        }
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

          SqlRecordSet query(*mDB);
          query.query(String("CREATE INDEX ") + UseTables::FolderMessage_name() + "i" + UseTables::indexFolderRecord + " ON " + UseTables::FolderMessage_name() + "(" + UseTables::indexFolderRecord + ")");
          query.query(String("CREATE INDEX ") + UseTables::FolderMessage_name() + "i" + UseTables::messageID + " ON " + UseTables::FolderMessage_name() + "(" + UseTables::messageID + ")");
        }
        {
          SqlTable table(*mDB, UseTables::FolderVersionedMessage_name(), UseTables::FolderVersionedMessage());
          table.create();

          SqlRecordSet query(*mDB);
          query.query(String("CREATE INDEX ") + UseTables::FolderVersionedMessage_name() + "i" + UseTables::indexFolderRecord + " ON " + UseTables::FolderMessage_name() + "(" + UseTables::indexFolderRecord + ")");
          query.query(String("CREATE INDEX ") + UseTables::FolderVersionedMessage_name() + "i" + UseTables::messageID + " ON " + UseTables::FolderMessage_name() + "(" + UseTables::messageID + ")");
        }
        {
          SqlTable table(*mDB, UseTables::Message_name(), UseTables::Message());
          SqlRecordSet query(*mDB);
          query.query(String("CREATE INDEX ") + UseTables::Message_name() + "i" + UseTables::serverVersion + " ON " + UseTables::Message_name() + "(" + UseTables::serverVersion + ")");
          query.query(String("CREATE INDEX ") + UseTables::Message_name() + "i" + UseTables::downloadedVersion + " ON " + UseTables::Message_name() + "(" + UseTables::downloadedVersion + ")");
          query.query(String("CREATE INDEX ") + UseTables::Message_name() + "i" + UseTables::downloadedEncryptedData + " ON " + UseTables::Message_name() + "(" + UseTables::downloadedEncryptedData + ")");
          query.query(String("CREATE INDEX ") + UseTables::Message_name() + "i" + UseTables::hasDecryptedData + " ON " + UseTables::Message_name() + "(" + UseTables::hasDecryptedData + ")");
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
