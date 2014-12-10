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

#include <openpeer/stack/IServicePushMailbox.h>

#include <openpeer/services/IDHKeyDomain.h>

#define OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN (-1)
#define OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN (-1)


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
      #pragma mark IServicePushMailboxSessionDatabaseAbstractionTypes
      #pragma mark

      interaction IServicePushMailboxSessionDatabaseAbstractionTypes
      {
        typedef int index;
        ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSession::PushInfoList, PushInfoList)

        ZS_DECLARE_STRUCT_PTR(SettingsRecord)
        ZS_DECLARE_STRUCT_PTR(FolderRecord)
        ZS_DECLARE_STRUCT_PTR(FolderMessageRecord)
        ZS_DECLARE_STRUCT_PTR(FolderVersionedMessageRecord)
        ZS_DECLARE_STRUCT_PTR(MessageRecord)
        ZS_DECLARE_STRUCT_PTR(MessageDeliveryStateRecord)
        ZS_DECLARE_STRUCT_PTR(PendingDeliveryMessageRecord)
        ZS_DECLARE_STRUCT_PTR(ListRecord)
        ZS_DECLARE_STRUCT_PTR(ListURIRecord)
        ZS_DECLARE_STRUCT_PTR(KeyDomainRecord)
        ZS_DECLARE_STRUCT_PTR(SendingKeyRecord)
        ZS_DECLARE_STRUCT_PTR(ReceivingKeyRecord)

        typedef std::list<FolderRecord> FolderRecordList;
        ZS_DECLARE_PTR(FolderRecordList)

        typedef std::list<FolderVersionedMessageRecord> FolderVersionedMessageRecordList;
        ZS_DECLARE_PTR(FolderVersionedMessageRecordList)

        typedef std::list<MessageRecord> MessageRecordList;
        ZS_DECLARE_PTR(MessageRecordList)

        typedef std::list<MessageDeliveryStateRecord> MessageDeliveryStateRecordList;
        ZS_DECLARE_PTR(MessageDeliveryStateRecordList)

        typedef std::list<PendingDeliveryMessageRecord> PendingDeliveryMessageRecordList;
        ZS_DECLARE_PTR(PendingDeliveryMessageRecordList)

        typedef std::list<ListRecord> ListRecordList;
        ZS_DECLARE_PTR(ListRecordList)

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark SettingsRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct SettingsRecord
        {
          String mLastDownloadedVersionForFolders;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FolderRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct FolderRecord
        {
          index   mIndex {-1};              // [auto, unique]
          String  mUniqueID;                // [unique, alphanum, default = random ID]
          String  mFolderName;              // [unique]
          String  mServerVersion;
          String  mDownloadedVersion;
          ULONG   mTotalUnreadMessages {0};
          ULONG   mTotalMessages {0};
          Time    mUpdateNext;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FolderMessageRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct FolderMessageRecord
        {
          index   mIndex {-1};              // [auto, unique]
          index   mIndexFolderRecord {-1};
          String  mMessageID;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FolderVersionedMessageRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct FolderVersionedMessageRecord
        {
          int     mIndex {-1};              // [auto, unique]
          int     mIndexFolderRecord {-1};
          String  mMessageID;
          bool    mRemovedFlag {false};     // true = message is removed
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct MessageRecord
        {
          index         mIndex {-1};                  // [auto, unique]
          String        mMessageID;
          String        mServerVersion;
          String        mDownloadedVersion;
          String        mTo;
          String        mFrom;
          String        mCC;
          String        mBCC;
          String        mType;
          String        mMimeType;
          String        mEncoding;
          String        mPushType;
          PushInfoList  mPushInfos;                   // small amount of data can be converted into an encoded string or put into its own table associated to message
          Time          mSent;
          Time          mExpires;
          ULONGEST      mEncryptedDataLength {0};     // size of encrypted data in bytes (size of encrypted data blob if present)
          SecureByteBlockPtr  mEncryptedData;
          SecureByteBlockPtr  mDecryptedData;
          String        mEncryptedFileName;
          String        mDecryptedFileName;
          bool          mHasEncryptedData {false};
          bool          mHasDecryptedData {false};
          bool          mDownloadedEncryptedData {false};
          bool          mProcessedKey {false};
          String        mDecryptKeyID;
          bool          mDecryptLater {false};
          bool          mDecryptFailure {false};
          bool          mNeedsNotification {false};
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageDeliveryStateRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct MessageDeliveryStateRecord
        {
          index   mIndex {-1};            // [auto, unique]
          index   mIndexMessage {-1};
          String  mFlag;                  // represented flag
          String  mURI;                   // uri of delivered party (empty = no list)
          int     mErrorCode {0};         // optional error code
          String  mErrorReason;           // optional error reason
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PendingDeliveryMessageRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct PendingDeliveryMessageRecord
        {
          index     mIndex {-1};            // [auto, unique]
          index     mIndexMessage {-1};
          String    mRemoteFolder;          // deliver to remote folder
          bool      mCopyToSent {false};    // copy to sent folder
          UINT      mSubscribeFlags;
          ULONGEST  mEncryptedDataLength {};
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ListRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct ListRecord
        {
          index   mIndex {-1};            // [auto, unique]
          String  mListID;                // [unique]
          bool    mNeedsDownload {true};
          bool    mFailedDownload {false};
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ListURIRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct ListURIRecord
        {
          index   mIndex {-1};            // [auto, unique]
          index   mIndexListRecord {-1};
          int     mOrder {-1};            // each URI in the list is given an order ID starting at 0 for each list
          String  mURI;                   // uri of delivered party (empty = no list)
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark KeyDomainRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct KeyDomainRecord
        {
          index   mIndex {-1};            // [auto, unique]
          int     mKeyDomain {0};         // [unqiue]
          String  mDHStaticPrivateKey;
          String  mDHStaticPublicKey;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark SendingKeyRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct SendingKeyRecord
        {
          index   mIndex {-1};                // [auto, unique]
          String  mKeyID;                     // [unique]
          String  mURI;                       // this party (or list) is allowed to access this key
          String  mRSAPassphrase;             // passphrase protected with RSA
          String  mDHPassphrase;              // diffie hellman - perfect forward secrecy passphrase
          int     mKeyDomain {0};             // the key domain to use with this DH key pair
          String  mDHEphemeralPrivateKey;     // diffie hellman ephemeral private key used in offer/answer
          String  mDHEphemeralPublicKey;      // diffie hellman ephemeral private key used in offer/answer
          size_t  mListSize {0};              // if this key is for a list then how many parties in the list
          size_t  mTotalWithDHPassphrase {0}; // how many parties of the list have the DH key (when listSize == totalWithDhPassphrase then everyone does)
          String  mAckDHPassphraseSet;        // the set representation of which parties have received the DH key and which have not
          Time    mActiveUntil;               // this key can be used actively until a particular date
          Time    mExpires;                   // this key can be deleted after this date
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ReceivingKeyRecord
        #pragma mark

        //---------------------------------------------------------------------
        struct ReceivingKeyRecord
        {
          index   mIndex {-1};                // [auto, unique]
          String  mKeyID;                     // [unique]
          String  mURI;                       // this key is from this user
          String  mPassphrase;                // the passphrase for this key
          int     mKeyDomain {0};             // the key domain to use with this DH key pair
          String  mDHEphemeralPrivateKey;     // diffie hellman ephemeral private key used in offer/answer
          String  mDHEphemeralPublicKey;      // diffie hellman ephemeral private key used in offer/answer
          Time    mExpires;                   // when is this key no longer valid
        };
      };

      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      //-------------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSessionDatabaseAbstraction
      #pragma mark

      interaction IServicePushMailboxSessionDatabaseAbstraction : public IServicePushMailboxSessionDatabaseAbstractionTypes
      {
        ZS_DECLARE_INTERACTION_PTR(ISettingsTable)
        ZS_DECLARE_INTERACTION_PTR(IFolderTable)
        ZS_DECLARE_INTERACTION_PTR(IFolderMessageTable)
        ZS_DECLARE_INTERACTION_PTR(IFolderVersionedMessageTable)
        ZS_DECLARE_INTERACTION_PTR(IMessageTable)
        ZS_DECLARE_INTERACTION_PTR(IMessageDeliveryStateTable)
        ZS_DECLARE_INTERACTION_PTR(IPendingDeliveryMessageTable)
        ZS_DECLARE_INTERACTION_PTR(IListTable)
        ZS_DECLARE_INTERACTION_PTR(IListURITable)
        ZS_DECLARE_INTERACTION_PTR(IKeyDomainTable)
        ZS_DECLARE_INTERACTION_PTR(ISendingKeyTable)
        ZS_DECLARE_INTERACTION_PTR(IReceivingKeyTable)
        ZS_DECLARE_INTERACTION_PTR(IStorage)


        static ElementPtr toDebug(IServicePushMailboxSessionDatabaseAbstractionPtr session);

        static IServicePushMailboxSessionDatabaseAbstractionPtr create(
                                                                       const char *inHashRepresentingUser,
                                                                       const char *inUserTemporaryFilePath,
                                                                       const char *inUserStorageFilePath
                                                                       );

        virtual PUID getID() const = 0;

        virtual ISettingsTablePtr settingsTable() const = 0;
        virtual IFolderTablePtr folderTable() const = 0;
        virtual IFolderVersionedMessageTablePtr folderVersionedMessageTable() const = 0;
        virtual IFolderMessageTablePtr folderMessageTable() const = 0;
        virtual IMessageTablePtr messageTable() const = 0;
        virtual IMessageDeliveryStateTablePtr messageDeliveryStateTable() const = 0;
        virtual IPendingDeliveryMessageTablePtr pendingDeliveryMessageTable() const = 0;
        virtual IListTablePtr listTable() const = 0;
        virtual IListURITablePtr listURITable() const = 0;
        virtual IKeyDomainTablePtr keyDomainTable() const = 0;
        virtual ISendingKeyTablePtr sendingKeyTable() const = 0;
        virtual IReceivingKeyTablePtr receivingKeyTable() const = 0;
        virtual IStoragePtr storage() const = 0;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ISettingsTable
        #pragma mark

        interaction ISettingsTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Get the previously set value related to the version of
          //          the folder list.
          virtual String getLastDownloadedVersionForFolders() const = 0;


          //-------------------------------------------------------------------
          // PURPOSE: Set the version string associated to the folder list
          virtual void setLastDownloadedVersionForFolders(const char *version) = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IFolderTable
        #pragma mark

        interaction IFolderTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Deletet all entries in the folders table
          virtual void flushAll() = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Deletet a folder from the folders table
          virtual void removeByIndex(index indexFolderRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get the index of an existing folder
          // RETURNS: The index of the folder or
          //          OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN if folder is
          //          not known.
          virtual index getIndex(const char *inFolderName) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Given a folder name get the folder index and unique ID
          //          for a given folder
          // RETURNS: Record containing index and unique ID or NULL
          virtual FolderRecordPtr getIndexAndUniqueID(const char *inFolderName) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Given a folder index return the folder name associated
          // RETURNS: The folder name if the index is value or String() if not
          virtual String getName(index indexFolderRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Add or update a record in the folders table
          virtual void addOrUpdate(const FolderRecord &folder) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Rename an existing folder's name
          virtual void updateFolderName(
                                        index indexFolderRecord,
                                        const char *newfolderName
                                        ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Update the folder's server version if the folder exists
          virtual void updateServerVersionIfFolderExists(
                                                         const char *inFolderName,
                                                         const char *inServerVersion
                                                         ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Returns a list of folders needing update
          // NOTE:    "WHERE now >= updateNext OR
          //          downloadedVersion != serverVersion"
          virtual FolderRecordListPtr getNeedingUpdate(const Time &now) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Updates a folder's download information
          // NOTE:    Messages related to a folder have been downloaded from
          //          the server and the information about the downloaded
          //          messages must be recorded
          virtual void updateDownloadInfo(
                                          index indexFolderRecord,
                                          const char *inDownloadedVersion,
                                          const Time &inUpdateNext   // Time() if should not check again
                                          ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Assign a brand new unique random ID to the folder
          //          specified
          virtual void resetUniqueID(index indexFolderIndex) = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IFolderMessageTable
        #pragma mark

        interaction IFolderMessageTable
        {
          typedef std::list<index> IndexList;
          ZS_DECLARE_PTR(IndexList)

          //-------------------------------------------------------------------
          // PURPOSE: Remove all entries in the folder messages table
          virtual void flushAll() = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Add a new entry for a folder and message ID if a previous
          //          entry does not exist
          // NOTE:    if an existing entry contains the same
          //          "mIndexFolderRecord" and "mMessageID" values then do not
          //          add a new entry.
          virtual void addToFolderIfNotPresent(const FolderMessageRecord &folderMessage) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: If an entry contains the folder index and the message ID
          //          then remove the entry.
          // NOTE:    If an existing entry contains the same
          //          "mIndexFolderRecord" and "mMessageID" values then ii must
          //          be removed.
          virtual void removeFromFolder(const FolderMessageRecord &folderMessage) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: remove all entries that contain a particular folder index
          //          value
          virtual void removeAllFromFolder(index folderIndex) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of folder indexes which have the supplied
          //          message ID
          virtual IndexListPtr getWithMessageID(const char *messageID) const = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IFolderVersionedMessageTable
        #pragma mark

        interaction IFolderVersionedMessageTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Remove all entries from the folder versioned messages
          //          table.
          virtual void flushAll() = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Add a new entry to the end of the folder versioned
          //          messages table
          virtual void add(const FolderVersionedMessageRecord &message) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Add an entry that contains a "removed" flag for a given
          //          folderIndex and messageID pairing
          // NOTES:   Only add if:
          //          - a previous entry exists containing the same
          //            "folderIndex" and "messageID"
          //          - if the last occurance of that entry does not have a
          //            "removedFlag" set to true.
          virtual void addRemovedEntryIfNotAlreadyRemoved(const FolderVersionedMessageRecord &message) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Remove any folder versioned message whose folderIndex matches
          virtual void removeAllRelatedToFolder(index indexFolderRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Obtain a batch of folder versions messages whose index
          //          value is greater than the index supplied and whose
          //          indexFolderRecord matches the supplied value
          virtual FolderVersionedMessageRecordListPtr getBatchAfterIndex(
                                                                         index indexFolderVersionMessageRecord,    // if < 0 then return all 0 or >
                                                                         index indexFolderRecord
                                                                         ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Obtain the last index of a folder version message record
          //          for any message whose
          //          indexFolderRecord matches the "inFolderIndex" value.
          // RETURNS: index of latest folder versioned message if found or
          //          OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN if not
          //          present.
          virtual index getLastIndexForFolder(index indexFolderRecord) const = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IMessageTable
        #pragma mark

        interaction IMessageTable
        {
          //-----------------------------------------------------------------------
          struct MessageNeedingUpdateInfo
          {
            decltype(MessageRecord::mIndex) mIndexMessageRecord {-1};
            decltype(MessageRecord::mMessageID) mMessageID;
          };
          ZS_DECLARE_PTR(MessageNeedingUpdateInfo)

          typedef std::list<MessageNeedingUpdateInfo> MessageNeedingUpdateList;
          ZS_DECLARE_PTR(MessageNeedingUpdateList)

          //-------------------------------------------------------------------
          // PURPOSE: Creates or updates an entry about a message that needs
          //          updating
          virtual void addOrUpdate(
                                   const char *messageID,
                                   const char *serverVersion
                                   ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get the index of a message record if the message exists
          // RETURNS: The index of the message or
          //          OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN if not known.
          virtual index getIndex(const char *messageID) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: remove a message given its message index
          virtual void remove(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Update meta data for a message
          virtual void update(const MessageRecord &message) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Update a particular message's encoding scheme
          virtual void updateEncodingAndEncryptedDataLength(
                                                            index indexMessageRecord,
                                                            const char *encoding,
                                                            ULONGEST encryptedDataLength
                                                            ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Update a particular message's encoding scheme
          virtual void updateEncodingAndFileNames(
                                                  index indexMessageRecord,
                                                  const char *encoding,
                                                  const char *encryptedFileName,
                                                  const char *decryptedFileName,
                                                  ULONGEST encryptedDataLength
                                                  ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Record that encrypted data is not recorded into a file
          virtual void updateEncryptionFileName(
                                                index indexMessageRecord,
                                                const char *encryptedFileName
                                                ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Record that encrypted data is not recorded into a file
          virtual void updateEncryptionStorage(
                                               index indexMessageRecord,
                                               const char *encryptedFileName,
                                               ULONGEST encryptedDataLength
                                               ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Insert a message into the message table
          // RETURNS: The index of the newly created message record
          virtual index insert(const MessageRecord &message) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Gets existing message's details by message index
          virtual MessageRecordPtr getByIndex(index indexMessageRecord) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Gets existing message's details by message ID
          // RETURNS: Message record or NULL if empty
          virtual MessageRecordPtr getByMessageID(const char *messageID) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Updates a message's server version if the message exists
          virtual void updateServerVersionIfExists(
                                                   const char *messageID,
                                                   const char *serverVersion
                                                   ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Update the data on an existing message
          virtual void updateEncryptedData(
                                           index indexMessageRecord,
                                           const SecureByteBlock &encryptedData
                                           ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get a small batch of messages whose server version does
          //          not match the downloaded version
          virtual MessageNeedingUpdateListPtr getBatchNeedingUpdate() = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get a small batch of messages whose has a data length > 0
          //          but contains no associated data blob
          // NOTES:   To prevent downloading of messages that are too big this
          //          method can filter out messages larger than the maximum
          //          size the application wishes to download.
          //
          //          Also if data failed to download, the message can be excluded
          //          from downloading for a period of time or indefinitely. A
          //          failure typically means the message is no longer on the
          //          server but sometimes the server maybe merely in a funky
          //          state.
          //
          //          If the message already has a decrypted file name then it does
          //          not need data to be downloaded.
          //
          //          This routine should filter all messages already marked as
          //          downloaded.
          //
          //          Messages that failed to download can be blocked from
          //          download reattempt for a period of time.
          virtual MessageRecordListPtr getBatchNeedingData() const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Set flag that a message has downloaded (or not
          //          downloaded)
          virtual void notifyDownload(
                                      index indexMessageRecord,
                                      bool downloaded
                                      ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Notify that a message failed to download so it can be
          //          blocked from repeated downloading for a period of time.
          virtual void notifyDownloadFailure(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get a small batch of messages which has "processedKey"
          //          set to false.
          // NOTES:   - only return messages whose "processedKey" value is false
          //          - only return messages whose messageType matches the supplied
          //            value
          //          - only return messages whose mimeType matches the supplied
          //            value
          //          - only return messages which have a "data" blob
          virtual MessageRecordListPtr getBatchNeedingKeysProcessing(
                                                                     index indexFolderRecord,
                                                                     const char *whereMessageTypeIs,
                                                                     const char *whereMimeType
                                                                     ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: sets a message's "processedKey" value to true
          virtual void notifyKeyingProcessed(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get a small batch of messages that need to be decrypted
          // NOTES:   - only return messages where decryptLater is false
          //          - only return messages which have a "data" blob
          //          - exclude messages where message type matches the value of
          //            excludeWhereMessageTypeIs
          //          - exclude messages that already have a decryption blob
          //          - exclude messages that have a decrypted file name
          //          - exclude messages that previously failed to decrypt
          virtual MessageRecordListPtr getBatchNeedingDecrypting(const char *excludeWhereMessageTypeIs) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Sets a message's "decryptLater" value to true
          virtual void notifyDecryptLater(
                                          index indexMessageRecord,
                                          const char *decryptKeyID
                                          ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Sets a message's "decryptFailure" value to true
          virtual void notifyDecryptionFailure(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Sets the "decryptLater" value to false for all messages
          //          whose "decryptKeyID" matches the value of
          //          "whereDecryptKeyIDIs"
          virtual void notifyDecryptNowForKeys(const char *whereDecryptKeyIDIs) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: sets the message's "decryptedData" and sets the
          //          "needsNotification" flag accordingly
          virtual void notifyDecrypted(
                                       index indexMessageRecord,
                                       SecureByteBlockPtr decryptedData,
                                       bool needsNotification
                                       ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Sets the message's "decryptedFileName" and sets the
          //          "needsNotification" flag accordingly
          virtual void notifyDecrypted(
                                       index indexMessageRecord,
                                       const char *encrytpedFileName,
                                       const char *decryptedFileName,
                                       bool needsNotification
                                       ) = 0;


          //-------------------------------------------------------------------
          // PURPOSE: gets a batch of messages whose "needsNotification" is set to
          //          true
          virtual MessageRecordListPtr getBatchNeedingNotification() const = 0;
          
          //-------------------------------------------------------------------
          // PURPOSE: sets a message's "needsNotification" to false
          virtual void clearNeedsNotification(index indexMessageRecord) = 0;
          
          
          //-------------------------------------------------------------------
          // PURPOSE: gets a batch of messages whose expiry time is previous to now
          virtual MessageRecordListPtr getBatchNeedingExpiry(Time now) const = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IMessageDeliveryStateTable
        #pragma mark

        interaction IMessageDeliveryStateTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Remove any entries whose "messageIndex" matches the
          //          supplied value
          virtual void removeForMessage(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Add a list of entries whose "messageIndex" and "flag"
          //          match the supplied value
          // NOTES:   - each entry contains unique values for the uri,
          //            errorCode, and errorReason from the "uris" list
          //          - if the URIs list is empty then a single entry with an
          //            empty uri is created
          virtual void updateForMessage(
                                        index indexMessageRecord,
                                        const MessageDeliveryStateRecordList &states
                                        ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: retrivies every entry whose "messageIndex" matches the
          //          the supplied value
          virtual MessageDeliveryStateRecordListPtr getForMessage(index indexMessageRecord) const = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IPendingDeliveryMessageTable
        #pragma mark

        interaction IPendingDeliveryMessageTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Insert a new entry into the pending delivery messages
          //          table
          virtual void insert(const PendingDeliveryMessageRecord &message) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Get a batch of messages needing to be delivered
          virtual PendingDeliveryMessageRecordListPtr getBatchToDeliver() = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Remove any entry whose messageIndex matches the supplied
          //          value (should only be one entry at most)
          virtual void removeByMessageIndex(index indexMessageRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Remove the entry whose "index" matches the supplied value
          virtual void remove(index indexPendingDeliveryRecord) = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IListTable
        #pragma mark

        interaction IListTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Creates a new list entry if no entry with the given
          //          listID exists
          // RETURNS: the index of the new entry or the index of the existing entry
          virtual index addOrUpdateListID(const char *listID) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Returns true if the given listID already has an entry in
          //          the list table.
          // RETURNS: true if an entry exists, otherwise false
          virtual bool hasListID(const char *listID) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Marks a list as having downloaded.
          // NOTES:   Set the "needsDownload" flag and the "faledDownload" flag
          //          to false.
          virtual void notifyDownloaded(index indexListRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Marks a list as having failed to download
          // NOTES:   A list can be temporarily or premanently removed from the
          //          "getBatchNeedingDownload" if the message failed to
          //          download
          virtual void notifyFailedToDownload(index indexListRecord) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: get a list of lists that need to be downloaded from the
          //          server
          virtual ListRecordListPtr getBatchNeedingDownload() const = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IListURITable
        #pragma mark

        interaction IListURITable
        {
          typedef String URI;
          typedef std::list<URI> URIList;

          ZS_DECLARE_PTR(URIList)

          //-------------------------------------------------------------------
          // PURPOSE: Stores each URI associated with a list in the list URI
          //          table.
          // NOTES:   Any previous entries whose "listIndex" matches the
          //          supplied value must be deleted before inserting new
          //          records for the supplied URIs.
          virtual void update(
                              index indexListRecord,
                              const URIList &uris
                              ) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Gets the full list of URIs whose listID matches the
          //          supplied value.
          // NOTES:   DB will have to first obtain the listIndex from the list
          //          table to perform the getting of the list URIs.
          virtual URIListPtr getURIs(const char *listID) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Given a list ID and a URI return the order number for the
          //          given URI within the list
          // NOTE:    Each URI for a given listIndex has an order number that
          //          starts at zero and continues until total list
          //          entries - 1. Returns
          //          OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN if the entry
          //          does not exist.
          virtual int getOrder(
                               const char *listID,
                               const char *uri
                               ) = 0;

        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IKeyDomainTable
        #pragma mark

        interaction IKeyDomainTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: given a keyDomain return the static private/public key
          virtual KeyDomainRecordPtr getByKeyDomain(int keyDomain) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: add a static private/public key entry for a keyDomain
          virtual void add(const KeyDomainRecord &keyDomain) = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ISendingKeyTable
        #pragma mark

        interaction ISendingKeyTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Given a "keyID" return the associated sending key record.
          // RETURNS: The sending key or NULL if does not exist.
          virtual SendingKeyRecordPtr getByKeyID(const char *keyID) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Given an "uri" return only the latest "active" sending key
          //          (if any)
          // NOTES:   - only return the latest sending key (i.e. highest index)
          //          - only return if the "activeUntil" is greater than "now"
          //          - only return if the "uri" passed in matches the "uri" in
          //            the table
          virtual SendingKeyRecordPtr getActive(
                                                const char *uri,
                                                Time now
                                                ) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Adds a new sending key or updates an existing sending key
          // NOTES:   If "mIndex" is < 0 then a new key is to be added
          //          otherwise an existing key is to be updated
          virtual void addOrUpdate(const SendingKeyRecord &key) = 0;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IReceivingKeyTable
        #pragma mark

        interaction IReceivingKeyTable
        {
          //-------------------------------------------------------------------
          // PURPOSE: Get an existing receiving key from the receiving keys
          //          table.
          // RETURNS: The receiving key record or null if it does not exist.
          virtual ReceivingKeyRecordPtr getByKeyID(const char *keyID) const = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Adds a new receiving key or updates an existing receiving
          //          key.
          // NOTES:   If "mIndex" is < 0 then a new key is to be added
          //          otherwise an existing key is to be updated
          virtual void addOrUpdate(const ReceivingKeyRecord &key) = 0;
        };


        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IStorage
        #pragma mark

        interaction IStorage
        {
          //-------------------------------------------------------------------
          // PURPOSE: Stores a buffer to a temporary file.
          // RETURNS: File name containing binary data or String() if failed.
          virtual String storeToTemporaryFile(const SecureByteBlock &buffer) = 0;

          //-------------------------------------------------------------------
          // PURPOSE: Returns a file name that can be used to store data.
          // RETURNS: Non existing file name that can be used to store data.
          virtual String getStorageFileName() const = 0;
        };
      };

    }
  }
}
