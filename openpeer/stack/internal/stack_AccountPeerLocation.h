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
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>
#include <openpeer/stack/internal/stack_IFinderConnection.h>

#include <openpeer/stack/message/peer-finder/PeerLocationFindNotify.h>

#include <openpeer/stack/message/peer-to-peer/PeerIdentifyResult.h>
#include <openpeer/stack/message/peer-to-peer/PeerKeepAliveResult.h>

#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/ILocation.h>

#include <openpeer/services/IRUDPTransport.h>
#include <openpeer/services/IRUDPMessaging.h>
#include <openpeer/services/ITransportStream.h>

#include <openpeer/services/IWakeDelegate.h>
#include <openpeer/services/IMessageLayerSecurityChannel.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

#include <map>
#include <list>


#define OPENPEER_STACK_SETTING_ACCOUNT_PEER_LOCATION_DEBUG_FORCE_MESSAGES_OVER_RELAY "openpeer/stack/debug/force-p2p-messages-over-finder-relay"


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IPeerForAccount;
      interaction IAccountForAccountPeerLocation;
      interaction IMessageMonitorManagerForAccountPeerLocation;

      using services::IICESocket;

      using services::IRUDPMessagingPtr;
      using services::IMessageLayerSecurityChannel;
      using services::IMessageLayerSecurityChannelPtr;

      using message::peer_finder::PeerLocationFindRequestPtr;
      using message::peer_finder::PeerLocationFindNotify;
      using message::peer_finder::PeerLocationFindNotifyPtr;
      using message::peer_finder::ChannelMapNotifyPtr;

      using message::peer_to_peer::PeerIdentifyResult;
      using message::peer_to_peer::PeerIdentifyResultPtr;
      using message::peer_to_peer::PeerKeepAliveResult;
      using message::peer_to_peer::PeerKeepAliveResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationForAccount
      #pragma mark

      interaction IAccountPeerLocationForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IAccountPeerLocationForAccount, ForAccount)

        typedef IAccount::AccountStates AccountStates;
        typedef IFinderConnection::ChannelNumber ChannelNumber;

        static ElementPtr toDebug(ForAccountPtr peerLocation);

        static ForAccountPtr createFromIncomingPeerLocationFind(
                                                                IAccountPeerLocationDelegatePtr delegate,
                                                                AccountPtr outer,
                                                                PeerLocationFindRequestPtr request,
                                                                IDHPrivateKeyPtr localPrivateKey,
                                                                IDHPublicKeyPtr localPublicKey
                                                                );

        static ForAccountPtr createFromPeerLocationFindResult(
                                                              IAccountPeerLocationDelegatePtr delegate,
                                                              AccountPtr outer,
                                                              PeerLocationFindRequestPtr request,
                                                              LocationInfoPtr locationInfo
                                                              );

        virtual PUID getID() const = 0;

        virtual LocationPtr getLocation() const = 0;
        virtual LocationInfoPtr getLocationInfo() const = 0;

        virtual void shutdown() = 0;

        virtual AccountStates getState() const = 0;
        virtual bool shouldRefindNow() const = 0;

        virtual bool isConnected() const = 0;
        virtual Time getTimeOfLastActivity() const = 0;

        virtual bool wasCreatedFromIncomingFind() const = 0;
        virtual Time getCreationFindRequestTimestamp() const = 0;
        virtual String getFindRequestContext() const = 0;

        virtual bool hasReceivedCandidateInformation() const = 0;
        virtual bool hasReceivedFinalCandidateInformation() const = 0;

        virtual bool send(MessagePtr message) const = 0;
        virtual IMessageMonitorPtr sendRequest(
                                               IMessageMonitorDelegatePtr delegate,
                                               MessagePtr message,
                                               Seconds duration
                                               ) const = 0;

        virtual void sendKeepAlive() = 0;

        virtual bool handleIncomingChannelMapNotify(
                                                    ChannelMapNotifyPtr notify,
                                                    const Token &relayToken
                                                    ) = 0;

        virtual ChannelNumber getIncomingRelayChannelNumber() const = 0;

        virtual void notifyIncomingRelayChannel(
                                                IFinderRelayChannelPtr channel,
                                                ITransportStreamPtr receiveStream,
                                                ITransportStreamPtr sendStream
                                                ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation
      #pragma mark

      class AccountPeerLocation : public Noop,
                                  public MessageQueueAssociator,
                                  public SharedRecursiveLock,
                                  public IAccountPeerLocationForAccount,
                                  public IWakeDelegate,
                                  public ITimerDelegate,
                                  public IDNSDelegate,
                                  public IFinderRelayChannelDelegate,
                                  public IICESocketDelegate,
                                  public IRUDPTransportDelegate,
                                  public services::IRUDPMessagingDelegate,
                                  public services::ITransportStreamWriterDelegate,
                                  public services::ITransportStreamReaderDelegate,
                                  public services::IMessageLayerSecurityChannelDelegate,
                                  public IMessageMonitorResultDelegate<PeerLocationFindNotify>,
                                  public IMessageMonitorResultDelegate<PeerIdentifyResult>,
                                  public IMessageMonitorResultDelegate<PeerKeepAliveResult>
      {
      public:
        friend interaction IAccountPeerLocationFactory;
        friend interaction IAccountPeerLocationForAccount;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForAccountPeerLocation, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForAccount, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(IPeerForAccount, UsePeer)
        ZS_DECLARE_TYPEDEF_PTR(IMessageMonitorManagerForAccountPeerLocation, UseMessageMonitorManager)

        typedef IFinderConnection::ChannelNumber ChannelNumber;

        enum CreatedFromReasons
        {
          CreatedFromReason_IncomingFind,
          CreatedFromReason_OutgoingFind,
        };

        static const char *toString(CreatedFromReasons reason);

      protected:
        AccountPeerLocation(
                            IMessageQueuePtr queue,
                            IAccountPeerLocationDelegatePtr delegate,
                            AccountPtr outer,
                            PeerLocationFindRequestPtr request,
                            IDHPrivateKeyPtr localPrivateKey,
                            IDHPublicKeyPtr localPublicKey
                            );

        AccountPeerLocation(
                            IMessageQueuePtr queue,
                            IAccountPeerLocationDelegatePtr delegate,
                            AccountPtr outer,
                            PeerLocationFindRequestPtr request,
                            LocationInfoPtr locationInfo
                            );

        AccountPeerLocation(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~AccountPeerLocation();

        static AccountPeerLocationPtr convert(ForAccountPtr object);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IAccountPeerLocationForAccount
        #pragma mark

        static ElementPtr toDebug(AccountPeerLocationPtr peerLocation);

        static ForAccountPtr createFromIncomingPeerLocationFind(
                                                                IAccountPeerLocationDelegatePtr delegate,
                                                                AccountPtr outer,
                                                                PeerLocationFindRequestPtr request,
                                                                IDHPrivateKeyPtr localPrivateKey,
                                                                IDHPublicKeyPtr localPublicKey
                                                                );

        static ForAccountPtr createFromPeerLocationFindResult(
                                                              IAccountPeerLocationDelegatePtr delegate,
                                                              AccountPtr outer,
                                                              PeerLocationFindRequestPtr request,
                                                              LocationInfoPtr locationInfo
                                                              );

        virtual PUID getID() const {return mID;}

        virtual LocationPtr getLocation() const;
        virtual LocationInfoPtr getLocationInfo() const;

        virtual void shutdown();

        virtual AccountStates getState() const;

        virtual bool shouldRefindNow() const;

        virtual bool isConnected() const;
        virtual Time getTimeOfLastActivity() const;

        virtual bool wasCreatedFromIncomingFind() const;
        virtual Time getCreationFindRequestTimestamp() const;
        virtual String getFindRequestContext() const;

        virtual bool hasReceivedCandidateInformation() const;
        virtual bool hasReceivedFinalCandidateInformation() const;

        virtual bool send(MessagePtr message) const;
        virtual IMessageMonitorPtr sendRequest(
                                               IMessageMonitorDelegatePtr delegate,
                                               MessagePtr message,
                                               Seconds duration
                                               ) const;

        virtual void sendKeepAlive();

        virtual bool handleIncomingChannelMapNotify(
                                                    ChannelMapNotifyPtr notify,
                                                    const Token &relayToken
                                                    );

        virtual ChannelNumber getIncomingRelayChannelNumber() const;

        virtual void notifyIncomingRelayChannel(
                                                IFinderRelayChannelPtr channel,
                                                ITransportStreamPtr receiveStream,
                                                ITransportStreamPtr sendStream
                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IDNSDelegate
        #pragma mark

        virtual void onLookupCompleted(IDNSQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IFinderRelayChannelDelegate
        #pragma mark

        virtual void onFinderRelayChannelStateChanged(
                                                      IFinderRelayChannelPtr channel,
                                                      IFinderRelayChannel::SessionStates state
                                                      );

        virtual void onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IICESocketDelegate
        #pragma mark

        virtual void onICESocketStateChanged(
                                             IICESocketPtr socket,
                                             ICESocketStates state
                                             );
        virtual void onICESocketCandidatesChanged(IICESocketPtr socket);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IRUDPTransportDelegate
        #pragma mark

        virtual void onRUDPTransportStateChanged(
                                                 IRUDPTransportPtr session,
                                                 RUDPTransportStates state
                                                 );

        virtual void onRUDPTransportChannelWaiting(IRUDPTransportPtr session);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IRUDPMessagingDelegate
        #pragma mark

        virtual void onRUDPMessagingStateChanged(
                                                 IRUDPMessagingPtr session,
                                                 RUDPMessagingStates state
                                                 );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageLayerSecurityChannelDelegate
        #pragma mark

        virtual void onMessageLayerSecurityChannelStateChanged(
                                                               IMessageLayerSecurityChannelPtr channel,
                                                               IMessageLayerSecurityChannel::SessionStates state
                                                               );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerLocationFindNotify>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerLocationFindNotifyPtr result
                                                        );

        virtual void onMessageMonitorTimedOut(
                                              IMessageMonitorPtr monitor,
                                              PeerLocationFindNotifyPtr response  // will always be NULL
                                              );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerIdentifyResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerIdentifyResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerIdentifyResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerKeepAliveResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerKeepAliveResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerKeepAliveResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => (internal)
        #pragma mark

        bool isPending() const      {return IAccount::AccountState_Pending == mCurrentState;}
        bool isReady() const        {return IAccount::AccountState_Ready == mCurrentState;}
        bool isShuttingDown() const {return IAccount::AccountState_ShuttingDown == mCurrentState;}
        bool isShutdown() const     {return IAccount::AccountState_Shutdown == mCurrentState;}

        bool isCreatedFromIncomingFind() const {return CreatedFromReason_IncomingFind == mCreatedReason;}
        bool isCreatedFromOutgoingFind() const  {return CreatedFromReason_OutgoingFind == mCreatedReason;}

        IICESocketPtr getSocket() const;

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void cancel();

        void step();
        bool stepSocketSubscription(IICESocketPtr socket);
        bool stepOutgoingRelayChannel();
        bool stepIncomingRelayChannel();
        bool stepAnyConnectionPresent();
        bool stepConnectIncomingFind();
        bool stepSendNotify(IICESocketPtr socket);
        bool stepRUDPSocketSession();
        bool stepCheckIncomingIdentify();
        bool stepIdentify();
        bool stepMessaging();
        bool stepMLS();

        void setState(AccountStates state);

        void handleMessage(MessagePtr message);
        bool isLegalDuringPreIdentify(MessagePtr message) const;

        void connectLocation(
                             const char *remoteICEUsernameFrag,
                             const char *remoteICEPassword,
                             const CandidateList &candidates,
                             bool candidatesFinal
                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark AccountPeerLocation => (data)
        #pragma mark

        AutoPUID mID;
        mutable AccountPeerLocationWeakPtr mThisWeak;

        IAccountPeerLocationDelegatePtr mDelegate;
        UseAccountWeakPtr mOuter;
        AccountPeerLocationPtr mGracefulShutdownReference;

        String mLocalContext;
        String mRemoteContext;

        IDHPrivateKeyPtr mDHLocalPrivateKey;
        IDHPublicKeyPtr mDHLocalPublicKey;
        IDHPublicKeyPtr mDHRemotePublicKey;

        AccountStates mCurrentState;
        bool mShouldRefindNow {};

        mutable Time mLastActivity;

        // information about the location found
        LocationInfoPtr mLocationInfo;
        UseLocationPtr mLocation;
        UsePeerPtr mPeer;

        IICESocketSubscriptionPtr mSocketSubscription;
        IICESocketSessionPtr mSocketSession;   // this will only become valid when a connection is establishing
        IRUDPTransportPtr mRUDPSocketSession;
        IRUDPTransportSubscriptionPtr mRUDPSocketSessionSubscription; // REMOVE
        IRUDPMessagingPtr mMessaging;
        ITransportStreamReaderPtr mMessagingReceiveStream;
        ITransportStreamWriterPtr mMessagingSendStream;
        bool mCandidatesFinal {};
        String mLastCandidateVersionSent;

        IMessageLayerSecurityChannelPtr mMLSChannel;
        ITransportStreamReaderPtr mMLSReceiveStream;
        ITransportStreamWriterPtr mMLSSendStream;
        bool mMLSDidConnect {};

        CreatedFromReasons mCreatedReason;

        PeerLocationFindRequestPtr mFindRequest;

        IMessageMonitorPtr mFindRequestMonitor;
        TimerPtr mFindRequestTimer;

        IDNSQueryPtr mOutgoingSRVLookup;
        IDNS::SRVResultPtr mOutgoingSRVResult;
        IFinderRelayChannelPtr mOutgoingRelayChannel;
        ITransportStreamReaderPtr mOutgoingRelayReceiveStream;
        ITransportStreamWriterPtr mOutgoingRelaySendStream;

        ChannelNumber mIncomingRelayChannelNumber;

        IFinderRelayChannelPtr mIncomingRelayChannel;
        IFinderRelayChannelSubscriptionPtr mIncomingRelayChannelSubscription;

        ITransportStreamReaderPtr mIncomingRelayReceiveStream;
        ITransportStreamWriterPtr mIncomingRelaySendStream;
        ITransportStreamReaderSubscriptionPtr mIncomingRelayReceiveStreamSubscription;
        ITransportStreamWriterSubscriptionPtr mIncomingRelaySendStreamSubscription;

        bool mHadConnection {};
        bool mHadPeerConnection {};

        Time mIdentifyTime;

        IMessageMonitorPtr mIdentifyMonitor;
        IMessageMonitorPtr mKeepAliveMonitor;

        bool mDebugForceMessagesOverRelay;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationDelegate
      #pragma mark

      interaction IAccountPeerLocationDelegate
      {
        typedef IAccount::AccountStates AccountStates;

        virtual void onAccountPeerLocationStateChanged(
                                                       AccountPeerLocationPtr peerLocation,
                                                       AccountStates state
                                                       ) = 0;

        virtual void onAccountPeerLocationMessageIncoming(
                                                          AccountPeerLocationPtr peerLocation,
                                                          MessagePtr message
                                                          ) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationFactory
      #pragma mark

      interaction IAccountPeerLocationFactory
      {
        static IAccountPeerLocationFactory &singleton();

        virtual IAccountPeerLocationForAccount::ForAccountPtr createFromIncomingPeerLocationFind(
                                                                                                 IAccountPeerLocationDelegatePtr delegate,
                                                                                                 AccountPtr outer,
                                                                                                 PeerLocationFindRequestPtr request,
                                                                                                 IDHPrivateKeyPtr localPrivateKey,
                                                                                                 IDHPublicKeyPtr localPublicKey
                                                                                                 );

        virtual IAccountPeerLocationForAccount::ForAccountPtr createFromPeerLocationFindResult(
                                                                                               IAccountPeerLocationDelegatePtr delegate,
                                                                                               AccountPtr outer,
                                                                                               PeerLocationFindRequestPtr request,
                                                                                               LocationInfoPtr locationInfo
                                                                                               );
      };

      class AccountPeerLocationFactory : public IFactory<IAccountPeerLocationFactory> {};

    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::internal::IAccountPeerLocationDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::internal::AccountPeerLocationPtr, AccountPeerLocationPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::internal::AccountPeerLocation::AccountStates, AccountStates)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::message::MessagePtr, MessagePtr)
ZS_DECLARE_PROXY_METHOD_2(onAccountPeerLocationStateChanged, AccountPeerLocationPtr, AccountStates)
ZS_DECLARE_PROXY_METHOD_2(onAccountPeerLocationMessageIncoming, AccountPeerLocationPtr, MessagePtr)
ZS_DECLARE_PROXY_END()
