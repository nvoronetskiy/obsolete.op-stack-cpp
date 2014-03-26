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

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_AccountFinder.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>

#include <openpeer/stack/message/peer-finder/SessionDeleteRequest.h>
#include <openpeer/stack/message/peer-finder/SessionCreateRequest.h>
#include <openpeer/stack/message/peer-finder/SessionCreateResult.h>
#include <openpeer/stack/message/peer-finder/SessionKeepAliveRequest.h>
#include <openpeer/stack/message/peer-finder/SessionKeepAliveResult.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindRequest.h>

#include <openpeer/stack/message/MessageResult.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

#include <boost/shared_array.hpp>

#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

#ifndef _WIN32
#include <unistd.h>
#endif //_WIN32

#define OPENPEER_STACK_SESSION_CREATE_REQUEST_TIMEOUT_IN_SECONDS (60)

#define OPENPEER_STACK_SESSION_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS (60)

#define OPENPEER_STACK_SESSION_DELETE_REQUEST_TIMEOUT_IN_SECONDS (5)

#define OPENPEER_STACK_ACCOUNT_FINDER_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS (20)
#define OPENPEER_STACK_ACCOUNT_FINDER_EXPECT_SESSION_DATA_IN_SECONDS (90)

#define OPENPEER_STACK_ACCOUNT_BACKGROUNDING_TIMEOUT_IN_SECONDS (OPENPEER_STACK_ACCOUNT_FINDER_EXPECT_SESSION_DATA_IN_SECONDS + 40)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;
      using services::IWakeDelegateProxy;

      using message::peer_finder::SessionCreateRequest;
      using message::peer_finder::SessionCreateRequestPtr;
      using message::peer_finder::SessionKeepAliveRequest;
      using message::peer_finder::SessionKeepAliveRequestPtr;
      using message::peer_finder::SessionDeleteRequest;
      using message::peer_finder::SessionDeleteRequestPtr;

      typedef IAccountFinderForAccount::ForAccountPtr ForAccountPtr;

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
      #pragma mark IAccountFinderForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IAccountFinderForAccount::toDebug(ForAccountPtr object)
      {
        return AccountFinder::toDebug(AccountFinder::convert(object));
      }

      //-----------------------------------------------------------------------
      ForAccountPtr IAccountFinderForAccount::create(
                                                     IAccountFinderDelegatePtr delegate,
                                                     AccountPtr outer
                                                     )
      {
        return IAccountFinderFactory::singleton().create(delegate, outer);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder
      #pragma mark


      //-----------------------------------------------------------------------
      AccountFinder::AccountFinder(
                                   IMessageQueuePtr queue,
                                   IAccountFinderDelegatePtr delegate,
                                   AccountPtr outer
                                   ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mDelegate(IAccountFinderDelegateProxy::createWeak(UseStack::queueStack(), delegate)),
        mOuter(outer),
        mCurrentState(IAccount::AccountState_Pending)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //---------------------------------------------------------------------
      void AccountFinder::init()
      {
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //---------------------------------------------------------------------
      AccountFinder::~AccountFinder()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      AccountFinderPtr AccountFinder::convert(ForAccountPtr object)
      {
        return dynamic_pointer_cast<AccountFinder>(object);
      }

      //-----------------------------------------------------------------------
      ElementPtr AccountFinder::toDebug(AccountFinderPtr finder)
      {
        if (!finder) return ElementPtr();
        return finder->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IAccountFinderForAccount
      #pragma mark

      //---------------------------------------------------------------------
      AccountFinder::ForAccountPtr AccountFinder::create(
                                                         IAccountFinderDelegatePtr delegate,
                                                         AccountPtr outer
                                                         )
      {
        AccountFinderPtr pThis(new AccountFinder(UseStack::queueStack(), delegate, outer));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates AccountFinder::getState() const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //---------------------------------------------------------------------
      void AccountFinder::shutdown()
      {
        ZS_LOG_DEBUG(log("shutdown requested"))
        cancel();
      }

      //---------------------------------------------------------------------
      bool AccountFinder::send(MessagePtr message) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(*this);
        if (!message) {
          ZS_LOG_ERROR(Detail, log("message to send was NULL"))
          return false;
        }

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("attempted to send a message but the location is shutdown"))
          return false;
        }

        if (!isReady()) {
          if (!isShuttingDown()) {
            SessionCreateRequestPtr sessionCreateRequest = SessionCreateRequest::convert(message);
            if (!sessionCreateRequest) {
              ZS_LOG_WARNING(Detail, log("attempted to send a message when the finder is not ready"))
              return false;
            }
          } else {
            SessionDeleteRequestPtr sessionDeleteRequest = SessionDeleteRequest::convert(message);
            if (!sessionDeleteRequest) {
              ZS_LOG_WARNING(Detail, log("attempted to send a message when the finder is not ready"))
              return false;
            }
          }
        }

        if (!mSendStream) {
          ZS_LOG_WARNING(Detail, log("requested to send a message but send stream is not ready"))
          return false;
        }

        DocumentPtr document = message->encode();

        boost::shared_array<char> output;
        size_t length = 0;
        output = document->writeAsJSON(&length);

        if (ZS_IS_LOGGING(Detail)) {
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("FINDER SEND MESSAGE") + ZS_PARAM("json out", ((CSTR)(output.get()))))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >> >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        mSendStream->write((const BYTE *)(output.get()), length);
        return true;
      }

      //---------------------------------------------------------------------
      IMessageMonitorPtr AccountFinder::sendRequest(
                                                    IMessageMonitorDelegatePtr delegate,
                                                    MessagePtr requestMessage,
                                                    Duration timeout
                                                    ) const
      {
        IMessageMonitorPtr monitor = IMessageMonitor::monitor(delegate, requestMessage, timeout);
        if (!monitor) {
          ZS_LOG_WARNING(Detail, log("failed to create monitor"))
          return IMessageMonitorPtr();
        }

        bool result = send(requestMessage);
        if (!result) {
          // notify that the message requester failed to send the message...
          UseMessageMonitorManager::notifyMessageSendFailed(requestMessage);
          return monitor;
        }

        ZS_LOG_DEBUG(log("request successfully created"))
        return monitor;
      }

      //---------------------------------------------------------------------
      Finder AccountFinder::getCurrentFinder(
                                             String *outServerAgent,
                                             IPAddress *outIPAddress
                                             ) const
      {
        if (outServerAgent) *outServerAgent = mServerAgent;
        if (outIPAddress) *outIPAddress = mFinderIP;
        return mFinder;
      }

      //-----------------------------------------------------------------------
      void AccountFinder::getFinderRelayInformation(
                                                    IPAddress &outFinderIP,
                                                    String &outFinderRelayAccessToken,
                                                    String &outFinderRelayAccessSecret
                                                    ) const
      {
        AutoRecursiveLock lock(*this);
        outFinderIP = mFinderIP;
        outFinderRelayAccessToken = mRelayAccessToken;
        outFinderRelayAccessSecret = mRelayAccessSecret;
      }

      //-----------------------------------------------------------------------
      void AccountFinder::notifyFinderDNSComplete()
      {
        ZS_LOG_DEBUG(log("notified finder DNS complete"))
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::onWake()
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IFinderConnectionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::onFinderConnectionStateChanged(
                                                         IFinderConnectionPtr connection,
                                                         IFinderConnection::SessionStates state
                                                         )
      {
        ZS_LOG_DEBUG(log("finder connection state changed") + ZS_PARAM("finder connection ID", connection->getID()) + ZS_PARAM("state", IFinderConnection::toString(state)))

        AutoRecursiveLock lock(*this);
        step();
      }


      //-----------------------------------------------------------------------
      void AccountFinder::onFinderConnectionIncomingRelayChannel(IFinderConnectionPtr connection)
      {
        ZS_LOG_DEBUG(log("finder connection incoming relay channel"))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("finder already shutdown"))
          return;
        }

        if (mFinderConnection != connection) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete finder connection") + ZS_PARAM("connection", connection->getID()))
          return;
        }

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("outer account is gone"))
          return;
        }

        ITransportStreamPtr receiveStream = ITransportStream::create();
        ITransportStreamPtr sendStream = ITransportStream::create();

        IFinderConnection::ChannelNumber channel = 0;

        IFinderRelayChannelPtr relayChannel = connection->accept(IFinderRelayChannelDelegatePtr(), Account::convert(outer), receiveStream, sendStream, &channel);

        if (relayChannel) {
          ZS_LOG_DEBUG(log("relay channel accepted") + IFinderRelayChannel::toDebug(relayChannel))
          mDelegate->onAccountFinderIncomingRelayChannel(mThisWeak.lock(), relayChannel, receiveStream, sendStream, channel);
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        ZS_LOG_TRACE(log("send stream write ready (ignored)"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        ZS_LOG_TRACE(log("RUDP messaging read ready"))

        AutoRecursiveLock lock(*this);

        if (reader != mReceiveStream) {
          ZS_LOG_DEBUG(log("RUDP messaging ready came in about obsolete messaging"))
          return;
        }

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("message arrived after shutdown"))
          return;
        }

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("account is gone thus cannot read message"))
          return;
        }

        while (true) {
          SecureByteBlockPtr buffer = mReceiveStream->read();
          if (!buffer) {
            ZS_LOG_TRACE(log("no data read"))
            return;
          }

          const char *bufferStr = (CSTR)(buffer->BytePtr());

          if (0 == strcmp(bufferStr, "\n")) {
            ZS_LOG_TRACE(log("received new line ping"))
            continue;
          }

          DocumentPtr document = Document::createFromAutoDetect(bufferStr);
          message::MessagePtr message = Message::create(document, Location::convert(UseLocation::getForFinder(Account::convert(outer))));

          if (ZS_IS_LOGGING(Detail)) {
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("FINDER RECEIVED MESSAGE") + ZS_PARAM("json in", bufferStr))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          }

          if (!message) {
            ZS_LOG_WARNING(Detail, log("failed to create a message from the document"))
            continue;
          }

          if (IMessageMonitor::handleMessageReceived(message)) {
            ZS_LOG_DEBUG(log("message requester handled the message"))
            continue;
          }

          try {
            mDelegate->onAccountFinderMessageIncoming(mThisWeak.lock(), message);
          } catch(IAccountFinderDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IMessageMonitorResultDelegate<SessionCreateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             SessionCreateResultPtr result
                                                             )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionCreateMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete session create event"))
          return false;
        }

        mSessionCreatedTime = zsLib::now();

        mSessionCreateMonitor.reset();

        setTimeout(result->expires());
        mServerAgent = result->serverAgent();

        mRelayAccessToken = result->relayAccessToken();
        mRelayAccessSecret = result->relayAccessSecret();

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorErrorResultReceived(
                                                                  IMessageMonitorPtr monitor,
                                                                  SessionCreateResultPtr ignore, // will always be NULL
                                                                  MessageResultPtr result
                                                                  )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionCreateMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete session create error event"))
          return false;
        }

        ZS_LOG_DEBUG(log("requester message session create received error reply") + Message::toDebug(result))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IMessageMonitorResultDelegate<SessionKeepAliveResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             SessionKeepAliveResultPtr result
                                                             )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete keep alive event"))
          return false;
        }

        mSessionKeepAliveMonitor->cancel();
        mSessionKeepAliveMonitor.reset();

        setTimeout(result->expires());

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorErrorResultReceived(
                                                                  IMessageMonitorPtr monitor,
                                                                  SessionKeepAliveResultPtr ignore, // will always be NULL
                                                                  MessageResultPtr result
                                                                  )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete session keep alive error event"))
          return false;
        }

        ZS_LOG_DEBUG(log("requester message session keep alive received error reply") + Message::toDebug(result))
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => IMessageMonitorResultDelegate<SessionDeleteResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             SessionDeleteResultPtr result
                                                             )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionDeleteMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete session delete event"))
          return false;
        }

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountFinder::handleMessageMonitorErrorResultReceived(
                                                                  IMessageMonitorPtr monitor,
                                                                  SessionDeleteResultPtr ignore, // will always be NULL
                                                                  MessageResultPtr result
                                                                  )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mSessionDeleteMonitor) {
          ZS_LOG_WARNING(Detail, log("received an obsolete session delete event"))
          return false;
        }

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::onTimer(TimerPtr timer)
      {
        ZS_LOG_TRACE(log("timer fired"))

        if (!isReady()) return;
        if (timer != mKeepAliveTimer) return;

        if (mSessionKeepAliveMonitor) return;

        ZS_LOG_DEBUG(log("sending out keep alive request"))

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("account object is gone"))
          return;
        }

        SessionKeepAliveRequestPtr request = SessionKeepAliveRequest::create();
        request->domain(outer->getDomain());

        mSessionKeepAliveMonitor = sendRequest(IMessageMonitorResultDelegate<SessionKeepAliveResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SESSION_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountFinder => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountFinder::setTimeout(Time expires)
      {
        Time tick = zsLib::now();

        if (Time() == expires) {
          expires = tick;
        }

        if (tick > expires) {
          expires = tick;
        }

        Duration difference = expires - tick;

        ULONG maxKeepAlive = services::ISettings::getUInt(OPENPEER_STACK_SETTING_FINDER_MAX_CLIENT_SESSION_KEEP_ALIVE_IN_SECONDS);

        if (0 != maxKeepAlive) {
          if (difference > Seconds(maxKeepAlive)) {
            difference = Seconds(maxKeepAlive);
          }
        }

        if (difference < Seconds(120))
          difference = Seconds(120);

        difference -= Seconds(60); // timeout one minute before expiry

        if (mKeepAliveTimer) {
          mKeepAliveTimer->cancel();
          mKeepAliveTimer.reset();
        }

        mKeepAliveTimer = Timer::create(mThisWeak.lock(), difference);
      }

      //-----------------------------------------------------------------------
      Log::Params AccountFinder::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("AccountFinder");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params AccountFinder::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr AccountFinder::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("AccountFinder");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "state", IAccount::toString(mCurrentState));
        IHelper::debugAppend(resultEl, "finder connection id", mFinderConnection ? mFinderConnection->getID() : 0);
        IHelper::debugAppend(resultEl, "receive stream id", mReceiveStream ? mReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "send stream id", mSendStream ? mSendStream->getID() : 0);
        IHelper::debugAppend(resultEl, mFinder.toDebug());
        IHelper::debugAppend(resultEl, "finder IP", !mFinderIP.isAddressEmpty() ? mFinderIP.string() : String());
        IHelper::debugAppend(resultEl, "server agent", mServerAgent);
        IHelper::debugAppend(resultEl, "created time", mSessionCreatedTime);
        IHelper::debugAppend(resultEl, "session create monitor", (bool)mSessionCreateMonitor);
        IHelper::debugAppend(resultEl, "session keep alive monitor", (bool)mSessionKeepAliveMonitor);
        IHelper::debugAppend(resultEl, "session delete monitor", (bool)mSessionDeleteMonitor);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void AccountFinder::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called"))

        AutoRecursiveLock lock(*this);    // just in case

        if (isShutdown()) return;

        bool wasReady = isReady();

        setState(IAccount::AccountState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mKeepAliveTimer) {
          ZS_LOG_DEBUG(log("cancel stopping keep alive timer"))

          mKeepAliveTimer->cancel();
          mKeepAliveTimer.reset();
        }

        if (mSessionCreateMonitor) {
          ZS_LOG_DEBUG(log("shutdown for create session request"))

          mSessionCreateMonitor->cancel();
          mSessionCreateMonitor.reset();
        }

        if (mSessionKeepAliveMonitor) {
          ZS_LOG_DEBUG(log("shutdown for keep alive session request"))

          mSessionKeepAliveMonitor->cancel();
          mSessionKeepAliveMonitor.reset();
        }

        UseAccountPtr outer = mOuter.lock();

        if (mGracefulShutdownReference) {

          if (mFinderConnection) {
            if (wasReady) {
              if ((!mSessionDeleteMonitor) &&
                  (outer)) {
                ZS_LOG_DEBUG(log("sending delete session request"))
                SessionDeleteRequestPtr request = SessionDeleteRequest::create();
                request->domain(outer->getDomain());

                mSessionDeleteMonitor = sendRequest(IMessageMonitorResultDelegate<SessionDeleteResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SESSION_DELETE_REQUEST_TIMEOUT_IN_SECONDS));
                return;
              }
            }
          }

          if (mSessionDeleteMonitor) {
            if (!mSessionDeleteMonitor->isComplete()) {
              ZS_LOG_DEBUG(log("shutting down waiting for delete session request to complete"))
              return;
            }
          }
        }

        setState(IAccount::AccountState_Shutdown);

        mGracefulShutdownReference.reset();
        mOuter.reset();

        if (mFinderConnection) {
          mFinderConnection->cancel();
          mFinderConnection.reset();
        }

        if (mSessionDeleteMonitor) {
          ZS_LOG_DEBUG(log("hard shutdown for delete session request"))

          mSessionDeleteMonitor->cancel();
          mSessionDeleteMonitor.reset();
        }

        ZS_LOG_DEBUG(log("shutdown complete"))
      }

      //-----------------------------------------------------------------------
      void AccountFinder::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("account object is gone thus shutting down"))
          cancel();
          return;
        }

        if (!stepConnection()) return;
        if (!stepCreateSession()) return;

        setState(IAccount::AccountState_Ready);

        ZS_LOG_TRACE(debug("step complete"))
      }

      //-----------------------------------------------------------------------
      bool AccountFinder::stepConnection()
      {
        if (mFinderConnection) {
          WORD error = 0;
          String reason;

          IFinderConnection::SessionStates state = mFinderConnection->getState(&error, &reason);
          switch (state) {
            case IFinderConnection::SessionState_Pending: {
              ZS_LOG_TRACE(log("waiting for finder connection to connect"))
              return false;
            }
            case IFinderConnection::SessionState_Connected: {
              ZS_LOG_TRACE(log("finder connection is ready"))
              return true;
            }
            case IFinderConnection::SessionState_Shutdown: {
              ZS_LOG_WARNING(Detail, debug("finder connection failed") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              cancel();
              return false;
            }
          }
          ZS_THROW_BAD_STATE("missing state")
        }

        UseAccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        if (!outer->extractNextFinder(mFinder, mFinderIP)) {
          ZS_LOG_TRACE(log("waiting for account to obtain a finder"))
          return false;
        }

        ITransportStreamPtr receiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        mFinderConnection = IFinderConnection::connect(mThisWeak.lock(), mFinderIP, receiveStream, sendStream);
        if (!mFinderConnection) {
          ZS_LOG_ERROR(Detail, log("cannot create a socket session"))
          cancel();
          return false;
        }

        mReceiveStream = receiveStream->getReader();
        mSendStream = sendStream->getWriter();

        mReceiveStream->notifyReaderReadyToRead();
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountFinder::stepCreateSession()
      {
        if (mSessionCreateMonitor) {
          ZS_LOG_TRACE(log("waiting for session create request to complete"))
          return false;
        }

        if (Time() != mSessionCreatedTime) {
          ZS_LOG_TRACE(log("session already created"))
          return true;
        }

        UseAccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        IPeerFilesPtr peerFiles = outer->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_ERROR(Detail, log("no peer files found for session"))
          cancel();
          return false;
        }

        SessionCreateRequestPtr request = SessionCreateRequest::create();
        request->domain(outer->getDomain());

        request->finderID(mFinder.mID);

        UseLocationPtr selfLocation = UseLocation::getForLocal(Account::convert(outer));
        LocationInfoPtr locationInfo = selfLocation->getLocationInfo();
        locationInfo->mCandidates.clear();
        request->locationInfo(locationInfo);
        request->peerFiles(peerFiles);

        ZS_LOG_DEBUG(log("sending session create request"))
        mSessionCreateMonitor = sendRequest(IMessageMonitorResultDelegate<SessionCreateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SESSION_CREATE_REQUEST_TIMEOUT_IN_SECONDS));

        return false;
      }

      //-----------------------------------------------------------------------
      void AccountFinder::setState(IAccount::AccountStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("current state changed") + ZS_PARAM("old state", IAccount::toString(mCurrentState)) + ZS_PARAM("new state", IAccount::toString(state)))

        mCurrentState = state;

        if (isShutdown()) {
          UseMessageMonitorManager::notifyMessageSenderObjectGone(mID);
        }

        if (!mDelegate) return;

        AccountFinderPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            mDelegate->onAccountFinderStateChanged(mThisWeak.lock(), state);
          } catch(IAccountFinderDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
