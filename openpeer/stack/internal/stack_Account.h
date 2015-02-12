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
#include <openpeer/stack/internal/stack_AccountFinder.h>
#include <openpeer/stack/internal/stack_AccountPeerLocation.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>

#include <openpeer/stack/message/peer-finder/PeerLocationFindResult.h>
#include <openpeer/stack/message/bootstrapped-servers/ServersGetResult.h>

#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerSubscription.h>
#include <openpeer/stack/IKeyGenerator.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/ITransportStream.h>
#include <openpeer/services/IBackgrounding.h>

#include <zsLib/MessageQueueAssociator.h>

#include <zsLib/Timer.h>

#include <map>

#define OPENPEER_STACK_SETTING_BACKGROUNDING_ACCOUNT_PHASE "openpeer/stack/backgrounding-phase-account"
#define OPENPEER_STACK_SETTING_FINDER_TOTAL_SERVERS_TO_GET "openpeer/stack/finder-total-servers-to-get"
#define OPENPEER_STACK_SETTING_ACCOUNT_SHUTDOWN_ON_ICE_SOCKET_FAILURE "openpeer/stack/account-shutdown-on-ice-socket-failure"

#define OPENPEER_STACK_SETTING_ACCOUNT_DEFAULT_KEY_DOMAIN "openpeer/stack/account-default-key-domain"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction ILocationForAccount;
      interaction ILocationSubscriptionForAccount;
      interaction IPeerForAccount;
      interaction IPeerSubscriptionForAccount;
      interaction IAccountFinderForAccount;
      interaction IAccountPeerLocationForAccount;
      interaction IBootstrappedNetworkForAccount;
      interaction IMessageIncomingForAccount;
      interaction IPublicationRepositoryForAccount;
      interaction IServiceLockboxSessionForAccount;

      ZS_DECLARE_USING_PTR(message::peer_finder, PeerLocationFindRequest)
      ZS_DECLARE_USING_PTR(message::peer_finder, PeerLocationFindResult)
      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetResult)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForAccountFinder
      #pragma mark

      interaction IAccountForAccountFinder
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForAccountFinder, ForAccountFinder)

        virtual String getDomain() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;

        virtual bool extractNextFinder(
                                       Server &outFinder,
                                       IPAddress &outFinderIP
                                       ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForFinderRelayChannel
      #pragma mark

      interaction IAccountForFinderRelayChannel
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForFinderRelayChannel, ForRelayChannel)

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForAccountPeerLocation
      #pragma mark

      interaction IAccountForAccountPeerLocation
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForAccountPeerLocation, ForAccountPeerLocation)

        virtual String getDomain() const = 0;

        virtual IICESocketPtr getSocket() const = 0;

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForLocation
      #pragma mark

      interaction IAccountForLocation
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocation, ForLocation)

        virtual LocationPtr findExisting(
                                         const String &peerURI,
                                         const String &locationID
                                         ) const = 0;
        virtual LocationPtr findExistingOrUse(LocationPtr location) = 0;
        virtual LocationPtr getLocationForLocal() const = 0;
        virtual LocationPtr getLocationForFinder() const = 0;
        virtual void notifyDestroyed(Location &location) = 0;

        virtual const String &getLocationID() const = 0;
        virtual PeerPtr getPeerForLocal() const = 0;

        virtual LocationInfoPtr getLocationInfo(LocationPtr location) const = 0;

        virtual ILocation::LocationConnectionStates getConnectionState(LocationPtr location) const = 0;

        virtual bool send(
                          LocationPtr location,
                          MessagePtr message,
                          PUID *outSentViaObjectID = NULL
                          ) const = 0;

        virtual void hintNowAvailable(LocationPtr location) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForLocation
      #pragma mark

      interaction IAccountForLocationDatabases
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocationDatabases, ForLocationDatabases)

        virtual IServiceLockboxSessionPtr getLockboxSession() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForLocationSubscription
      #pragma mark

      interaction IAccountForLocationSubscription
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocationSubscription, ForLocationSubscription)

        virtual void subscribe(LocationSubscriptionPtr subscription) = 0;
        virtual void notifyDestroyed(LocationSubscription &subscription) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForMessageIncoming
      #pragma mark

      interaction IAccountForMessageIncoming
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForMessageIncoming, ForMessageIncoming)

        virtual bool send(
                          LocationPtr location,
                          MessagePtr response,
                          PUID *outSentViaObjectID = NULL
                          ) const = 0;
        virtual void notifyMessageIncomingResponseNotSent(MessageIncoming &messageIncoming) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForMessages
      #pragma mark

      interaction IAccountForMessages
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForMessages, ForMessages)

        virtual IPeerFilesPtr getPeerFiles() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPeer
      #pragma mark

      interaction IAccountForPeer
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForPeer, ForPeer)

        virtual PeerPtr findExisting(const String &peerUR) const = 0;
        virtual PeerPtr findExistingOrUse(PeerPtr peer) = 0;
        virtual void notifyDestroyed(Peer &peer) = 0;

        virtual IPeer::PeerFindStates getPeerState(const String &peerURI) const = 0;
        virtual LocationListPtr getPeerLocations(
                                                 const String &peerURI,
                                                 bool includeOnlyConnectedLocations
                                                 ) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPeerSubscription
      #pragma mark

      interaction IAccountForPeerSubscription
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForPeerSubscription, ForPeerSubscription)

        virtual void subscribe(PeerSubscriptionPtr subscription) = 0;
        virtual void notifyDestroyed(PeerSubscription &subscription) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForPublicationRepository
      #pragma mark

      interaction IAccountForPublicationRepository
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForPublicationRepository, ForPublicationRepository)

        virtual PublicationRepositoryPtr getRepository() const = 0;

        virtual String getDomain() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForServicePushMailbox
      #pragma mark

      interaction IAccountForServicePushMailbox
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForServicePushMailbox, ForPushMailbox)

        virtual PUID getID() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForServiceLockboxSession
      #pragma mark

      interaction IAccountForServiceLockboxSession
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountForServiceLockboxSession, ForServiceLockboxSession)

        virtual IKeyGeneratorPtr takeOverRSAGeyGeneration() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account
      #pragma mark

      class Account : public Noop,
                      public MessageQueueAssociator,
                      public SharedRecursiveLock,
                      public IAccount,
                      public IAccountForAccountFinder,
                      public IAccountForFinderRelayChannel,
                      public IAccountForAccountPeerLocation,
                      public IAccountForLocation,
                      public IAccountForLocationDatabases,
                      public IAccountForLocationSubscription,
                      public IAccountForMessageIncoming,
                      public IAccountForMessages,
                      public IAccountForPeer,
                      public IAccountForPeerSubscription,
                      public IAccountForPublicationRepository,
                      public IAccountForServicePushMailbox,
                      public IAccountForServiceLockboxSession,
                      public IWakeDelegate,
                      public IAccountFinderDelegate,
                      public IAccountPeerLocationDelegate,
                      public IDNSDelegate,
                      public IICESocketDelegate,
                      public ITimerDelegate,
                      public IBackgroundingDelegate,
                      public IKeyGeneratorDelegate,
                      public IServiceLockboxSessionDelegate,
                      public IMessageMonitorResultDelegate<PeerLocationFindResult>,
                      public IMessageMonitorResultDelegate<ServersGetResult>
      {
      public:
        friend interaction IAccountFactory;
        friend interaction IAccount;

        ZS_DECLARE_TYPEDEF_PTR(ILocationForAccount, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(ILocationSubscriptionForAccount, UseLocationSubscription)
        ZS_DECLARE_TYPEDEF_PTR(IPeerForAccount, UsePeer)
        ZS_DECLARE_TYPEDEF_PTR(IPeerSubscriptionForAccount, UsePeerSubscription)
        ZS_DECLARE_TYPEDEF_PTR(IAccountFinderForAccount, UseAccountFinder)
        ZS_DECLARE_TYPEDEF_PTR(IAccountPeerLocationForAccount, UseAccountPeerLocation)
        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForAccount, UseBootstrappedNetwork)
        ZS_DECLARE_TYPEDEF_PTR(IMessageIncomingForAccount, UseMessageIncoming)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationRepositoryForAccount, UsePublicationRepository)
        ZS_DECLARE_TYPEDEF_PTR(IServiceLockboxSessionForAccount, UseServiceLockboxSession)

        ZS_DECLARE_STRUCT_PTR(PeerInfo)

        friend struct PeerInfo;

        typedef IAccount::AccountStates AccountStates;

        typedef ULONG ChannelNumber;

        typedef String PeerURI;
        typedef String LocationID;
        typedef std::pair<PeerURI, LocationID> PeerLocationIDPair;

        typedef std::map<PeerURI, UsePeerWeakPtr> PeerMap;
        typedef std::map<PeerURI, PeerInfoPtr> PeerInfoMap;

        typedef PUID PeerSubscriptionID;
        typedef std::map<PeerSubscriptionID, UsePeerSubscriptionWeakPtr> PeerSubscriptionMap;

        typedef PUID LocationSubscriptionID;
        typedef std::map<LocationSubscriptionID, UseLocationSubscriptionWeakPtr> LocationSubscriptionMap;

        typedef std::map<PeerLocationIDPair, UseLocationWeakPtr> LocationMap;

        typedef std::pair<IDHPrivateKeyPtr, IDHPublicKeyPtr> DHKeyPair;
        typedef std::map<IDHKeyDomain::KeyDomainPrecompiledTypes, DHKeyPair> DHKeyPairTemplates;

        typedef std::list<PeerLocationFindRequestPtr> IncomingFindRequestList;

      protected:
        Account(
                IMessageQueuePtr queue,
                IAccountDelegatePtr delegate,
                ServiceLockboxSessionPtr peerContactSession
                );
        
        Account(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~Account();

        static AccountPtr convert(IAccountPtr account);
        static AccountPtr convert(ForAccountFinderPtr account);
        static AccountPtr convert(ForRelayChannelPtr account);
        static AccountPtr convert(ForAccountPeerLocationPtr account);
        static AccountPtr convert(ForLocationPtr account);
        static AccountPtr convert(ForLocationDatabasesPtr account);
        static AccountPtr convert(ForLocationSubscriptionPtr account);
        static AccountPtr convert(ForMessagesPtr account);
        static AccountPtr convert(ForPeerPtr account);
        static AccountPtr convert(ForPeerSubscriptionPtr account);
        static AccountPtr convert(ForPublicationRepositoryPtr account);
        static AccountPtr convert(ForPushMailboxPtr account);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccount
        #pragma mark

        static ElementPtr toDebug(IAccountPtr account);

        static AccountPtr create(
                                 IAccountDelegatePtr delegate,
                                 IServiceLockboxSessionPtr peerContactSession
                                 );

        virtual PUID getID() const {return mID;}

        virtual AccountStates getState(
                                       WORD *outLastErrorCode = NULL,
                                       String *outLastErrorReason = NULL
                                       ) const;

        virtual IServiceLockboxSessionPtr getLockboxSession() const;

        virtual void getNATServers(
                                   IICESocket::TURNServerInfoList &outTURNServers,
                                   IICESocket::STUNServerInfoList &outSTUNServers
                                   ) const;

        virtual void shutdown();


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForAccountFinder
        #pragma mark

        virtual String getDomain() const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        virtual bool extractNextFinder(
                                       Server &outFinder,
                                       IPAddress &outFinderIP
                                       );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForFinderRelayChannel
        #pragma mark

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForAccountPeerLocation
        #pragma mark

        // (duplicate) virtual String getDomain() const;

        virtual IICESocketPtr getSocket() const;

        // (duplicate) virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForLocation
        #pragma mark

        virtual LocationPtr findExisting(
                                         const String &peerURI,
                                         const String &locationID
                                         ) const;
        virtual LocationPtr findExistingOrUse(LocationPtr location);
        virtual LocationPtr getLocationForLocal() const;
        virtual LocationPtr getLocationForFinder() const;
        virtual void notifyDestroyed(Location &location);

        virtual const String &getLocationID() const;
        virtual PeerPtr getPeerForLocal() const;

        virtual LocationInfoPtr getLocationInfo(LocationPtr location) const;

        virtual ILocation::LocationConnectionStates getConnectionState(LocationPtr location) const;

        virtual bool send(
                          LocationPtr location,
                          MessagePtr message,
                          PUID *outSentViaObjectID = NULL
                          ) const;

        virtual void hintNowAvailable(LocationPtr location);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForLocationDatabases
        #pragma mark

        // (duplicate) virtual IServiceLockboxSessionPtr getLockboxSession() const = 0;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForLocationSubscription
        #pragma mark

        virtual void subscribe(LocationSubscriptionPtr subscription);
        virtual void notifyDestroyed(LocationSubscription &subscription);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForMessageIncoming
        #pragma mark

        // (duplicate) virtual bool send(
        //                              LocationPtr location,
        //                              MessagePtr response,
        //                              PUID *outSentViaObjectID = NULL
        //                              ) const;
        virtual void notifyMessageIncomingResponseNotSent(MessageIncoming &messageIncoming);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForMessages
        #pragma mark

        virtual IPeerFilesPtr getPeerFiles() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPeer
        #pragma mark

        virtual PeerPtr findExisting(const String &peerUR) const;
        virtual PeerPtr findExistingOrUse(PeerPtr peer);
        virtual void notifyDestroyed(Peer &peer);

        virtual IPeer::PeerFindStates getPeerState(const String &peerURI) const;
        virtual LocationListPtr getPeerLocations(
                                                 const String &peerURI,
                                                 bool includeOnlyConnectedLocations
                                                 ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPeerSubscription
        #pragma mark

        virtual void subscribe(PeerSubscriptionPtr subscription);
        virtual void notifyDestroyed(PeerSubscription &subscription);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForPublicationRepository
        #pragma mark

        virtual PublicationRepositoryPtr getRepository() const;

        // (duplicate) virtual String getDomain() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForServicePushMailbox
        #pragma mark

        // (duplicate) virtual PUID getID() const = 0;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountForServiceLockboxSession
        #pragma mark

        virtual IKeyGeneratorPtr takeOverRSAGeyGeneration();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountFinderDelegate
        #pragma mark

        virtual void onAccountFinderStateChanged(
                                                 AccountFinderPtr finder,
                                                 AccountStates state
                                                 );

        virtual void onAccountFinderMessageIncoming(
                                                    AccountFinderPtr peerLocation,
                                                    MessagePtr message
                                                    );

        virtual void onAccountFinderIncomingRelayChannel(
                                                         AccountFinderPtr finder,
                                                         IFinderRelayChannelPtr relayChannel,
                                                         ITransportStreamPtr receiveStream,
                                                         ITransportStreamPtr sendStream,
                                                         ChannelNumber channelNumber
                                                         );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IAccountPeerLocationDelegate
        #pragma mark

        virtual void onAccountPeerLocationStateChanged(
                                                       AccountPeerLocationPtr peerLocation,
                                                       AccountStates state
                                                       );

        virtual void onAccountPeerLocationMessageIncoming(
                                                          AccountPeerLocationPtr peerLocation,
                                                          MessagePtr message
                                                          );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IDNSDelegate
        #pragma mark

        virtual void onLookupCompleted(IDNSQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IICESocketDelegate
        #pragma mark

        virtual void onICESocketStateChanged(
                                             IICESocketPtr socket,
                                             ICESocketStates state
                                             );

        virtual void onICESocketCandidatesChanged(IICESocketPtr socket);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerLocationFindResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerLocationFindResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerLocationFindResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<FindersGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ServersGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ServersGetResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IBackgroundingDelegate
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
        #pragma mark Account => IKeyGeneratorDelegate
        #pragma mark

        virtual void onKeyGenerated(IKeyGeneratorPtr generator);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => IServiceLockboxSessionDelegate
        #pragma mark

        virtual void onServiceLockboxSessionStateChanged(
                                                         IServiceLockboxSessionPtr session,
                                                         IServiceLockboxSession::SessionStates state
                                                         );

        virtual void onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session);

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (internal)
        #pragma mark

        bool isPending() const      {return AccountState_Pending == mCurrentState;}
        bool isReady() const        {return AccountState_Ready == mCurrentState;}
        bool isShuttingDown() const {return AccountState_ShuttingDown ==  mCurrentState;}
        bool isShutdown() const     {return AccountState_Shutdown ==  mCurrentState;}

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();
        void step();
        bool stepTimer();
        bool stepRepository();
        bool stepRSAKeyGeneration();
        bool stepDefaultDHKeyTemplate();
        bool stepLockboxSession();
        bool stepLocations();
        bool stepSocket();
        bool stepRespondToPeerLocationFindReqests();
        bool stepFinderDNS();
        bool stepFinder();
        bool stepPeers();
        bool stepFinderBackgrounding();

        void setState(AccountStates accountState);
        void setError(WORD errorCode, const char *reason = NULL);

        virtual CandidatePtr getRelayCandidate(const String &localContext) const;

        DHKeyPair getDHKeyPairTemplate(IDHKeyDomain::KeyDomainPrecompiledTypes type);

        void setFindState(
                          PeerInfo &peerInfo,
                          IPeer::PeerFindStates state
                          );

        bool shouldFind(
                        const String &peerURI,
                        const PeerInfoPtr &peerInfo
                        ) const;

        void shutdownAllLocationsDueToBackgrounding(
                                                    const String &contactID,
                                                    PeerInfoPtr &peer
                                                    );

        bool shouldShutdownInactiveLocations(
                                             const String &contactID,
                                             const PeerInfoPtr &peer
                                             ) const;

        void shutdownPeerLocationsNotNeeded(
                                            const String &peerURI,
                                            PeerInfoPtr &peerInfo
                                            );

        void sendPeerKeepAlives(
                                const String &peerURI,
                                PeerInfoPtr &peerInfo
                                );
        void performPeerFind(
                             const String &peerURI,
                             PeerInfoPtr &peerInfo
                             );

        void handleFindRequestComplete(IMessageMonitorPtr requester);

        void handleFinderRelatedFailure();

        void notifySubscriptions(
                                 UseLocationPtr location,
                                 ILocation::LocationConnectionStates state
                                 );

        void notifySubscriptions(
                                 UsePeerPtr peer,
                                 IPeer::PeerFindStates state
                                 );

        void notifySubscriptions(UseMessageIncomingPtr messageIncoming);

      public:

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account::PeerInfo
        #pragma mark

        struct PeerInfo
        {
          typedef std::map<LocationID, UseAccountPeerLocationPtr> PeerLocationMap;  // every location needs a session
          typedef std::map<LocationID, LocationID> FindingBecauseOfLocationIDMap;   // using this to track the reason why the find needs to be initated or reinitated

          static ElementPtr toDebug(PeerInfoPtr peerInfo);

          static PeerInfoPtr create();
          ~PeerInfo();
          void findTimeReset();
          void findTimeScheduleNext();
          Log::Params log(const char *message) const;
          ElementPtr toDebug() const;

          AutoPUID mID;

          UsePeerPtr mPeer;
          PeerLocationMap mLocations;                                 // list of connecting/connected peer locations

          IMessageMonitorPtr mPeerFindMonitor;                        // the request monitor when a search is being conducted
          FindingBecauseOfLocationIDMap mPeerFindBecauseOfLocations;  // peer find is being done because of locations that are known but not yet discovered

          FindingBecauseOfLocationIDMap mPeerFindNeedsRedoingBecauseOfLocations;  // peer find needs to be redone as soon as complete because of locations that are known but not yet discovered

          IPeer::PeerFindStates mCurrentFindState;
          ULONG mTotalSubscribers;                                    // total number of external subscribers to this peer

          // If a peer location was NOT found, we need to keep trying the search periodically but with exponential back off.
          // These variables keep track of that backoff. We don't need to do any finds once connecting/connected to a single location
          // because the peer location will notify us of other peer locations for the existing peer.
          // NOTE: Presence can also give us a hint to when we should redo the search.
          Time mNextScheduledFind;                                    // if peer was not found, schedule finds to try again
          Seconds mLastScheduleFindDuration {};                       // how long was the duration between finds (used because it will double each time a search is completed)

          bool mFindAgainAfterBackgrounded {};
        };

      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Account => (data)
        #pragma mark

        AutoPUID mID;
        AccountWeakPtr mThisWeak;
        AccountPtr mGracefulShutdownReference;

        AccountStates mCurrentState;
        WORD mLastError;
        String mLastErrorReason;

        IAccountDelegatePtr mDelegate;

        bool mBackgroundingEnabled {};
        IBackgroundingSubscriptionPtr mBackgroundingSubscription;
        IBackgroundingNotifierPtr mBackgroundingNotifier;

        TimerPtr mTimer;

        UseServiceLockboxSessionPtr mLockboxSession;
        IServiceLockboxSessionSubscriptionPtr mLockboxSubscription;

        Service::MethodListPtr mTURN;
        Service::MethodListPtr mSTUN;

        IICESocketPtr mSocket;

        String mLocationID;

        UsePublicationRepositoryPtr mRepository;

        DHKeyPairTemplates mDHKeyPairTemplates;

        bool mBlockRSAKeyGeneration {};
        IKeyGeneratorPtr mRSAKeyGenerator;

        UseAccountFinderPtr mFinder;

        Time mFinderRetryAfter;
        Seconds mLastRetryFinderAfterDuration {};

        ServerList mAvailableFinders;
        IDNS::SRVResultPtr mAvailableFinderSRVResult;
        IMessageMonitorPtr mFindersGetMonitor;
        IDNSQueryPtr mFinderDNSLookup;

        IncomingFindRequestList mIncomingFindRequests;

        PeerInfoMap mPeerInfos;

        PeerSubscriptionMap mPeerSubscriptions;
        LocationSubscriptionMap mLocationSubscriptions;

        class PeersDB : public SharedRecursiveLock
        {
        public:
          PeersDB() : SharedRecursiveLock(SharedRecursiveLock::create()) {}

          UsePeerPtr getLocal() const;
          void setLocal(UsePeerPtr peer);

          UsePeerPtr findExisting(
                                  AccountPtr account,
                                  const String &peerURI
                                  ) const;

          UsePeerPtr findExistingOrUse(
                                       AccountPtr account,
                                       UsePeerPtr peer
                                       );

          bool remove(const String &peerURI);

          size_t size() const;

        protected:
          UsePeerPtr mSelf;
          PeerMap mPeers;
        };

        class LocationsDB : public SharedRecursiveLock
        {
        public:
          LocationsDB() : SharedRecursiveLock(SharedRecursiveLock::create()) {}

          UseLocationPtr findExisting(
                                      AccountPtr account,
                                      const String &peerURI,
                                      const String &locationID
                                      ) const;

          UseLocationPtr findExistingOrUse(
                                           AccountPtr account,
                                           UseLocationPtr inLocation
                                           );

          UseLocationPtr getLocal() const;
          UseLocationPtr getFinder() const;

          void setLocal(UseLocationPtr location);
          void setFinder(UseLocationPtr location);

          bool remove(
                      const String &locationID,
                      const String &peerURI
                      );

          size_t size() const;

        protected:
          UseLocationPtr mSelf;
          UseLocationPtr mFinder;

          LocationMap mLocations;
        };

        PeersDB mPeersDB;
        LocationsDB mLocationsDB;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      interaction IAccountFactory
      {
        static IAccountFactory &singleton();

        virtual AccountPtr create(
                                  IAccountDelegatePtr delegate,
                                  IServiceLockboxSessionPtr peerContactSession
                                  );
      };

      class AccountFactory : public IFactory<IAccountFactory> {};

    }
  }
}
