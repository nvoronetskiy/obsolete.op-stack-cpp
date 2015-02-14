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
#include <openpeer/stack/internal/stack_IFinderConnectionRelayChannel.h>
#include <openpeer/stack/internal/stack_IFinderConnection.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>

#include <openpeer/stack/message/types.h>
#include <openpeer/stack/message/peer-finder/ChannelMapResult.h>

#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/IMessageSource.h>

#include <openpeer/services/ITransportStream.h>
#include <openpeer/services/ITCPMessaging.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/Timer.h>

#include <list>
#include <map>

#define OPENPEER_STACK_SETTING_FINDER_CONNECTION_MUST_SEND_PING_IF_NO_SEND_ACTIVITY_IN_SECONDS "openpeer/stack/finder-connection-send-ping-keep-alive-after-in-seconds"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IFinderRelayChannelForFinderConnection;

      using peer_finder::ChannelMapResult;
      using peer_finder::ChannelMapResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection
      #pragma mark

      class FinderConnection : public Noop,
                               public MessageQueueAssociator,
                               public SharedRecursiveLock,
                               public IFinderConnection,
                               public ITimerDelegate,
                               public IWakeDelegate,
                               public ITCPMessagingDelegate,
                               public ITransportStreamWriterDelegate,
                               public ITransportStreamReaderDelegate,
                               public IFinderConnectionRelayChannelDelegate,
                               public IMessageMonitorResultDelegate<ChannelMapResult>,
                               public IMessageSource
      {
      public:
        typedef IFinderConnection::SessionStates SessionStates;

        friend interaction IFinderConnectionRelayChannelFactory;

        friend class FinderConnectionManager;
        friend class Channel;

        ZS_DECLARE_TYPEDEF_PTR(IFinderRelayChannelForFinderConnection, UseFinderRelayChannel)

        ZS_DECLARE_TYPEDEF_PTR(std::list<PromiseWeakPtr>, PromiseWeakList)

        ZS_DECLARE_CLASS_PTR(Channel)

        typedef IFinderConnection::ChannelNumber ChannelNumber;
        typedef std::map<ChannelNumber, ChannelPtr> ChannelMap;

      protected:
        FinderConnection(
                         FinderConnectionManagerPtr outer,
                         IMessageQueuePtr queue,
                         IPAddress remoteFinderIP
                         );

        FinderConnection(Noop) :
          Noop(true),
          zsLib::MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        virtual ~FinderConnection();

        static FinderConnectionPtr convert(IFinderConnectionPtr connection);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => friend IFinderConnectionRelayChannelFactory
        #pragma mark

        static IFinderConnectionRelayChannelPtr connect(
                                                        IFinderConnectionRelayChannelDelegatePtr delegate,
                                                        const IPAddress &remoteFinderIP,
                                                        const char *localContextID,
                                                        const char *remoteContextID,
                                                        const char *relayDomain,
                                                        const Token &relayToken,
                                                        ITransportStreamPtr receiveStream,
                                                        ITransportStreamPtr sendStream
                                                        );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => IFinderConnection
        #pragma mark

        static ElementPtr toDebug(IFinderConnectionPtr connection);

        static IFinderConnectionPtr connect(
                                            IFinderConnectionDelegatePtr delegate,
                                            const IPAddress &remoteFinderIP,
                                            ITransportStreamPtr receiveStream,
                                            ITransportStreamPtr sendStream
                                            );

        virtual PUID getID() const {return mID;}

        virtual IFinderConnectionSubscriptionPtr subscribe(IFinderConnectionDelegatePtr delegate);

        virtual void cancel();

        virtual SessionStates getState(
                                       WORD *outLastErrorCode = NULL,
                                       String *outLastErrorReason = NULL
                                       ) const;

        virtual IFinderRelayChannelPtr accept(
                                              IFinderRelayChannelDelegatePtr delegate,        // can pass in IFinderRelayChannelDelegatePtr() if not interested in the events
                                              AccountPtr account,
                                              ITransportStreamPtr receiveStream,
                                              ITransportStreamPtr sendStream,
                                              ChannelNumber *outChannelNumber = NULL
                                              );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => ITCPMessagingDelegate
        #pragma mark

        virtual void onTCPMessagingStateChanged(
                                                ITCPMessagingPtr messaging,
                                                ITCPMessaging::SessionStates state
                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => ITransportStreamWriterDelegate
        #pragma mark

        virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => ITransportStreamReaderDelegate
        #pragma mark

        virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => IFinderConnectionRelayChannelDelegate
        #pragma mark

        virtual void onFinderConnectionRelayChannelStateChanged(
                                                                IFinderConnectionRelayChannelPtr channel,
                                                                IFinderConnectionRelayChannel::SessionStates state
                                                                );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => IMessageMonitorResultDelegate<ChannelMapResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ChannelMapResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ChannelMapResultPtr ignore, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );


        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => friend Channel
        #pragma mark

        void notifyOuterWriterReady();

        void sendBuffer(
                        ChannelNumber channelNumber,
                        SecureByteBlockPtr buffer
                        );
        void notifyDestroyed(ChannelNumber channelNumber);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => (internal)
        #pragma mark

        bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}
        bool isFinderSessionConnection() const;
        bool isFinderRelayConnection() const;

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void setState(SessionStates state);
        void setError(WORD errorCode, const char *inReason = NULL);

        void step();
        bool stepCleanRemoval();
        bool stepConnectWire();
        bool stepMasterChannel();
        bool stepChannelMapRequest();
        bool stepReceiveData();
        bool stepSelfDestruct();

        IFinderConnectionRelayChannelPtr connect(
                                                 IFinderConnectionRelayChannelDelegatePtr delegate,
                                                 const char *localContextID,
                                                 const char *remoteContextID,
                                                 const char *relayDomain,
                                                 const Token &relayToken,
                                                 ITransportStreamPtr receiveStream,
                                                 ITransportStreamPtr sendStream
                                                 );

        void resolveAllPromises(PromiseWeakList &promises);
        void rejectAllPromises(PromiseWeakList &promises);

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection::Channel
        #pragma mark

        class Channel : public MessageQueueAssociator,
                        public SharedRecursiveLock,
                        public IFinderConnectionRelayChannel,
                        public IFinderRelayChannelDelegate,
                        public ITransportStreamWriterDelegate,
                        public ITransportStreamReaderDelegate
        {
        public:
          friend class FinderConnection;
          typedef IFinderConnectionRelayChannel::SessionStates SessionStates;

        protected:
          Channel(
                  IMessageQueuePtr queue,
                  FinderConnectionPtr outer,
                  IFinderConnectionRelayChannelDelegatePtr delegate,
                  ITransportStreamPtr receiveStream,
                  ITransportStreamPtr sendStream,
                  ChannelNumber channelNumber
                  );

          struct ConnectionInfo
          {
            String mLocalContextID;
            String mRemoteContextID;
            String mRelayDomain;
            Token mRelayToken;
          };

          void init();

        public:
          ~Channel();

          static ChannelPtr convert(IFinderConnectionRelayChannelPtr channel);

        protected:
          static ElementPtr toDebug(IFinderConnectionRelayChannelPtr channel);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => IFinderConnectionRelayChannel
          #pragma mark

          static ChannelPtr connect(
                                    FinderConnectionPtr outer,
                                    IFinderConnectionRelayChannelDelegatePtr delegate,
                                    const char *localContextID,
                                    const char *remoteContextID,
                                    const char *relayDomain,
                                    const Token &relayToken,
                                    ITransportStreamPtr receiveStream,
                                    ITransportStreamPtr sendStream,
                                    ULONG channelNumber
                                    );

          virtual PUID getID() const {return mID;}

          virtual void cancel();

          virtual SessionStates getState(
                                         WORD *outLastErrorCode = NULL,
                                         String *outLastErrorReason = NULL
                                         ) const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => IFinderRelayChannelDelegate
          #pragma mark

          virtual void onFinderRelayChannelStateChanged(
                                                        IFinderRelayChannelPtr channel,
                                                        IFinderRelayChannel::SessionStates state
                                                        );

          virtual void onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => ITransportStreamWriterDelegate
          #pragma mark

          virtual void onTransportStreamWriterReady(ITransportStreamWriterPtr writer);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => ITransportStreamReaderDelegate
          #pragma mark

          virtual void onTransportStreamReaderReady(ITransportStreamReaderPtr reader);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => friend FinderConnection
          #pragma mark

          static ChannelPtr incoming(
                                     FinderConnectionPtr outer,
                                     IFinderConnectionRelayChannelDelegatePtr delegate,
                                     ITransportStreamPtr receiveStream,
                                     ITransportStreamPtr sendStream,
                                     ULONG channelNumber
                                     );

          void notifyIncomingFinderRelayChannel(FinderRelayChannelPtr relayChannel);

          void notifyReceivedWireWriteReady();

          // (duplicate) virtual SessionStates getState(
          //                                            WORD *outLastErrorCode = NULL,
          //                                            String *outLastErrorReason = NULL
          //                                            ) const;
          // (duplicate) void setError(WORD errorCode, const char *inReason = NULL);

          void notifyDataReceived(SecureByteBlockPtr buffer);

          void getStreams(
                          ITransportStreamPtr &outReceiveStream,
                          ITransportStreamPtr &outSendStream
                          );

          const ConnectionInfo &getConnectionInfo() {return mConnectionInfo;}

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark FinderConnection::Channel => (internal)
          #pragma mark

          Log::Params log(const char *message) const;
          Log::Params debug(const char *message) const;

          bool isShutdown() const {return SessionState_Shutdown == mCurrentState;}

          virtual ElementPtr toDebug() const;

          void setState(SessionStates state);
          void setError(WORD errorCode, const char *inReason = NULL);

          void step();
          bool stepSendData();

        protected:
          AutoPUID mID;
          ChannelWeakPtr mThisWeak;

          FinderConnectionWeakPtr mOuter;

          IFinderConnectionRelayChannelDelegatePtr mDelegate;

          SessionStates mCurrentState;

          WORD mLastError {};
          String mLastErrorReason;

          ChannelNumber mChannelNumber;

          ITransportStreamWriterPtr mOuterReceiveStream;
          ITransportStreamWriterSubscriptionPtr mOuterReceiveStreamSubscription;
          ITransportStreamReaderPtr mOuterSendStream;
          ITransportStreamReaderSubscriptionPtr mOuterSendStreamSubscription;

          bool mWireStreamNotifiedReady {};
          bool mOuterStreamNotifiedReady {};

          ConnectionInfo mConnectionInfo;

          IFinderRelayChannelSubscriptionPtr mRelayChannelSubscription;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnection => (data)
        #pragma mark

        AutoPUID mID;
        FinderConnectionWeakPtr mThisWeak;

        FinderConnectionManagerWeakPtr mOuter;

        IFinderConnectionDelegateSubscriptions mSubscriptions;
        IFinderConnectionSubscriptionPtr mDefaultSubscription;

        SessionStates mCurrentState;

        WORD mLastError {};
        String mLastErrorReason;

        IPAddress mRemoteIP;
        ITCPMessagingPtr mTCPMessaging;

        ITransportStreamReaderPtr mWireReceiveStream;
        ITransportStreamWriterPtr mWireSendStream;

        bool mSendStreamNotifiedReady {};

        Seconds mSendKeepAliveAfter {};
        Time mLastSentData;
        TimerPtr mPingTimer;

        ChannelMap mChannels;

        ChannelMap mPendingMapRequest;
        ChannelMap mIncomingChannels;
        ChannelMap mRemoveChannels;

        IMessageMonitorPtr mMapRequestChannelMonitor;
        ChannelNumber mMapRequestChannelNumber {};

        PromiseWeakList mSendPromises;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderConnectionRelayChannelFactory
      #pragma mark

      interaction IFinderConnectionRelayChannelFactory
      {
        static IFinderConnectionRelayChannelFactory &singleton();

        virtual IFinderConnectionPtr connect(
                                             IFinderConnectionDelegatePtr delegate,
                                             const IPAddress &remoteFinderIP,
                                             ITransportStreamPtr receiveStream,
                                             ITransportStreamPtr sendStream
                                             );

        virtual IFinderConnectionRelayChannelPtr connect(
                                                         IFinderConnectionRelayChannelDelegatePtr delegate,
                                                         const IPAddress &remoteFinderIP,
                                                         const char *localContextID,
                                                         const char *remoteContextID,
                                                         const char *relayDomain,
                                                         const Token &relayToken,
                                                         ITransportStreamPtr receiveStream,
                                                         ITransportStreamPtr sendStream
                                                         );
      };

      class FinderConnectionRelayChannelFactory : public IFactory<IFinderConnectionRelayChannelFactory> {};
      
    }
  }
}
