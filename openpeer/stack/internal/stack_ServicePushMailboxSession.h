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

#include <openpeer/stack/internal/types.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePushMailbox.h>
#include <openpeer/stack/IServiceNamespaceGrant.h>

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
#include <openpeer/services/ITCPMessaging.h>
#include <openpeer/services/ITransportStream.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

#define OPENPEER_STACK_SETTING_BACKGROUNDING_PUSH_MAILBOX_PHASE "openpeer/stack/backgrounding-phase-push-mailbox"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_TOTAL_SERVERS_TO_GET "openpeer/stack/push-mailbox-total-servers-to-get"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_SERVERS_GET_TIMEOUT_IN_SECONDS "openpeer/stack/push-mailbox-servers-get-timeout-in-seconds"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_ACCESS_TIMEOUT_IN_SECONDS "openpeer/stack/push-mailbox-access-timeout-in-seconds"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_NAMESPACE_GRANT_TIMEOUT_IN_SECONDS "openpeer/stack/push-mailbox-namespace-grant-timeout-in-seconds"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_PEER_VALIDATE_TIMEOUT_IN_SECONDS "openpeer/stack/push-mailbox-peer-validate-timeout-in-seconds"

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT "openpeer/stack/push-mailbox-inactivity-timeout"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS "openpeer/stack/push-mailbox-retry-connection-in-seconds"
#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_RETRY_CONNECTION_IN_SECONDS "openpeer/stack/push-mailbox-max-retry-connection-in-seconds"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForServices;

      ZS_DECLARE_USING_PTR(openpeer::services, IDNSDelegate)
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
                                        public ITimerDelegate,
                                        public ITCPMessagingDelegate,
                                        public ITransportStreamReaderDelegate,
                                        public IDNSDelegate,
                                        public IWakeDelegate,
                                        public IBackgroundingDelegate,
                                        public IBootstrappedNetworkDelegate,
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
                                        public IMessageMonitorResultDelegate<message::push_mailbox::ListFetchResult>,
                                        public IMessageMonitorResultDelegate<message::push_mailbox::RegisterPushResult>
      {
      public:
        friend interaction IServicePushMailboxSessionFactory;
        friend interaction IServicePushMailboxSession;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, UseBootstrappedNetwork)
        ZS_DECLARE_TYPEDEF_PTR(IServiceNamespaceGrantSessionForServices, UseServiceNamespaceGrantSession)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForServicePushMailbox, UseServiceLockboxSession)

        typedef IServicePushMailboxSession::SessionStates SessionStates;

        ZS_DECLARE_TYPEDEF_PTR(services::IDNS::SRVResult, SRVResult)

        ZS_DECLARE_TYPEDEF_PTR(IMessageMonitorManagerForPushMailbox, UseMessageMonitorManager)

        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::AccessResult, AccessResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::NamespaceGrantChallengeValidateResult, NamespaceGrantChallengeValidateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::PeerValidateResult, PeerValidateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FoldersGetResult, FoldersGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FolderUpdateResult, FolderUpdateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::FolderGetResult, FolderGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessagesDataGetResult, MessagesDataGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessagesMetaDataGetResult, MessagesMetaDataGetResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::MessageUpdateResult, MessageUpdateResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::ListFetchResult, ListFetchResult)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::ChangedNotify, ChangedNotify)
        ZS_DECLARE_TYPEDEF_PTR(message::push_mailbox::RegisterPushResult, RegisterPushResult)
        ZS_DECLARE_TYPEDEF_PTR(message::bootstrapped_servers::ServersGetResult, ServersGetResult)

      protected:
        ServicePushMailboxSession(
                                  IMessageQueuePtr queue,
                                  BootstrappedNetworkPtr network,
                                  IServicePushMailboxSessionDelegatePtr delegate,
                                  IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
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
                                                                   const char *deviceToken,
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
                                                            const PeerOrIdentityList &to,
                                                            const PeerOrIdentityList &cc,
                                                            const PeerOrIdentityList &bcc,
                                                            const PushMessage &message,
                                                            bool copyToSentFolder = true
                                                            );

        virtual void recheckNow();

        virtual void markPushMessageRead(const char *messageID);
        virtual void deletePushMessage(const char *messageID);

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

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<RegisterPushResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        RegisterPushResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             RegisterPushResultPtr ignore,          // will always be NULL
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
        bool stepAccess();

        bool stepGrantLockClear();
        bool stepPeerValidate();
        bool stepGrantChallenge();

        bool stepBackgroundingReady();

        void postStep();

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);

        void cancel();
        void connectionFailure();
        void connectionReset();

        bool send(MessagePtr message) const;
        IMessageMonitorPtr sendRequest(
                                       IMessageMonitorDelegatePtr delegate,
                                       MessagePtr requestMessage,
                                       Duration timeout
                                       );

        virtual void handleChanged(ChangedNotifyPtr notify);

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

        UseBootstrappedNetworkPtr mBootstrappedNetwork;

        IBackgroundingSubscriptionPtr mBackgroundingSubscription;
        IBackgroundingNotifierPtr mBackgroundingNotifier;
        AutoBool mBackgroundingEnabled;

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

        String mPeerChallengeID;

        NamespaceGrantChallengeInfo mNamespaceGrantChallengeInfo;

        IMessageMonitorPtr mServersGetMonitor;
        IMessageMonitorPtr mAccessMonitor;
        IMessageMonitorPtr mGrantMonitor;
        IMessageMonitorPtr mPeerValidateMonitor;
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
                                                    IServiceNamespaceGrantSessionPtr grantSession,
                                                    IServiceLockboxSessionPtr lockboxSession
                                                    );
      };
      
    }
  }
}
