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

#include <openpeer/stack/internal/stack_ServerMessaging.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IBackgrounding.h>
#include <openpeer/services/IDHKeyDomain.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>
#include <openpeer/services/IHelper.h>
#include <openpeer/services/IHTTP.h>
#include <openpeer/services/IMessageLayerSecurityChannel.h>
#include <openpeer/services/ISettings.h>

#include <cryptopp/osrng.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using CryptoPP::AutoSeededRandomPool;

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      ZS_DECLARE_USING_PTR(services, IBackgrounding)

      //      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static bool compare(const Server &first, const Server &second)
      {
        if (first.mPriority < second.mPriority)
          return true;
        if (first.mPriority > second.mPriority)
          return false;

        DWORD total = (((DWORD)first.mWeight)) + (((DWORD)second.mWeight));

        // they are equal, we have to compare relative weight
        DWORD random = 0;

        AutoSeededRandomPool rng;
        rng.GenerateBlock((BYTE *)&random, sizeof(random));
        if (0 == total)
          return (0 == (random % 2) ? true : false);  // equal chance, 50-50

        random %= total;
        if (random < (((DWORD)first.mWeight)))
          return true;
        
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      ServerMessaging::ServerMessaging(
                                       IMessageQueuePtr queue,
                                       IServerMessagingDelegatePtr delegate,
                                       IPeerFilesPtr peerFiles,
                                       ITransportStreamPtr receiveStream,
                                       ITransportStreamPtr sendStream,
                                       bool messagesHaveChannelNumber,
                                       const ServerList &possibleServers,
                                       size_t maxMessageSizeInBytes
                                       ) :
        zsLib::MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mCurrentState(SessionState_Pending),
        mPeerFiles(peerFiles),
        mMaxMessageSizeInBytes(maxMessageSizeInBytes),
        mMessagesHaveChannelNumber(messagesHaveChannelNumber),
        mEnableKeepAlive(false),
        mReceiveStreamDecoded(receiveStream->getWriter()),
        mSendStreamDecoded(sendStream->getReader()),
        mServerList(possibleServers),
        mBackgrounding(false)
      {
        ZS_LOG_DEBUG(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate);
        ZS_THROW_BAD_STATE_IF(!mDefaultSubscription)

        mServerList.sort(compare);
      }

      //-----------------------------------------------------------------------
      ServerMessaging::~ServerMessaging()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::init()
      {
        AutoRecursiveLock lock(*this);

        mBackgroundingSubscription = IBackgrounding::subscribe(
                                                               mThisWeak.lock(),
                                                               UseSettings::getUInt(OPENPEER_STACK_SETTING_SERVERMESSAGING_BACKGROUNDING_PHASE)
                                                               );
      }

      //-----------------------------------------------------------------------
      ServerMessagingPtr ServerMessaging::convert(IServerMessagingPtr query)
      {
        return ZS_DYNAMIC_PTR_CAST(ServerMessaging, query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => IServerMessaging
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServerMessaging::toDebug(IServerMessagingPtr messaging)
      {
        if (!messaging) return ElementPtr();
        return ServerMessaging::convert(messaging)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServerMessagingPtr ServerMessaging::connect(
                                                  IServerMessagingDelegatePtr delegate,
                                                  IPeerFilesPtr peerFiles,
                                                  ITransportStreamPtr receiveStream,
                                                  ITransportStreamPtr sendStream,
                                                  bool messagesHaveChannelNumber,
                                                  const ServerList &possibleServers,
                                                  size_t maxMessageSizeInBytes
                                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!peerFiles)
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)
        ZS_THROW_INVALID_ARGUMENT_IF(possibleServers.size() < 1)

        ServerMessagingPtr pThis(new ServerMessaging(UseStack::queueStack(), delegate, peerFiles, receiveStream, sendStream, messagesHaveChannelNumber, possibleServers, maxMessageSizeInBytes));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServerMessagingSubscriptionPtr ServerMessaging::subscribe(IServerMessagingDelegatePtr originalDelegate)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("subscribe called") + ZS_PARAM("delegate", (bool)originalDelegate))

        if (!originalDelegate) return mDefaultSubscription;

        IServerMessagingSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate);

        IServerMessagingDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          ServerMessagingPtr pThis = mThisWeak.lock();

          if (SessionState_Pending != mCurrentState) {
            delegate->onServerMessagingStateChanged(pThis, mCurrentState);
          }
        }

        if (isShutdown()) {
          mSubscriptions.clear();
        }
        
        return subscription;
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::shutdown(Duration lingerTime)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("shutdown called") + ZS_PARAM("duration", lingerTime))
        cancel();
      }

      //-----------------------------------------------------------------------
      ServerMessaging::SessionStates ServerMessaging::getState(
                                                               WORD *outLastErrorCode,
                                                               String *outLastErrorReason
                                                               ) const
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("get state") + ZS_PARAM("current state", toString(mCurrentState)) + ZS_PARAM("last error", mLastError) + ZS_PARAM("error reason", mLastErrorReason))
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::enableKeepAlive(bool enable)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("enable keep alive") + ZS_PARAM("enable", enable))
        mEnableKeepAlive = enable;
        if (mTCPMessaging) mTCPMessaging->enableKeepAlive(enable);
      }

      //-----------------------------------------------------------------------
      IPAddress ServerMessaging::getRemoteIP() const
      {
        AutoRecursiveLock lock(*this);
        IPAddress ip;
        if (mTCPMessaging) ip = mTCPMessaging->getRemoteIP();
        ZS_LOG_TRACE(log("remote ip") + ZS_PARAM("ip", string(ip)))
        return ip;
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::setMaxMessageSizeInBytes(size_t maxMessageSizeInBytes)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("max message size") + ZS_PARAM("max message size", maxMessageSizeInBytes))
        mMaxMessageSizeInBytes = maxMessageSizeInBytes;
        if (mTCPMessaging) mTCPMessaging->setMaxMessageSizeInBytes(maxMessageSizeInBytes);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => ITCPMessagingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onTCPMessagingStateChanged(
                                                       ITCPMessagingPtr messaging,
                                                       ITCPMessaging::SessionStates state
                                                       )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("state changed") + ZS_PARAM("state", ITCPMessaging::toString(state)) + ITCPMessaging::toDebug(messaging))
        step();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => IMessageLayerSecurityChannelDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onMessageLayerSecurityChannelStateChanged(
                                                                      IMessageLayerSecurityChannelPtr channel,
                                                                      IMessageLayerSecurityChannel::SessionStates state
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("state changed") + ZS_PARAM("state", IMessageLayerSecurityChannel::toString(state)) + IMessageLayerSecurityChannel::toDebug(channel))
        step();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onBackgroundingGoingToBackground(
                                                             IBackgroundingSubscriptionPtr subscription,
                                                             IBackgroundingNotifierPtr notifier
                                                             )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("notified going to background"))

        mNotifier = notifier;
        mBackgrounding = true;

        step();
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("notified going to background now"))

        mNotifier.reset();
        mBackgrounding = true;
        step();
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("notified returning from background"))

        mNotifier.reset();
        mBackgrounding = false;
        step();
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => ITransportStreamReaderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        AutoRecursiveLock lock(*this);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        AutoRecursiveLock lock(*this);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onWake()
      {
        ZS_LOG_TRACE(log("on wake"))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => IDNSDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServerMessaging::onLookupCompleted(IDNSQueryPtr query)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("lookup complete") + ZS_PARAM("query", query ? query->getID() : 0))
        step();
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServerMessaging => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServerMessaging::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServerMessaging");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServerMessaging::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServerMessaging::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServerMessaging");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        UseServicesHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());
        UseServicesHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        UseServicesHelper::debugAppend(resultEl, "backgrounding subscription", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "current state", IServerMessaging::toString(mCurrentState));

        UseServicesHelper::debugAppend(resultEl, "error code", mLastError);
        UseServicesHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, IPeerFiles::toDebug(mPeerFiles));

        UseServicesHelper::debugAppend(resultEl, "max message size", mMaxMessageSizeInBytes);

        UseServicesHelper::debugAppend(resultEl, "messages must have channel number", mMessagesHaveChannelNumber);
        UseServicesHelper::debugAppend(resultEl, "enable keep alive", mEnableKeepAlive);

        UseServicesHelper::debugAppend(resultEl, "receive stream encoded", mReceiveStreamEncoded ? mReceiveStreamEncoded->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "receive stream decoded", mReceiveStreamDecoded ? mReceiveStreamDecoded->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "send stream decoded", mSendStreamDecoded ? mSendStreamDecoded->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "send stream encoded", mSendStreamEncoded ? mSendStreamEncoded->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "server list", mServerList.size());
        UseServicesHelper::debugAppend(resultEl, mActiveServer.toDebug());
        UseServicesHelper::debugAppend(resultEl, "dns query", mDNSQuery ? mDNSQuery->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "dns result", mDNSResult ? mDNSResult->mRecords.size() : 0);
        UseServicesHelper::debugAppend(resultEl, "active IP", string(mActiveIP));

        UseServicesHelper::debugAppend(resultEl, "tcp messaging", ITCPMessaging::toDebug(mTCPMessaging));
        UseServicesHelper::debugAppend(resultEl, "mls", IMessageLayerSecurityChannel::toDebug(mMLS));

        UseServicesHelper::debugAppend(resultEl, "backgrounding", mBackgrounding);
        UseServicesHelper::debugAppend(resultEl, "notifier", (bool)mNotifier);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::step()
      {
        ZS_LOG_DEBUG(log("step") + toDebug())

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_TRACE(log("step forwarding to cancel"))
          cancel();
          return;
        }

        if (!stepPickServer()) return;
        if (!stepDNS()) return;
        if (!stepPickIP()) return;
        if (!stepTCP()) return;
        if (!stepMLS()) return;

        setState(SessionState_Connected);

        ZS_LOG_TRACE(log("step complete") + toDebug())
      }

      //-----------------------------------------------------------------------
      bool ServerMessaging::stepPickServer()
      {
        if (mActiveServer.hasData()) {
          ZS_LOG_TRACE(log("already have server") + mActiveServer.toDebug() + mActiveProtocol.toDebug())
          return true;
        }

        if (mBackgrounding) {
          ZS_LOG_TRACE(log("do not pick a server when going to background"))
          mNotifier.reset();
          return false;
        }

        ZS_LOG_DEBUG(log("pick server"))

        for (ServerList::iterator iter_doNotUse = mServerList.begin(); iter_doNotUse != mServerList.end(); ) {

          ServerList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          const Server &server = (*current);
          const Server::ProtocolList &protocols = server.mProtocols;

          for (Server::ProtocolList::const_iterator protIter = protocols.begin(); protIter != protocols.end(); ++protIter) {

            const Server::Protocol &protocol = (*protIter);
            if (mMessagesHaveChannelNumber) {
              if (OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_MLS_TCP == protocol.mTransport) {
                mActiveProtocol = protocol;
              }
            } else {
              if (OPENPEER_STACK_TRANSPORT_JSON_MLS_TCP == protocol.mTransport) {
                mActiveProtocol = protocol;
              }
            }
            if (mActiveProtocol.hasData()) {
              mActiveServer = server;
            }

            if (mActiveProtocol.hasData()) break;
          }

          if (!mActiveProtocol.hasData()) {
            mServerList.erase(current);
            continue;
          }

          break;
        }

        if (!mActiveServer.hasData()) {
          ZS_LOG_WARNING(Detail, log("no server was found"))
          setError(IHTTP::HTTPStatusCode_NotFound, "server was not found");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("found active server and protocol") + mActiveServer.toDebug() + mActiveProtocol.toDebug())
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServerMessaging::stepDNS()
      {
        if (mDNSResult) {
          ZS_LOG_TRACE(log("already have an SRV result"))
          return true;
        }

        if (mDNSQuery) {
          if (!mDNSQuery->isComplete()) {
            ZS_LOG_TRACE(log("dns query is ongoing"))
            return false;
          }

          mDNSResult = IDNS::convertAorAAAAResultToSRVResult(
                                                             "service",
                                                             "protocol",
                                                             mDNSQuery->getA(),                      // either aResult or aaaaResult must be set
                                                             mDNSQuery->getAAAA()
                                                             );
          if (mDNSResult) {
            ZS_LOG_DEBUG(log("dns result completed") + ZS_PARAM("records", mDNSResult->mRecords.size()))
            return true;
          }

          ZS_LOG_WARNING(Detail, log("dns failed to resolve to a result"))

          // pick another server
          mActiveServer = Server();
          mActiveProtocol = Server::Protocol();

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return false;
        }

        if (mBackgrounding) {
          ZS_LOG_TRACE(log("do not perform DNS query when backgrounding"))
          mNotifier.reset();
          return false;
        }

        ZS_LOG_DEBUG(log("performing DNS lookup") + ZS_PARAM("host", mActiveProtocol.mHost))

        mDNSQuery = IDNS::lookupAorAAAA(mThisWeak.lock(), mActiveProtocol.mHost);

        if (!mDNSQuery) {
          ZS_LOG_TRACE(log("dns query failure") + ZS_PARAM("host", mActiveProtocol.mHost))

          mActiveServer = Server();
          mActiveProtocol = Server::Protocol();

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return false;
        }

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServerMessaging::stepPickIP()
      {
        if (!mActiveIP.isEmpty()) {
          ZS_LOG_TRACE(log("already picked IP address"))
          return true;
        }

        if (!IDNS::extractNextIP(mDNSResult, mActiveIP)) {
          ZS_LOG_TRACE(log("dns extraction failure (pick a new server)"))

          mActiveServer = Server();
          mActiveProtocol = Server::Protocol();

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return false;
        }

        ZS_LOG_DEBUG(log("server IP picked") + ZS_PARAM("server ip", string(mActiveIP)))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServerMessaging::stepTCP()
      {
        if (mTCPMessaging) {

          if (mBackgrounding) {
            mTCPMessaging->shutdown();
          }

          WORD errorCode = 0;
          String reason;
          switch (mTCPMessaging->getState(&errorCode, &reason)) {
            case ITCPMessaging::SessionState_Pending: {
              ZS_LOG_TRACE(log("waiting for TCP to connect"))
              break;
            }
            case ITCPMessaging::SessionState_Connected: {
              ZS_LOG_TRACE(log("TCP is connected"))
              return true;
            }
            case ITCPMessaging::SessionState_ShuttingDown: {
              ZS_LOG_TRACE(log("TCP is shutting down"))
              return false;
            }
            case ITCPMessaging::SessionState_Shutdown: {
              if (isConnected()) {
                ZS_LOG_WARNING(Detail, log("TCP messaging is shutdown thus connection must shutdown") + ZS_PARAM("error code", errorCode) + ZS_PARAM("reason", reason))
                setError(errorCode, reason);
                cancel();
                return false;
              }

              ZS_LOG_WARNING(Detail, log("TCP messaging shutdown but never fully connected thus will attempt reconnect") + ZS_PARAM("error code", errorCode) + ZS_PARAM("reason", reason))

              if (mMLS) {
                mMLS->cancel();
                mMLS.reset();
              }
              if (mReceiveStreamEncoded) {
                mReceiveStreamEncoded->cancel();
                mReceiveStreamEncoded.reset();
              }
              if (mSendStreamEncoded) {
                mSendStreamEncoded->cancel();
                mSendStreamEncoded.reset();
              }

              if (!mBackgrounding) {
                mActiveIP.clear();  // reset active IP to pick another one
              }

              mNotifier.reset();

              IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
              return false;
            }
          }

          return false;
        }

        ZS_LOG_DEBUG(log("issuing TCP connect request"))

        mReceiveStreamEncoded = ITransportStream::create()->getReader();
        mSendStreamEncoded = ITransportStream::create()->getWriter();

        mTCPMessaging = ITCPMessaging::connect(mThisWeak.lock(), mReceiveStreamEncoded->getStream(), mSendStreamDecoded->getStream(), mMessagesHaveChannelNumber, mActiveIP);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServerMessaging::stepMLS()
      {
        if (mMLS) {
          WORD errorCode = 0;
          String reason;
          switch (mMLS->getState(&errorCode, &reason)) {
            case IMessageLayerSecurityChannel::SessionState_Pending: {
              ZS_LOG_TRACE(log("mls is pending"))
              break;
            }
            case IMessageLayerSecurityChannel::SessionState_WaitingForNeededInformation: {
              ZS_LOG_TRACE(log("mls is waiting for information"))

              if (mMLS->needsSendKeyingToeBeSigned()) {
                IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();
                IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();

                ZS_THROW_INVALID_ASSUMPTION_IF(!peerFilePrivate)
                ZS_THROW_INVALID_ASSUMPTION_IF(!peerFilePublic)

                DocumentPtr doc;
                ElementPtr signElement;
                mMLS->getSendKeyingNeedingToBeSigned(doc, signElement);

                if ((doc) &&
                    (signElement)) {
                  peerFilePrivate->signElement(signElement);
                  mMLS->notifySendKeyingSigned(peerFilePrivate->getPrivateKey(), peerFilePublic->getPublicKey());
                }
              }
              break;
            }
            case IMessageLayerSecurityChannel::SessionState_Connected: {
              ZS_LOG_TRACE(log("mls is connected"))
              return true;
            }
            case IMessageLayerSecurityChannel::SessionState_Shutdown: {
              ZS_LOG_ERROR(Detail, log("mls shutdown unexpectedly") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))
              setError(errorCode, reason);
              cancel();
              return false;
            }
          }
          return false;
        }

        ZS_LOG_DEBUG(log("creating MLS"))

        mMLS = IMessageLayerSecurityChannel::create(mThisWeak.lock(), mReceiveStreamEncoded->getStream(), mReceiveStreamDecoded->getStream(), mSendStreamDecoded->getStream(), mSendStreamEncoded->getStream(), UseServicesHelper::randomString(20));
        if (!mMLS) {
          ZS_LOG_ERROR(Detail, log("failed to create mls"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "failed to create mls");
          cancel();
          return false;
        }

        String namespaceStr = UseSettings::getString(OPENPEER_STACK_SETTING_SERVERMESSAGING_DEFAULT_KEY_DOMAIN);

        IDHKeyDomainPtr domain = IDHKeyDomain::loadPrecompiled(IDHKeyDomain::fromNamespace(namespaceStr));

        if (!domain) {
          ZS_LOG_ERROR(Detail, log("failed to load precompiled key domain") + ZS_PARAM("namespace", namespaceStr))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "failed to load precompiled key domain");
          cancel();
          return false;
        }

        IDHPublicKeyPtr publicKey;
        IDHPrivateKeyPtr privateKey = IDHPrivateKey::generate(domain, publicKey);

        mMLS->setLocalKeyAgreement(privateKey, publicKey, false);
        mMLS->setReceiveKeyingSigningPublicKey(mActiveServer.mPublicKey);

        return false;
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::cancel()
      {
        ZS_LOG_DEBUG(log("cancel called") + toDebug())

        if (isShutdown()) return;

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mGracefulShutdownReference) {
          if (mTCPMessaging) {
            mTCPMessaging->shutdown();

            if (ITCPMessaging::SessionState_Shutdown != mTCPMessaging->getState()) {
              ZS_LOG_TRACE(log("waiting for TCP socket to shutdown"))
              return;
            }
          }
        }

        setState(SessionState_Shutdown);

        mGracefulShutdownReference.reset();

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
          mTCPMessaging.reset();
        }

        if (mMLS) {
          mMLS->cancel();
          mMLS.reset();
        }

        if (mReceiveStreamEncoded) {
          mReceiveStreamEncoded->cancel();
        }
        mReceiveStreamDecoded->cancel();
        mSendStreamDecoded->cancel();
        if (mSendStreamEncoded) {
          mSendStreamEncoded->cancel();
        }

        mNotifier.reset();
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_DETAIL(log("state changed") + ZS_PARAM("state", IServerMessaging::toString(state)) + ZS_PARAM("old state", IServerMessaging::toString(mCurrentState)))
        mCurrentState = state;

        ServerMessagingPtr pThis = mThisWeak.lock();
        if (pThis) {
          ZS_LOG_DEBUG(debug("attempting to report state to delegate") + ZS_PARAM("total", mSubscriptions.size()))
          mSubscriptions.delegate()->onServerMessagingStateChanged(pThis, mCurrentState);
        }

        if (SessionState_Connected == mCurrentState) {
          mSendStreamDecoded->notifyReaderReadyToRead();
        }
      }

      //-----------------------------------------------------------------------
      void ServerMessaging::setError(WORD errorCode, const char *inReason)
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
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServerMessagingFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServerMessagingFactory &IServerMessagingFactory::singleton()
      {
        return ServerMessagingFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServerMessagingPtr IServerMessagingFactory::connect(
                                                          IServerMessagingDelegatePtr delegate,
                                                          IPeerFilesPtr peerFiles,
                                                          ITransportStreamPtr receiveStream,
                                                          ITransportStreamPtr sendStream,
                                                          bool messagesHaveChannelNumber,
                                                          const ServerList &possibleServers,
                                                          size_t maxMessageSizeInBytes
                                                          )
      {
        if (this) {}
        return ServerMessaging::connect(delegate, peerFiles, receiveStream, sendStream, messagesHaveChannelNumber, possibleServers, maxMessageSizeInBytes);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServerMessaging
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IServerMessaging::toDebug(IServerMessagingPtr query)
    {
      return internal::ServerMessaging::toDebug(query);
    }

    //-------------------------------------------------------------------------
    IServerMessagingPtr IServerMessaging::connect(
                                                  IServerMessagingDelegatePtr delegate,
                                                  IPeerFilesPtr peerFiles,
                                                  ITransportStreamPtr receiveStream,
                                                  ITransportStreamPtr sendStream,
                                                  bool messagesHaveChannelNumber,
                                                  const ServerList &possibleServers,
                                                  size_t maxMessageSizeInBytes
                                                  )
    {
      return internal::IServerMessagingFactory::singleton().connect(delegate, peerFiles, receiveStream, sendStream, messagesHaveChannelNumber, possibleServers, maxMessageSizeInBytes);
    }
  }
}
