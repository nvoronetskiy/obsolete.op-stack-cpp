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
#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/IMessageSource.h>
#include <openpeer/stack/IServiceNamespaceGrant.h>

#include <openpeer/stack/IServiceSalt.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

#include <list>

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
      #pragma mark IServiceNamespaceGrantSessionForServices
      #pragma mark

      interaction IServiceNamespaceGrantSessionForServices
      {
        ZS_DECLARE_TYPEDEF_PTR(IServiceNamespaceGrantSessionForServices, ForServices)

        typedef IServiceNamespaceGrantSession::SessionStates SessionStates;

        virtual PUID getID() const = 0;

        virtual String getGrantID() const = 0;

        virtual IServiceNamespaceGrantSessionWaitPtr obtainWaitToProceed(
                                                                         IServiceNamespaceGrantSessionWaitDelegatePtr waitForWaitUponFailingToObtainDelegate = IServiceNamespaceGrantSessionWaitDelegatePtr()
                                                                         ) = 0;  // returns IServiceNamespaceGrantSessionWaitPtr() (i.e. NULL) if not obtain to wait at this time

        virtual IServiceNamespaceGrantSessionQueryPtr query(
                                                            IServiceNamespaceGrantSessionQueryDelegatePtr delegate,
                                                            const NamespaceGrantChallengeInfo &challengeInfo
                                                            ) = 0;

        virtual bool isNamespaceURLInNamespaceGrantChallengeBundle(
                                                                   ElementPtr bundle,
                                                                   const char *namespaceURL
                                                                   ) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionWait
      #pragma mark

      interaction IServiceNamespaceGrantSessionWait
      {
        virtual PUID getID() const = 0;

        virtual void cancel() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionWaitDelegate
      #pragma mark

      interaction IServiceNamespaceGrantSessionWaitDelegate
      {
        virtual void onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionQuery
      #pragma mark

      interaction IServiceNamespaceGrantSessionQuery
      {
        virtual PUID getID() const = 0;

        virtual void cancel() = 0;

        virtual bool isComplete() const = 0;
        virtual ElementPtr getNamespaceGrantChallengeBundle() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionQueryDelegate
      #pragma mark

      interaction IServiceNamespaceGrantSessionQueryDelegate
      {
        virtual void onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                            IServiceNamespaceGrantSessionQueryPtr query,
                                                                            ElementPtr namespaceGrantChallengeBundleEl
                                                                            ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceNamespaceGrantSession
      #pragma mark

      class ServiceNamespaceGrantSession : public Noop,
                                           public zsLib::MessageQueueAssociator,
                                           public SharedRecursiveLock,
                                           public IServiceNamespaceGrantSession,
                                           public IMessageSource,
                                           public IServiceNamespaceGrantSessionForServices,
                                           public IWakeDelegate,
                                           public IBootstrappedNetworkDelegate
      {
      public:
        friend interaction IServiceNamespaceGrantSessionFactory;
        friend interaction IServiceNamespaceGrantSession;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, UseBootstrappedNetwork)

        typedef IServiceNamespaceGrantSession::SessionStates SessionStates;

        ZS_DECLARE_CLASS_PTR(Wait)
        ZS_DECLARE_CLASS_PTR(Query)

        friend class Wait;
        friend class Query;

        typedef PUID QueryID;
        typedef std::map<QueryID, QueryPtr> QueryMap;

        typedef std::pair<DocumentPtr, MessagePtr> DocumentMessagePair;
        typedef std::list<DocumentMessagePair> DocumentList;

        typedef std::list<IServiceNamespaceGrantSessionWaitDelegatePtr> WaitingDelegateList;

      protected:
        ServiceNamespaceGrantSession(
                                     IMessageQueuePtr queue,
                                     IServiceNamespaceGrantSessionDelegatePtr delegate,
                                     const char *outerFrameURLUponReload,
                                     const char *grantID
                                     );

        ServiceNamespaceGrantSession(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~ServiceNamespaceGrantSession();

        static ServiceNamespaceGrantSessionPtr convert(IServiceNamespaceGrantSessionPtr session);
        static ServiceNamespaceGrantSessionPtr convert(ForServicesPtr session);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => IServiceNamespaceGrantSession
        #pragma mark

        static ElementPtr toDebug(IServiceNamespaceGrantSessionPtr session);

        static ServiceNamespaceGrantSessionPtr create(
                                                      IServiceNamespaceGrantSessionDelegatePtr delegate,
                                                      const char *outerFrameURLUponReload,
                                                      const char *grantID
                                                      );

        virtual PUID getID() const {return mID;}

        virtual SessionStates getState(
                                       WORD *lastErrorCode,
                                       String *lastErrorReason
                                       ) const;

        virtual String getGrantID() const;

        virtual String getInnerBrowserWindowFrameURL() const;
        virtual String getBrowserWindowRedirectURL() const;

        virtual void notifyBrowserWindowVisible();
        virtual void notifyBrowserWindowRedirected();
        virtual void notifyBrowserWindowClosed();

        virtual DocumentPtr getNextMessageForInnerBrowerWindowFrame();
        virtual void handleMessageFromInnerBrowserWindowFrame(DocumentPtr unparsedMessage);

        virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => IMessageSource
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => IServiceNamespaceGrantSessionForServices
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual String getGrantID() const;

        virtual IServiceNamespaceGrantSessionWaitPtr obtainWaitToProceed(IServiceNamespaceGrantSessionWaitDelegatePtr waitForWaitUponFailingToObtainDelegate = IServiceNamespaceGrantSessionWaitDelegatePtr());

        virtual IServiceNamespaceGrantSessionQueryPtr query(
                                                            IServiceNamespaceGrantSessionQueryDelegatePtr delegate,
                                                            const NamespaceGrantChallengeInfo &challengeInfo
                                                            );

        virtual bool isNamespaceURLInNamespaceGrantChallengeBundle(
                                                                   ElementPtr bundle,
                                                                   const char *namespaceURL
                                                                   ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => friend class Query
        #pragma mark

        void notifyQueryGone(PUID queryID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => friend class Wait
        #pragma mark

        void notifyWaitGone(PUID waitID);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        void step();
        bool stepWaitForServices();
        bool stepPrepareQueries();
        bool stepBootstrapper();
        bool stepLoadGrantWindow();
        bool stepMakeGrantWindowVisible();
        bool stepBrowserRedirect();
        bool stepSendNamespaceGrantStartNotification();
        bool stepWaitForPermission();
        bool stepCloseBrowserWindow();
        bool stepExpiresCheck();

        bool stepRestart();

        void postStep();

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *reason = NULL);
        void sendInnerWindowMessage(MessagePtr message);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession::Wait
        #pragma mark

      public:
        class Wait : public SharedRecursiveLock,
                     public IServiceNamespaceGrantSessionWait
        {
        public:
          friend class ServiceNamespaceGrantSession;

        protected:
          Wait(ServiceNamespaceGrantSessionPtr outer);

        public:
          ~Wait();

        protected:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ServiceNamespaceGrantSession::Wait => IServiceNamespaceGrantSessionWait
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual void cancel();

        protected:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ServiceNamespaceGrantSession::Wait => friend class ServiceNamespaceGrantSession
          #pragma mark

          static WaitPtr create(ServiceNamespaceGrantSessionPtr outer);

          // (duplicate) virtual PUID getID() const;

        public:
          AutoPUID mID;
          ServiceNamespaceGrantSessionPtr mOuter;
        };

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession::Query
        #pragma mark

      public:
        class Query : public SharedRecursiveLock,
                      public IServiceNamespaceGrantSessionQuery
        {
        public:
          friend class ServiceNamespaceGrantSession;

        protected:
          Query(
                ServiceNamespaceGrantSessionPtr outer,
                IServiceNamespaceGrantSessionQueryDelegatePtr delegate,
                const NamespaceGrantChallengeInfo &challengeInfo
                );

        public:
          ~Query();

        protected:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ServiceNamespaceGrantSession::Query => IServiceNamespaceGrantSessionQuery
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual void cancel();

          virtual bool isComplete() const;
          virtual ElementPtr getNamespaceGrantChallengeBundle() const;

        protected:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ServiceNamespaceGrantSession::Query => friend class ServiceNamespaceGrantSession
          #pragma mark

          static QueryPtr create(
                                 ServiceNamespaceGrantSessionPtr outer,
                                 IServiceNamespaceGrantSessionQueryDelegatePtr delegate,
                                 const NamespaceGrantChallengeInfo &challengeInfo
                                 );

          // (duplicate) virtual PUID getID() const;

          const NamespaceGrantChallengeInfo &getChallengeInfo() const {return mChallengeInfo;}

          void notifyComplete(ElementPtr bundleEl);

        protected:
          //---------------------------------------------------------------------
          #pragma mark
          #pragma mark ServiceNamespaceGrantSession::Query => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

        public:
          AutoPUID mID;
          QueryWeakPtr mThisWeak;
          ServiceNamespaceGrantSessionWeakPtr mOuter;

          IServiceNamespaceGrantSessionQueryDelegatePtr mDelegate;

          NamespaceGrantChallengeInfo mChallengeInfo;

          ElementPtr mNamespaceGrantChallengeBundleEl;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServiceNamespaceGrantSession => (data)
        #pragma mark

        AutoPUID mID;
        ServiceNamespaceGrantSessionWeakPtr mThisWeak;

        IServiceNamespaceGrantSessionDelegatePtr mDelegate;

        UseBootstrappedNetworkPtr mBootstrappedNetwork;

        IMessageMonitorPtr mNamespaceGrantValidateMonitor;

        SessionStates mCurrentState;

        WORD mLastError;
        String mLastErrorReason;

        String mOuterFrameURLUponReload;

        String mGrantID;

        bool mBrowserWindowReady;
        bool mBrowserWindowVisible;
        bool mBrowserWindowClosed;

        bool mNeedsBrowserWindowVisible;
        String mNeedsBrowserWindowRedirectURL;

        bool mNamespaceGrantStartNotificationSent;
        bool mReceivedNamespaceGrantCompleteNotify;

        DocumentList mPendingMessagesToDeliver;

        ULONG mTotalWaits;

        QueryMap mQueriesInProcess;
        QueryMap mPendingQueries;

        WaitingDelegateList mWaitingDelegates;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionFactory
      #pragma mark

      interaction IServiceNamespaceGrantSessionFactory
      {
        static IServiceNamespaceGrantSessionFactory &singleton();

        virtual ServiceNamespaceGrantSessionPtr create(
                                                       IServiceNamespaceGrantSessionDelegatePtr delegate,
                                                       const char *outerFrameURLUponReload,
                                                       const char *grantID
                                                       );
      };
      
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::internal::IServiceNamespaceGrantSessionWaitDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServiceNamespaceGrantSessionPtr, IServiceNamespaceGrantSessionPtr)
ZS_DECLARE_PROXY_METHOD_1(onServiceNamespaceGrantSessionForServicesWaitComplete, IServiceNamespaceGrantSessionPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::internal::IServiceNamespaceGrantSessionQueryDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::internal::IServiceNamespaceGrantSessionQueryPtr, IServiceNamespaceGrantSessionQueryPtr)
ZS_DECLARE_PROXY_TYPEDEF(zsLib::XML::ElementPtr, ElementPtr)
ZS_DECLARE_PROXY_METHOD_2(onServiceNamespaceGrantSessionForServicesQueryComplete, IServiceNamespaceGrantSessionQueryPtr, ElementPtr)
ZS_DECLARE_PROXY_END()
