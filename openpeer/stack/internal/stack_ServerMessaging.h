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
#include <openpeer/stack/IServerMessaging.h>

#include <openpeer/services/IBackgrounding.h>
#include <openpeer/services/IMessageLayerSecurityChannel.h>
#include <openpeer/services/ITCPMessaging.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>


#define OPENPEER_STACK_SETTING_SERVERMESSAGING_BACKGROUNDING_PHASE "openpeer/stack/backgrounding-phase-server-messaging"

#define OPENPEER_STACK_SETTING_SERVERMESSAGING_DEFAULT_KEY_DOMAIN "openpeer/stack/server-messaging-default-key-domain"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForServices;

      ZS_DECLARE_USING_PTR(message::peer_salt, SignedSaltGetResult)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging
      #pragma mark

      class ServerMessaging : public Noop,
                              public zsLib::MessageQueueAssociator,
                              public SharedRecursiveLock,
                              public IBackgroundingDelegate,
                              public IServerMessaging,
                              public ITCPMessagingDelegate,
                              public services::IMessageLayerSecurityChannelDelegate,
                              public ITransportStreamReaderDelegate,
                              public ITransportStreamWriterDelegate,
                              public IWakeDelegate,
                              public IDNSDelegate
      {
      public:
        friend interaction IServerMessagingFactory;
        friend interaction IServerMessaging;

        typedef IServerMessaging::SessionStates SessionStates;

        ZS_DECLARE_TYPEDEF_PTR(services::IMessageLayerSecurityChannel, IMessageLayerSecurityChannel)

      protected:
        ServerMessaging(
                        IMessageQueuePtr queue,
                        IServerMessagingDelegatePtr delegate,
                        IPeerFilesPtr peerFiles,
                        ITransportStreamPtr receiveStream,
                        ITransportStreamPtr sendStream,
                        bool messagesHaveChannelNumber,
                        const ServerList &possibleServers,
                        size_t maxMessageSizeInBytes
                        );

        ServerMessaging(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
          {}
        
        void init();

      public:
        ~ServerMessaging();

        static ServerMessagingPtr convert(IServerMessagingPtr messaging);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => IServerMessaging
        #pragma mark

        static ElementPtr toDebug(IServerMessagingPtr query);

        static ServerMessagingPtr connect(
                                          IServerMessagingDelegatePtr delegate,
                                          IPeerFilesPtr peerFiles,
                                          ITransportStreamPtr receiveStream,
                                          ITransportStreamPtr sendStream,
                                          bool messagesHaveChannelNumber,
                                          const ServerList &possibleServers,
                                          size_t maxMessageSizeInBytes = OPENPEER_SERVICES_ITCPMESSAGING_MAX_MESSAGE_SIZE_IN_BYTES
                                          );

        virtual PUID getID() const {return mID;}

        virtual IServerMessagingSubscriptionPtr subscribe(IServerMessagingDelegatePtr delegate);

        virtual void shutdown(Seconds lingerTime = Seconds(OPENPEER_SERVICES_CLOSE_LINGER_TIMER_IN_SECONDS));

        virtual SessionStates getState(
                                       WORD *outLastErrorCode = NULL,
                                       String *outLastErrorReason = NULL
                                       ) const;

        virtual void enableKeepAlive(bool enable = true);

        virtual IPAddress getRemoteIP() const;

        virtual void setMaxMessageSizeInBytes(size_t maxMessageSizeInBytes);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => ITCPMessagingDelegate
        #pragma mark

        virtual void onTCPMessagingStateChanged(
                                                ITCPMessagingPtr messaging,
                                                ITCPMessaging::SessionStates state
                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => IMessageLayerSecurityChannelDelegate
        #pragma mark

        virtual void onMessageLayerSecurityChannelStateChanged(
                                                               IMessageLayerSecurityChannelPtr channel,
                                                               IMessageLayerSecurityChannel::SessionStates state
                                                               );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => IBackgroundingDelegate
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
        #pragma mark ServerMessaging => ITransportStreamReaderDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => IDNSDelegate
        #pragma mark

        virtual void onLookupCompleted(IDNSQueryPtr query);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => (internal)
        #pragma mark

        bool isConnected() const {return SessionState_Connected == mCurrentState;}
        bool isShuttingDown() const {return SessionState_ShuttingDown == mCurrentState;}
        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void step();
        bool stepPickServer();
        bool stepDNS();
        bool stepPickIP();
        bool stepTCP();
        bool stepMLS();

        void cancel();
        void setState(SessionStates state);
        void setError(WORD errorCode, const char *inReason);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServerMessaging => (data)
        #pragma mark

        ServerMessagingWeakPtr mThisWeak;

        AutoPUID mID;
        ServerMessagingPtr mGracefulShutdownReference;

        IServerMessagingDelegateSubscriptions mSubscriptions;
        IServerMessagingSubscriptionPtr mDefaultSubscription;

        IBackgroundingSubscriptionPtr mBackgroundingSubscription;

        SessionStates mCurrentState;

        WORD mLastError {};
        String mLastErrorReason;

        IPeerFilesPtr mPeerFiles;

        size_t mMaxMessageSizeInBytes;

        bool mMessagesHaveChannelNumber;
        bool mEnableKeepAlive;

        ITransportStreamReaderPtr mReceiveStreamEncoded;  // connected to incoming on-the-wire transport
        ITransportStreamWriterPtr mReceiveStreamDecoded;  // connected to "outer" layer for "outer" to receive decoded on-the-wire data
        ITransportStreamReaderPtr mSendStreamDecoded;     // connected to "outer" layer for "outer" to send and encode data on-on-the-wire
        ITransportStreamWriterPtr mSendStreamEncoded;     // connected to outgoing on-the-wire transport

        ServerList mServerList;
        Server mActiveServer;
        Server::Protocol mActiveProtocol;
        IDNSQueryPtr mDNSQuery;
        IDNS::SRVResultPtr mDNSResult;
        IPAddress mActiveIP;

        ITCPMessagingPtr mTCPMessaging;
        IMessageLayerSecurityChannelPtr mMLS;

        bool mBackgrounding;
        IBackgroundingNotifierPtr mNotifier;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServerMessagingFactory
      #pragma mark

      interaction IServerMessagingFactory
      {
        static IServerMessagingFactory &singleton();

        virtual ServerMessagingPtr connect(
                                           IServerMessagingDelegatePtr delegate,
                                           IPeerFilesPtr peerFiles,
                                           ITransportStreamPtr receiveStream,
                                           ITransportStreamPtr sendStream,
                                           bool messagesHaveChannelNumber,
                                           const ServerList &possibleServers,
                                           size_t maxMessageSizeInBytes = OPENPEER_SERVICES_ITCPMESSAGING_MAX_MESSAGE_SIZE_IN_BYTES
                                           );
      };

      class ServerMessagingFactory : public IFactory<IServerMessagingFactory> {};

    }
  }
}
