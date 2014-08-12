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

#include <openpeer/stack/types.h>
#include <openpeer/services/IDHKeyDomain.h>

#define OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN (-1)


namespace openpeer
{
  namespace stack
  {

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxDatabaseAbstractionDelegateTypes
    #pragma mark

    interaction IServicePushMailboxDatabaseAbstractionDelegateTypes
    {
      //-----------------------------------------------------------------------
      typedef IServicePushMailboxSession::PushInfoList PushInfoList;

      //-----------------------------------------------------------------------
      struct FolderNeedingUpdateInfo
      {
        int    mFolderIndex;
        String mFolderName;
        String mDownloadedVersion;

        FolderNeedingUpdateInfo() : mFolderIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<FolderNeedingUpdateInfo> FolderNeedingUpdateList;

      //-----------------------------------------------------------------------
      typedef int FolderIndex;
      typedef std::list<FolderIndex> FolderIndexList;

      //-----------------------------------------------------------------------
      struct MessageNeedingUpdateInfo
      {
        int mMessageIndex;
        String mMessageID;

        MessageNeedingUpdateInfo() : mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<MessageNeedingUpdateInfo> MessageNeedingUpdateList;

      //-----------------------------------------------------------------------
      struct MessageNeedingDataInfo
      {
        int mMessageIndex;
        String mMessageID;
        size_t mEncryptedDataLength;

        String mDecryptedFileName;
        String mEncryptedFileName;

        MessageNeedingDataInfo() : mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<MessageNeedingDataInfo> MessageNeedingDataList;

      //-----------------------------------------------------------------------
      struct MessagesNeedingKeyingProcessingInfo
      {
        int mMessageIndex;
        String mFrom;

        String mEncoding;
        SecureByteBlockPtr mEncryptedData;

        MessagesNeedingKeyingProcessingInfo() : mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<MessagesNeedingKeyingProcessingInfo> MessagesNeedingKeyingProcessingList;

      //-----------------------------------------------------------------------
      struct MessagesNeedingDecryptingInfo
      {
        int mMessageIndex;
        String mMessageID;
        String mEncoding;
        SecureByteBlockPtr mEncryptedData;
        String mEncryptedFileName;

        MessagesNeedingDecryptingInfo() : mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<MessagesNeedingDecryptingInfo> MessagesNeedingDecryptingList;

      //-----------------------------------------------------------------------
      struct MessageNeedingNotificationInfo
      {
        int mMessageIndex;
        String mMessageID;
        size_t mEncryptedDataLength;
        bool mHasData;
        bool mHasDecryptedData;

        MessageNeedingNotificationInfo() :
          mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mEncryptedDataLength(0),
          mHasData(false),
          mHasDecryptedData(false) {}
      };
      typedef std::list<MessageNeedingNotificationInfo> MessageNeedingNotificationList;

      //-----------------------------------------------------------------------
      struct MessagesNeedingExpiryInfo
      {
        int mMessageIndex;
        String mMessageID;
        Time mExpiry;

        MessagesNeedingExpiryInfo() : mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<MessagesNeedingExpiryInfo> MessagesNeedingExpiryList;

      //-----------------------------------------------------------------------
      struct FolderVersionedMessagesInfo
      {
        int mFolderVersionedMessageIndex;
        int mFolderIndex;
        String mMessageID;
        bool mRemovedFlag;

        FolderVersionedMessagesInfo() :
          mFolderVersionedMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mFolderIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mRemovedFlag(false) {}
      };
      typedef std::list<FolderVersionedMessagesInfo> FolderVersionedMessagesList;

      //-----------------------------------------------------------------------
      struct DeliveryInfo
      {
        String mURI;
        int mErrorCode;
        String mErrorReason;

        DeliveryInfo() : mErrorCode(0) {}
      };

      //-----------------------------------------------------------------------
      typedef std::list<DeliveryInfo> DeliveryInfoList;

      //-----------------------------------------------------------------------
      struct DeliveryInfoWithFlag : public DeliveryInfo
      {
        String mFlag;
      };

      //-----------------------------------------------------------------------
      typedef std::list<DeliveryInfoWithFlag> DeliveryInfoWithFlagList;

      //-----------------------------------------------------------------------
      struct PendingDeliveryMessageInfo
      {
        int     mPendingDeliveryMessageIndex;
        int     mMessageIndex;
        String  mRemoteFolder;
        bool    mCopyToSent;
        int     mSubscribeFlags;
        size_t  mEncryptedDataLength;

        PendingDeliveryMessageInfo() :
          mPendingDeliveryMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mMessageIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mCopyToSent(false),
          mSubscribeFlags(false),
          mEncryptedDataLength(0) {}
      };

      //-----------------------------------------------------------------------
      typedef std::list<PendingDeliveryMessageInfo> PendingDeliveryMessageList;

      //-----------------------------------------------------------------------
      struct ListsNeedingDownloadInfo
      {
        int mListIndex;
        String mListID;

        ListsNeedingDownloadInfo() : mListIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN) {}
      };
      typedef std::list<ListsNeedingDownloadInfo> ListsNeedingDownloadList;

      //-----------------------------------------------------------------------
      typedef String URI;
      typedef std::list<URI> URIList;

      //-----------------------------------------------------------------------
      struct SendingKeyInfo
      {
        int mSendingKeyIndex;                 // if < 0 then index is unknown
        String mKeyID;
        String mURI;
        String mRSAPassphrase;
        String mDHPassphrase;
        int mKeyDomain;
        String mDHEphemeralPrivateKey;
        String mDHEphemeralPublicKey;
        int mListSize;
        int mTotalWithPassphrase;
        String mAckDHPassphraseSet;
        Time mActiveUntil;
        Time mExpires;

        SendingKeyInfo() :
          mSendingKeyIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mKeyDomain(services::IDHKeyDomain::KeyDomainPrecompiledType_4096),
          mListSize(0),
          mTotalWithPassphrase(0) {}
      };

      //-----------------------------------------------------------------------
      struct ReceivingKeyInfo
      {
        int mReceivingKeyIndex;               // if < 0 then index is unknown
        String mKeyID;
        String mURI;
        String mPassphrase;
        int mKeyDomain;
        String mDHEphemerialPrivateKey;
        String mDHEphemerialPublicKey;
        Time mExpires;

        ReceivingKeyInfo() :
          mReceivingKeyIndex(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN),
          mKeyDomain(services::IDHKeyDomain::KeyDomainPrecompiledType_4096) {}
      };
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxDatabaseAbstractionDelegate
    #pragma mark

    interaction IServicePushMailboxDatabaseAbstractionDelegate : public IServicePushMailboxDatabaseAbstractionDelegateTypes
    {
      // ======================================================================
      // ======================================================================
      // SETTINGS TABLE
      // ======================================================================
      // String lastDownloadedVersionForFolders

      //-----------------------------------------------------------------------
      // PURPOSE: get the previously set value related to the version of the
      //          folder list.
      virtual String getLastDownloadedVersionForFolders() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: set the version string associated to the folder list
      virtual void setLastDownloadedVersionForFolders(const char *version) = 0;


      // ======================================================================
      // ======================================================================
      // FOLDERS TABLE
      // ======================================================================
      // int    index         [auto, unique]
      // String uniqueID      [unique, alphanum, default = assign random ID]
      // String folderName    [unique]
      // String serverVersion
      // String downloadedVersion
      // ULONG  totalUnreadMessages
      // ULONG  totalMessages
      // Time   updateNext

      //-----------------------------------------------------------------------
      // PURPOSE: Deletet all entries in the folders table
      virtual void flushFolders() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Deletet a folder from the folders table
      virtual void removeFolder(int folderIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get the index of an existing folder
      // RETURNS: true if folder exists, otherwise false
      virtual bool getFolderIndex(
                                  const char *inFolderName,
                                  int &outFolderIndex
                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: given a folder name get the folder index and unique ID for
      //          a given folder
      // RETURNS: true if folder exists, otherwise false
      virtual bool getFolderIndexAndUniqueID(
                                             const char *inFolderName,
                                             int &outFolderIndex,
                                             String &outUniqueID
                                             ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: given a folder index return the folder name associated
      // RETURNS: the folder name if the index is value or String() if not
      virtual String getFolderName(int folderIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Add or update a record in the folders table
      virtual void addOrUpdateFolder(
                                     const char *inFolderName,
                                     const char *inServerVersion,
                                     ULONG totalUnreadMessages,
                                     ULONG totalMessages
                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Rename an existing folder's name
      virtual void updateFolderName(
                                    int infolderIndex,
                                    const char *newfolderName
                                    ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Update the folder's server version if the folder exists
      virtual void updateFolderServerVersionIfFolderExists(
                                                           const char *inFolderName,
                                                           const char *inServerVersion
                                                           ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Returns a list of folders needing update
      // NOTE:    "WHERE now >= updateNext OR downloadedVersion != serverVersion"
      virtual void getFoldersNeedingUpdate(
                                           const Time &now,
                                           FolderNeedingUpdateList &outFolderNames
                                           ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Updates a folder's download information
      // NOTE:    Messages related to a folder have been downloaded from the
      //          server and the information about the downloaded messages must
      //          be recorded
      virtual void updateFolderDownloadInfo(
                                            int folderIndex,
                                            const char *inDownloadedVersion,
                                            const Time &inUpdateNext   // Time() if should not check again
                                            ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Assign a brand new unique random ID to the folder specified
      virtual void resetFolderUniqueID(int folderIndex) = 0;


      // ======================================================================
      // ======================================================================
      // FOLDER MESSAGES TABLE
      // ======================================================================
      // int    index         [auto, unique]
      // int    folderIndex
      // String messageID

      //-----------------------------------------------------------------------
      // PURPOSE: remove all entries in the folder messages table
      virtual void flushFolderMessages() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: add a new entry for a folder and message ID if a previous
      //          entry does not exist
      // NOTE:    if an existing entry contains the same "folderIndex" and
      //          "messageID" values then do not add a new entry
      virtual void addMessageToFolderIfNotPresent(
                                                  int folderIndex,
                                                  const char *messageID
                                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: if an entry contains the folder index and the message ID
      //          then remove the entry
      // NOTE:    if an existing entry contains the same "folderIndex" and
      //          "messageID" values then if must be removed
      virtual void removeMessageFromFolder(
                                           int folderIndex,
                                           const char *messageID
                                           ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: remove all entries that contain a particular folder index
      //          value
      virtual void removeAllMessagesFromFolder(int folderIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a list of folder indexes which have the supplied message
      //          ID
      virtual void getFoldersWithMessage(
                                         const char *messageID,
                                         FolderIndexList &outFolders
                                         ) = 0;



      // ======================================================================
      // ======================================================================
      // FOLDER VERSIONED MESSAGES TABLE
      // ======================================================================
      // int    index               [auto, unique]
      // int    folderIndex
      // String messageID
      // bool   removedFlag         [default = false (i.e. true = message is removed)]

      //-----------------------------------------------------------------------
      // PURPOSE: remove all entries from the folder versioned messages table
      virtual void flushFolderVersionedMessages() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: add a new entry to the end of the folder versioned messages
      //          table
      virtual void addFolderVersionedMessage(
                                             int folderIndex,
                                             const char *messageID
                                             ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: add an entry that contains a "removed" flag for a given
      //          folderIndex and messageID pairing
      // NOTES:   Only add if:
      //          - a previous entry exists containing the same "folderIndex"
      //            and "messageID"
      //          - if the last occurance of that entry does not have a
      //            "removedFlag" set to true.
      virtual void addRemovedFolderVersionedMessageEntryIfMessageNotRemoved(
                                                                            int folderIndex,
                                                                            const char *messageID
                                                                            ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: remove any folder versioned message whose folderIndex matches
      virtual void removeAllVersionedMessagesFromFolder(int folderIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: obtain a batch of folder versions messages whose
      //          index value is greater than the index supplied and whose
      //          folderIndex matches the supplied value
      virtual void getBatchOfFolderVersionedMessagesAfterIndex(
                                                               int versionIndex, // if < 0 then return all 0 or >
                                                               int folderIndex,
                                                               FolderVersionedMessagesList &outFolderVersionsMessages
                                                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: obtain the last version index available for any message whose
      //          folderIndex matches the "inFolderIndex" value
      // RETURNS: true if a record was found otherwise false
      virtual bool getLastFolderVersionedMessageForFolder(
                                                          int inFolderIndex,
                                                          int &outVersionIndex
                                                          ) = 0;


      // ======================================================================
      // ======================================================================
      // MESSAGES TABLE
      // ======================================================================
      // int    index                   [auto, unique]
      // String messageID               [unique]
      // String serverVersion
      // String downloadedVersion
      // String to
      // String from
      // String cc
      // String bcc
      // String type
      // String mimeType
      // String encoding
      // String pushType
      // PushInfoList pushInfos           // small amount of data can be converted into an encoded string or put into its own table associated to message
      // Time   sent
      // Time   expires
      // size_t encryptedDataLength       [default 0] // size of encrypted data in bytes (size of encrypted data blob if present)
      // Blob   encryptedData
      // Blob   decryptedData
      // String encryptedDataFileName
      // String decryptedDataFileName
      // bool   downloadedEncryptedData   // indicates that the file has downloaded
      // bool   processedKey              [default false]
      // String decryptKeyID
      // bool   decryptLater              [default false]
      // bool   decryptFailure            [default false]
      // bool   downloadFailure           [default false]
      // bool   needsNotification         [default false]

      //-----------------------------------------------------------------------
      // PURPOSE: creates or updates an entry about a message that needs
      //          updating
      virtual void addOrUpdateMessage(
                                      const char *messageID,
                                      const char *serverVersion
                                      ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get the index of a message if the message exists
      // RETURNS: true if message exists, otherwise false
      virtual bool getMessageIndex(
                                   const char *messageID,
                                   int &outMessageIndex
                                   ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: remove a message given its message index
      virtual void removeMessage(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: update meta data for a message
      virtual void updateMessage(
                                 int index,
                                 const char *downloadedVersion,
                                 const char *toURI,
                                 const char *fromURI,
                                 const char *ccURI,
                                 const char *bccURI,
                                 const char *type,
                                 const char *mimeType,
                                 const char *encoding,
                                 const char *pushType,
                                 const PushInfoList &pushInfos, // list will be empty is no values
                                 Time sent,
                                 Time expires,
                                 size_t encryptedDataLength,
                                 bool needsNotification
                                 ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: update a particular message's encoding scheme
      virtual void updateMessageEncodingAndEncryptedDataLength(
                                                               int index,
                                                               const char *encoding,
                                                               size_t encryptedDataLength
                                                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: update a particular message's encoding scheme
      virtual void updateMessageEncodingAndFileNames(
                                                     int index,
                                                     const char *encoding,
                                                     const char *encryptedFileName,
                                                     const char *decryptedFileName,
                                                     size_t encryptedDataLength
                                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: record that encrypted data is not recorded into a file
      virtual void updateMessageEncryptionFileName(
                                                   int index,
                                                   const char *encryptedFileName
                                                   ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: record that encrypted data is not recorded into a file
      virtual void updateMessageEncryptionStorage(
                                                  int index,
                                                  const char *encryptedFileName,
                                                  size_t encryptedDataLength,
                                                  SecureByteBlockPtr encryptedData  // will always be null SecureByteBlockPtr()
                                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: insert a message into the message table
      // RETURNS: the index of the newly created message
      virtual int insertMessage(
                                const char *messageID,
                                const char *toURI,
                                const char *fromURI,
                                const char *ccURI,
                                const char *bccURI,
                                const char *type,
                                const char *mimeType,
                                const char *encoding,
                                const char *pushType,
                                const PushInfoList &pushInfos,    // list will be empty is no values
                                Time sent,
                                Time expires,
                                const char *decryptedFileName,    // will be NULL if not applicable
                                SecureByteBlockPtr encryptedData,
                                SecureByteBlockPtr decryptedData,
                                bool needsNotifications
                                ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: gets existing message's details by message index
      virtual bool getMessageDetails(
                                     int messageIndex,
                                     String &outMessageID,
                                     String &outTo,
                                     String &outFrom,
                                     String &outCC,
                                     String &outBCC,
                                     String &outType,
                                     String &outMimeType,
                                     String &outEncoding,
                                     String &outPushType,
                                     PushInfoList &outPushInfos,
                                     Time &outSent,
                                     Time &outExpires,
                                     String &outEncryptedFileName,
                                     String &outDecryptedFileName,
                                     SecureByteBlockPtr &outEncryptedData
                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: gets existing message's details by message ID
      virtual bool getMessageDetails(
                                     const char *messageID,
                                     int &outMessageIndex,
                                     String &outTo,
                                     String &outFrom,
                                     String &outCC,
                                     String &outBCC,
                                     String &outType,
                                     String &outMimeType,
                                     String &outPushType,
                                     PushInfoList &outPushInfos,
                                     Time &outSent,
                                     Time &outExpires,
                                     SecureByteBlockPtr &outDecryptedData,
                                     String &outDecryptedFileName
                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: updates a message's server version if the message exists
      virtual void updateMessageServerVersionIfExists(
                                                      const char *messageID,
                                                      const char *serverVersion
                                                      ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: update the data on an existing message
      virtual void updateMessageEncryptedData(
                                              int messageIndex,
                                              const SecureByteBlock &encryptedData
                                              ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a small batch of messages whose server version does not
      //          match the downloaded version
      virtual void getBatchOfMessagesNeedingUpdate(MessageNeedingUpdateList &outMessagesToUpdate) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a small batch of messages whose has a data length > 0
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
      virtual void getBatchOfMessagesNeedingData(MessageNeedingDataList &outMessagesToUpdate) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: set flag that a message has downloaded (or not downloaded)
      virtual void notifyDownload(
                                  int messageIndex,
                                  bool downloaded
                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: notify that a message failed to download so it can be blocked
      //          from repeated downloading for a period of time
      virtual void notifyDownloadFailure(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a small batch of messages which has "processedKey" set to
      //          false.
      // NOTES:   - only return messages whose "processedKey" value is false
      //          - only return messages whose messageType matches the supplied
      //            value
      //          - only return messages whose mimeType matches the supplied
      //            value
      //          - only return messages which have a "data" blob
      virtual void getBatchOfMessagesNeedingKeysProcessing(
                                                           int folderIndex,
                                                           const char *whereMessageTypeIs,
                                                           const char *whereMimeType,
                                                           MessagesNeedingKeyingProcessingList &outMessages
                                                           ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets a message's "processedKey" value to true
      virtual void notifyMessageKeyingProcessed(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a small batch of messages that need to be decrypted
      // NOTES:   - only return messages where decryptLater is false
      //          - only return messages which have a "data" blob
      //          - exclude messages where message type matches the value of
      //            excludeWhereMessageTypeIs
      //          - exclude messages that already have a decryption blob
      //          - exclude messages that have a decrypted file name
      //          - exclude messages that previously failed to decrypt
      virtual void getBatchOfMessagesNeedingDecrypting(
                                                       const char *excludeWhereMessageTypeIs,
                                                       MessagesNeedingDecryptingList &outMessagesToDecrypt
                                                       ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets a message's "decryptLater" value to true
      virtual void notifyMessageDecryptLater(
                                             int messageIndex,
                                             const char *decryptKeyID
                                             ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets a message's "decryptFailure" value to true
      virtual void notifyMessageDecryptionFailure(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets the "decryptLater" value to false for all messages
      //          whose "decryptKeyID" matches the value of
      //          "whereDecryptKeyIDIs"
      virtual void notifyMessageDecryptNowForKeys(const char *whereDecryptKeyIDIs) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets the message's "decryptedData" and sets the
      //          "needsNotification" flag accordingly
      virtual void notifyMessageDecrypted(
                                          int messageIndex,
                                          SecureByteBlockPtr decryptedData,
                                          bool needsNotification
                                          ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets the message's "decryptedFileName" and sets the
      //          "needsNotification" flag accordingly
      virtual void notifyMessageDecrypted(
                                          int messageIndex,
                                          const char *encrytpedFileName,
                                          const char *decryptedFileName,
                                          bool needsNotification
                                          ) = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: gets a batch of messages whose "needsNotification" is set to
      //          true
      virtual void getBatchOfMessagesNeedingNotification(MessageNeedingNotificationList &outNeedsNotification) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: sets a message's "needsNotification" to false
      virtual void clearMessageNeedsNotification(int messageIndex) = 0;


      //-----------------------------------------------------------------------
      // PURPOSE: gets a batch of messages whose expiry time is previous to now
      virtual void getBatchOfMessagesNeedingExpiry(
                                                   Time now,
                                                   MessagesNeedingExpiryList &outNeedsExpiry
                                                   ) = 0;


      // ======================================================================
      // ======================================================================
      // MESSAGES DELIVERY STATE TABLE
      // ======================================================================
      // int    index         [auto, unique]
      // int    messageIndex  // message index from messages table
      // String flag          // represented flag
      // String uri           // uri of delivered party (empty = no list)
      // int    errorCode     // [default = 0] optional error code
      // String errorReason   // optional error reason

      //-----------------------------------------------------------------------
      // PURPOSE: remove any entries whose "messageIndex" matches the supplied
      //          value
      virtual void removeMessageDeliveryStatesForMessage(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: add a list of entries whose "messageIndex" and "flag"
      //          match the supplied value
      // NOTES:   - each entry contains unique values for the uri, errorCode,
      //            and errorReason from the "uris" list
      //          - if the URIs list is empty then a single entry with an
      //            empty uri is created
      virtual void updateMessageDeliverStateForMessage(
                                                       int messageIndex,
                                                       const char *flag,
                                                       const DeliveryInfoList &uris
                                                       ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: retrivies every entry whose "messageIndex" matches the
      //          the supplied value
      virtual void getMessageDeliverStateForMessage(
                                                    int messageIndex,
                                                    const DeliveryInfoWithFlagList &infos
                                                    ) = 0;

      // ======================================================================
      // ======================================================================
      // PENDING DELIVERY MESSAGES TABLE
      // ======================================================================
      // int    index             [auto, unique]
      // int    messageIndex      [unique] // index of message in messages table
      // String remoteFolder      // deliver to remote folder
      // bool   copyToSent        // copy to sent folder
      // int    subscribeFlags

      //-----------------------------------------------------------------------
      // PURPOSE: insert a new entry into the pending delivery messages table
      virtual int insertPendingDeliveryMessage(
                                               int messageIndex,
                                               const char *remoteFolder,
                                               bool copyToSendFolder,
                                               int subscribeFlags
                                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a batch of messages needing to be delivered
      virtual void getBatchOfMessagesToDeliver(PendingDeliveryMessageList &outPending) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: remove any entry whose messageIndex matches the supplied
      //          value (should only be one entry at most)
      virtual void removePendingDeliveryMessageByMessageIndex(int messageIndex) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: remove the entry whose "index" matches the supplied value
      virtual void removePendingDeliveryMessage(int pendingIndex) = 0;


      // ======================================================================
      // ======================================================================
      // LIST TABLE
      // ======================================================================
      // int    index           [auto, unique]
      // String listID          [unique]
      // bool   needsDownload   [default *true*]
      // bool   failedDownload  [default false]

      //-----------------------------------------------------------------------
      // PURPOSE: creates a new list entry if no entry with the given listID
      //          exists
      // RETURNS: the index of the new entry or the index of the existing entry
      virtual int addOrUpdateListID(const char *listID) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: returns true if the given listID already has an entry in the
      //          list table
      // RETURNS: true if an entry exists, otherwise false
      virtual bool hasListID(const char *listID) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: marks a list as having downloaded
      // NOTES:   set the "needsDownload" flag and the "faledDownload" flag to
      //          false
      virtual void notifyListDownloaded(int index) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: marks a list as having failed to download
      // NOTES:   a list can be temporarily or premanently removed from the
      //          "getBatchOfListsNeedingDownload" if the message failed to
      //          download
      virtual void notifyListFailedToDownload(int index) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: get a list of lists that need to be downloaded from the
      //          server
      virtual void getBatchOfListsNeedingDownload(ListsNeedingDownloadList &outLists) = 0;


      // ======================================================================
      // ======================================================================
      // LIST URI TABLE
      // ======================================================================
      // int    index         [auto, unique]
      // int    listIndex
      // int    order         // each URI in the list is given an order ID starting at 0 for each list
      // String uri           // uri of delivered party (empty = no list)

      //-----------------------------------------------------------------------
      // PURPOSE: stores each URI associated with a list in the list URI table
      // NOTES:   any previous entries whose "listIndex" matches the supplied
      //          value must be deleted before inserting new records for the
      //          supplied URIs.
      virtual void updateListURIs(
                                  int listIndex,
                                  const URIList &uris
                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: gets the full list of URIs whose listID matches the supplied
      //          value
      // NOTES:   DB will have to first obtain the listIndex from the list
      //          table to perform the getting of the list URIs
      virtual void getListURIs(
                               const char *listID,
                               URIList &outURIs
                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: given a list ID and a URI return the order number for the
      //          given URI within the list
      // NOTE:    each URI for a given listIndex has an order number that
      //          starts at zero and continues until total list entries - 1
      virtual bool getListOrder(
                                const char *listID,
                                const char *uri,
                                int &outIndex
                                ) = 0;

      // ======================================================================
      // ======================================================================
      // KEY DOMAIN TABLE
      // ======================================================================
      // int    index           [auto, unique]
      // int    keyDomain       [unqiue]
      // String dhStaticPrivateKey
      // String dhStaticPublicKey

      //-----------------------------------------------------------------------
      // PURPOSE: given a keyDomain return the static private/public key
      virtual bool getKeyDomain(
                                int keyDomain,
                                String &outStaticPrivateKey,
                                String &outStaticPublicKey
                                ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: add a static private/public key entry for a keyDomain
      virtual void addKeyDomain(
                                int keyDomain,
                                const char *staticPrivateKey,
                                const char *staticPublicKey
                                ) = 0;


      // ======================================================================
      // ======================================================================
      // SENDING KEYS TABLE
      // ======================================================================
      // int    index                 [auto, unique]
      // String keyID                 [unique]
      // String uri                   // this party (or list) is allowed to access this key
      // String rsaPassphrase         // passphrase protected with RSA
      // String dhPassphrase          // diffie hellman - perfect forward secrecy passphrase
      // int    keyDomain             // the key domain to use with this DH key pair
      // String dhEphemeralPrivateKey // diffie hellman ephemeral private key used in offer/answer
      // String dhEphemeralPublicKey  // diffie hellman ephemeral private key used in offer/answer
      // int    listSize              // if this key is for a list then how many parties in the list
      // int    totalWithDhPassphrase // how many parties of the list have the dh key (when listSize == totalWithDhPassphrase then everyone does)
      // String ackDHPassphraseSet    // the set representation of which parties have received the DH key and which have not
      // Time   activeUntil           // this key can be used actively until a particular date
      // Time   expires               // this key can be deleted after this date

      //-----------------------------------------------------------------------
      // PURPOSE: given a "keyID" return the associated sending key information
      // RETURNS: true if sending key exists, otherwise false
      virtual bool getSendingKey(
                                 const char *keyID,
                                 SendingKeyInfo &outInfo
                                 ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: given an "uri" return only the latest "active" sending key
      //          (if any)
      // NOTES:   - only return the latest sending key (i.e. highest index)
      //          - only return if the "activeUntil" is greater than "now"
      //          - only return if the "uri" passed in matches the "uri" in
      //            the table
      virtual bool getActiveSendingKey(
                                       const char *uri,
                                       Time now,
                                       SendingKeyInfo &outInfo
                                       );

      //-----------------------------------------------------------------------
      // PURPOSE: adds a new sending key or updates an existing sending key
      // NOTES:   if "mIndex" is < 0 then a new key is to be added otherwise an
      //          existing key is to be updated
      virtual void addOrUpdateSendingKey(const SendingKeyInfo &inInfo) = 0;


      // ======================================================================
      // ======================================================================
      // RECEIVING KEYS TABLE
      // ======================================================================
      // int    index                 [auto, unique]
      // String keyID                 [unique]
      // String uri                   // this key is from this user
      // String passphrase            // the passphrase for this key
      // int    keyDomain             // the key domain to use with this DH key pair
      // String dhEphemeralPrivateKey // diffie hellman ephemeral private key used in offer/answer
      // String dhEphemeralPublicKey  // diffie hellman ephemeral private key used in offer/answer
      // Time   expires               // when is this key no longer valid

      //-----------------------------------------------------------------------
      // PURPOSE: get an existing receiving key from the receiving keys table
      // RETURNS: true if key exists, otherwise false
      virtual bool getReceivingKey(
                                   const char *keyID,
                                   ReceivingKeyInfo &outInfo
                                   ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: adds a new receiving key or updates an existing receiving key
      // NOTES:   if "mIndex" is < 0 then a new key is to be added otherwise an
      //          existing key is to be updated
      virtual void addOrUpdateReceivingKey(const ReceivingKeyInfo &inInfo) = 0;


      // ======================================================================
      // ======================================================================
      // MISC
      // ======================================================================

      //-----------------------------------------------------------------------
      // PURPOSE: stores a buffer to a temporary file
      // RETURNS: file name containing binary data or String() if failed
      virtual String storeToTemporaryFile(const SecureByteBlock &buffer) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: returns a file name that can be used to store data
      // RETURNS: non existing file name that can be used to store data
      virtual String getStorageFileName() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: upload a file to a url
      // NOTES:   - this upload should occur even while the application goes
      //            to the background
      //          - this method is called asynchronously on the application's
      //            thread
      virtual void asyncUploadFileDataToURL(
                                            const char *postURL,
                                            const char *fileNameContainingData,
                                            size_t totalFileSizeInBytes,                                // the total bytes that exists within the file
                                            size_t remainingBytesToUpload,                              // the file should be seeked to the position of (total size - remaining) and upload the remaining bytes from this position in the file
                                            IServicePushMailboxDatabaseAbstractionNotifierPtr notifier
                                            ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: download a file from a URL
      // NOTES:   - this download should occur even while the application goes
      //            to the background
      //          - this method is called asynchronously on the application's
      //            thread
      virtual void asyncDownloadDataFromURL(
                                            const char *getURL,
                                            const char *fileNameToAppendData,               // the existing file name to open and append
                                            size_t finalFileSizeInBytes,                    // when the download completes the file size will be this size
                                            size_t remainingBytesToBeDownloaded,            // the downloaded data will be appended to the end of the existing file and this is the total bytes that are to be downloaded
                                            IServicePushMailboxDatabaseAbstractionNotifierPtr notifier
                                            ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxDatabaseAbstractionNotifier
    #pragma mark

    interaction IServicePushMailboxDatabaseAbstractionNotifier
    {
      virtual void notifyComplete(bool wasSuccessful) = 0;
    };
  }
}
