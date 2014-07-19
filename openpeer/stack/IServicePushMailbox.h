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


namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailbox
    #pragma mark

    interaction IServicePushMailbox
    {
      static IServicePushMailboxPtr createServicePushMailboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

      virtual PUID getID() const = 0;

      virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSession
    #pragma mark

    interaction IServicePushMailboxSession
    {
      enum SessionStates
      {
        SessionState_Pending,
        SessionState_Connecting,
        SessionState_Connected,
        SessionState_GoingToSleep,
        SessionState_Sleeping,
        SessionState_ShuttingDown,
        SessionState_Shutdown,
      };
      static const char *toString(SessionStates state);

      enum PushStates
      {
        PushState_Read,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,
      };

      static const char *toString(PushStates state);

      typedef String ValueType;
      typedef std::list<ValueType> ValueList;

      typedef String PeerOrIdentityURI;
      typedef String MessageID;

      ZS_DECLARE_TYPEDEF_PTR(std::list<PeerOrIdentityURI>, PeerOrIdentityList)

      struct PushStatePeerDetail
      {
        PeerOrIdentityURI mURI;

        WORD mErrorCode;
        String mErrorReason;
      };

      ZS_DECLARE_PTR(PushStatePeerDetail)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushStatePeerDetailPtr>, PushStatePeerDetailList)

      struct PushStateDetail
      {
        PushStates mState;
        PushStatePeerDetailList mRelatedPeers;
      };

      ZS_DECLARE_PTR(PushStateDetail)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushStateDetailPtr>, PushStateDetailList)

      struct PushMessage
      {
        MessageID mMessageID;             // system will assign this value
        String mMessageVersion;           // system will assign this value

        String mMessageType;
        SecureByteBlockPtr mFullMessage;

        ValueList mValues;                // values related to mapped type
        ElementPtr mCustomPushData;       // extended push related custom data

        Time mSent;                       // when was the message sent
        Time mExpires;                    // optional, system will assign a long life time if not specified

        IPeerPtr mFrom;                   // the peer that sent the message

        PeerOrIdentityListPtr mTo;
        PeerOrIdentityListPtr mCC;
        PeerOrIdentityListPtr mBCC;

        PushStateDetailListPtr mPushDetails;   // detailed related state information about the push
      };

      ZS_DECLARE_PTR(PushMessage)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushMessagePtr>, PushMessageList)

      ZS_DECLARE_TYPEDEF_PTR(std::list<MessageID>, MessageIDList)

      static ElementPtr toDebug(IServicePushMailboxSessionPtr session);

      static IServicePushMailboxSessionPtr create(
                                                  IServicePushMailboxSessionDelegatePtr delegate,
                                                  IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                  IServicePushMailboxPtr servicePushMailbox,
                                                  IAccountPtr account,
                                                  IServiceNamespaceGrantSessionPtr grantSession,
                                                  IServiceLockboxSessionPtr lockboxSession
                                                  );

      virtual PUID getID() const = 0;

      virtual IServicePushMailboxSessionSubscriptionPtr subscribe(IServicePushMailboxSessionDelegatePtr delegate) = 0;

      virtual IServicePushMailboxPtr getService() const = 0;

      virtual SessionStates getState(
                                     WORD *lastErrorCode,
                                     String *lastErrorReason
                                     ) const = 0;

      virtual void shutdown() = 0;

      virtual IServicePushMailboxRegisterQueryPtr registerDevice(
                                                                 IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                                 const char *deviceToken,
                                                                 const char *folder,        // what folder to monitor for push requests
                                                                 Time expires,              // how long should the registrration to the device last
                                                                 const char *mappedType,    // for APNS maps to "loc-key"
                                                                 bool unreadBadge,          // true causes total unread messages to be displayed in badge
                                                                 const char *sound,         // what sound to play upon receiving a message. For APNS, maps to "sound" field
                                                                 const char *action,        // for APNS, maps to "action-loc-key"
                                                                 const char *launchImage,   // for APNS, maps to "launch-image"
                                                                 unsigned int priority      // for APNS, maps to push priority
                                                                 ) = 0;

      virtual void monitorFolder(const char *folderName) = 0;

      // if returns false then all messages for the folder must be flushed and call with String() as the last version to download new contacts
      virtual bool getFolderMessageUpdates(
                                           const char *inFolder,
                                           String inLastVersionDownloaded,    // pass in String() if no previous version known
                                           String &outUpdatedToVersion,       // updated to this version (if same as passed in then no change available)
                                           PushMessageListPtr &outMessagesAdded,
                                           MessageIDListPtr &outMessagesRemoved
                                           ) = 0;

      virtual IServicePushMailboxSendQueryPtr sendMessage(
                                                          IServicePushMailboxSendQueryDelegatePtr delegate,
                                                          const PeerOrIdentityList &to,
                                                          const PeerOrIdentityList &cc,
                                                          const PeerOrIdentityList &bcc,
                                                          const PushMessage &message,
                                                          bool copyToSentFolder = true
                                                          ) = 0;

      virtual void recheckNow() = 0;

      virtual void markPushMessageRead(const char *messageID) = 0;
      virtual void deletePushMessage(const char *messageID) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSessionDelegate
    #pragma mark

    interaction IServicePushMailboxSessionDelegate
    {
      typedef IServicePushMailboxSession::SessionStates SessionStates;

      virtual void onServicePushMailboxSessionStateChanged(
                                                           IServicePushMailboxSessionPtr session,
                                                           SessionStates state
                                                           ) = 0;

      virtual void onServicePushMailboxSessionFolderChanged(
                                                            IServicePushMailboxSessionPtr session,
                                                            const char *folder
                                                            ) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSessionSubscription
    #pragma mark

    interaction IServicePushMailboxSessionSubscription
    {
      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual void background() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSendQuery
    #pragma mark

    interaction IServicePushMailboxSendQuery
    {
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSession::PushMessage, PushMessage)

      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual PushMessagePtr getPushMessage() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSendQueryDelegate
    #pragma mark

    interaction IServicePushMailboxSendQueryDelegate
    {
      virtual void onPushMailboxQueryPushStatesChanged(IServicePushMailboxSendQueryPtr query) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxRegisterQuery
    #pragma mark

    interaction IServicePushMailboxRegisterQuery
    {
      virtual PUID getID() const = 0;

      virtual bool isComplete(
                              WORD *outErrorCode = NULL,
                              String *outErrorReason = NULL
                              ) const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxRegisterQueryDelegate
    #pragma mark

    interaction IServicePushMailboxRegisterQueryDelegate
    {
      virtual void onPushMailboxRegisterQueryCompleted(IServicePushMailboxRegisterQueryPtr query) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxDatabaseAbstractionDelegate
    #pragma mark

    interaction IServicePushMailboxDatabaseAbstractionDelegate
    {
      //-----------------------------------------------------------------------
      struct FolderNeedingUpdateInfo
      {
        int    mIndex;
        String mFolderName;
        String mDownloadedVersion;
      };
      typedef std::list<FolderNeedingUpdateInfo> FolderNeedingUpdateList;

      //-----------------------------------------------------------------------
      struct MessageNeedingUpdateInfo
      {
        int mIndex;
        String mMessageID;
      };
      typedef std::list<MessageNeedingUpdateInfo> MessageNeedingUpdateList;

      //-----------------------------------------------------------------------
      struct MessageNeedingDataInfo
      {
        int mIndex;
        String mMessageID;
        size_t mDataLength;
      };
      typedef std::list<MessageNeedingDataInfo> MessageNeedingDataList;

      //-----------------------------------------------------------------------
      typedef IServicePushMailboxSession::ValueList ValueList;

      //-----------------------------------------------------------------------
      struct DeliveryInfo
      {
        String mURI;
        int mErrorCode;
        String mErrorReason;
      };

      typedef std::list<DeliveryInfo> DeliveryInfoList;

      //-----------------------------------------------------------------------
      struct ListsNeedingDownloadInfo
      {
        int mIndex;
        String mListID;
      };
      typedef std::list<ListsNeedingDownloadInfo> ListsNeedingDownloadList;

      //-----------------------------------------------------------------------
      typedef String URI;
      typedef std::list<URI> URIList;

      //-----------------------------------------------------------------------
      struct MessagesNeedingKeyingProcessingInfo
      {
        int mMessageIndex;
        String mFrom;

        String mEncoding;
        SecureByteBlockPtr mData;
      };
      typedef std::list<MessagesNeedingKeyingProcessingInfo> MessagesNeedingKeyingProcessingList;

      //-----------------------------------------------------------------------
      struct SendingKeyInfo
      {
        int mIndex;                   // if < 0 then index is unknown
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
      };

      //-----------------------------------------------------------------------
      struct ReceivingKeyInfo
      {
        int mIndex;                   // if < 0 then index is unknown
        String mKeyID;
        String mURI;
        String mPassphrase;
        int mKeyDomain;
        String mDHEphemerialPrivateKey;
        String mDHEphemerialPublicKey;
        Time mExpires;
      };


      // SETTINGS TABLE
      // ==============
      // String lastDownloadedVersionForFolders

      virtual String getLastDownloadedVersionForFolders() = 0;
      virtual void setLastDownloadedVersionForFolders(const char *version) = 0;


      // FOLDERS TABLE
      // =============
      // int    index         [auto, unique]
      // String uniqueID      [unique, alphanum, default = assign random ID]
      // String folderName    [unique]
      // String serverVersion
      // String downloadedVersion
      // ULONG  totalUnreadMessages
      // ULONG  totalMessages
      // Time   updateNext

      //-----------------------------------------------------------------------
      // PURPOSE: Deletet all folders and folder related data
      // NOTE:    All data in "Folders Table", "Folder Message Table" and
      //          "Version Folder Messages Table" are deleted.
      virtual void flushFolders() = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Deletet a folder and all related data for the folder
      // NOTE:    Deletes folder entry and any related messages to the folder
      //          in "Folder Messages" Table or messages in "Versioned Folder
      //          Messages Table"
      virtual void removeFolder(const char *inFolderName) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: find the index of an existing folder (if one exists)
      // RETURNS: true if folder exists, otherwise false
      virtual bool getFolderIndex(
                                  const char *inFolderName,
                                  int &outFolderIndex
                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Add or update a record in the "Folders Table"
      virtual void addOrUpdateFolder(
                                     const char *inFolderName,
                                     const char *inServerVersion,
                                     ULONG totalUnreadMessages,
                                     ULONG totalMessages
                                     ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Update a record in the "Folders Table"
      virtual void updateFolderIfExits(
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
      // NOTE:    Messages have been downloaded from the server and the
      //          information about the downloaded messages must be recorded
      virtual void updateFolderDownloadInfo(
                                            int folderIndex,
                                            const char *inDownloadedVersion,
                                            const Time &inUpdateNext   // Time() if should not check again
                                            ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Assign a brand new unique random ID to the folder specified
      virtual void resetFolderUniqueID(int folderIndex) = 0;


      // FOLDER MESSAGES TABLE
      // =====================
      // int    index         [auto, unique]
      // int    folderIndex
      // String messageID

      virtual void addMessageToFolderIfNotPresent(
                                                  int folderIndex,
                                                  const char *messageID
                                                  ) = 0;

      virtual void removeMessageFromFolder(
                                           int folderIndex,
                                           const char *messageID
                                           ) = 0;

      virtual void removeAllMessagesFromFolder(int folderIndex) = 0;


      // FOLDER VERSIONED MESSAGES TABLE
      // ===============================
      // int    index               [auto, unique]
      // int    folderIndex
      // String messageID
      // String downloadedVersion
      // bool   removedFlag   [default = false where true = removed]

      //-----------------------------------------------------------------------
      // PURPOSE: If the last message entry for a given folder index contains
      //          the message ID but does not have a "removedFlag" true then
      //          add a new entry for this message entry for the folder that
      //          contains the removed flag being true.
      virtual void addRemovedVersionedMessageEntryIfMessageNotRemoved(
                                                                      int folderIndex,
                                                                      const char *messageID
                                                                      ) = 0;

      virtual void removeAllVersionedMessagesFromFolder(int folderIndex) = 0;


      // MESSAGES TABLE
      // ==============
      // int    index               [auto, unique]
      // String messageID           [unique]
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
      // ValueList pushValues       small amount of data can be converted into an encoded string or put into its own table assoicated to message
      // Time   sent
      // Time   expires
      // Time   dataLength
      // Blob   data
      // bool   processedKey          [default false]
      // bool   needListFetch         [default false]
      // bool   updateFailed          [default false]
      // bool   dataDownloadFailed    [default false]

      //-----------------------------------------------------------------------
      // PURPOSE: Create an entry for a message or update it to notify that
      //          the message may require updating.
      virtual void addOrUpdateMessage(
                                      const char *messageID,
                                      const char *serverVersion
                                      ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Update meta data for a message
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
                                 const ValueList &pushValues,  // list will be empty is no values
                                 ElementPtr pushCustomData, // will be ElementPtr() if no custom data
                                 Time sent,
                                 Time expires,
                                 size_t dataLength
                                 ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Update meta data for a message
      virtual void updateMessageIfExists(
                                         const char *messageID,
                                         const char *serverVersion
                                         ) = 0;

      virtual void notifyMessageFailedToUpdate(int index) = 0;

      virtual void notifyMessageRemovedFromServer(const char *messageID) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Update the data on a message
      virtual void updateMessageData(
                                     int index,
                                     const BYTE *dataBuffer,
                                     size_t dataBufferLengthInBytes
                                     ) = 0;

      virtual void notifyMessageFailedToDownloadData(int index) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get a small batch of messages whose server version does not
      //          match the downloaded version.
      // NOTES:   Can and should filter out messages that failed to update
      //          either temporarily or indefinitely. This typically means
      //          the message is no longer present on the server but sometimes
      //          the server maybe merely in a funky state.
      virtual void getBatchOfMessagesNeedingUpdate(MessageNeedingUpdateList &outMessagesToUpdate) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get a small batch of messages whose has a data length > 0
      //          but no associated data blob.
      // NOTES:   To prevent downloading of messages that are too big this
      //          method can filter out messages larger than the maximum
      //          size the application wishes to download.
      //
      //          Also if data failed to download, the message can be excluded
      //          from downloading for a period of time or indefinitely. A
      //          failure typically means the message is no longer on the
      //          server but sometimes the server maybe merely in a funky state.
      virtual void getBatchOfMessagesNeedingData(MessageNeedingDataList &outMessagesToUpdate) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get a small batch of messages which has "processedKey" set to
      //          false.
      // NOTES:   - if whereMimeType is specified then the mime type must match
      //          - only return messages which have a "data" blob
      virtual void getBatchOfMessagesNeedingKeysProcessing(
                                                           int folderIndex,
                                                           const char *whereMimeType,
                                                           MessagesNeedingKeyingProcessingList &outMessages
                                                           ) = 0;

      virtual void notifyMessageKeyingProcessed(int messageIndex) = 0;


      // MESSAGES DELIVERY STATE TABLE
      // =============================
      // int    index         [auto, unique]
      // int    messageIndex  // message index from messages table
      // String flag          // represented flag
      // String uri           // uri of delivered party (empty = no list)
      // int    errorCode     // [default = 0] optional error code
      // String errorReason   // optional error reason

      virtual void removeMessageDeliveryStatesForMessage(int messageIndex) = 0;
      virtual void updateMessageDeliverStateForMessage(
                                                       int messageIndex,
                                                       const char *flag,
                                                       const DeliveryInfoList &uris
                                                       ) = 0;

      // LIST TABLE
      // ==========
      // int    index           [auto, unique]
      // String listID          [unique]
      // bool   needsDownload   [default *true*]
      // bool   failedDownload  [default false]

      virtual void addOrUpdateListID(const char *listID) = 0;

      virtual void notifyListFailedToDownload(int index) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Get the next list that needs to be downloaded from the server
      // RETRUNS: true if needing download otherwise false
      virtual void getBatchOfListNeedingDownload(ListsNeedingDownloadList &outLists) = 0;


      // LIST URI TABLE
      // ==============
      // int    index         [auto, unique]
      // int    listIndex
      // int    order         // each URI in the list is given an order ID starting at 0 for each list
      // String uri           // uri of delivered party (empty = no list)

      //-----------------------------------------------------------------------
      // PURPOSE: Stores the list into the database
      // NOTES:   will replace any existing contents if any list is present
      //          for a given list index
      virtual void updateListURIs(
                                  int listIndex,
                                  const URIList &uris
                                  ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Gets the full list associated to a list ID
      virtual void getListURIs(
                               const char *listID,
                               URIList &outURIs
                               ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Given a list ID and a URI return the order number for the
      //          given URI within the list
      virtual bool getListOrder(
                                const char *listID,
                                const char *uri,
                                int &outIndex
                                ) = 0;

      // KEY DOMAIN TABLE
      // ================
      // int    index           [auto, unique]
      // int    keyDomain       [unqiue]
      // String dhStaticPrivateKey
      // String dhStaticPublicKey

      virtual bool getKeyDomain(
                                int keyDomain,
                                String &outStaticPrivateKey,
                                String &outStaticPublicKey
                                ) = 0;

      virtual void addKeyDomain(
                                int keyDomain,
                                const char *staticPrivateKey,
                                const char *staticPublicKey
                                ) = 0;


      // SENDING KEYS TABLE
      // ==================
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

      virtual bool getSendingKey(
                                 const char *keyID,
                                 SendingKeyInfo &outInfo
                                 ) = 0;

      virtual bool addOrUpdateSendingKey(const SendingKeyInfo &inInfo) = 0;

      // RECEIVING KEYS TABLE
      // ====================
      // int    index                 [auto, unique]
      // String keyID                 [unique]
      // String uri                   // this key is from this user
      // String passphrase            // the passphrase for this key
      // int    keyDomain             // the key domain to use with this DH key pair
      // String dhEphemeralPrivateKey // diffie hellman ephemeral private key used in offer/answer
      // String dhEphemeralPublicKey  // diffie hellman ephemeral private key used in offer/answer
      // Time   expires               // when is this key no longer valid

      virtual bool getReceivingKey(
                                   const char *keyID,
                                   ReceivingKeyInfo &outInfo
                                   ) = 0;

      virtual void addOrUpdateReceivingKey(const ReceivingKeyInfo &inInfo) = 0;

    };
  }

}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IServicePushMailboxSessionDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServicePushMailboxSessionPtr, IServicePushMailboxSessionPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServicePushMailboxSessionDelegate::SessionStates, SessionStates)
ZS_DECLARE_PROXY_METHOD_2(onServicePushMailboxSessionStateChanged, IServicePushMailboxSessionPtr, SessionStates)
ZS_DECLARE_PROXY_METHOD_2(onServicePushMailboxSessionFolderChanged, IServicePushMailboxSessionPtr, const char *)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::IServicePushMailboxSessionDelegate, openpeer::stack::IServicePushMailboxSessionSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::IServicePushMailboxSessionPtr, IServicePushMailboxSessionPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::IServicePushMailboxSessionDelegate::SessionStates, SessionStates)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_2(onServicePushMailboxSessionStateChanged, IServicePushMailboxSessionPtr, SessionStates)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_2(onServicePushMailboxSessionFolderChanged, IServicePushMailboxSessionPtr, const char *)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IServicePushMailboxSendQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServicePushMailboxSendQueryPtr, IServicePushMailboxSendQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMailboxQueryPushStatesChanged, IServicePushMailboxSendQueryPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IServicePushMailboxRegisterQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServicePushMailboxRegisterQueryPtr, IServicePushMailboxRegisterQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMailboxRegisterQueryCompleted, IServicePushMailboxRegisterQueryPtr)
ZS_DECLARE_PROXY_END()
