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

#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>
#include <openpeer/stack/message/types.h>
#include <openpeer/stack/internal/types.h>
#include <openpeer/stack/internal/stack_IFinderConnectionRelayChannel.h>

#include <openpeer/services/IMessageLayerSecurityChannel.h>
#include <openpeer/services/ITransportStream.h>

#include <list>
#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForFinderRelayChannel;

      using services::IMessageLayerSecurityChannel;
      using services::IMessageLayerSecurityChannelPtr;

      using services::IMessageLayerSecurityChannelDelegate;
      using services::IMessageLayerSecurityChannelDelegatePtr;

      interaction IFinderRelayChannelForFinderConnection
      {
        ZS_DECLARE_TYPEDEF_PTR(IFinderRelayChannelForFinderConnection, ForFinderConnection)

        static ForFinderConnectionPtr createIncoming(
                                                     IFinderRelayChannelDelegatePtr delegate, // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                                     AccountPtr account,
                                                     ITransportStreamPtr outerReceiveStream,
                                                     ITransportStreamPtr outerSendStream,
                                                     ITransportStreamPtr wireReceiveStream,
                                                     ITransportStreamPtr wireSendStream
                                                     );

        virtual PUID getID() const = 0;

        virtual IFinderRelayChannelSubscriptionPtr subscribe(IFinderRelayChannelDelegatePtr delegate) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderRelayChannel
      #pragma mark

      class FinderRelayChannel : public Noop,
                                 public zsLib::MessageQueueAssociator,
                                 public IFinderRelayChannel,
                                 public IFinderRelayChannelForFinderConnection,
                                 public IFinderConnectionRelayChannelDelegate,
                                 public IMessageLayerSecurityChannelDelegate
      {
      public:
        friend interaction IFinderRelayChannelFactory;
        friend interaction IFinderRelayChannel;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForFinderRelayChannel, UseAccount)

        typedef IFinderRelayChannel::SessionStates SessionStates;

        struct ConnectInfo
        {
          IPAddress mFinderIP;
          String mLocalContextID;
          String mRemoteContextID;
          String mRelayDomain;
          String mRelayAccessToken;
          String mRelayAccessSecretProof;
          IDHPrivateKeyPtr mDHLocalPrivateKey;
          IDHPublicKeyPtr mDHLocalPublicKey;
          IDHPublicKeyPtr mDHRemotePublicKey;
        };

        struct IncomingContext
        {
          String mLocalContextID;
          IDHPrivateKeyPtr mDHLocalPrivateKey;
          IDHPublicKeyPtr mDHLocalPublicKey;
          IPeerPtr mRemotePeer;
        };

      protected:
        FinderRelayChannel(
                           IMessageQueuePtr queue,
                           IFinderRelayChannelDelegatePtr delegate,
                           AccountPtr account,
                           ITransportStreamPtr outerReceiveStream,
                           ITransportStreamPtr outerSendStream
                           );

        FinderRelayChannel(Noop) :
          Noop(true),
          zsLib::MessageQueueAssociator(IMessageQueuePtr()) {}

        void init();

      public:
        ~FinderRelayChannel();

        static FinderRelayChannelPtr convert(IFinderRelayChannelPtr channel);
        static FinderRelayChannelPtr convert(ForFinderConnectionPtr channel);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => IFinderRelayChannel
        #pragma mark

        static ElementPtr toDebug(IFinderRelayChannelPtr channel);

        static FinderRelayChannelPtr connect(
                                             IFinderRelayChannelDelegatePtr delegate,        // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                             AccountPtr account,
                                             ITransportStreamPtr receiveStream,
                                             ITransportStreamPtr sendStream,
                                             IPAddress remoteFinderIP,
                                             const char *localContextID,
                                             const char *remoteContextID,
                                             const char *relayDomain,
                                             const char *relayAccessToken,
                                             const char *relayAccessSecretProof,
                                             IDHPrivateKeyPtr localPrivateKey,
                                             IDHPublicKeyPtr localPublicKey,
                                             IDHPublicKeyPtr remotePublicKey
                                             );

        virtual PUID getID() const {return mID;}

        virtual IFinderRelayChannelSubscriptionPtr subscribe(IFinderRelayChannelDelegatePtr delegate);

        virtual void cancel();

        virtual SessionStates getState(
                                       WORD *outLastErrorCode = NULL,
                                       String *outLastErrorReason = NULL
                                       ) const;

        virtual void setIncomingContext(
                                        const char *localContextID,
                                        IDHPrivateKeyPtr localPrivateKey,
                                        IDHPublicKeyPtr localPublicKey,
                                        IPeerPtr remotePeer
                                        );

        virtual String getLocalContextID() const;

        virtual String getRemoteContextID() const;

        virtual IPeerPtr getRemotePeer() const;

        virtual IRSAPublicKeyPtr getRemotePublicKey() const;

        virtual IDHKeyDomainPtr getDHRemoteKeyDomain() const;

        virtual IDHPublicKeyPtr getDHRemotePublicKey() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => IFinderRelayChannelForFinderConnection
        #pragma mark

        static FinderRelayChannelPtr createIncoming(
                                                    IFinderRelayChannelDelegatePtr delegate, // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                                    AccountPtr account,
                                                    ITransportStreamPtr outerReceiveStream,
                                                    ITransportStreamPtr outerSendStream,
                                                    ITransportStreamPtr wireReceiveStream,
                                                    ITransportStreamPtr wireSendStream
                                                    );

        // (duplicate) virtual PUID getID() const;
        // (duplicate) virtual IFinderRelayChannelSubscriptionPtr subscribe(IFinderRelayChannelDelegatePtr delegate);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => IFinderConnectionRelayChannelDelegate
        #pragma mark

        virtual void onFinderConnectionRelayChannelStateChanged(
                                                                IFinderConnectionRelayChannelPtr channel,
                                                                IFinderConnectionRelayChannel::SessionStates state
                                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => IMessageLayerSecurityChannelDelegate
        #pragma mark

        virtual void onMessageLayerSecurityChannelStateChanged(
                                                               IMessageLayerSecurityChannelPtr channel,
                                                               IMessageLayerSecurityChannel::SessionStates state
                                                               );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => (internal)
        #pragma mark

        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        RecursiveLock &getLock() const;
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *inReason = NULL);

        void step();
        bool stepMLS();
        bool stepConnectionRelayChannel();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderRelayChannel => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        FinderRelayChannelWeakPtr mThisWeak;

        IFinderRelayChannelDelegateSubscriptions mSubscriptions;
        IFinderRelayChannelSubscriptionPtr mDefaultSubscription;

        SessionStates mCurrentState;

        AutoWORD mLastError;
        String mLastErrorReason;

        UseAccountWeakPtr mAccount;

        AutoBool mIncoming;
        ConnectInfo mConnectInfo;
        IncomingContext mIncomingInfo;
        IFinderConnectionRelayChannelPtr mConnectionRelayChannel;

        IMessageLayerSecurityChannelPtr mMLSChannel;

        AutoBool mNotifiedNeedsContext;
        IPeerPtr mRemotePeer;
        IRSAPublicKeyPtr mRemotePublicKey;

        ITransportStreamPtr mOuterReceiveStream;
        ITransportStreamPtr mOuterSendStream;

        ITransportStreamPtr mWireReceiveStream;
        ITransportStreamPtr mWireSendStream;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderRelayChannelFactory
      #pragma mark

      interaction IFinderRelayChannelFactory
      {
        static IFinderRelayChannelFactory &singleton();

        virtual FinderRelayChannelPtr connect(
                                              IFinderRelayChannelDelegatePtr delegate,        // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                              AccountPtr account,
                                              ITransportStreamPtr receiveStream,
                                              ITransportStreamPtr sendStream,
                                              IPAddress remoteFinderIP,
                                              const char *localContextID,
                                              const char *remoteContextID,
                                              const char *relayDomain,
                                              const char *relayAccessToken,
                                              const char *relayAccessSecretProof,
                                              IDHPrivateKeyPtr localPrivateKey,
                                              IDHPublicKeyPtr localPublicKey,
                                              IDHPublicKeyPtr remotePublicKey
                                              );

        virtual FinderRelayChannelPtr createIncoming(
                                                     IFinderRelayChannelDelegatePtr delegate, // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                                     AccountPtr account,
                                                     ITransportStreamPtr outerReceiveStream,
                                                     ITransportStreamPtr outerSendStream,
                                                     ITransportStreamPtr wireReceiveStream,
                                                     ITransportStreamPtr wireSendStream
                                                     );
      };

    }
  }
}
