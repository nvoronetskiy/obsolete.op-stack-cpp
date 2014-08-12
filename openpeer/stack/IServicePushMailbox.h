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
        PushState_None,

        PushState_Read,
        PushState_Answered,
        PushState_Flagged,
        PushState_Deleted,
        PushState_Draft,
        PushState_Recent,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,
      };

      static const char *toString(PushStates state);
      static PushStates toPushState(const char *state);
      static bool canSubscribeState(PushStates state);

      typedef String ValueName;
      typedef std::list<ValueName> ValueNameList;

      typedef String PeerOrIdentityURI;
      typedef String MessageID;

      ZS_DECLARE_TYPEDEF_PTR(std::list<PeerOrIdentityURI>, PeerOrIdentityList)

      struct PushStatePeerDetail
      {
        PeerOrIdentityURI mURI;

        WORD mErrorCode;
        String mErrorReason;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushStatePeerDetail>, PushStatePeerDetailList)

      typedef std::map<PushStates, PushStatePeerDetailList> PushStateDetailMap;

      struct PushInfo
      {
        String mServiceType;              // e.g. "apns", "gcm", or "all"
        ElementPtr mValues;               // "values" related to mapped type
        ElementPtr mCustom;               // extended push related custom data
      };

      typedef std::list<PushInfo> PushInfoList;

      struct PushMessage
      {
        MessageID mMessageID;             // system will assign this value

        String mMessageType;
        String mMimeType;
        SecureByteBlockPtr mFullMessage;  // if data fits in memory the data will be included here
        String mFullMessageFileName;      // if data is too large then this file will contain the data

        String mPushType;
        PushInfoList mPushInfos;

        Time mSent;                       // when was the message sent
        Time mExpires;                    // optional, system will assign a long life time if not specified

        IPeerPtr mFrom;                   // the peer that sent the message

        PeerOrIdentityListPtr mTo;
        PeerOrIdentityListPtr mCC;
        PeerOrIdentityListPtr mBCC;

        PushStateDetailMap mPushStateDetails;   // detailed related state information about the push
      };

      ZS_DECLARE_PTR(PushMessage)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushMessagePtr>, PushMessageList)

      ZS_DECLARE_TYPEDEF_PTR(std::list<MessageID>, MessageIDList)

      static ElementPtr toDebug(IServicePushMailboxSessionPtr session);

      static IServicePushMailboxSessionPtr create(
                                                  IServicePushMailboxSessionDelegatePtr delegate,
                                                  IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                  IMessageQueuePtr databaseDelegateAsyncQueue,
                                                  IServicePushMailboxPtr servicePushMailbox,
                                                  IAccountPtr account,
                                                  IServiceNamespaceGrantSessionPtr grantSession,
                                                  IServiceLockboxSessionPtr lockboxSession
                                                  );

      virtual PUID getID() const = 0;

      virtual IServicePushMailboxSessionSubscriptionPtr subscribe(IServicePushMailboxSessionDelegatePtr delegate) = 0;

      virtual IServicePushMailboxPtr getService() const = 0;

      virtual SessionStates getState(
                                     WORD *lastErrorCode = NULL,
                                     String *lastErrorReason = NULL
                                     ) const = 0;

      virtual void shutdown() = 0;

      virtual IServicePushMailboxRegisterQueryPtr registerDevice(
                                                                 IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                                 const char *deviceToken,
                                                                 const char *folder,              // what folder to monitor for push requests
                                                                 Time expires,                    // how long should the registrration to the device last
                                                                 const char *mappedType,          // for APNS maps to "loc-key"
                                                                 bool unreadBadge,                // true causes total unread messages to be displayed in badge
                                                                 const char *sound,               // what sound to play upon receiving a message. For APNS, maps to "sound" field
                                                                 const char *action,              // for APNS, maps to "action-loc-key"
                                                                 const char *launchImage,         // for APNS, maps to "launch-image"
                                                                 unsigned int priority,           // for APNS, maps to push priority
                                                                 const ValueNameList &valueNames  // list of values requested from each push from the push server (in order they should be delivered); empty = all values
                                                                 ) = 0;

      virtual void monitorFolder(const char *folderName) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Fetch a batch of new or removed messages for a folder since
      //          the last version
      // RETURNS: true if successful, false if not successful or for a version
      //          conflict.
      // NOTES:   Keep calling again until inLastVersionDownloaded is the same
      //          as outUpdatedToVersion to retreive all updates. When false is
      //          returned the version passed in must be considered invalid
      //          and all messages for a folder need to be flushed then
      //          the routine should be called again with a String() version
      //          to get all the messages from the start. If the routien returns
      //          false and the version passed in was String() then the folder
      //          cannot be fetched.
      virtual bool getFolderMessageUpdates(
                                           const char *inFolder,
                                           const String &inLastVersionDownloaded,    // pass in String() if no previous version known
                                           String &outUpdatedToVersion,       // updated to this version (if same as passed in then no change available)
                                           PushMessageListPtr &outMessagesAdded,
                                           MessageIDListPtr &outMessagesRemoved
                                           ) = 0;

      virtual String getLatestDownloadVersionAvailableForFolder(const char *inFolder) = 0;

      virtual IServicePushMailboxSendQueryPtr sendMessage(
                                                          IServicePushMailboxSendQueryDelegatePtr delegate,
                                                          const PushMessage &message,
                                                          const char *remoteFolder,
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

      virtual bool isUploaded() const = 0;
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
      virtual void onPushMailboxSendQueryMessageUploaded(IServicePushMailboxSendQueryPtr query) = 0;
      virtual void onPushMailboxSendQueryPushStatesChanged(IServicePushMailboxSendQueryPtr query) = 0;
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
ZS_DECLARE_PROXY_METHOD_1(onPushMailboxSendQueryMessageUploaded, IServicePushMailboxSendQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMailboxSendQueryPushStatesChanged, IServicePushMailboxSendQueryPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IServicePushMailboxRegisterQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServicePushMailboxRegisterQueryPtr, IServicePushMailboxRegisterQueryPtr)
ZS_DECLARE_PROXY_METHOD_1(onPushMailboxRegisterQueryCompleted, IServicePushMailboxRegisterQueryPtr)
ZS_DECLARE_PROXY_END()
