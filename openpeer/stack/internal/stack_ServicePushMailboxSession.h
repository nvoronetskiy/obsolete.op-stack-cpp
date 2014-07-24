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

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
#include <openpeer/stack/IServiceNamespaceGrant.h>

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_ServiceNamespaceGrantSession.h>

#include <openpeer/stack/message/bootstrapped-servers/ServersGetResult.h>
#include <openpeer/stack/message/push-mailbox/AccessResult.h>
#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateResult.h>
#include <openpeer/stack/message/push-mailbox/PeerValidateResult.h>
#include <openpeer/stack/message/push-mailbox/FoldersGetResult.h>
#include <openpeer/stack/message/push-mailbox/FolderUpdateResult.h>
#include <openpeer/stack/message/push-mailbox/FolderGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessagesDataGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessagesMetaDataGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessageUpdateResult.h>
#include <openpeer/stack/message/push-mailbox/ListFetchResult.h>
#include <openpeer/stack/message/push-mailbox/RegisterPushResult.h>

#include <openpeer/services/IBackgrounding.h>
#include <openpeer/services/IDNS.h>
#include <openpeer/services/IReachability.h>
#include <openpeer/services/ITCPMessaging.h>
#include <openpeer/services/ITransportStream.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

#define OPENPEER_STACK_SETTING_BACKGROUNDING_PUSH_MAILBOX_PHASE "openpeer/stack/backgrounding-phase-push-mailbox"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_TOTAL_SERVERS_TO_GET "openpeer/stack/push-mailbox-total-servers-to-get"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS "openpeer/stack/push-mailbox-request-timeout-in-seconds"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT "openpeer/stack/push-mailbox-inactivity-timeout"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS "openpeer/stack/push-mailbox-retry-connection-in-seconds"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_RETRY_CONNECTION_IN_SECONDS "openpeer/stack/push-mailbox-max-retry-connection-in-seconds"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_DEFAULT_DH_KEY_DOMAIN "openpeer/stack/push-mailbox-default-dh-key-domain"

#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE "http://meta.openpeer.org/2014/06/17/json-push-mailbox-key#aes-cfb-32-16-16-sha1-md5"

#define OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_ACTIVE_UNTIL_IN_SECONDS   "openpeer/stack/push-mailbox-sending-key-active-until-in-seconds"
#define OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_EXPIRES_AFTER_IN_SECONDS  "openpeer/stack/push-mailbox-sending-key-expires-after-in-seconds"

#define OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS     "openpeer/stack/push-mailbox-max-message-upload-time-in-seconds"
#define OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_CHUNK_SIZE_IN_BYTES "openpeer/stack/push-mailbox-max-message-upload-chunk-size-in-bytes"



#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_PKI       "pki"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT "agreement"

#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_REQUEST_OFFER           "request-offer"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_OFFER_REQUEST_EXISTING  "offer"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_ANSWER                  "answer"

