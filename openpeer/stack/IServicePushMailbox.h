/*

 Copyright (c) 2013, SMB Phone Inc.
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
        SessionState_PendingPeerFilesGeneration,
        SessionState_Ready,
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

      typedef String IdentityURI;

      ZS_DECLARE_TYPEDEF_PTR(std::list<IPeerPtr>, PeerList)
      ZS_DECLARE_TYPEDEF_PTR(std::list<IdentityURI>, IdentityList)

      struct PushDestination
      {
        PeerList mDeliverPeers;
        IdentityList mDeliverIdentities;
      };

      ZS_DECLARE_PTR(PushDestination)

      struct PushStatePeerDetail
      {
        IPeerPtr mPeer;

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
        String mMessageID;                // system will assign this value
        String mMessageVersion;           // system will assign this value

        String mMessageType;
        SecureByteBlockPtr mFullMessage;

        ValueList mValues;                // values related to mapped type
        ElementPtr mCustomPushData;       // extended push related custom data

        Time mSent;                       // when was the message sent
        Time mExpires;                    // optional, system will assign a long life time if not specified

        IPeerPtr mFrom;                   // the peer that sent the message

        PushDestinationPtr mTo;
        PushDestinationPtr mCC;
        PushDestinationPtr mBCC;

        PushStateDetailListPtr mPushDetails;   // detailed related state information about the push
      };

      ZS_DECLARE_PTR(PushMessage)

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushMessagePtr>, PushMessageList)

      static ElementPtr toDebug(IServicePushMailboxSessionPtr session);

      static IServicePushMailboxSessionPtr create(
                                                  IServicePushMailboxSessionDelegatePtr delegate,
                                                  IServicePushMailboxPtr servicePushMailbox,
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

      virtual void cancel() = 0;

      virtual void registerDevice(
                                  const char *deviceToken,
                                  const char *mappedType,   // for APNS maps to "loc-key"
                                  bool unreadBadge,         // true causes total unread messages to be displayed in badge
                                  const char *sound,        // what sound to play upon receiving a message. For APNS, maps to "sound" field
                                  const char *action,       // for APNS, maps to "action-loc-key"
                                  const char *launchImage,  // for APNS, maps to "launch-image"
                                  unsigned int priority     // for APNS, maps to push priority
                                  ) = 0;

      virtual void monitorFolder(const char *folderName) = 0;

      virtual PushMessageListPtr getFolderMessageUpdates(
                                                         const char *folder,
                                                         String &ioLastVersionDownloaded,   // pass in String() if no previous version known
                                                         bool &outFlushAllPreviousMessages  // returns true if all messages associated to the folder need to be flushed
                                                         ) = 0;

      virtual IServicePushMailboxSendQueryPtr sendMessage(
                                                          IServicePushMailboxSendQueryDelegatePtr delegate,
                                                          const PushDestination &to,
                                                          const PushDestination &cc,
                                                          const PushDestination &bcc,
                                                          const PushMessage &message,
                                                          bool copyToSentFolder = true
                                                          ) = 0;

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
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSession::PushStateDetailList, PushStateDetailList)

      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual PushMessagePtr getPushMessage() = 0;
      virtual PushStateDetailListPtr getPushStates() = 0;
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
