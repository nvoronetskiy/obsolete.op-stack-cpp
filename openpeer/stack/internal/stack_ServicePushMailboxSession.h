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
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_ServiceNamespaceGrantSession.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT "openpeer/stack/push-mailbox-inactivity-timeout"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForServices;

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
                                        public IWakeDelegate,
                                        public IBootstrappedNetworkDelegate,
                                        public IServiceNamespaceGrantSessionWaitDelegate,
                                        public IServiceNamespaceGrantSessionQueryDelegate
      {
      public:
        friend interaction IServicePushMailboxSessionFactory;
        friend interaction IServicePushMailboxSession;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, UseBootstrappedNetwork)
        ZS_DECLARE_TYPEDEF_PTR(IServiceNamespaceGrantSessionForServices, UseServiceNamespaceGrantSession)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForServicePushMailbox, UseServiceLockboxSession)

        typedef IServicePushMailboxSession::SessionStates SessionStates;

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

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        bool isShuttingDown() const {return SessionState_ShuttingDown == mCurrentState;}
        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        void step();
        bool stepBootstrapper();
        bool stepGrantLock();
        bool stepGrantLockClear();
        bool stepGrantChallenge();

        void postStep();

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);

        void cancel();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePushMailboxSession => (data)
        #pragma mark

        AutoPUID mID;
        ServicePushMailboxSessionWeakPtr mThisWeak;

        IServicePushMailboxSessionDelegateSubscriptions mSubscriptions;
        IServicePushMailboxSessionSubscriptionPtr mDefaultSubscription;

        IServicePushMailboxDatabaseAbstractionDelegatePtr mDB;

        SessionStates mCurrentState;

        AutoWORD mLastError;
        String mLastErrorReason;

        UseBootstrappedNetworkPtr mBootstrappedNetwork;
        UseServiceLockboxSessionPtr mLockbox;

        UseServiceNamespaceGrantSessionPtr mGrantSession;
        IServiceNamespaceGrantSessionQueryPtr mGrantQuery;
        IServiceNamespaceGrantSessionWaitPtr mGrantWait;

        AutoBool mObtainedLock;

        Duration mInactivityTimeout;
        Time mLastActivity;
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
