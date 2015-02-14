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

#include <openpeer/stack/internal/stack_FinderConnection.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_FinderRelayChannel.h>

#include <openpeer/stack/message/peer-finder/ChannelMapRequest.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IHTTP.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/Promise.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

#define OPENPEER_STACK_CHANNEL_MAP_REQUEST_TIMEOUT_SECONDS (60*2)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      typedef ITCPMessaging::ChannelHeader ChannelHeader;
      typedef ITCPMessaging::ChannelHeaderPtr ChannelHeaderPtr;

      typedef ITransportStream::StreamHeader StreamHeader;
      typedef ITransportStream::StreamHeaderPtr StreamHeaderPtr;

      using peer_finder::ChannelMapRequest;
      using peer_finder::ChannelMapRequestPtr;

//      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnectionManager
      #pragma mark

      class FinderConnectionManager : public SharedRecursiveLock
      {
      public:
        friend class FinderConnection;

        typedef String RemoteIPString;
        typedef std::map<RemoteIPString, FinderConnectionPtr> FinderConnectionMap;

      protected:
        //---------------------------------------------------------------------
        FinderConnectionManager() :
          SharedRecursiveLock(SharedRecursiveLock::create())
        {
        }

        //---------------------------------------------------------------------
        void init()
        {
        }

        //---------------------------------------------------------------------
        static FinderConnectionManagerPtr create()
        {
          FinderConnectionManagerPtr pThis(new FinderConnectionManager);
          return pThis;
        }

      public:
        //---------------------------------------------------------------------
        static FinderConnectionManagerPtr singleton()
        {
          static SingletonLazySharedPtr<FinderConnectionManager> singleton(create());
          FinderConnectionManagerPtr result = singleton.singleton();
          if (!result) {
            ZS_LOG_WARNING(Detail, slog("singleton gone"))
          }
          return result;
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnectionManager => friend FinderConnection
        #pragma mark

        // (duplicate) virtual RecursiveLock &*this const;

        //---------------------------------------------------------------------
        FinderConnectionPtr find(const IPAddress &remoteIP)
        {
          FinderConnectionMap::iterator found = mRelays.find(remoteIP.string());
          if (found == mRelays.end()) return FinderConnectionPtr();

          return (*found).second;
        }

        //---------------------------------------------------------------------
        void add(
                 const IPAddress &remoteIP,
                 FinderConnectionPtr relay
                 )
        {
          mRelays[remoteIP.string()] = relay;
        }

        //---------------------------------------------------------------------
        void remove(const IPAddress &remoteIP)
        {
          FinderConnectionMap::iterator found = mRelays.find(remoteIP.string());
          if (found == mRelays.end()) return;

          mRelays.erase(found);
        }

        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "stack::FinderConnectionManager");
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnectionManager => (internal)
        #pragma mark

        //---------------------------------------------------------------------
        virtual const SharedRecursiveLock &getLock() const {return *this;}

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark FinderConnectionManager => (data)
        #pragma mark

        FinderConnectionManagerWeakPtr mThisWeak;

        FinderConnectionMap mRelays;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection
      #pragma mark

      //-----------------------------------------------------------------------
      FinderConnection::FinderConnection(
                                         FinderConnectionManagerPtr outer,
                                         IMessageQueuePtr queue,
                                         IPAddress remoteFinderIP
                                         ) :
        zsLib::MessageQueueAssociator(queue),
        SharedRecursiveLock(outer ? *outer : SharedRecursiveLock(SharedRecursiveLock::create())),
        mOuter(outer),
        mCurrentState(SessionState_Pending),
        mRemoteIP(remoteFinderIP),
        mLastSentData(zsLib::now()),
        mSendKeepAliveAfter(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_FINDER_CONNECTION_MUST_SEND_PING_IF_NO_SEND_ACTIVITY_IN_SECONDS)))
      {
        ZS_LOG_DETAIL(log("created"))

        if (mSendKeepAliveAfter < Seconds(1)) {
          mSendKeepAliveAfter = Seconds();
        }
      }

      //-----------------------------------------------------------------------
      void FinderConnection::init()
      {
        AutoRecursiveLock lock(*this);

        mWireReceiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock())->getReader();
        mWireSendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr())->getWriter();

        mTCPMessaging = ITCPMessaging::connect(mThisWeak.lock(), mWireReceiveStream->getStream(), mWireSendStream->getStream(), true, mRemoteIP);

        if (Seconds() != mSendKeepAliveAfter) {
          mPingTimer = Timer::create(mThisWeak.lock(), Seconds(5));
        }

        step();
      }

      //-----------------------------------------------------------------------
      FinderConnection::~FinderConnection()
      {
        ZS_LOG_DETAIL(log("destroyed"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      FinderConnectionPtr FinderConnection::convert(IFinderConnectionPtr connection)
      {
        return ZS_DYNAMIC_PTR_CAST(FinderConnection, connection);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => friend FinderConnectionManager
      #pragma mark

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelPtr FinderConnection::connect(
                                                                 IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                 const IPAddress &remoteFinderIP,
                                                                 const char *localContextID,
                                                                 const char *remoteContextID,
                                                                 const char *relayDomain,
                                                                 const Token &relayToken,
                                                                 ITransportStreamPtr receiveStream,
                                                                 ITransportStreamPtr sendStream
                                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(remoteFinderIP.isAddressEmpty())
        ZS_THROW_INVALID_ARGUMENT_IF(remoteFinderIP.isPortEmpty())
        ZS_THROW_INVALID_ARGUMENT_IF(!localContextID)
        ZS_THROW_INVALID_ARGUMENT_IF(!remoteContextID)
        ZS_THROW_INVALID_ARGUMENT_IF(!relayDomain)
        ZS_THROW_INVALID_ARGUMENT_IF(!relayToken.hasData())
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        RecursiveLock bogusLock;

        FinderConnectionManagerPtr manager = FinderConnectionManager::singleton();

        FinderConnectionPtr existing = manager ? manager->find(remoteFinderIP) : FinderConnectionPtr();

        if (existing) {
          ZS_LOG_DEBUG(existing->log("reusing existing connection"))
          return existing->connect(delegate, localContextID, remoteContextID, relayDomain, relayToken, receiveStream, sendStream);
        }

        FinderConnectionPtr pThis(new FinderConnection(manager, UseStack::queueStack(), remoteFinderIP));
        pThis->mThisWeak = pThis;
        pThis->init();

        if (manager) {
          manager->add(remoteFinderIP, pThis);
        }

        return pThis->connect(delegate, localContextID, remoteContextID, relayDomain, relayToken, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => IFinderConnection
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr FinderConnection::toDebug(IFinderConnectionPtr connection)
      {
        if (!connection) return ElementPtr();

        FinderConnectionPtr pThis = FinderConnection::convert(connection);
        return pThis->toDebug();
      }

      //-----------------------------------------------------------------------
      IFinderConnectionPtr FinderConnection::connect(
                                                     IFinderConnectionDelegatePtr delegate,
                                                     const IPAddress &remoteFinderIP,
                                                     ITransportStreamPtr receiveStream,
                                                     ITransportStreamPtr sendStream
                                                     )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        FinderConnectionPtr pThis(new FinderConnection(FinderConnectionManagerPtr(), UseStack::queueStack(), remoteFinderIP));
        pThis->mThisWeak = pThis;

        AutoRecursiveLock lock(*pThis);

        if (delegate) {
          pThis->mDefaultSubscription = pThis->subscribe(delegate);
        }

        pThis->init();

        ChannelPtr channel = Channel::incoming(pThis, pThis, receiveStream, sendStream, 0);
        pThis->mChannels[0] = channel;

        return pThis;
      }

      //-----------------------------------------------------------------------
      IFinderConnectionSubscriptionPtr FinderConnection::subscribe(IFinderConnectionDelegatePtr originalDelegate)
      {
        ZS_LOG_TRACE(log("subscribe called"))

        AutoRecursiveLock lock(*this);

        if (!originalDelegate) return mDefaultSubscription;

        IFinderConnectionSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate, UseStack::queueDelegate());

        IFinderConnectionDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          FinderConnectionPtr pThis(mThisWeak.lock());

          if (SessionState_Pending != mCurrentState) {
            delegate->onFinderConnectionStateChanged(pThis, mCurrentState);
          }

          if (isShutdown()) {
            ZS_LOG_WARNING(Detail, log("subscription created after shutdown"))
            return subscription;
          }

          for (ChannelMap::iterator iter = mIncomingChannels.begin(); iter != mIncomingChannels.end(); ++iter)
          {
            delegate->onFinderConnectionIncomingRelayChannel(pThis);
          }
        }
        
        return subscription;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        setState(SessionState_Shutdown);

        mThisWeak.reset();

        mSubscriptions.clear();
        if (mDefaultSubscription) {
          mDefaultSubscription->cancel();
          mDefaultSubscription.reset();
        }

        FinderConnectionManagerPtr outer = mOuter.lock();
        if (outer) {
          outer->remove(mRemoteIP);
        }

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
          mTCPMessaging.reset();
        }

        mWireReceiveStream->cancel();
        mWireSendStream->cancel();

        if (mPingTimer) {
          mPingTimer->cancel();
          mPingTimer.reset();
        }

        // scope: clear out channels
        {
          for (ChannelMap::iterator iter = mChannels.begin(); iter != mChannels.end(); ++iter)
          {
            ChannelPtr channel = (*iter).second;
            channel->cancel();
          }
          mChannels.clear();
        }

        mPendingMapRequest.clear();
        mIncomingChannels.clear();
        mRemoveChannels.clear();

        if (mMapRequestChannelMonitor) {
          mMapRequestChannelMonitor->cancel();
          mMapRequestChannelMonitor.reset();
        }

        rejectAllPromises(mSendPromises);
      }

      //-----------------------------------------------------------------------
      FinderConnection::SessionStates FinderConnection::getState(
                                                                 WORD *outLastErrorCode,
                                                                 String *outLastErrorReason
                                                                 ) const
      {
        AutoRecursiveLock lock(*this);
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      IFinderRelayChannelPtr FinderConnection::accept(
                                                      IFinderRelayChannelDelegatePtr delegate,
                                                      AccountPtr account,
                                                      ITransportStreamPtr receiveStream,
                                                      ITransportStreamPtr sendStream,
                                                      ChannelNumber *outChannelNumber
                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        if (outChannelNumber) {
          *outChannelNumber = 0;
        }

        ZS_LOG_DEBUG(log("accept called"))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot accept channel as already shutdown"))
          return IFinderRelayChannelPtr();
        }

        stepCleanRemoval();

        if (mIncomingChannels.size() < 1) {
          ZS_LOG_WARNING(Detail, log("no pending channels found"))
          return IFinderRelayChannelPtr();
        }

        ChannelMap::iterator found = mIncomingChannels.begin();

        ChannelNumber channelNumber = (*found).first;
        ChannelPtr channel = (*found).second;

        ZS_LOG_DEBUG(log("accepting channel") + ZS_PARAM("channel number", channelNumber))

        ITransportStreamPtr wireReceiveStream;
        ITransportStreamPtr wireSendStream;

        channel->getStreams(wireReceiveStream, wireSendStream);

        UseFinderRelayChannelPtr relay = UseFinderRelayChannel::createIncoming(delegate, account, receiveStream, sendStream, wireReceiveStream, wireSendStream);
        channel->notifyIncomingFinderRelayChannel(FinderRelayChannel::convert(relay));

        mIncomingChannels.erase(found);

        if (mSendStreamNotifiedReady) {
          ZS_LOG_DEBUG(log("notify channel that it's now write ready"))
          channel->notifyReceivedWireWriteReady();
        }

        if (outChannelNumber) {
          *outChannelNumber = channelNumber;
        }

        return FinderRelayChannel::convert(relay);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(*this);

        if (timer != mPingTimer) {
          ZS_LOG_WARNING(Detail, log("notified about an obsolete timer") + ZS_PARAM("timer ID", timer->getID()))
          return;
        }

        Time tick = zsLib::now();

        if (mLastSentData + mSendKeepAliveAfter > tick) {
          ZS_LOG_INSANE(log("activity within window thus no need to send ping"))
          return;
        }

        if (!mWireSendStream) {
          ZS_LOG_WARNING(Detail, log("wire stream gone"))
          return;
        }

        ChannelHeaderPtr header(new ChannelHeader);
        header->mChannelID = static_cast<decltype(header->mChannelID)>(0);

        mWireSendStream->write((const BYTE *)"\n", sizeof(char), header);

        mLastSentData = tick;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::onWake()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => ITCPMessagingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::onTCPMessagingStateChanged(
                                                        ITCPMessagingPtr messaging,
                                                        ITCPMessaging::SessionStates state
                                                        )
      {
        AutoRecursiveLock lock(*this);
        if (ITCPMessaging::SessionState_Connected == state) {
          ZS_LOG_TRACE(log("enabling TCP keep-alive"))
          messaging->enableKeepAlive();
        }
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("on transport stream write ready"))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return;
        }

        if (writer == mWireSendStream) {
          mSendStreamNotifiedReady = true;

          resolveAllPromises(mSendPromises);

          for (ChannelMap::iterator iter = mChannels.begin(); iter != mChannels.end(); ++iter)
          {
            ChannelNumber channelNumber = (*iter).first;
            ChannelPtr &channel = (*iter).second;

            ChannelMap::iterator found = mPendingMapRequest.find(channelNumber);
            if (found != mPendingMapRequest.end()) {
              ZS_LOG_DEBUG(log("cannot notify about write ready because channel map request has not completed yet") + ZS_PARAM("channel number", channelNumber))
              continue;
            }
            channel->notifyReceivedWireWriteReady();
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => ITransportStreamReaderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on transport stream read ready"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => IFinderConnectionRelayChannelDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::onFinderConnectionRelayChannelStateChanged(
                                                                        IFinderConnectionRelayChannelPtr channel,
                                                                        IFinderConnectionRelayChannel::SessionStates state
                                                                        )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("notified master relay channel state changed") + ZS_PARAM("channel", channel->getID()))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => IMessageMonitorResultDelegate<ChannelMapResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool FinderConnection::handleMessageMonitorResultReceived(
                                                                IMessageMonitorPtr monitor,
                                                                ChannelMapResultPtr result
                                                                )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mMapRequestChannelMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_DEBUG(log("channel map request completed successfully") + ZS_PARAM("channel number", mMapRequestChannelNumber))

        mMapRequestChannelMonitor.reset();

        // scope: remove from pending list
        {
          ChannelMap::iterator found = mPendingMapRequest.find(mMapRequestChannelNumber);
          if (found != mPendingMapRequest.end()) {
            ZS_LOG_TRACE(log("removed from pending map"))

            if (mSendStreamNotifiedReady) {
              ChannelPtr channel = (*found).second;
              ZS_THROW_BAD_STATE_IF(!channel)
              ZS_LOG_DEBUG(log("notifying channel of write ready"))
              channel->notifyReceivedWireWriteReady();
            }
            mPendingMapRequest.erase(found);
          }
        }

        mMapRequestChannelNumber = 0;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::handleMessageMonitorErrorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     ChannelMapResultPtr ignore, // will always be NULL
                                                                     message::MessageResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mMapRequestChannelMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("channel map request failed") + ZS_PARAM("channel number", mMapRequestChannelNumber))

        mMapRequestChannelMonitor.reset();

        // scope: remove from channels list
        {
          ChannelMap::iterator found = mChannels.find(mMapRequestChannelNumber);
          if (found != mChannels.end()) {
            ZS_LOG_TRACE(log("removed from channels map"))
            ChannelPtr channel = (*found).second;
            ZS_THROW_BAD_STATE_IF(!channel)
            channel->cancel();
            mChannels.erase(found);
          }
        }

        // scope: remove from pending list
        {
          ChannelMap::iterator found = mPendingMapRequest.find(mMapRequestChannelNumber);
          if (found != mPendingMapRequest.end()) {
            ZS_LOG_TRACE(log("removed from pending map"))
            mPendingMapRequest.erase(found);
          }
        }

        mMapRequestChannelNumber = 0;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => friend ChannelOutgoing
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::notifyOuterWriterReady()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("notify outer writer ready"))

        if (mWireReceiveStream) {
          mWireReceiveStream->notifyReaderReadyToRead();
        }
      }

      //-----------------------------------------------------------------------
      void FinderConnection::sendBuffer(
                                        ChannelNumber channelNumber,
                                        SecureByteBlockPtr buffer
                                        )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!buffer)

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot send data while shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("send buffer called") + ZS_PARAM("channel number", channelNumber) + ZS_PARAM("buffer size", buffer->SizeInBytes()))

        ChannelHeaderPtr header(new ChannelHeader);
        header->mChannelID = static_cast<decltype(header->mChannelID)>(channelNumber);

        mWireSendStream->write(buffer, header);

        mLastSentData = zsLib::now();
      }

      //-----------------------------------------------------------------------
      void FinderConnection::notifyDestroyed(ChannelNumber channelNumber)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("channel is destroyed") + ZS_PARAM("channel number", channelNumber))

        if (isShutdown()) {
          ZS_LOG_WARNING(Trace, log("finder connection already destroyed (probably okay)"))
          return;
        }

        mRemoveChannels[channelNumber] = ChannelPtr();
        
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection => (internal)
      #pragma mark


      //-----------------------------------------------------------------------
      bool FinderConnection::isFinderSessionConnection() const
      {
        return !(mOuter.lock());  // if points to outer, this is a relay-only channel
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::isFinderRelayConnection() const
      {
        return !isFinderSessionConnection();
      }

      //-----------------------------------------------------------------------
      Log::Params FinderConnection::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("FinderConnection");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params FinderConnection::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr FinderConnection::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("FinderConnection");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "outer", (bool)mOuter.lock());

        IHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());
        IHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, "last error", mLastError);
        IHelper::debugAppend(resultEl, "last reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, "remote ip", mRemoteIP.string());
        IHelper::debugAppend(resultEl, "tcp messaging", ITCPMessaging::toDebug(mTCPMessaging));

        IHelper::debugAppend(resultEl, "wire recv stream", ITransportStream::toDebug(mWireReceiveStream->getStream()));
        IHelper::debugAppend(resultEl, "wire send stream", ITransportStream::toDebug(mWireSendStream->getStream()));

        IHelper::debugAppend(resultEl, "send stream notified ready", mSendStreamNotifiedReady);

        IHelper::debugAppend(resultEl, "send keep alive (s)", mSendKeepAliveAfter);
        IHelper::debugAppend(resultEl, "last sent data", mLastSentData);
        IHelper::debugAppend(resultEl, "ping timer", (bool)mPingTimer);

        IHelper::debugAppend(resultEl, "channels", mChannels.size());

        IHelper::debugAppend(resultEl, "pending map request channels", mPendingMapRequest.size());
        IHelper::debugAppend(resultEl, "incoming channels", mIncomingChannels.size());
        IHelper::debugAppend(resultEl, "remove channels", mRemoveChannels.size());

        IHelper::debugAppend(resultEl, "map request monitor", (bool)mMapRequestChannelMonitor);
        IHelper::debugAppend(resultEl, "map request channel number", mMapRequestChannelNumber);

        IHelper::debugAppend(resultEl, "send promises", mSendPromises.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DETAIL(debug("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))

        mCurrentState = state;
        FinderConnectionPtr pThis = mThisWeak.lock();

        if (pThis) {
          mSubscriptions.delegate()->onFinderConnectionStateChanged(pThis, mCurrentState);
        }
      }

      //-----------------------------------------------------------------------
      void FinderConnection::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, debug("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      void FinderConnection::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step continue to shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepCleanRemoval()) return;
        if (!stepConnectWire()) return;
        if (!stepMasterChannel()) return;
        if (!stepChannelMapRequest()) return;
        if (!stepReceiveData()) return;

        setState(SessionState_Connected);

        if (!stepSelfDestruct()) return;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepCleanRemoval()
      {
        if (mRemoveChannels.size() < 1) {
          ZS_LOG_TRACE(log("no channels to remove"))
          return true;
        }

        for (ChannelMap::iterator iter = mRemoveChannels.begin(); iter != mRemoveChannels.end(); ++iter) {
          ChannelNumber channelNumber = (*iter).first;

          ZS_LOG_DEBUG(log("removing channel") + ZS_PARAM("channel number", channelNumber))

          bool foundInPendingMapRequest = false;

          // scope: remove from pending map request
          {
            ChannelMap::iterator found = mPendingMapRequest.find(channelNumber);
            if (found != mPendingMapRequest.end()) {
              ZS_LOG_DEBUG(log("removing channel from pending map request"))
              foundInPendingMapRequest = true;
              mPendingMapRequest.erase(found);
            }
          }

          if ((mMapRequestChannelMonitor) &&
              (mMapRequestChannelNumber == channelNumber)) {
            ZS_LOG_WARNING(Detail, log("outstanding channel map request is being cancelled for channel"))
            mMapRequestChannelMonitor->cancel();
            mMapRequestChannelMonitor.reset();
          }

          // scope: remove from incoming channels
          {
            ChannelMap::iterator found = mIncomingChannels.find(channelNumber);
            if (found != mIncomingChannels.end()) {
              ZS_LOG_DEBUG(log("removing channel from incoming channels"))
              mIncomingChannels.erase(found);
            }
          }

          // scope: remove from channels
          {
            ChannelMap::iterator found = mChannels.find(channelNumber);
            if (found != mChannels.end()) {
              ZS_LOG_DEBUG(log("removing channel from channels"))

              ChannelPtr channel = (*found).second;

              ZS_THROW_BAD_STATE_IF(!channel)

              channel->cancel();

              if (0 == channelNumber) {
                // in this special case, everything must shutdown...
                WORD errorCode = 0;
                String errorReason;
                channel->getState(&errorCode, &errorReason);

                ZS_LOG_DEBUG(log("master relay channel is shutdown (so must now self destruct)") + ZS_PARAM("error code", errorCode) + ZS_PARAM("reason", errorReason))

                setError(errorCode, errorReason);
                cancel();
                return false;
              }

              mChannels.erase(found);
            }
          }

          if (!foundInPendingMapRequest) {
            ZS_LOG_DEBUG(log("will notify channel is closed") + ZS_PARAM("channel", channelNumber))

            // notify remote party of channel closure
            ChannelHeaderPtr header(new ChannelHeader);
            header->mChannelID = static_cast<decltype(header->mChannelID)>(channelNumber);

            SecureByteBlockPtr buffer(new SecureByteBlock);

            // by writing a buffer of "0" size to the channel number, it will cause the channel to close
            mWireSendStream->write(buffer, header);

            mLastSentData = zsLib::now();
          }

        }

        mRemoveChannels.clear();

        ZS_LOG_TRACE(log("remove channels completed"))

        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepConnectWire()
      {
        WORD error = 0;
        String reason;
        switch (mTCPMessaging->getState(&error, &reason))
        {
          case ITCPMessaging::SessionState_Pending:
          {
            ZS_LOG_TRACE(log("waiting for TCP messaging to connect"))
            return false;
          }
          case ITCPMessaging::SessionState_Connected:
          {
            ZS_LOG_TRACE(log("TCP messaging connected"))
            break;
          }
          case ITCPMessaging::SessionState_ShuttingDown:
          case ITCPMessaging::SessionState_Shutdown:      {
            ZS_LOG_WARNING(Detail, log("TCP messaging is shutting down") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
            if (0 != error) {
              setError(error, reason);
            }
            cancel();
            return false;
          }
        }

        if (!mSendStreamNotifiedReady) {
          ZS_LOG_TRACE(log("waiting for TCP messaging send stream to be notified as ready"))
          return false;
        }

        ZS_LOG_TRACE("TCP messaging notified write ready")
        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepMasterChannel()
      {
        if (isFinderRelayConnection()) {
          ZS_LOG_TRACE(log("finder relay connections do not have a master channel"))
          return true;
        }

        ChannelMap::iterator found = mChannels.find(0);
        if (found == mChannels.end()) {
          ZS_LOG_WARNING(Detail, log("could not find master channel thus must shutdown"))
          setError(IHTTP::HTTPStatusCode_NotFound, "could not find master channel");
          cancel();
          return false;
        }

        const ChannelPtr &channel = (*found).second;

        WORD error = 0;
        String reason;

        switch (channel->getState(&error, &reason)) {
          case IFinderConnectionRelayChannel::SessionState_Pending:   {
            ZS_LOG_TRACE(log("waiting for master channel to connect"))
            return false;
          }
          case IFinderConnectionRelayChannel::SessionState_Connected: break;
          case IFinderConnectionRelayChannel::SessionState_Shutdown:  {
            ZS_LOG_WARNING(Detail, log("finder channel is shutdown thus finder connection must shutdown"))
            setError(error, reason);
            return false;
          }
        }

        ZS_LOG_TRACE(log("master channel connected"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepChannelMapRequest()
      {
        if (mMapRequestChannelMonitor) {
          ZS_LOG_TRACE(log("pending channel map request is already outstanding"))
          return true;
        }
        if (mPendingMapRequest.size() < 1) {
          ZS_LOG_TRACE(log("no pending channels needing to be notified"))
          return true;
        }

        ChannelMap::iterator found = mPendingMapRequest.begin();
        ZS_THROW_BAD_STATE_IF(found == mPendingMapRequest.end())

        ChannelNumber channelNumber = (*found).first;
        ChannelPtr channel = (*found).second;

        ZS_LOG_DEBUG(log("sending channel map request") + ZS_PARAM("channel", channelNumber))

        const Channel::ConnectionInfo &info = channel->getConnectionInfo();

        ChannelMapRequestPtr request = ChannelMapRequest::create();
        request->domain(info.mRelayDomain);
        request->channelNumber(static_cast<ChannelMapRequest::ChannelNumber>(channelNumber));
        request->localContextID(info.mLocalContextID);
        request->remoteContextID(info.mRemoteContextID);
        request->relayToken(info.mRelayToken);

        PromisePtr sendPromise = Promise::create(UseStack::queueDelegate());

        PromisePtr promise = Promise::create(UseStack::queueDelegate());
        mMapRequestChannelMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<ChannelMapResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_CHANNEL_MAP_REQUEST_TIMEOUT_SECONDS), promise);
        mMapRequestChannelNumber = channelNumber;

        DocumentPtr doc = request->encode();

        size_t outputLength = 0;
        GeneratorPtr generator = Generator::createJSONGenerator();
        std::unique_ptr<char[]> output = generator->write(doc, &outputLength);

        ChannelHeaderPtr header(new ChannelHeader);
        header->mChannelID = 0;

        mWireSendStream->write((const BYTE *) (output.get()), outputLength, header);

        mSendPromises.push_back(promise);

        mLastSentData = zsLib::now();

        if (ZS_IS_LOGGING(Debug)) {
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log(") ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) )"))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log("MESSAGE INFO") + Message::toDebug(request))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log("CHANNEL SEND MESSAGE") + ZS_PARAM("json out", ((CSTR)(output.get()))))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_DETAIL(log(") ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) ) )"))
          ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
        }

        mPendingMapRequest.erase(found);
        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepReceiveData()
      {
        if (mWireReceiveStream->getTotalReadBuffersAvailable() < 1) {
          ZS_LOG_TRACE(log("no data to read"))
          return true;
        }

        while (mWireReceiveStream->getTotalReadBuffersAvailable() > 0) {
          StreamHeaderPtr header;
          SecureByteBlockPtr buffer = mWireReceiveStream->read(&header);

          ChannelHeaderPtr channelHeader = ChannelHeader::convert(header);

          if ((!buffer) ||
              (!channelHeader)) {
            ZS_LOG_WARNING(Detail, log("failed to read buffer"))
            setError(IHTTP::HTTPStatusCode_InternalServerError, "failed to read buffer");
            cancel();
            return false;
          }

          ZS_LOG_TRACE(log("received data") + ZS_PARAM("channel number", channelHeader->mChannelID) + ZS_PARAM("size", buffer->SizeInBytes()))

          ChannelMap::iterator found = mChannels.find(channelHeader->mChannelID);
          ChannelPtr channel;
          if (found != mChannels.end()) {
            channel = (*found).second;
          }

          if (buffer->SizeInBytes() < 1) {
            // special close request
            if (found == mChannels.end()) {
              ZS_LOG_WARNING(Detail, log("channel closed but channel is not known"))
              continue;
            }
            ZS_LOG_DEBUG(log("channel is being shutdown"))
            channel->cancel();
            continue;
          }

          if (!channel) {
            if (isFinderRelayConnection()) {
              if (0 == channelHeader->mChannelID) {
                
                DocumentPtr document = Document::createFromAutoDetect((CSTR)(buffer->BytePtr()));
                MessagePtr message = Message::create(document, mThisWeak.lock());

                if (ZS_IS_LOGGING(Debug)) {
                  ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
                  ZS_LOG_DETAIL(log("( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ("))
                  ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
                  ZS_LOG_DETAIL(log("MESSAGE INFO") + Message::toDebug(message))
                  ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
                  ZS_LOG_DETAIL(log("CHANNEL MESSAGE") + ZS_PARAM("json in", ((CSTR)(buffer->BytePtr()))))
                  ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
                  ZS_LOG_DETAIL(log("( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ( ("))
                  ZS_LOG_DETAIL(log("-------------------------------------------------------------------------------------------"))
                }

                if (!message) {
                  ZS_LOG_WARNING(Detail, log("failed to create a message object from incoming message"))
                  continue;
                }

                if (IMessageMonitor::handleMessageReceived(message)) {
                  ZS_LOG_DEBUG(log("handled message via message handler"))
                  continue;
                }

                ZS_LOG_WARNING(Detail, log("message not understood"))
                continue;
              }

              ZS_LOG_WARNING(Detail, log("received data on an non-mapped relay channel (thus ignoring)") + ZS_PARAM("channel", channelHeader->mChannelID))
              continue;
            }

            ZS_LOG_DEBUG(log("notified of new incoming channel"))

            // this is s a new incoming channel
            ITransportStreamPtr receiveStream = ITransportStream::create();
            ITransportStreamPtr sendStream = ITransportStream::create();
            channel = Channel::incoming(mThisWeak.lock(), IFinderConnectionRelayChannelDelegatePtr(), receiveStream, sendStream, channelHeader->mChannelID);

            mChannels[channelHeader->mChannelID] = channel;
            mIncomingChannels[channelHeader->mChannelID] = channel;
            mSubscriptions.delegate()->onFinderConnectionIncomingRelayChannel(mThisWeak.lock());
          }

          channel->notifyDataReceived(buffer);
        }

        ZS_LOG_TRACE(log("receive complete"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::stepSelfDestruct()
      {
        if (mChannels.size() > 0) {
          ZS_LOG_TRACE(log("has active channels"))
          return true;
        }

        if ((mPendingMapRequest.size() > 0) ||
            (mMapRequestChannelMonitor)) {
          ZS_LOG_TRACE(log("has pending map request"))
          return true;
        }

        if (mIncomingChannels.size() > 0) {
          ZS_LOG_TRACE(log("has pending incoming channels"))
          return true;
        }

        if (mRemoveChannels.size() > 0) {
          ZS_LOG_TRACE(log("has pending remove channels"))
          return true;
        }

        ZS_LOG_DEBUG(log("no more activity detected on finder connection thus going to self destruct now"))

        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelPtr FinderConnection::connect(
                                                                 IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                 const char *localContextID,
                                                                 const char *remoteContextID,
                                                                 const char *relayDomain,
                                                                 const Token &relayToken,
                                                                 ITransportStreamPtr receiveStream,
                                                                 ITransportStreamPtr sendStream
                                                                 )
      {
        ZS_THROW_INVALID_USAGE_IF(isFinderSessionConnection())

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return IFinderConnectionRelayChannelPtr();
        }

        ChannelNumber channelNumber = static_cast<ChannelNumber>(zsLib::createPUID());

        ChannelPtr channel = Channel::connect(mThisWeak.lock(), delegate, localContextID, remoteContextID, relayDomain, relayToken, receiveStream, sendStream, channelNumber);

        mChannels[channelNumber] = channel;
        mPendingMapRequest[channelNumber] = channel;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return channel;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::resolveAllPromises(PromiseWeakList &promises)
      {
        Promise::resolveAll(promises);
        promises.clear();
      }

      //-----------------------------------------------------------------------
      void FinderConnection::rejectAllPromises(PromiseWeakList &promises)
      {
        Promise::rejectAll(promises, PromiseRejectionStatus::create(IHTTP::HTTPStatusCode_Gone));
        promises.clear();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel
      #pragma mark

      //-----------------------------------------------------------------------
      FinderConnection::Channel::Channel(
                                         IMessageQueuePtr queue,
                                         FinderConnectionPtr outer,
                                         IFinderConnectionRelayChannelDelegatePtr delegate,
                                         ITransportStreamPtr receiveStream,
                                         ITransportStreamPtr sendStream,
                                         ChannelNumber channelNumber
                                         ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mCurrentState(SessionState_Pending),
        mOuterReceiveStream(receiveStream->getWriter()),
        mOuterSendStream(sendStream->getReader()),
        mChannelNumber(channelNumber)
      {
        ZS_LOG_DEBUG(log("created"))
        if (delegate) {
          mDelegate = IFinderConnectionRelayChannelDelegateProxy::createWeak(UseStack::queueDelegate(), delegate);
        }
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::init()
      {
        AutoRecursiveLock lock(*this);

        mOuterReceiveStreamSubscription = mOuterReceiveStream->subscribe(mThisWeak.lock());
        mOuterSendStreamSubscription = mOuterSendStream->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      FinderConnection::Channel::~Channel()
      {
        ZS_LOG_DEBUG(log("destroyed"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      FinderConnection::ChannelPtr FinderConnection::Channel::convert(IFinderConnectionRelayChannelPtr channel)
      {
        return ZS_DYNAMIC_PTR_CAST(Channel, channel);
      }

      //-----------------------------------------------------------------------
      ElementPtr FinderConnection::Channel::toDebug(IFinderConnectionRelayChannelPtr channel)
      {
        if (!channel) return ElementPtr();

        ChannelPtr pThis = Channel::convert(channel);
        return pThis->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => IFinderConnectionRelayChannel
      #pragma mark

      //-----------------------------------------------------------------------
      FinderConnection::ChannelPtr FinderConnection::Channel::connect(
                                                                      FinderConnectionPtr outer,
                                                                      IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                      const char *localContextID,
                                                                      const char *remoteContextID,
                                                                      const char *relayDomain,
                                                                      const Token &relayToken,
                                                                      ITransportStreamPtr receiveStream,
                                                                      ITransportStreamPtr sendStream,
                                                                      ULONG channelNumber
                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!outer)
        ZS_THROW_INVALID_ARGUMENT_IF(!localContextID)
        ZS_THROW_INVALID_ARGUMENT_IF(!remoteContextID)
        ZS_THROW_INVALID_ARGUMENT_IF(!relayDomain)
        ZS_THROW_INVALID_ARGUMENT_IF(!relayToken.hasData())
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        ChannelPtr pThis(new Channel(UseStack::queueStack(), outer, delegate, receiveStream, sendStream, channelNumber));
        pThis->mThisWeak = pThis;
        pThis->mConnectionInfo.mLocalContextID = String(localContextID);
        pThis->mConnectionInfo.mRemoteContextID = String(remoteContextID);
        pThis->mConnectionInfo.mRelayDomain = String(relayDomain);
        pThis->mConnectionInfo.mRelayToken = relayToken;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::cancel()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) return;

        setState(SessionState_Shutdown);

        FinderConnectionPtr connection = mOuter.lock();
        if (connection) {
          connection->notifyDestroyed(mChannelNumber);
        }

        mDelegate.reset();

        mOuterReceiveStream->cancel();
        mOuterSendStream->cancel();

        if (mRelayChannelSubscription) {
          mRelayChannelSubscription->cancel();
          mRelayChannelSubscription.reset();
        }
      }

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannel::SessionStates FinderConnection::Channel::getState(
                                                                                       WORD *outLastErrorCode,
                                                                                       String *outLastErrorReason
                                                                                       ) const
      {
        AutoRecursiveLock lock(*this);
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("notified write ready"))
        if (writer == mOuterReceiveStream) {
          if (!mOuterStreamNotifiedReady) {
            mOuterStreamNotifiedReady = true;

            FinderConnectionPtr outer = mOuter.lock();
            if (outer) {
              outer->notifyOuterWriterReady();
            }
          }
        }
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => IFinderRelayChannelDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::onFinderRelayChannelStateChanged(
                                                                       IFinderRelayChannelPtr channel,
                                                                       IFinderRelayChannel::SessionStates state
                                                                       )
      {
        ZS_LOG_TRACE(log("finder relay channel state changed") + ZS_PARAM("channel ID", channel->getID()) + ZS_PARAM("state", IFinderRelayChannel::toString(state)))

        switch (state) {
          case IFinderRelayChannel::SessionState_Pending:
          case IFinderRelayChannel::SessionState_Connected: {
            break;
          }
          case IFinderRelayChannel::SessionState_Shutdown:  {
            ZS_LOG_DEBUG(log("finder relay channel is shutdown thus finder channel must shutdown"))
            cancel();
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel)
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => ITransportStreamReaderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("notified read ready"))
        step();
      }

      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => friend FinderConnection
      #pragma mark

      FinderConnection::ChannelPtr FinderConnection::Channel::incoming(
                                                                       FinderConnectionPtr outer,
                                                                       IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                       ITransportStreamPtr receiveStream,
                                                                       ITransportStreamPtr sendStream,
                                                                       ULONG channelNumber
                                                                       )
      {
        ChannelPtr pThis(new Channel(UseStack::queueStack(), outer, delegate, receiveStream, sendStream, channelNumber));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::notifyIncomingFinderRelayChannel(FinderRelayChannelPtr inRelayChannel)
      {
        UseFinderRelayChannelPtr relayChannel = inRelayChannel;

        if (!relayChannel) {
          ZS_LOG_ERROR(Detail, log("incoming finder relay channel is missing"))
          return;
        }

        ZS_LOG_DEBUG(log("monitoring incoming finder relay channel for shutdown (to ensure related channel gets shutdown too)") + ZS_PARAM("finder relay channel", relayChannel->getID()))

        AutoRecursiveLock lock(*this);
        mRelayChannelSubscription = relayChannel->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::notifyReceivedWireWriteReady()
      {
        ZS_LOG_TRACE(log("notified received wire write ready"))
        mWireStreamNotifiedReady = true;
        if (mOuterSendStream) {
          mOuterSendStream->notifyReaderReadyToRead();
        }
        step();
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::notifyDataReceived(SecureByteBlockPtr buffer)
      {
        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot receive data as already shutdown"))
          return;
        }
        mOuterReceiveStream->write(buffer);
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::getStreams(
                                                 ITransportStreamPtr &outReceiveStream,
                                                 ITransportStreamPtr &outSendStream
                                                 )
      {
        outReceiveStream = mOuterReceiveStream->getStream();
        outSendStream = mOuterSendStream->getStream();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FinderConnection::Channel => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params FinderConnection::Channel::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("FinderConnection::Channel");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params FinderConnection::Channel::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr FinderConnection::Channel::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("FinderConnection::Channel");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "state", IFinderConnectionRelayChannel::toString(mCurrentState));
        IHelper::debugAppend(resultEl, "last error", mLastError);
        IHelper::debugAppend(resultEl, "last reason", mLastErrorReason);
        IHelper::debugAppend(resultEl, "channel number", mChannelNumber, false);
        IHelper::debugAppend(resultEl, "outer recv stream", ITransportStream::toDebug(mOuterReceiveStream->getStream()));
        IHelper::debugAppend(resultEl, "outer send stream", ITransportStream::toDebug(mOuterSendStream->getStream()));
        IHelper::debugAppend(resultEl, "outer receive stream subscription", (bool)mOuterReceiveStreamSubscription);
        IHelper::debugAppend(resultEl, "outer send stream subscription", (bool)mOuterSendStreamSubscription);
        IHelper::debugAppend(resultEl, "wire stream notified ready", mWireStreamNotifiedReady);
        IHelper::debugAppend(resultEl, "outer stream notified ready", mOuterStreamNotifiedReady);
        IHelper::debugAppend(resultEl, "local context", mConnectionInfo.mLocalContextID);
        IHelper::debugAppend(resultEl, "remote context", mConnectionInfo.mRemoteContextID);
        IHelper::debugAppend(resultEl, "relay domain", mConnectionInfo.mRelayDomain);
        IHelper::debugAppend(resultEl, mConnectionInfo.mRelayToken.toDebug());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DETAIL(debug("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))

        mCurrentState = state;
        ChannelPtr pThis = mThisWeak.lock();

        if ((pThis) &&
            (mDelegate)) {
          try {
            mDelegate->onFinderConnectionRelayChannelStateChanged(pThis, mCurrentState);
          } catch (IFinderConnectionRelayChannelDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void FinderConnection::Channel::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, debug("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      void FinderConnection::Channel::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step continue to shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepSendData()) return;

        setState(SessionState_Connected);
      }

      //-----------------------------------------------------------------------
      bool FinderConnection::Channel::stepSendData()
      {
        if (!mWireStreamNotifiedReady) {
          ZS_LOG_TRACE(log("have not received wire ready to send yet"))
          return false;
        }

        if (mOuterSendStream->getTotalReadBuffersAvailable() < 1) {
          ZS_LOG_TRACE(log("no data to send"))
          return true;
        }

        FinderConnectionPtr connection = mOuter.lock();
        if (!connection) {
          ZS_LOG_WARNING(Detail, log("connection is gone, must shutdown"))
          setError(IHTTP::HTTPStatusCode_NotFound, "connection object gone");
          cancel();
          return false;
        }

        while (mOuterSendStream->getTotalReadBuffersAvailable() > 0) {
          SecureByteBlockPtr buffer = mOuterSendStream->read();
          if (buffer->SizeInBytes() < 1) {
            ZS_LOG_WARNING(Detail, log("no data read"))
            continue;
          }

          ZS_LOG_TRACE(log("buffer to send read") + ZS_PARAM("size", buffer->SizeInBytes()))
          connection->sendBuffer(mChannelNumber, buffer);
        }

        ZS_LOG_TRACE(log("buffer to send read complete"))
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderConnection
      #pragma mark

      //-----------------------------------------------------------------------
      const char *IFinderConnection::toString(SessionStates state)
      {
        switch (state)
        {
          case SessionState_Pending:      return "Pending";
          case SessionState_Connected:    return "Connected";
          case SessionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      IFinderConnectionPtr IFinderConnection::connect(
                                                      IFinderConnectionDelegatePtr delegate,
                                                      const IPAddress &remoteFinderIP,
                                                      ITransportStreamPtr receiveStream,
                                                      ITransportStreamPtr sendStream
                                                      )
      {
        return internal::IFinderConnectionRelayChannelFactory::singleton().connect(delegate, remoteFinderIP, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderConnectionRelayChannel
      #pragma mark

      //-----------------------------------------------------------------------
      const char *IFinderConnectionRelayChannel::toString(SessionStates state)
      {
        switch (state)
        {
          case SessionState_Pending:      return "Pending";
          case SessionState_Connected:    return "Connected";
          case SessionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelPtr IFinderConnectionRelayChannel::connect(
                                                                              IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                              const IPAddress &remoteFinderIP,
                                                                              const char *localContextID,
                                                                              const char *remoteContextID,
                                                                              const char *relayDomain,
                                                                              const Token &relayToken,
                                                                              ITransportStreamPtr receiveStream,
                                                                              ITransportStreamPtr sendStream
                                                                              )
      {
        return internal::IFinderConnectionRelayChannelFactory::singleton().connect(delegate, remoteFinderIP, localContextID, remoteContextID, relayDomain, relayToken, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderConnectionRelayChannelFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelFactory &IFinderConnectionRelayChannelFactory::singleton()
      {
        return FinderConnectionRelayChannelFactory::singleton();
      }

      //-----------------------------------------------------------------------
      IFinderConnectionPtr IFinderConnectionRelayChannelFactory::connect(
                                                                         IFinderConnectionDelegatePtr delegate,
                                                                         const IPAddress &remoteFinderIP,
                                                                         ITransportStreamPtr receiveStream,
                                                                         ITransportStreamPtr sendStream
                                                                         )
      {
        if (this) {}
        return FinderConnection::connect(delegate, remoteFinderIP, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelPtr IFinderConnectionRelayChannelFactory::connect(
                                                                                     IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                                     const IPAddress &remoteFinderIP,
                                                                                     const char *localContextID,
                                                                                     const char *remoteContextID,
                                                                                     const char *relayDomain,
                                                                                     const Token &relayToken,
                                                                                     ITransportStreamPtr receiveStream,
                                                                                     ITransportStreamPtr sendStream
                                                                                     )
      {
        if (this) {}
        return FinderConnection::connect(delegate, remoteFinderIP, localContextID, remoteContextID, relayDomain, relayToken, receiveStream, sendStream);
      }

    }
  }
}