#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE "keying"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME "key:"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForServices;

      ZS_DECLARE_USING_PTR(openpeer::services, IDNSDelegate)
      ZS_DECLARE_USING_PTR(openpeer::services, IReachabilityDelegate)
      ZS_DECLARE_USING_PTR(openpeer::services, ITCPMessagingDelegate)
      ZS_DECLARE_USING_PTR(openpeer::services, ITransportStreamReaderDelegate)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession
      #pragma mark

      class ServicePushMailboxSession : public Noop,
                                        public zsLib::MessageQueueAssociator,
                                        public SharedRecursiveLock,
                                        public IServicePushMailboxSession,
                                        public IMessageSource,
                                        public ITimerDelegate,
                                        public ITCPMessagingDelegate,
                                        public ITransportStreamReaderDelegate,
                                        public ITransportStreamWriterDelegate,
                                        public IDNSDelegate,
                                        public IWakeDelegate,
                                        public IBackgroundingDelegate,
                                        public IReachabilityDelegate,
                                        public IBootstrappedNetworkDelegate,
                                        public IServiceLockboxSessionForInternalDelegate,
                                        public IServiceNamespaceGrantSessionWaitDelegate,
                                        public IServiceNamespaceGrantSessionQueryDelegate,
                                        public IMessageMonitorResultDelegate<message::bootstrapped_servers::ServersGetResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::AccessResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::NamespaceGrantChallengeValidateResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::PeerValidateResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::FoldersGetResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::FolderUpdateResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::FolderGetResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::MessagesDataGetResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::MessagesMetaDataGetResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::MessageUpdateResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::ListFetchResult>
      {
      public:
        friend interaction IServicePushMailboxSessionFactory;
        friend interaction IServicePushMailboxSession;

        ZS_DECLARE_CLASS_PTR(RegisterQuery)
        ZS_DECLARE_TYPEDEF_PTR(std::list<RegisterQueryPtr>, RegisterQueryList)

        typedef String MessageID;

        ZS_DECLARE_CLASS_PTR(SendQuery)
        typedef std::map<MessageID, SendQueryWeakPtr> SendQueryMap;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForServicePushMailbox, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, UseBootstrappedNetwork)
        ZS_DECLARE_TYPEDEF_PTR(IServiceNamespaceGrantSessionForServices, UseServiceNamespaceGrantSession)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForServicePushMailbox, UseServiceLockboxSession)

        typedef IServicePushMailboxSession::SessionStates SessionStates;

        ZS_DECLARE_TYPEDEF_PTR(services::IDNS::SRVResult, SRVResult)

        ZS_DECLARE_TYPEDEF_PTR(IMessageMonitorManagerForPushMailbox, UseMessageMonitorManager)

        ZS_DECLARE_TYPEDEF_PTR(services::IReachability, IReachability)
        ZS_DECLARE_TYPEDEF_PTR(services::IReachabilitySubscription, IReachabilitySubscription)
        ZS_DECLARE_TYPEDEF_PTR(services::IReachability::InterfaceTypes, InterfaceTypes)

        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::AccessResult, AccessResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::NamespaceGrantChallengeValidateResult, NamespaceGrantChallengeValidateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::PeerValidateResult, PeerValidateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FoldersGetResult, FoldersGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FolderUpdateResult, FolderUpdateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FolderGetResult, FolderGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessagesDataGetResult, MessagesDataGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessagesMetaDataGetResult, MessagesMetaDataGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessageUpdateRequest, MessageUpdateRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessageUpdateResult, MessageUpdateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::ListFetchRequest, ListFetchRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::ListFetchResult, ListFetchResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::ChangedNotify, ChangedNotify)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::RegisterPushRequest, RegisterPushRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::RegisterPushResult, RegisterPushResult)
        ZS_DECLARE_TYPEDEF_PTR(message::bootstrapped_servers::ServersGetResult, ServersGetResult)

        typedef String FolderName;
        typedef std::map<FolderName, FolderName> FolderNameMap;

        struct ProcessedFolderNeedingUpdateInfo
        {
          typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderNeedingUpdateInfo FolderNeedingUpdateInfo;

          FolderNeedingUpdateInfo mInfo;
          bool mSentRequest;
        };

        typedef std::map<FolderName, ProcessedFolderNeedingUpdateInfo> FolderUpdateMap;

        typedef std::list<IMessageMonitorPtr> MonitorList;

        struct ProcessedMessageNeedingUpdateInfo
        {
          typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingUpdateInfo MessageNeedingUpdateInfo;

          MessageNeedingUpdateInfo mInfo;
          bool mSentRequest;
        };

        typedef std::map<MessageID, ProcessedMessageNeedingUpdateInfo> MessageUpdateMap;


        struct ProcessedMessageNeedingDataInfo
        {
          typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingDataInfo MessageNeedingDataInfo;

          MessageNeedingDataInfo mInfo;
          bool mSentRequest;

          DWORD  mChannelID;

          size_t mReceivedData;
          SecureByteBlockPtr mBuffer;
        };

        typedef std::map<MessageID, ProcessedMessageNeedingDataInfo> MessageDataMap;

        typedef DWORD ChannelID;
        typedef std::map<ChannelID, MessageID> ChannelToMessageMap;

        typedef std::pair<ChannelID, SecureByteBlockPtr> PendingChannelData;
        typedef std::list<PendingChannelData> PendingChannelDataList;

        struct ProcessedListsNeedingDownloadInfo
        {
          typedef IServicePushMailboxDatabaseAbstractionDelegate::ListsNeedingDownloadInfo ListsNeedingDownloadInfo;

          ListsNeedingDownloadInfo mInfo;
          bool mSentRequest;
        };
        typedef String ListID;
        typedef std::map<ListID, ProcessedListsNeedingDownloadInfo> ListDownloadMap;

        struct ProcessedPendingDeliveryMessageInfo
        {
          typedef IServicePushMailboxDatabaseAbstractionDelegate::PendingDeliveryMessageInfo PendingDeliveryMessageInfo;

          PendingDeliveryMessageInfo mInfo;

          PushMessageInfo mMessage;

          size_t mSent;
          SecureByteBlockPtr mData;
        };
        typedef std::map<MessageID, ProcessedPendingDeliveryMessageInfo> PendingDeliveryMap;

        struct KeyingBundle
        {
          String mReferencedKeyID;

          Time mExpires;

          String mType;
          String mMode;
          String mAgreement;
          String mSecret;

          IPeerPtr mValidationPeer;
        };

        typedef std::map<MessageID, MessageID> MessageMap;

      protected:
        ServicePushMailboxSession(
                                  IMessageQueuePtr queue,
                                  BootstrappedNetworkPtr network,
                                  IServicePushMailboxSessionDelegatePtr delegate,
                                  IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                  AccountPtr account,
                                  ServiceNamespaceGrantSessionPtr grantSession,
                                  ServiceLockboxSessionPtr lockboxSession
                                  );
        
        ServicePushMailboxSession(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create()) {}

        void init();

      public:
        ~ServicePushMailboxSession();

        static ServicePushMailboxSessionPtr convert(IServicePushMailboxSessionPtr session);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IServicePushMailboxSession
        #pragma mark

        static ElementPtr toDebug(IServicePushMailboxSessionPtr session);

        static ServicePushMailboxSessionPtr create(
                                                   IServicePushMailboxSessionDelegatePtr delegate,
                                                   IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                   IServicePushMailboxPtr servicePushMailbox,
                                                   IAccountPtr account,
                                                   IServiceNamespaceGrantSessionPtr grantSession,
                                                   IServiceLockboxSessionPtr lockboxSession
                                                   );

        virtual PUID getID() const;

        virtual IServicePushMailboxSessionSubscriptionPtr subscribe(IServicePushMailboxSessionDelegatePtr delegate);

        virtual IServicePushMailboxPtr getService() const;

        virtual SessionStates getState(
                                       WORD *lastErrorCode,
                                       String *lastErrorReason
                                       ) const;

        virtual void shutdown();

        virtual IServicePushMailboxRegisterQueryPtr registerDevice(
                                                                   IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                                   const char *deviceToken,
                                                                   const char *folder,
                                                                   Time expires,
                                                                   const char *mappedType,
                                                                   bool unreadBadge,
                                                                   const char *sound,
                                                                   const char *action,
                                                                   const char *launchImage,
                                                                   unsigned int priority
                                                                   );

        virtual void monitorFolder(const char *folderName);

        virtual bool getFolderMessageUpdates(
                                             const char *inFolder,
                                             String inLastVersionDownloaded,
                                             String &outUpdatedToVersion,
                                             PushMessageListPtr &outMessagesAdded,
                                             MessageIDListPtr &outMessagesRemoved
                                             );

        virtual IServicePushMailboxSendQueryPtr sendMessage(
                                                            IServicePushMailboxSendQueryDelegatePtr delegate,
                                                            const PushMessage &message,
                                                            const char *remoteFolder,
                                                            bool copyToSentFolder = true
                                                            );

        virtual void recheckNow();

        virtual void markPushMessageRead(const char *messageID);
        virtual void deletePushMessage(const char *messageID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => friend RegisterQuery
        #pragma mark

        virtual void notifyComplete(RegisterQuery &query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => friend SendQuery
        #pragma mark

        virtual void notifyComplete(SendQuery &query);
        virtual PushMessagePtr getPushMessage(const String &messageID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => ITCPMessagingDelegate
        #pragma mark

        virtual void onTCPMessagingStateChanged(
                                                ITCPMessagingPtr messaging,
                                                ITCPMessagingDelegate::SessionStates state
                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => ITransportStreamReaderDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IDNSDelegate
        #pragma mark

        virtual void onLookupCompleted(IDNSQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IBackgroundingDelegate
        #pragma mark

        virtual void onBackgroundingGoingToBackground(
                                                      IBackgroundingSubscriptionPtr subscription,
                                                      IBackgroundingNotifierPtr notifier
                                                      );

        virtual void onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription);

        virtual void onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription);

        virtual void onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IReachabilityDelegate
        #pragma mark

        virtual void onReachabilityChanged(
                                           IReachabilitySubscriptionPtr subscription,
                                           InterfaceTypes interfaceTypes
                                           );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IServiceLockboxSessionForInternalDelegate
        #pragma mark

        virtual void onServiceLockboxSessionStateChanged(ServiceLockboxSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IServiceNamespaceGrantSessionWaitDelegate
        #pragma mark

        virtual void onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => IServiceNamespaceGrantSessionQueryDelegate
        #pragma mark

        virtual void onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                            IServiceNamespaceGrantSessionQueryPtr query,
                                                                            ElementPtr namespaceGrantChallengeBundleEl
                                                                            );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<ServersGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ServersGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ServersGetResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<AccessResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        AccessResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             AccessResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<NamespaceGrantChallengeValidateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        NamespaceGrantChallengeValidateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             NamespaceGrantChallengeValidateResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerValidateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerValidateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerValidateResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<FoldersGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        FoldersGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             FoldersGetResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<FolderUpdateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        FolderUpdateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             FolderUpdateResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<FolderGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        FolderGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             FolderGetResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<MessagesDataGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        MessagesDataGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             MessagesDataGetResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<MessagesMetaDataGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        MessagesMetaDataGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             MessagesMetaDataGetResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<MessageUpdateResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        MessageUpdateResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             MessageUpdateResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<ListFetchResultPtr>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ListFetchResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ListFetchResultPtr ignore,          // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        bool isConnected() const {return SessionState_Connected == mCurrentState;}
        bool isShuttingDown() const {return SessionState_ShuttingDown == mCurrentState;}
        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        void step();
        bool stepBootstrapper();
        bool stepGrantLock();

        bool stepShouldConnect();
        bool stepDNS();
        bool stepLockboxAccess();

        bool stepConnect();
        bool stepRead();

        bool stepAccess();

        bool stepGrantLockClear();
        bool stepPeerValidate();
        bool stepGrantChallenge();

        bool stepFullyConnected();
        bool stepRegisterQueries();

        bool stepRefreshFolders();
        bool stepCheckFoldersNeedingUpdate();
        bool stepFolderGet();

        bool stepMakeKeysAndSentFolders();

        bool stepCheckMessagesNeedingUpdate();
        bool stepMessagesMetaDataGet();

        bool stepCheckMessagesNeedingData();
        bool stepMessagesDataGet();

        bool stepCheckListNeedingDownload();
        bool stepListFetch();

        bool stepProcessKeysFolder();
        bool stepDecryptMessages();

        bool stepPendingDelivery();
        bool stepPendingDeliveryUpdateRequest();

        bool stepPrepareVersionedFolderMessages();

        bool stepMarkReadOrRemove();

        bool stepExpireMessages();

        bool stepBackgroundingReady();

        void postStep();

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);

        void cancel(bool completed = false);
        void connectionFailure();
        void connectionReset();

        void notifyChangedFolder(const String &folderName);

        bool send(MessagePtr message) const;
        IMessageMonitorPtr sendRequest(
                                       IMessageMonitorDelegatePtr delegate,
                                       MessagePtr requestMessage,
                                       Duration timeout
                                       );
        bool sendRequest(MessagePtr requestMessage);

        virtual void handleChanged(ChangedNotifyPtr notify);
        virtual void handleListFetch(ListFetchRequestPtr request);
        virtual void handelUpdateMessageGone(
                                             MessageUpdateRequestPtr request,
                                             bool notFoundError
                                             );
        virtual void handleMessageGone(const String &messageID);

        virtual void handleChannelMessage(
                                          DWORD channel,
                                          SecureByteBlockPtr buffer
                                          );
        virtual void processChannelMessages();

        virtual String prepareMessageID(const String &inMessageID);

        bool areRegisterQueriesActive() const;
        bool isFolderListUpdating() const;
        bool areAnyFoldersUpdating() const;
        bool areAnyMessagesUpdating() const;
        bool areAnyMessagesDownloadingData() const;
        bool areAnyListsFetching() const;
        bool areKeysAndSentFolderReady() const;
        bool areAnyMessagesDecrypting() const;
        bool areAnyMessagesUploading() const;
        bool areAnyFoldersNeedingVersionedMessages() const;
        bool areAnyMessagesMarkReadOrDeleting() const;
        bool areAnyMessagesExpiring() const;

        bool extractKeyingBundle(
                                 SecureByteBlockPtr buffer,
                                 KeyingBundle &outBundle
                                 );

        ElementPtr createKeyingBundle(const KeyingBundle &inBundle);

        String extractRSA(const String &secret);

        bool prepareDHKeyDomain(
                                int keyDomain,
                                IDHKeyDomainPtr &outKeyDomain,
                                SecureByteBlockPtr &outStaticPrivateKey,
                                SecureByteBlockPtr &outStaticPublicKey
                                );

        bool prepareDHKeys(
                           int keyDomain,
                           const String &ephemeralPrivateKey,
                           const String &ephemeralPublicKey,
                           IDHPrivateKeyPtr &outPrivateKey,
                           IDHPublicKeyPtr &outPublicKey
                           );

        bool prepareNewDHKeys(
                              const char *inKeyDomainStr,
                              int &outKeyDomain,
                              String &outEphemeralPrivateKey,
                              String &outEphemeralPublicKey,
                              IDHPrivateKeyPtr &outPrivateKey,
                              IDHPublicKeyPtr &outPublicKey
                              );

        bool extractDHFromAgreement(
                                    const String &agreement,
                                    int &outKeyDomain,
                                    IDHPublicKeyPtr &outPublicKey
                                    );

        bool sendKeyingBundle(
                              const String &toURI,
                              const KeyingBundle &inBundle
                              );

        bool sendRequestOffer(
                              const String &toURI,
                              const String &keyID,
                              int inKeyDomain,
                              IDHPublicKeyPtr publicKey,
                              Time expires
                              );

        bool sendOfferRequestExisting(
                                      const String &toURI,
                                      const String &keyID,
                                      int inKeyDomain,
                                      IDHPublicKeyPtr publicKey,
                                      Time expires
                                      );

        bool sendPKI(
                     const String &toURI,
                     const String &keyID,
                     const String &passphrase,
                     IPeerPtr validationPeer,
                     Time expires
                     );

        bool sendAnswer(
                        const String &toURI,
                        const String &keyID,
                        const String &passphrase,
                        const String &remoteAgreement,
                        int inKeyDomain,
                        IDHPrivateKeyPtr privateKey,
                        IDHPublicKeyPtr publicKey,
                        Time expires
                        );

        String getAnswerPassphrase(
                                   int inKeyDomain,
                                   const String &ephemeralPrivateKey,
                                   const String &ephemeralPublicKey,
                                   const String &remoteAgreement,
                                   const String &secret
                                   );

        void addToMasterList(
                             PeerOrIdentityListPtr &ioMaster,
                             const PeerOrIdentityListPtr &source
                             );
        void addToMasterList(
                             PeerOrIdentityList &ioMaster,
                             const PeerOrIdentityList &source
                             );

        String convertToDatabaseList(
                                     const PeerOrIdentityListPtr &source,
                                     size_t &outListSize
                                     );
        String convertToDatabaseList(
                                     const PeerOrIdentityList &source,
                                     size_t &outListSize
                                     );

        PeerOrIdentityListPtr convertFromDatabaseList(const String &uri);

        void deliverNewSendingKey(
                                  const String &uri,
                                  PeerOrIdentityListPtr peerList,
                                  String &outKeyID,
                                  String &outInitialPassphrase,
                                  Time &outExpires
                                  );

        bool encryptMessage(
                            const String &inPassphraseID,
                            const String &inPassphrase,
                            SecureByteBlock &inOriginalData,
                            String &outEncodingScheme,
                            SecureByteBlockPtr &outEncryptedMessage
                            );

        bool decryptMessage(
                            const String &inPassphraseID,
                            const String &inPassphrase,
                            const String &inSalt,
                            const String &inProof,
                            SecureByteBlock &inOriginalData,
                            SecureByteBlockPtr &outDecryptedMessage
                            );

      public:
#define OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_REGISTER_QUERY
#include <openpeer/stack/internal/stack_ServicePushMailboxSession_RegisterQuery.h>
#undef OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_REGISTER_QUERY

#define OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_SEND_QUERY
#include <openpeer/stack/internal/stack_ServicePushMailboxSession_SendQuery.h>
#undef OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_SEND_QUERY

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => (data)
        #pragma mark

        AutoPUID mID;
        ServicePushMailboxSessionWeakPtr mThisWeak;
        ServicePushMailboxSessionPtr mGracefulShutdownReference;

        IServicePushMailboxSessionDelegateSubscriptions mSubscriptions;
        IServicePushMailboxSessionSubscriptionPtr mDefaultSubscription;

        IServicePushMailboxDatabaseAbstractionDelegatePtr mDB;

        SessionStates mCurrentState;

        AutoWORD mLastError;
        String mLastErrorReason;

        UseAccountWeakPtr mAccount;

        UseBootstrappedNetworkPtr mBootstrappedNetwork;

        IBackgroundingSubscriptionPtr mBackgroundingSubscription;
        IBackgroundingNotifierPtr mBackgroundingNotifier;
        AutoBool mBackgroundingEnabled;

        IReachabilitySubscriptionPtr mReachabilitySubscription;

        IServiceLockboxSessionForInternalSubscriptionPtr mLockboxSubscription;

        PUID mSentViaObjectID;

        ITCPMessagingPtr mTCPMessaging;
        ITransportStreamPtr mWireStream;

        ITransportStreamReaderPtr mReader;
        ITransportStreamWriterPtr mWriter;

        UseServiceLockboxSessionPtr mLockbox;
        LockboxInfo mLockboxInfo;
        IPeerFilesPtr mPeerFiles;

        UseServiceNamespaceGrantSessionPtr mGrantSession;
        IServiceNamespaceGrantSessionQueryPtr mGrantQuery;
        IServiceNamespaceGrantSessionWaitPtr mGrantWait;

        AutoBool mObtainedLock;

        bool mRequiresConnection;
        Duration mInactivityTimeout;
        Time mLastActivity;

        Duration mDefaultLastRetryDuration;
        Duration mLastRetryDuration;
        Time mDoNotRetryConnectionBefore;
        TimerPtr mRetryTimer;

        ServerList mPushMailboxServers;
        IDNSQueryPtr mServerLookup;
        SRVResultPtr mServerSRV;
        IPAddress mServerIP;

        String mPeerURIFromAccessResult;

        NamespaceGrantChallengeInfo mNamespaceGrantChallengeInfo;

        IMessageMonitorPtr mServersGetMonitor;
        IMessageMonitorPtr mAccessMonitor;
        IMessageMonitorPtr mGrantMonitor;
        IMessageMonitorPtr mPeerValidateMonitor;

        String mDeviceToken;
        RegisterQueryList mRegisterQueries;

        SendQueryMap mSendQueries;

        // folder list updating
        bool mRefreshFolders;
        FolderNameMap mMonitoredFolders;
        FolderNameMap mNotifiedMonitoredFolders;
        IMessageMonitorPtr mFoldersGetMonitor;

        TimerPtr mNextFoldersUpdateTimer;

        // folder updating
        bool mRefreshFoldersNeedingUpdate;
        FolderUpdateMap mFoldersNeedingUpdate;
        MonitorList mFolderGetMonitors;

        // keys and folder creation
        bool mRefreshKeysAndSentFolder;
        int mKeysFolderIndex;
        int mSentFolderIndex;
        IMessageMonitorPtr mKeyFolderUpdateMonitor;
        IMessageMonitorPtr mSentFolderUpdateMonitor;

        // messages needing update
        bool mRefreshMessagesNeedingUpdate;
        MessageUpdateMap mMessagesNeedingUpdate;
        IMessageMonitorPtr mMessageMetaDataGetMonitor;

        // messages needing data
        bool mRefreshMessagesNeedingData;
        MessageDataMap mMessagesNeedingData;
        ChannelToMessageMap mMessagesNeedingDataChannels;
        IMessageMonitorPtr mMessageDataGetMonitor;

        PendingChannelDataList mPendingChannelData;

        // list fetching
        bool mRefreshListsNeedingDownload;
        ListDownloadMap mListsNeedingDownload;
        IMessageMonitorPtr mListFetchMonitor;

        bool mRefreshKeysFolderNeedsProcessing;

        bool mRefreshMessagesNeedingDecryption;

        // pending delivery
        bool mRefreshPendingDelivery;
        size_t mDeliveryMaxChunkSize;
        ChannelID mLastChannel;
        bool mPendingDeliveryPrecheckRequired;
        PendingDeliveryMap mPendingDelivery;
        IMessageMonitorPtr mPendingDeliveryPrecheckMessageMetaDataGetMonitor;
        MessageUpdateRequestPtr mPendingDeliveryMessageUpdateRequest;
        IMessageMonitorPtr mPendingDeliveryMessageUpdateErrorMonitor;
        IMessageMonitorPtr mPendingDeliveryMessageUpdateUploadCompleteMonitor;

        bool mRefreshVersionedFolders;

        // mark read or delete message
        MessageMap mMessagesToMarkRead;
        MessageMap mMessagesToRemove;
        MonitorList mMarkingMonitors;

        bool mRefreshMessagesNeedingExpiry;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSessionFactory
      #pragma mark

      interaction IServicePushMailboxSessionFactory
      {
        static IServicePushMailboxSessionFactory &singleton();

        virtual ServicePushMailboxSessionPtr create(
                                                    IServicePushMailboxSessionDelegatePtr delegate,
                                                    IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                    IServicePushMailboxPtr servicePushMailbox,
                                                    IAccountPtr account,
                                                    IServiceNamespaceGrantSessionPtr grantSession,
                                                    IServiceLockboxSessionPtr lockboxSession
                                                    );
      };
      
    }
  }
}
