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

#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_AccountPeerLocation.h>
#include <openpeer/stack/internal/stack_AccountFinder.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/ICache.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/stack/message/peer-to-peer/PeerIdentifyRequest.h>
#include <openpeer/stack/message/peer-to-peer/PeerKeepAliveRequest.h>

#include <openpeer/stack/message/peer-finder/ChannelMapNotify.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindNotify.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>
#include <zsLib/Log.h>
#include <zsLib/helpers.h>

#ifndef _WIN32
#include <unistd.h>
#endif //_WIN32

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_PEER_LOCATION_FIND_TIMEOUT_IN_SECONDS (60*2)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_CHANNEL_MAP_COOKIE_NONCE_CACHE_NAMESPACE "https://meta.openpeer.org/caching/channel-map/nonce/"

#define OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO "text/x-openpeer-json-plain"
#define OPENPEER_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS (2*60)
#define OPENPEER_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS (2*60)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS (15)
#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS (50)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_BACKGROUNDING_TIMEOUT_IN_SECONDS (OPENPEER_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS + 40)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_MIN_CONNECTION_TIME_NEEDED_TO_REFIND_IN_SECONDS (60)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_CHANNEL_MAP_NOTIFY_RESOURCE "peer-finder-channel-map"

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;
      using services::IMessageLayerSecurityChannel;

      using message::peer_finder::ChannelMapNotify;
      using message::peer_finder::ChannelMapNotifyPtr;

      using message::peer_to_peer::PeerIdentifyRequest;
      using message::peer_to_peer::PeerIdentifyRequestPtr;
      using message::peer_to_peer::PeerKeepAliveRequest;
      using message::peer_to_peer::PeerKeepAliveRequestPtr;

      typedef IAccountPeerLocationForAccount::ForAccountPtr ForAccountPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static String getMultiplexedJsonTCPTransport(const Server::ProtocolList &protocols)
      {
        for (Server::ProtocolList::const_iterator iter = protocols.begin(); iter != protocols.end(); ++iter)
        {
          const Server::Protocol &protocol = (*iter);

          if (OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_TCP == protocol.mTransport) {
            return protocol.mHost;
          }
        }
        return String();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IAccountPeerLocationForAccount::toDebug(ForAccountPtr peerLocation)
      {
        return AccountPeerLocation::toDebug(AccountPeerLocation::convert(peerLocation));
      }

      //-----------------------------------------------------------------------
      ForAccountPtr IAccountPeerLocationForAccount::createFromIncomingPeerLocationFind(
                                                                                       IAccountPeerLocationDelegatePtr delegate,
                                                                                       AccountPtr outer,
                                                                                       PeerLocationFindRequestPtr request,
                                                                                       IDHPrivateKeyPtr localPrivateKey,
                                                                                       IDHPublicKeyPtr localPublicKey
                                                                                       )
      {
        return IAccountPeerLocationFactory::singleton().createFromIncomingPeerLocationFind(delegate, outer, request, localPrivateKey, localPublicKey);
      }

      //-----------------------------------------------------------------------
      ForAccountPtr IAccountPeerLocationForAccount::createFromPeerLocationFindResult(
                                                                                     IAccountPeerLocationDelegatePtr delegate,
                                                                                     AccountPtr outer,
                                                                                     PeerLocationFindRequestPtr request,
                                                                                     LocationInfoPtr locationInfo
                                                                                     )
      {
        return IAccountPeerLocationFactory::singleton().createFromPeerLocationFindResult(delegate, outer, request, locationInfo);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      const char *AccountPeerLocation::toString(CreatedFromReasons reason)
      {
        switch (reason)
        {
          case CreatedFromReason_IncomingFind: return "Incoming find";
          case CreatedFromReason_OutgoingFind: return "Outgoing find";
        }
        return "UNKNOWN";
      }

      //-----------------------------------------------------------------------
      AccountPeerLocation::AccountPeerLocation(
                                               IMessageQueuePtr queue,
                                               IAccountPeerLocationDelegatePtr delegate,
                                               AccountPtr outer,
                                               PeerLocationFindRequestPtr request,
                                               IDHPrivateKeyPtr localPrivateKey,
                                               IDHPublicKeyPtr localPublicKey
                                               ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mDelegate(IAccountPeerLocationDelegateProxy::createWeak(UseStack::queueStack(), delegate)),
        mOuter(outer),

        mCreatedReason(CreatedFromReason_IncomingFind), // INCOMING
        mFindRequest(request),
        mIncomingRelayChannelNumber(0),

        mLocalContext(UseLocation::getForLocal(outer)->getLocationID()),  // must be set to "our" location ID
        mRemoteContext(request->context()),                               // whatever the remote party claims is the context

        mDHLocalPrivateKey(localPrivateKey),
        mDHLocalPublicKey(localPublicKey),
        mDHRemotePublicKey(request->dhPublicKey()),

        mCurrentState(IAccount::AccountState_Pending),
        mLastActivity(zsLib::now()),

        mLocationInfo(request->locationInfo()),
        mLocation(Location::convert(request->locationInfo()->mLocation)),

        mPeer(mLocation->getPeer()),
        mDebugForceMessagesOverRelay(services::ISettings::getBool(OPENPEER_STACK_SETTING_ACCOUNT_PEER_LOCATION_DEBUG_FORCE_MESSAGES_OVER_RELAY))
      {
        ZS_LOG_BASIC(debug("created"))
        ZS_THROW_BAD_STATE_IF(!mPeer)

        if (mFindRequest->didVerifySignature()) {
          ZS_LOG_TRACE(log("already verified identity from incoming find request (thus don't expect remote party to send an identify request)"))
          mIdentifyTime = zsLib::now();
        }
      }

      //-----------------------------------------------------------------------
      AccountPeerLocation::AccountPeerLocation(
                                               IMessageQueuePtr queue,
                                               IAccountPeerLocationDelegatePtr delegate,
                                               AccountPtr outer,
                                               PeerLocationFindRequestPtr request,
                                               LocationInfoPtr locationInfo
                                               ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mDelegate(IAccountPeerLocationDelegateProxy::createWeak(UseStack::queueStack(), delegate)),
        mOuter(outer),

        mLastCandidateVersionSent(request->locationInfo()->mCandidatesVersion),

        mCreatedReason(CreatedFromReason_OutgoingFind), // OUTGOING
        mFindRequest(request),
        mIncomingRelayChannelNumber(0),

        mLocalContext(request->context()),                                                            // whatever was told to the remote party
        mRemoteContext(UseLocationPtr(Location::convert(locationInfo->mLocation))->getLocationID()),  // expecting the remote party to respond with it's location ID

        mDHLocalPrivateKey(request->dhPrivateKey()),
        mDHLocalPublicKey(request->dhPublicKey()),
        // mDHRemotePublicKey(...) - // obtain this from the incoming relay channel

        mCurrentState(IAccount::AccountState_Pending),
        mLastActivity(zsLib::now()),

        mLocationInfo(locationInfo),
        mLocation(Location::convert(locationInfo->mLocation)),

        mPeer(mLocation->getPeer()),
        mDebugForceMessagesOverRelay(services::ISettings::getBool(OPENPEER_STACK_SETTING_ACCOUNT_PEER_LOCATION_DEBUG_FORCE_MESSAGES_OVER_RELAY))
      {
        ZS_LOG_BASIC(debug("created"))
        ZS_THROW_BAD_STATE_IF(!mPeer)
      }
      
      //---------------------------------------------------------------------
      void AccountPeerLocation::init()
      {
        ZS_LOG_DEBUG(debug("initialized"))

        AutoRecursiveLock lock(*this);

        ZS_THROW_INVALID_ASSUMPTION_IF(!mLocationInfo)

        // monitor to receive all incoming notifications
        mFindRequestMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerLocationFindNotify>::convert(mThisWeak.lock()), mFindRequest, Duration());
        mFindRequestTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_STACK_ACCOUNT_PEER_LOCATION_PEER_LOCATION_FIND_TIMEOUT_IN_SECONDS), false);

        // kick start the object
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //---------------------------------------------------------------------
      AccountPeerLocation::~AccountPeerLocation()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      AccountPeerLocationPtr AccountPeerLocation::convert(ForAccountPtr object)
      {
        return dynamic_pointer_cast<AccountPeerLocation>(object);
      }

      //-----------------------------------------------------------------------
      ElementPtr AccountPeerLocation::toDebug(AccountPeerLocationPtr peerLocation)
      {
        if (!peerLocation) return ElementPtr();
        return peerLocation->toDebug();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IAccountPeerLocationForAccount
      #pragma mark

      //---------------------------------------------------------------------
      AccountPeerLocation::ForAccountPtr AccountPeerLocation::createFromIncomingPeerLocationFind(
                                                                                                 IAccountPeerLocationDelegatePtr delegate,
                                                                                                 AccountPtr outer,
                                                                                                 PeerLocationFindRequestPtr request,
                                                                                                 IDHPrivateKeyPtr localPrivateKey,
                                                                                                 IDHPublicKeyPtr localPublicKey
                                                                                                 )
      {
        AccountPeerLocationPtr pThis(new AccountPeerLocation(UseStack::queueStack(), delegate, outer, request, localPrivateKey, localPublicKey));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //---------------------------------------------------------------------
      AccountPeerLocation::ForAccountPtr AccountPeerLocation::createFromPeerLocationFindResult(
                                                                                               IAccountPeerLocationDelegatePtr delegate,
                                                                                               AccountPtr outer,
                                                                                               PeerLocationFindRequestPtr request,
                                                                                               LocationInfoPtr locationInfo
                                                                                               )
      {
        AccountPeerLocationPtr pThis(new AccountPeerLocation(UseStack::queueStack(), delegate, outer, request, locationInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr AccountPeerLocation::getLocation() const
      {
        AutoRecursiveLock lock(*this);
        return Location::convert(mLocation);
      }

      //-----------------------------------------------------------------------
      LocationInfoPtr AccountPeerLocation::getLocationInfo() const
      {
        AutoRecursiveLock lock(*this);
        return mLocationInfo;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::shutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates AccountPeerLocation::getState() const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::shouldRefindNow() const
      {
        AutoRecursiveLock lock(*this);

        if ((mHadConnection) &&
            (Time() != mIdentifyTime)) {
          Time tick = zsLib::now();

          Duration timeConnected;
          if (tick > mIdentifyTime) {
            timeConnected = tick - mIdentifyTime;
          }

          if (timeConnected > Seconds(OPENPEER_STACK_ACCOUNT_PEER_LOCATION_MIN_CONNECTION_TIME_NEEDED_TO_REFIND_IN_SECONDS)) {
            ZS_LOG_DEBUG(log("allowing auto-refind now because connection time exceeded minimum connection time before refind is allowed") + ZS_PARAM("connected time", mIdentifyTime) + ZS_PARAM("now", tick) + ZS_PARAM("connected time (s)", timeConnected))
            return true;
          }
          ZS_LOG_WARNING(Detail, log("connected but it did not exceed time needed before auto-refind would be allowed") + ZS_PARAM("connected time", mIdentifyTime) + ZS_PARAM("now", tick) + ZS_PARAM("connected time (s)", timeConnected))
        }
        return mShouldRefindNow;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::isConnected() const
      {
        AutoRecursiveLock lock(*this);
        return isReady();
      }

      //-----------------------------------------------------------------------
      Time AccountPeerLocation::getTimeOfLastActivity() const
      {
        AutoRecursiveLock lock(*this);
        return mLastActivity;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::wasCreatedFromIncomingFind() const
      {
        AutoRecursiveLock lock(*this);
        return CreatedFromReason_IncomingFind == mCreatedReason;
      }

      //-----------------------------------------------------------------------
      Time AccountPeerLocation::getCreationFindRequestTimestamp() const
      {
        AutoRecursiveLock lock(*this);
        return mFindRequest->created();
      }

      //-----------------------------------------------------------------------
      String AccountPeerLocation::getFindRequestContext() const
      {
        AutoRecursiveLock lock(*this);
        return mFindRequest->context();
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::hasReceivedCandidateInformation() const
      {
        AutoRecursiveLock lock(*this);
        return (bool)mSocketSession;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::hasReceivedFinalCandidateInformation() const
      {
        AutoRecursiveLock lock(*this);
        return ((bool)mSocketSession) && (mCandidatesFinal);
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::send(MessagePtr message) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("attempted to send a message but the location is shutdown"))
          return false;
        }

        if (!isLegalDuringPreIdentify(message)) {
          ZS_LOG_WARNING(Detail, log("only identify result or peer location find notify requests may be sent out at this time"))
          return false;
        }

        DocumentPtr document = message->encode();
        if (!document) {
          ZS_LOG_WARNING(Detail, log("message failed to encode") + Message::toDebug(message))
          return false;
        }

        ElementPtr rootEl = document->getFirstChildElement();
        if (rootEl) {
          AttributePtr appID = rootEl->findAttribute("appid");
          if (appID) {
            ZS_LOG_TRACE(log("stripping \"appid\" attribute from root element") + ZS_PARAM("appid", appID->getValue()))
            appID->orphan();
          }
        }

        SecureByteBlockPtr output = IHelper::writeAsJSON(document);

        if (ZS_IS_LOGGING(Detail)) {
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("PEER SEND MESSAGE") + ZS_PARAM("json out", ((CSTR)(output->BytePtr()))))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        mLastActivity = zsLib::now();

        if (mMLSSendStream) {
          if (mMLSSendStream->isWriterReady()) {
            if (!mDebugForceMessagesOverRelay) {
              ZS_LOG_TRACE(log("message sent via RUDP/MLS"))

              mMLSSendStream->write(output->BytePtr(), output->SizeInBytes());
              return true;
            }
          }
        }

        if (mOutgoingRelaySendStream) {
          if (mOutgoingRelaySendStream->isWriterReady()) {
            ZS_LOG_TRACE(log("message send via outgoing relay"))
            mOutgoingRelaySendStream->write(output->BytePtr(), output->SizeInBytes());
            return true;
          }
        }

        if (mIncomingRelaySendStream) {
          if (mIncomingRelaySendStream->isWriterReady()) {
            ZS_LOG_TRACE(log("message send via incoming relay"))
            mIncomingRelaySendStream->write(output->BytePtr(), output->SizeInBytes());
            return true;
          }
        }

        ZS_LOG_WARNING(Detail, log("requested to send a message but messaging is not ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      IMessageMonitorPtr AccountPeerLocation::sendRequest(
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

      //-----------------------------------------------------------------------
      void AccountPeerLocation::sendKeepAlive()
      {
        AutoRecursiveLock lock(*this);

        if (mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("keep alive requester is already in progress"))
          return;
        }

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_ERROR(Debug, log("stack account appears to be gone thus cannot keep alive"))
          return;
        }

        if (isCreatedFromOutgoingFind()) {
          if (!mIdentifyMonitor) {
            if (Time() == mIdentifyTime) {
              ZS_LOG_WARNING(Detail, log("cannot send keep alive as identity request was not sent"))
              return;
            }
          }
        }

        PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::create();
        request->domain(outer->getDomain());

        mKeepAliveMonitor = sendRequest(IMessageMonitorResultDelegate<PeerKeepAliveResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleIncomingChannelMapNotify(
                                                               ChannelMapNotifyPtr channelMapNotify,
                                                               const Token &relayToken
                                                               )
      {
        AutoRecursiveLock lock(*this);

        if (!isCreatedFromOutgoingFind()) {
          ZS_LOG_TRACE(log("only the peer that issued the find request would be expecting a channel map notification for an incoming relay channel"))
          return false;
        }

        String localContext = channelMapNotify->localContext();
        String remoteContext = channelMapNotify->remoteContext();

        ZS_LOG_TRACE(log("checking to see if this channel map notify is for this location") + ZS_PARAM("channel map local context", localContext) + ZS_PARAM("channel map remote context", remoteContext))

        if ((localContext != mLocalContext) ||
            (remoteContext != mRemoteContext)) {
          ZS_LOG_TRACE(log("channel map notify is not related to this peer location (try a different peer location?)") + ZS_PARAM("local context", mLocalContext) + ZS_PARAM("remote context", mRemoteContext))
          return false;
        }

        // check to see if nonce was seen before
        {
          String hashNonce = IHelper::convertToHex(*IHelper::hash(relayToken.mNonce));
          String nonceNamespace = OPENPEER_STACK_ACCOUNT_PEER_LOCATION_CHANNEL_MAP_COOKIE_NONCE_CACHE_NAMESPACE + hashNonce;

          String result = ICache::fetch(nonceNamespace);
          if (result.hasData()) {
            ZS_LOG_ERROR(Detail, log("nonce was seen previously") + ZS_PARAM("nonce", relayToken.mNonce) + ZS_PARAM("nonce namespace", nonceNamespace))
            return false;
          }
        }

        ChannelMapNotify::ChannelNumber channel = channelMapNotify->channelNumber();

        Token relayProofToken = channelMapNotify->relayToken();

        if (!relayToken.validate(relayProofToken)) {
          ZS_LOG_WARNING(Detail, log("channel map notify proof token failed") + relayToken.toDebug() + relayProofToken.toDebug())
          return false;
        }

        if (OPENPEER_STACK_ACCOUNT_PEER_LOCATION_CHANNEL_MAP_NOTIFY_RESOURCE != relayProofToken.mResource) {
          ZS_LOG_WARNING(Detail, log("channel map notify proof validates but resource is not correct") + ZS_PARAM("expecting", OPENPEER_STACK_ACCOUNT_PEER_LOCATION_CHANNEL_MAP_NOTIFY_RESOURCE) + relayToken.toDebug() + relayProofToken.toDebug())
          return false;
        }

        ZS_LOG_DEBUG(log("channel map request accepted") + ZS_PARAM("channel number", channel))

        mIncomingRelayChannelNumber = channel;
        return true;
      }

      //-----------------------------------------------------------------------
      AccountPeerLocation::ChannelNumber AccountPeerLocation::getIncomingRelayChannelNumber() const
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("get incoming relay channel number called") + ZS_PARAM("channel number", mIncomingRelayChannelNumber))
        return mIncomingRelayChannelNumber;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::notifyIncomingRelayChannel(
                                                           IFinderRelayChannelPtr channel,
                                                           ITransportStreamPtr receiveStream,
                                                           ITransportStreamPtr sendStream
                                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!channel)
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        ZS_LOG_DEBUG(log("notify incoming relay channel") + ZS_PARAM("receive stream id", receiveStream->getID()) + ZS_PARAM("send stream id", sendStream->getID()) + IFinderRelayChannel::toDebug(channel))

        AutoRecursiveLock lock(*this);
        if (mIncomingRelayChannel == channel) {
          ZS_LOG_DEBUG(log("already notified about this incoming relay channel (thus ignoring)"))
          return;
        }

        mIncomingRelayChannel = channel;
        mIncomingRelayChannelSubscription = mIncomingRelayChannel->subscribe(mThisWeak.lock());

        mIncomingRelayReceiveStream = receiveStream->getReader();
        mIncomingRelayReceiveStreamSubscription = mIncomingRelayReceiveStream->subscribe(mThisWeak.lock());

        mIncomingRelaySendStream = sendStream->getWriter();
        mIncomingRelaySendStreamSubscription = mIncomingRelaySendStream->subscribe(mThisWeak.lock());

        mIncomingRelayReceiveStream->notifyReaderReadyToRead();

        mIncomingRelayChannel->setIncomingContext(mLocalContext, mDHLocalPrivateKey, mDHLocalPublicKey, Peer::convert(mPeer));

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer"))

        AutoRecursiveLock lock(*this);
        if (timer != mFindRequestTimer) {
          ZS_LOG_WARNING(Detail, log("received timer event for obsolete timer") + ZS_PARAM("timer", timer->getID()))
          return;
        }

        mFindRequestTimer->cancel();
        mFindRequestTimer.reset();

        // fake that the message monitor timed-out because it never really times out...
        onMessageMonitorTimedOut(mFindRequestMonitor, PeerLocationFindNotifyPtr());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onLookupCompleted(IDNSQueryPtr query)
      {
        ZS_LOG_DEBUG(log("on lookup complete"))

        AutoRecursiveLock lock(*this);

        if (query == mOutgoingSRVLookup) {
          mOutgoingSRVResult = query->getSRV();
          mOutgoingSRVLookup.reset();

          if (!mOutgoingSRVResult) {
            ZS_LOG_ERROR(Detail, log("failed to resolve SRV query for outgoing finder connection"))
            cancel();
            return;
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IFinderRelayChannelDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onFinderRelayChannelStateChanged(
                                                                 IFinderRelayChannelPtr channel,
                                                                 IFinderRelayChannel::SessionStates state
                                                                 )
      {
        ZS_LOG_DEBUG(log("on finder relay channel state changed") + ZS_PARAM("relay channel", channel->getID()) + ZS_PARAM("state", IFinderRelayChannel::toString(state)))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel)
      {
        AutoRecursiveLock lock(*this);
        if (mOutgoingRelayChannel == channel) {
          ZS_THROW_INVALID_ASSUMPTION(log("outgoing relay channels should not need context"))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IICESocketDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onICESocketStateChanged(
                                                        IICESocketPtr socket,
                                                        ICESocketStates state
                                                        )
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onICESocketCandidatesChanged(IICESocketPtr socket)
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPTransportDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPTransportStateChanged(
                                                            IRUDPTransportPtr session,
                                                            RUDPTransportStates state
                                                            )
      {
        ZS_LOG_DEBUG(log("on RUDP ICE socket session state changed") + ZS_PARAM("session", session->getID()) + ZS_PARAM("state", IRUDPTransport::toString(state)))

        AutoRecursiveLock lock(*this);
        step();
      }

      //---------------------------------------------------------------------
      void AccountPeerLocation::onRUDPTransportChannelWaiting(IRUDPTransportPtr session)
      {
        ZS_LOG_DEBUG(log("received RUDP channel waiting") + ZS_PARAM("sessionID", session->getID()))

        AutoRecursiveLock lock(*this);

        // scope: check if should accept channel
        {
          if ((isShutdown()) ||
              (isShuttingDown())) {
            ZS_LOG_WARNING(Debug, log("incoming RUDP session ignored during shutdown process"))
            goto ignore_incoming_channel;
          }

          if (session != mRUDPSocketSession) {
            ZS_LOG_WARNING(Detail, log("received incoming channel on obsolete RUDP session") + ZS_PARAM("session ID", session->getID()))
            goto ignore_incoming_channel;
          }

          if (!isCreatedFromIncomingFind()) {
            ZS_LOG_ERROR(Detail, debug("only the location that received the find request will listen for incoming RUDP channels"))
            goto ignore_incoming_channel;
          }

          if (mMessaging) {
            ZS_LOG_WARNING(Detail, debug("received incoming channel when existing messaging already exists"))
            goto ignore_incoming_channel;
          }

          goto accept_incoming_channel;
        }

      ignore_incoming_channel:
        {
          ITransportStreamPtr receiveStream = ITransportStream::create();
          ITransportStreamPtr sendStream = ITransportStream::create();

          IRUDPMessagingPtr messaging = IRUDPMessaging::acceptChannel(UseStack::queueServices(), mRUDPSocketSession, mThisWeak.lock(), receiveStream, sendStream);
          if (!messaging) return;

          messaging->shutdown();
          return;
        }

      accept_incoming_channel:
        {
          ZS_LOG_DEBUG(log("incoming messaging session being answered (will later plumb to the MLS channel)"))

          ITransportStreamPtr receiveStream = ITransportStream::create();
          ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

          // no messaging present, accept this incoming channel
          mMessaging = IRUDPMessaging::acceptChannel(
                                                     UseStack::queueServices(),
                                                     mRUDPSocketSession,
                                                     mThisWeak.lock(),
                                                     receiveStream,
                                                     sendStream
                                                     );

          mMessagingReceiveStream = receiveStream->getReader();
          mMessagingSendStream = sendStream->getWriter();

          mMessagingReceiveStream->notifyReaderReadyToRead();

          if (OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO != mMessaging->getRemoteConnectionInfo()) {
            ZS_LOG_WARNING(Detail, log("received unknown incoming connection type thus shutting down incoming connection") + ZS_PARAM("type", mMessaging->getRemoteConnectionInfo()))

            mMessaging->shutdown();
            mMessaging.reset();
            return;
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPMessagingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPMessagingStateChanged(
                                                            IRUDPMessagingPtr messaging,
                                                            RUDPMessagingStates state
                                                            )
      {
        ZS_LOG_DEBUG(log("on RUDP messaging state changed") + ZS_PARAM("messaging", messaging->getID()) + ZS_PARAM("state", IRUDPMessaging::toString(state)))

        AutoRecursiveLock lock(*this);
        if (messaging != mMessaging) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete RUDP messaging") + ZS_PARAM("messaging", messaging->getID()) + ZS_PARAM("state", IRUDPMessaging::toString(state)))
          return;
        }

        step();

        if ((state == IRUDPMessaging::RUDPMessagingState_ShuttingDown) ||
            (state == IRUDPMessaging::RUDPMessagingState_Shutdown)) {

          if ((isShuttingDown()) ||
              (isShutdown()))
          {
            ZS_LOG_TRACE(log("RUDP messaging being closed during shutdown"))
            return;
          }

          ZS_LOG_WARNING(Detail, log("due to the unexpected shutdown of the RUDP messaging which did not cause the peer connection to shutdown a keep alive is being forced over the channel"))

          // force a keep alive now to ensure the session is still alive
          Time lastActivity = mLastActivity;

          sendKeepAlive();

          // this keep alive will not be counted as intentional local activity meant to keep the connection alive
          mLastActivity = lastActivity;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageLayerSecurityChannelDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onMessageLayerSecurityChannelStateChanged(
                                                                          IMessageLayerSecurityChannelPtr channel,
                                                                          IMessageLayerSecurityChannel::SessionStates state
                                                                          )
      {
        ZS_LOG_DEBUG(log("on message layer security channel state changed") + ZS_PARAM("channel", channel->getID()) + ZS_PARAM("state", IMessageLayerSecurityChannel::toString(state)))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        ZS_LOG_TRACE(log("on stream write ready") + ZS_PARAM("stream writer", writer->getID()))

        AutoRecursiveLock lock(*this);

        if (mHadConnection) {
          ZS_LOG_TRACE(log("transport stream write ready already had connection (thus ignored)"))
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => ITransportStreamReaderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) return;

        if ((reader != mMLSReceiveStream) &&
            (reader != mIncomingRelayReceiveStream) &&
            (reader != mOutgoingRelayReceiveStream)) {
          ZS_LOG_WARNING(Debug, log("messaging reader ready arrived for obsolete stream") + ZS_PARAM("stream reader id", reader->getID()))
          return;
        }

        while (true) {
          SecureByteBlockPtr buffer = reader->read();
          if (!buffer) {
            ZS_LOG_TRACE(log("no data to read"))
            return;
          }

          const char *bufferStr = (CSTR)(buffer->BytePtr());

          if (0 == strcmp(bufferStr, "\n")) {
            ZS_LOG_TRACE(log("received new line ping"))
            continue;
          }

          mLastActivity = zsLib::now();

          DocumentPtr document = Document::createFromAutoDetect(bufferStr);
          MessagePtr message = Message::create(document, Location::convert(mLocation));
          
          if (ZS_IS_LOGGING(Detail)) {
            bool viaRelay = (reader == mOutgoingRelayReceiveStream) || (reader == mIncomingRelayReceiveStream);
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("PEER RECEIVED MESSAGE") + ZS_PARAM("via", viaRelay ? "RELAY" : "RUDP/MLS") + ZS_PARAM("json in", bufferStr))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          }

          if (!message) {
            ZS_LOG_WARNING(Detail, log("failed to create a message object from incoming message"))
            return;
          }

          if (IMessageMonitor::handleMessageReceived(message)) {
            ZS_LOG_DEBUG(log("handled message via message handler"))
            return;
          }

          handleMessage(message);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerLocationFindNotify>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   PeerLocationFindNotifyPtr notify
                                                                   )
      {
        ZS_LOG_DEBUG(log("handing peer location find notify") + ZS_PARAM("monitor", monitor ? monitor->getID() : 0))

        AutoRecursiveLock lock(*this);

        if (notify->context() != mRemoteContext) {
          ZS_LOG_TRACE(log("this notification belongs to a different location (thus the message is ignored - forked peer location find request?)") + ZS_PARAM("notify context", notify->context()) + ZS_PARAM("expecting remote context", mRemoteContext))
          return false;
        }

        UseLocationPtr location = Location::convert(notify->locationInfo()->mLocation);
        if (!location) {
          ZS_LOG_ERROR(Detail, log("location not properly identified"))
          cancel();
          return true;
        }

        // verify this notification comes from the expecting peer

        if (location->getPeerURI() != mPeer->getPeerURI()) {
          ZS_LOG_ERROR(Detail, log("peer URL does not match expecting peer URI") + UseLocation::toDebug(location) + UsePeer::toDebug(mPeer))
          cancel();
          return true;
        }

        if (isCreatedFromOutgoingFind()) {

          if (notify->validated()) {
            ZS_LOG_DEBUG(log("remote party does not not wish to receive peer-identify request"))
            mIdentifyTime = zsLib::now();
          }

          if (!mDHRemotePublicKey) {
            if (mIncomingRelayChannel) {
              IDHKeyDomainPtr remoteKeyDomain = mIncomingRelayChannel->getDHRemoteKeyDomain();
              IDHPublicKeyPtr remotePublicKey = mIncomingRelayChannel->getDHRemotePublicKey();

              // don't allow one without the other
              if (!remotePublicKey) remoteKeyDomain.reset();
              if (!remoteKeyDomain) remotePublicKey.reset();

              if (remoteKeyDomain) {
                ZS_LOG_DEBUG(log("extracted DH keying material from relay channel") + ZS_PARAM("remote public key", remotePublicKey->getID()) + ZS_PARAM("remote key domain", remoteKeyDomain->getID()))

                IDHKeyDomain::KeyDomainPrecompiledTypes remoteType = remoteKeyDomain->getPrecompiledType();
                IDHKeyDomain::KeyDomainPrecompiledTypes localType = mDHLocalPrivateKey->getKeyDomain()->getPrecompiledType();

                if (remoteType != localType) {
                  ZS_LOG_ERROR(Detail, log("remote key domain type does not match local key domain type") + ZS_PARAM("remote type", IDHKeyDomain::toNamespace(remoteType)) + ZS_PARAM("local type", IDHKeyDomain::toNamespace(localType)))

                  cancel();
                  return false;
                }
              }

              ZS_LOG_TRACE(log("obtained remote DH public key") + ZS_PARAM("remote public key", remotePublicKey->getID()))
              mDHRemotePublicKey = remotePublicKey;
            }
          }

        }

        connectLocation(
                        notify->iceUsernameFrag(),
                        notify->icePassword(),
                        notify->locationInfo()->mCandidates,
                        notify->final()
                        );

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        // request can never be handled because notifies can happen at any time in the future
        return false;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onMessageMonitorTimedOut(
                                                         IMessageMonitorPtr monitor,
                                                         PeerLocationFindNotifyPtr response
                                                         )
      {
        ZS_LOG_DEBUG(log("handing peer location find notify time out") + ZS_PARAM("monitor", monitor->getID()))

        AutoRecursiveLock lock(*this);

        if (!mHadConnection) {
          ZS_LOG_WARNING(Detail, log("failed to establish any kind of connection in a reasonable time frame so going to shutdown peer location"))
          cancel();
          return;
        }

        if (!mSocketSession) {
          ZS_LOG_WARNING(Detail, log("issued find request and received a result but do not have a socket session yet (thus time to give up on this location and shutdown)"))
          cancel();
          return;
        }

        // tell the socket session to give up on receiving any more remote candidates
        mSocketSession->endOfRemoteCandidates();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerIdentifyResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   PeerIdentifyResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentifyMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_DEBUG(log("received result to peer identity request"))

        mIdentifyMonitor->cancel();
        mIdentifyMonitor.reset();

        UseLocationPtr location;
        if (result->locationInfo()) {
          location = Location::convert(result->locationInfo()->mLocation);
        }

        if (!location) {
          ZS_LOG_ERROR(Detail, log("location not properly identified"))
          cancel();
          return true;
        }

        if (location->getPeerURI() != mPeer->getPeerURI()) {
          ZS_LOG_ERROR(Detail, log("peer URL does not match expecting peer URI") + UseLocation::toDebug(location) + UsePeer::toDebug(mPeer))
          cancel();
          return true;
        }

        mIdentifyTime = zsLib::now();

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        PeerIdentifyResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentifyMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("peer identify request received an error"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerKeepAliveResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   PeerKeepAliveResultPtr result
                                                                   )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        mKeepAliveMonitor->cancel();
        mKeepAliveMonitor.reset();
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        PeerKeepAliveResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        if (monitor != mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("keep alive request received an error"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      IICESocketPtr AccountPeerLocation::getSocket() const
      {
        UseAccountPtr outer = mOuter.lock();
        if (!outer) return IICESocketPtr();
        return outer->getSocket();
      }

      //-----------------------------------------------------------------------
      Log::Params AccountPeerLocation::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("AccountPeerLocation");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params AccountPeerLocation::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr AccountPeerLocation::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("AccountPeerLocation");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "outer", (bool)mOuter.lock());
        IHelper::debugAppend(resultEl, "graceful reference", (bool)mGracefulShutdownReference);

        IHelper::debugAppend(resultEl, "local context", mLocalContext);

        IHelper::debugAppend(resultEl, "remote context", mRemoteContext);

        IHelper::debugAppend(resultEl, "DH local private key", mDHLocalPrivateKey ? mDHLocalPrivateKey->getID() : 0);
        IHelper::debugAppend(resultEl, "DH local public key", mDHLocalPublicKey ? mDHLocalPublicKey->getID() : 0);
        IHelper::debugAppend(resultEl, "DH remote public key", mDHRemotePublicKey ? mDHRemotePublicKey->getID() : 0);

        IHelper::debugAppend(resultEl, "state", IAccount::toString(mCurrentState));
        IHelper::debugAppend(resultEl, "refind", mShouldRefindNow);

        IHelper::debugAppend(resultEl, "last activity", mLastActivity);

        IHelper::debugAppend(resultEl, "location info", mLocationInfo->toDebug());
        if (mLocation != Location::convert(mLocationInfo->mLocation)) {
          IHelper::debugAppend(resultEl, "location", UseLocation::toDebug(mLocation));
        }
        IHelper::debugAppend(resultEl, "peer", UsePeer::toDebug(mPeer));

        IHelper::debugAppend(resultEl, "ice socket subscription id", mSocketSubscription ? mSocketSubscription->getID() : 0);
        IHelper::debugAppend(resultEl, "ice socket session id", mSocketSession ? mSocketSession->getID() : 0);
        IHelper::debugAppend(resultEl, "rudp ice socket session id", mRUDPSocketSession ? mRUDPSocketSession->getID() : 0);
        IHelper::debugAppend(resultEl, "rudp ice socket subscription id", mRUDPSocketSessionSubscription ? mRUDPSocketSessionSubscription->getID() : 0);
        IHelper::debugAppend(resultEl, "rudp messagine id", mMessaging ? mMessaging->getID() : 0);
        IHelper::debugAppend(resultEl, "messaging receive stream id", mMessagingReceiveStream ? mMessagingReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "messaging send stream id", mMessagingSendStream ? mMessagingSendStream->getID() : 0);
        IHelper::debugAppend(resultEl, "candidates final", mCandidatesFinal);
        IHelper::debugAppend(resultEl, "last candidates version sent", mLastCandidateVersionSent);

        IHelper::debugAppend(resultEl, "mls id", mMLSChannel ? mMLSChannel->getID() : 0);
        IHelper::debugAppend(resultEl, "mls receive stream id", mMLSReceiveStream ? mMLSReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "mls send stream id", mMLSSendStream ? mMLSSendStream->getID() : 0);
        IHelper::debugAppend(resultEl, "mls did connect", mMLSDidConnect);

        IHelper::debugAppend(resultEl, "created from", toString(mCreatedReason));
        IHelper::debugAppend(resultEl, "find request", mFindRequest->messageID());

        IHelper::debugAppend(resultEl, "find request monitor", mFindRequestMonitor ? mFindRequestMonitor->getID() : 0);
        IHelper::debugAppend(resultEl, "find request timer", mFindRequestTimer ? mFindRequestTimer->getID() : 0);

        IHelper::debugAppend(resultEl, "outgoing relay channel id", mOutgoingRelayChannel ? mOutgoingRelayChannel->getID() : 0);
        IHelper::debugAppend(resultEl, "outgoing relay receive stream id", mOutgoingRelayReceiveStream ? mOutgoingRelayReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "outgoing relay send stream id", mOutgoingRelaySendStream ? mOutgoingRelaySendStream->getID() : 0);

        IHelper::debugAppend(resultEl, "incoming relay channel number", mIncomingRelayChannelNumber);

        IHelper::debugAppend(resultEl, "incoming relay channel id", mIncomingRelayChannel ? mIncomingRelayChannel->getID() : 0);
        IHelper::debugAppend(resultEl, "incoming relay channel subscription", (bool)mIncomingRelayChannelSubscription);

        IHelper::debugAppend(resultEl, "incoming relay channel receive stream", mIncomingRelayReceiveStream ? mIncomingRelayReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "incoming relay channel send stream", mIncomingRelaySendStream ? mIncomingRelaySendStream->getID() : 0);
        IHelper::debugAppend(resultEl, "incoming relay channel receive stream subscription", (bool)mIncomingRelayReceiveStreamSubscription);
        IHelper::debugAppend(resultEl, "incoming relay channel send stream subscription", (bool)mIncomingRelaySendStreamSubscription);

        IHelper::debugAppend(resultEl, "had any connection", mHadConnection);
        IHelper::debugAppend(resultEl, "had peer connection", mHadPeerConnection);

        IHelper::debugAppend(resultEl, "identify time", mIdentifyTime);

        IHelper::debugAppend(resultEl, "identity monitor", (bool)mIdentifyMonitor);
        IHelper::debugAppend(resultEl, "keep alive monitor", (bool)mKeepAliveMonitor);

        IHelper::debugAppend(resultEl, "force messages via relay", mDebugForceMessagesOverRelay);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::cancel()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return;
        }

        setState(IAccount::AccountState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mFindRequestMonitor) {
          mFindRequestMonitor->cancel();
          mFindRequestMonitor.reset();
        }

        if (mIdentifyMonitor) {
          mIdentifyMonitor->cancel();
          mIdentifyMonitor.reset();
        }

        if (mKeepAliveMonitor) {
          mKeepAliveMonitor->cancel();
          mKeepAliveMonitor.reset();
        }

        if (mFindRequestTimer) {
          mFindRequestTimer->cancel();
          mFindRequestTimer.reset();
        }

        mIncomingRelayChannelNumber = 0;

        if (mGracefulShutdownReference) {

          if (mMessaging) {
            ZS_LOG_DEBUG(log("requesting messaging shutdown"))
            mMessaging->shutdown();

            if (IRUDPMessaging::RUDPMessagingState_Shutdown != mMessaging->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP messaging to shutdown"))
              return;
            }

            mMessaging.reset();
          }

          if (mRUDPSocketSession) {
            ZS_LOG_DEBUG(log("requesting RUDP socket session shutdown"))
            mRUDPSocketSession->shutdown();

            if (IRUDPTransport::RUDPTransportState_Shutdown != mRUDPSocketSession->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP ICE socket session to shutdown"))
              return;
            }

            mRUDPSocketSession.reset();
          }
        }

        setState(IAccount::AccountState_Shutdown);

        mGracefulShutdownReference.reset();
        mDelegate.reset();

        if (mOutgoingRelayChannel) {
          mOutgoingRelayChannel->cancel();
          mOutgoingRelayChannel.reset();
        }

        if (mOutgoingRelayReceiveStream) {
          mOutgoingRelayReceiveStream->cancel();
          mOutgoingRelayReceiveStream.reset();
        }
        if (mOutgoingRelaySendStream) {
          mOutgoingRelaySendStream->cancel();
          mOutgoingRelaySendStream.reset();
        }

        if (mIncomingRelayChannel) {
          mIncomingRelayChannel->cancel();
          mIncomingRelayChannel.reset();
        }
        if (mIncomingRelayChannelSubscription) {
          mIncomingRelayChannelSubscription->cancel();
          mIncomingRelayChannelSubscription.reset();
        }

        if (mIncomingRelayReceiveStream) {
          mIncomingRelayReceiveStream->cancel();
          mIncomingRelayReceiveStream.reset();
        }
        if (mIncomingRelaySendStream) {
          mIncomingRelaySendStream->cancel();
          mIncomingRelaySendStream.reset();
        }
        if (mIncomingRelayReceiveStreamSubscription) {
          mIncomingRelayReceiveStreamSubscription->cancel();
          mIncomingRelayReceiveStreamSubscription.reset();
        }
        if (mIncomingRelaySendStreamSubscription) {
          mIncomingRelaySendStreamSubscription->cancel();
          mIncomingRelaySendStreamSubscription.reset();
        }

        if (mMessaging) {
          ZS_LOG_WARNING(Detail, log("hard shutdown of RUDP messaging"))
          mMessaging->shutdown();
          mMessaging.reset();
        }
        if (mMessagingReceiveStream) {
          mMessagingReceiveStream->cancel();
          mMessagingReceiveStream.reset();
        }
        if (mMessagingSendStream) {
          mMessagingSendStream->cancel();
          mMessagingSendStream.reset();
        }

        if (mSocketSubscription) {
          mSocketSubscription->cancel();
          mSocketSubscription.reset();
        }

        if (mSocketSession) {
          mSocketSession->close();
          mSocketSession.reset();
        }

        if (mSocketSession) {
          mSocketSession->close();
          mSocketSession.reset();
        }

        if (mRUDPSocketSession) {
          ZS_LOG_WARNING(Detail, log("hard shutdown of RUDP socket session"))
          mRUDPSocketSession->shutdown();
          mRUDPSocketSession.reset();
        }

        if (mRUDPSocketSessionSubscription) {
          mRUDPSocketSessionSubscription->cancel();
          mRUDPSocketSessionSubscription.reset();
        }

        ZS_LOG_DEBUG(log("cancel completed"))
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::step()
      {
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel since shutting down/shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_ERROR(Debug, log("stack account appears to be gone thus shutting down"))
          cancel();
          return;
        }

        IICESocketPtr socket = getSocket();
        if (!socket) {
          ZS_LOG_ERROR(Debug, log("attempted to get RUDP ICE socket but ICE socket is gone"))
          cancel();
          return;
        }

        if (!stepSocketSubscription(socket)) return;
        if (!stepOutgoingRelayChannel()) return;
        if (!stepIncomingRelayChannel()) return;

        if (stepRUDPSocketSession()) {
          if (stepMessaging()) {
            if (stepMLS()) {
              // NOTE: account can proceed to ready even if P2P channel cannot be established yet (as relay can be used)
            }
          }
        }

        if (!stepConnectIncomingFind()) return;
        if (!stepAnyConnectionPresent()) return;
        if (!stepSendNotify(socket)) return;
        if (!stepCheckIncomingIdentify()) return;
        if (!stepIdentify()) return;

        setState(IAccount::AccountState_Ready);

        ZS_LOG_TRACE(debug("step complete"))
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepSocketSubscription(IICESocketPtr socket)
      {
        if (mSocketSubscription) {
          socket->wakeup();

          if (IICESocket::ICESocketState_Ready != socket->getState()) {
            ZS_LOG_TRACE(log("ICE socket needs to wake up"))
            return true;
          }

          ZS_LOG_TRACE(log("ICE socket is awake"))
          return true;
        }

        ZS_LOG_DEBUG(log("subscribing to the socket state"))
        mSocketSubscription = socket->subscribe(mThisWeak.lock());
        if (!mSocketSubscription) {
          ZS_LOG_ERROR(Detail, log("failed to subscribe to socket"))
          cancel();
          return false;
        }

        // ensure the socket has been woken up during the subscription process
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepOutgoingRelayChannel()
      {
        if (mOutgoingSRVLookup) {
          ZS_LOG_TRACE(log("outgoing DNS server is still resolving"))
          return false;
        }

        if (!isCreatedFromIncomingFind()) {
          ZS_LOG_TRACE(log("only a location receiving find request will open an outgoing relay channel"))
          return true;
        }

        if (mOutgoingRelayChannel) {
          WORD error = 0;
          String reason;
          switch (mOutgoingRelayChannel->getState(&error, &reason))
          {
            case IFinderRelayChannel::SessionState_Pending:   {
              ZS_LOG_TRACE(log("waiting for relay channel to connect"))
              return false;
            }
            case IFinderRelayChannel::SessionState_Connected: {
              ZS_LOG_TRACE(log("relay channel connected"))
              return true;
            }
            case IFinderRelayChannel::SessionState_Shutdown: {
              ZS_LOG_WARNING(Debug, log("relay channel shutdown"))
              return true;
            }
          }
        }

        CandidateList remoteCandidates;
        remoteCandidates = mFindRequest->locationInfo()->mCandidates;

        CandidateList relayCandidates;

        for (CandidateList::iterator iter = remoteCandidates.begin(); iter != remoteCandidates.end(); ) {
          CandidateList::iterator current = iter;
          ++iter;

          Candidate &candidate = (*current);
          if (OPENPEER_STACK_CANDIDATE_NAMESPACE_ICE_CANDIDATES == candidate.mNamespace) continue;

          if (OPENPEER_STACK_CANDIDATE_NAMESPACE_FINDER_RELAY == candidate.mNamespace) {
            relayCandidates.push_back(candidate);
          }

          remoteCandidates.erase(current);
        }

        if (relayCandidates.size() < 1) {
          ZS_LOG_ERROR(Detail, log("no relay candidates found (thus impossible to open relay channel and must shutdown)"))
          cancel();
          return false;
        }

        Candidate relayCandidate = relayCandidates.front();

        UseAccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        IPAddress outgoingIP;
        if (!IDNS::extractNextIP(mOutgoingSRVResult, outgoingIP)) {
          // need to perform a DNS lookup
          String srv = getMultiplexedJsonTCPTransport(relayCandidate.mProtocols);

          if (srv.isEmpty()) {
            ZS_LOG_ERROR(Detail, log("unable to open relay channel as no supported relay protocol was found"))
            cancel();
            return false;
          }

          ZS_LOG_DEBUG(log("performing SRV lookup on canddiate") + ZS_PARAM("srv", srv))

          mOutgoingSRVLookup = IDNS::lookupSRV(mThisWeak.lock(), srv, "finder", "tcp");
          return false;
        }

        ITransportStreamPtr receiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        if (!mDHRemotePublicKey) {
          ZS_LOG_ERROR(Detail, log("remote party did not include DH remote public key"))
          cancel();
          return false;
        }

        Token relayToken = relayCandidate.mToken;
        String domain = outer->getDomain();

        mOutgoingRelayChannel = IFinderRelayChannel::connect(
                                                             mThisWeak.lock(),
                                                             Account::convert(outer),
                                                             receiveStream,
                                                             sendStream,
                                                             outgoingIP,
                                                             mLocalContext,
                                                             mRemoteContext,
                                                             domain,
                                                             relayToken,
                                                             mDHLocalPrivateKey,
                                                             mDHLocalPublicKey,
                                                             mFindRequest->dhPublicKey()
                                                             );

        if (!mOutgoingRelayChannel) {
          ZS_LOG_ERROR(Detail, log("failed to create outgoing relay channel"))
          cancel();
          return false;
        }

        mOutgoingRelayReceiveStream = receiveStream->getReader();
        mOutgoingRelaySendStream = sendStream->getWriter();

        mOutgoingRelayReceiveStream->notifyReaderReadyToRead();

        ZS_LOG_DEBUG(log("created outgoing relay channel") + IFinderRelayChannel::toDebug(mOutgoingRelayChannel))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIncomingRelayChannel()
      {
        if (!mIncomingRelayChannel) {
          ZS_LOG_TRACE(log("no incoming relay channel detected"))
          return true;
        }

        WORD errorCode = 0;
        String reason;

        switch (mIncomingRelayChannel->getState(&errorCode, &reason)) {
          case IFinderRelayChannel::SessionState_Pending:   {
            ZS_LOG_TRACE(log("incoming relay channel detected but not ready yet"))
            return true;
          }
          case IFinderRelayChannel::SessionState_Connected: {
            break;
          }
          case IFinderRelayChannel::SessionState_Shutdown:  {
            mIncomingRelayChannelNumber = 0;
            ZS_LOG_WARNING(Debug, log("incoming relay channel shutdown but RUDP channel is available (or pending)") + ZS_PARAM("error code", errorCode) + ZS_PARAM("reason", reason))
            return true;
          }
        }

        ZS_LOG_TRACE(log("incoming relay channel is operational") + IFinderRelayChannel::toDebug(mIncomingRelayChannel))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepRUDPSocketSession()
      {
        if (!mRUDPSocketSession) {
          ZS_LOG_TRACE(log("waiting for a RUDP ICE socket session connection"))
          return false;
        }

        if (IRUDPTransport::RUDPTransportState_Ready != mRUDPSocketSession->getState()) {
          ZS_LOG_TRACE(log("waiting for RUDP ICE socket session to complete"))
          return false;
        }

        ZS_LOG_TRACE(log("RUDP socket session is ready"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepMessaging()
      {
        if (mMessaging) {
          if (IRUDPMessaging::RUDPMessagingState_Connected != mMessaging->getState()) {
            ZS_LOG_TRACE(log("waiting for the RUDP messaging to connect"))
            return false;
          }
          ZS_LOG_TRACE(log("messaging is ready"))
          return true;
        }

        if (!isCreatedFromOutgoingFind()) {
          ZS_LOG_TRACE(log("only outgoing find request will issue RUDP channel connect"))
          return true;
        }

        ZS_LOG_TRACE(log("requesting messaging channel open"))

        ITransportStreamPtr receiveStream = ITransportStream::create();
        ITransportStreamPtr sendStream = ITransportStream::create();

        mMessaging = IRUDPMessaging::openChannel(
                                                 UseStack::queueServices(),
                                                 mRUDPSocketSession,
                                                 mThisWeak.lock(),
                                                 OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO,
                                                 receiveStream,
                                                 sendStream
                                                 );

        if (!mMessaging) {
          ZS_LOG_WARNING(Detail, log("unable to open a messaging channel to remote peer thus shutting down"))
          cancel();
          return false;
        }

        mMessagingReceiveStream = receiveStream->getReader();
        mMessagingSendStream = sendStream->getWriter();

        mMessagingReceiveStream->notifyReaderReadyToRead();

        ZS_LOG_DEBUG(log("waiting for messaging to complete"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepMLS()
      {
        if (!mMessaging) {
          ZS_LOG_TRACE(log("no messaging channel setup (thus cannot setup MLS)"))
          return false;
        }

        UseAccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        if (mMLSChannel) {
          WORD error = 0;
          String reason;
          switch (mMLSChannel->getState(&error, &reason)) {
            case IMessageLayerSecurityChannel::SessionState_Pending: {
              ZS_LOG_TRACE(log("MLS channel is pending"))
              return true;
              break;
            }
            case IMessageLayerSecurityChannel::SessionState_WaitingForNeededInformation: {
              ZS_LOG_TRACE(log("MLS waiting for needed information"))

              IPeerFilesPtr peerFiles = outer->getPeerFiles();
              IPeerFilePrivatePtr peerFilePrivate;
              IPeerFilePublicPtr peerFilePublic;
              if (peerFiles) {
                peerFilePrivate = peerFiles->getPeerFilePrivate();
                peerFilePublic = peerFiles->getPeerFilePublic();
              }
              IPeerFilePublicPtr remotePeerFilePublic;
              if (mPeer) {
                ZS_LOG_TRACE(log("remote public peer file is known"))
                remotePeerFilePublic = mPeer->getPeerFilePublic();
              }

              IMessageLayerSecurityChannel::KeyingTypes keyingType = IMessageLayerSecurityChannel::KeyingType_Unknown;
              if (mMLSChannel->needsSendKeying(&keyingType)) {
                switch (keyingType) {
                  case IMessageLayerSecurityChannel::KeyingType_Passphrase:   {
                    ZS_LOG_ERROR(Detail, log("set send keying decoding by passphrase is not recommended (as it doesn't support PFS)"))
                    break;
                  }
                  case IMessageLayerSecurityChannel::KeyingType_PublicKey:    {
                    if (remotePeerFilePublic) {
                      ZS_LOG_DEBUG(log("set send keying remote public key"))
                      mMLSChannel->setSendKeying(remotePeerFilePublic->getPublicKey());
                    }
                    break;
                  }
                  case IMessageLayerSecurityChannel::KeyingType_Unknown:
                  case IMessageLayerSecurityChannel::KeyingType_KeyAgreement: {
                    ZS_THROW_INVALID_ASSUMPTION_IF(!mDHLocalPublicKey)
                    ZS_THROW_INVALID_ASSUMPTION_IF(!mDHLocalPrivateKey)

                    if (mDHRemotePublicKey) {
                      mMLSChannel->setRemoteKeyAgreement(mDHRemotePublicKey);
                    }

                    ZS_LOG_DEBUG(log("set receive keying decoding by public / private key") + ZS_PARAM("local private key", mDHLocalPrivateKey->getID()) + ZS_PARAM("local public key", mDHLocalPublicKey->getID()))
                    mMLSChannel->setLocalKeyAgreement(mDHLocalPrivateKey, mDHLocalPublicKey, isCreatedFromOutgoingFind());
                  }
                }
              }

              keyingType = IMessageLayerSecurityChannel::KeyingType_Unknown;
              if (mMLSChannel->needsReceiveKeying(&keyingType)) {
                switch (keyingType) {
                  case IMessageLayerSecurityChannel::KeyingType_Passphrase:   {
                    ZS_LOG_ERROR(Detail, log("set send keying decoding by passphrase is not recommended (as it doesn't support PFS)"))
                    break;
                  }
                  case IMessageLayerSecurityChannel::KeyingType_PublicKey:    {
                    if (peerFilePublic) {
                      ZS_LOG_DEBUG(log("set receive keying public key"))
                      mMLSChannel->setSendKeying(peerFilePublic->getPublicKey());
                    }
                    break;
                  }
                  case IMessageLayerSecurityChannel::KeyingType_Unknown:
                  case IMessageLayerSecurityChannel::KeyingType_KeyAgreement: {
                    ZS_THROW_INVALID_ASSUMPTION_IF(!mDHLocalPublicKey)
                    ZS_THROW_INVALID_ASSUMPTION_IF(!mDHLocalPrivateKey)

                    ZS_LOG_DEBUG(log("set receive keying decoding by public / private key") + ZS_PARAM("local private key", mDHLocalPrivateKey->getID()) + ZS_PARAM("local public key", mDHLocalPublicKey->getID()))
                    mMLSChannel->setLocalKeyAgreement(mDHLocalPrivateKey, mDHLocalPublicKey, false);
                  }
                }
              }

              if ((mMLSChannel->needsReceiveKeyingSigningPublicKey()) &&
                  (remotePeerFilePublic)) {
                ZS_LOG_DEBUG(log("set receive keying signing public key"))
                mMLSChannel->setReceiveKeyingSigningPublicKey(remotePeerFilePublic->getPublicKey());
              }

              if ((mMLSChannel->needsSendKeyingToeBeSigned()) &&
                  (peerFilePrivate) &&
                  (peerFilePublic)) {
                DocumentPtr doc;
                ElementPtr signEl;
                mMLSChannel->getSendKeyingNeedingToBeSigned(doc, signEl);

                if (signEl) {
                  ZS_LOG_DEBUG(log("notified send keying material signed"))
                  peerFilePrivate->signElement(signEl);
                  mMLSChannel->notifySendKeyingSigned(peerFilePrivate->getPrivateKey(), peerFilePublic->getPublicKey());
                }
              }
              return true;
            }
            case IMessageLayerSecurityChannel::SessionState_Connected: {
              ZS_LOG_TRACE(log("MLS is connected"))
              get(mMLSDidConnect) = true;
              return true;
            }
            case IMessageLayerSecurityChannel::SessionState_Shutdown: {
              ZS_LOG_ERROR(Detail, log("MLS failed") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              cancel();
              return false;
            }
          }

          ZS_THROW_BAD_STATE(log("unhandled case (should never happen)"))
          return false;
        }

        ITransportStreamPtr receiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        mMLSChannel = IMessageLayerSecurityChannel::create(
                                                           mThisWeak.lock(),
                                                           mMessagingReceiveStream->getStream(),
                                                           receiveStream,
                                                           sendStream,
                                                           mMessagingSendStream->getStream(),
                                                           mLocalContext
                                                           );

        if (!mMLSChannel) {
          ZS_LOG_ERROR(Detail, log("could not create MLS"))
          cancel();
          return false;
        }

        mMLSReceiveStream = receiveStream->getReader();
        mMLSSendStream = sendStream->getWriter();

        mMLSReceiveStream->notifyReaderReadyToRead();
        
        ZS_LOG_DEBUG(log("waiting for MLS to complete"))
        
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepConnectIncomingFind()
      {
        if (!isCreatedFromIncomingFind()) {
          ZS_LOG_TRACE(log("not coming from an incoming find request"))
          return true;
        }

        if (mSocketSession) {
          ZS_LOG_TRACE(log("already have a connected session"))
          return true;
        }

        ZS_LOG_DEBUG(log("connecting to incoming find request location"))

        CandidateList remoteCandidates;
        remoteCandidates = mFindRequest->locationInfo()->mCandidates;

        CandidateList relayCandidates;

        for (CandidateList::iterator iter = remoteCandidates.begin(); iter != remoteCandidates.end(); ) {
          CandidateList::iterator current = iter;
          ++iter;

          Candidate &candidate = (*current);
          if (OPENPEER_STACK_CANDIDATE_NAMESPACE_ICE_CANDIDATES == candidate.mNamespace) continue;

          if (OPENPEER_STACK_CANDIDATE_NAMESPACE_FINDER_RELAY == candidate.mNamespace) {
            relayCandidates.push_back(candidate);
          }

          remoteCandidates.erase(current);
        }

        connectLocation(
                        mFindRequest->iceUsernameFrag(),
                        mFindRequest->icePassword(),
                        remoteCandidates,
                        mFindRequest->final()
                        );
        
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepAnyConnectionPresent()
      {
        bool foundRelay = false;
        bool foundPeer = false;

        bool incomingRelayFailed = false;
        bool outgoingRelayFailed = false;
        bool peerFailed = false;

        bool ready = false;

        if (mMLSSendStream) {
          if (mMLSSendStream->isWriterReady()) {
            foundRelay = ready = true;
          }
        }

        if (mOutgoingRelayReceiveStream) {
          if (mOutgoingRelaySendStream->isWriterReady()) {
            foundRelay = ready = true;
          }
        }

        if (mIncomingRelaySendStream) {
          if (mIncomingRelaySendStream->isWriterReady()) {
            foundPeer = ready = true;
          }
        }

        WORD error = 0;
        String reason;

        if (!foundRelay) {
          if (mIncomingRelayChannel) {
            switch (mIncomingRelayChannel->getState(&error, &reason)) {
              case IFinderRelayChannel::SessionState_Pending:
              case IFinderRelayChannel::SessionState_Connected: {
                ZS_LOG_TRACE(log("incoming relay channel may become ready"))
                foundRelay = true;
                break;
              }
              case IFinderRelayChannel::SessionState_Shutdown:  {
                ZS_LOG_WARNING(Trace, log("incoming relay channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
                incomingRelayFailed = true;
                break;
              }
            }
          }

          if (mOutgoingRelayChannel) {
            switch (mOutgoingRelayChannel->getState(&error, &reason))
            {
              case IFinderRelayChannel::SessionState_Pending:
              case IFinderRelayChannel::SessionState_Connected: {
                ZS_LOG_TRACE(log("outgoing relay channel pending (channel may still become available)"))
                foundRelay = true;
                break;
              }
              case IFinderRelayChannel::SessionState_Shutdown: {
                ZS_LOG_WARNING(Trace, log("outgoing relay channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
                outgoingRelayFailed = true;
                break;
              }
            }
          }
        }

        if (mSocketSession) {
          switch (mSocketSession->getState(&error, &reason)) {
            case IICESocketSession::ICESocketSessionState_Pending:
            case IICESocketSession::ICESocketSessionState_Prepared:
            case IICESocketSession::ICESocketSessionState_Searching:
            case IICESocketSession::ICESocketSessionState_Haulted:
            case IICESocketSession::ICESocketSessionState_Nominating:
            {
              ZS_LOG_TRACE(log("ICE socket session may become ready yet"))
              if (!foundRelay) {
                mSocketSession->endOfRemoteCandidates();  // no more candidates are likely going to arrive
              }
              foundPeer = true;
              break;
            }
            case IICESocketSession::ICESocketSessionState_Nominated:
            case IICESocketSession::ICESocketSessionState_Completed:
            {
              ZS_LOG_TRACE(log("ICE socket session is ready (but have to check if RUDP socket session is ready)"))
              ZS_THROW_BAD_STATE_IF(!mRUDPSocketSession)  // how is this possible since it's created with the ICE socket
              foundPeer = true;
              break;
            }
            case IICESocketSession::ICESocketSessionState_Shutdown:
              if (IICESocketSession::ICESocketSessionShutdownReason_BackgroundingTimeout == error) {
                get(mShouldRefindNow) = true;
              }
              ZS_LOG_WARNING(Trace, log("ice socket session is shutdown (thus unuseable)") + ZS_PARAM("should refind now", mShouldRefindNow) + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              peerFailed = true;
              break;
          }
        }

        if (mRUDPSocketSession) {
          switch (mRUDPSocketSession->getState(&error, &reason)) {
            case IRUDPTransport::RUDPTransportState_Pending:
            {
              ZS_LOG_TRACE(log("RUDP socket session may become ready yet"))
              foundPeer = true;
              break;
            }
            case IRUDPTransport::RUDPTransportState_Ready:
            {
              ZS_LOG_TRACE(log("RUDP socket session is ready (but have to check if messaging is ready)"))
              foundPeer = true;
              break;
            }
            case IRUDPTransport::RUDPTransportState_ShuttingDown:
            case IRUDPTransport::RUDPTransportState_Shutdown:
            {
              ZS_LOG_WARNING(Trace, log("rudp socket session is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              peerFailed = true;
              break;
            }
          }
        }

        if (mMessaging) {
          switch (mMessaging->getState(&error, &reason)) {
            case IRUDPMessaging::RUDPMessagingState_Connecting:
            {
              ZS_LOG_TRACE(log("messaging channel may become ready yet") + ZS_PARAM("messaging id", mMessaging->getID()))
              foundPeer = true;
              break;
            }
            case IRUDPMessaging::RUDPMessagingState_Connected:
            {
              ZS_LOG_TRACE(log("messaging channel is connected (but must see if MLS channel is ready)"))
              foundPeer = true;
              break;
            }
            case IRUDPMessaging::RUDPMessagingState_ShuttingDown:
            case IRUDPMessaging::RUDPMessagingState_Shutdown: {
              ZS_LOG_WARNING(Trace, log("messaging channel is shutdown (thus unuseable)") + ZS_PARAM("messaging id", mMessaging->getID()) + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              peerFailed = true;
              break;
            }
          }
        }

        if (mMLSChannel) {
          if (peerFailed) {
            mMLSChannel->cancel();
          }
          switch (mMLSChannel->getState(&error, &reason)) {
            case IMessageLayerSecurityChannel::SessionState_Pending:
            case IMessageLayerSecurityChannel::SessionState_WaitingForNeededInformation: {
              ZS_LOG_TRACE(log("MLS channel is still pending (thus communication channel is not available yet)"))
              foundPeer = true;
              break;
            }
            case IMessageLayerSecurityChannel::SessionState_Connected:  {
              ZS_LOG_TRACE(log("MLS channel is ready thus communication channel is available"))
              foundPeer = ready = true;
              get(mHadPeerConnection) = true;
              break;
            }
            case IMessageLayerSecurityChannel::SessionState_Shutdown: {
              ZS_LOG_WARNING(Trace, log("MLS channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              peerFailed = true;
              break;
            }
          }
        }

        if (mHadPeerConnection) {
          if (peerFailed) {
            ZS_LOG_WARNING(Detail, log("the peer connection is no longer available thus must shutdown (not going to continue via relay even if it is an option)"))
            cancel();
            return false;
          }
        }

        if (ready) {
          ZS_LOG_TRACE(log("at least one mechanism is ready to send"))
          get(mHadConnection) = true;
          return true;
        }

        if (foundPeer) {
          if (!peerFailed) {
            ZS_LOG_TRACE(log("found a peer connection but it's not ready so give it time to see if it will connect"))
            return false;
          }
        }

        if (foundRelay) {
          if ((incomingRelayFailed) || (outgoingRelayFailed)) {
            ZS_LOG_WARNING(Detail, log("did not find any relay or peer connection available and a failure is one of the relay channels (thus must shutdown)"))
            cancel();
            return false;
          }
          
          ZS_LOG_TRACE(log("found a relay connection thus give it time to see if it will connect"))
          return false;
        }

        if (peerFailed) {
          ZS_LOG_WARNING(Detail, log("something went wrong in peer connection thus must shutdown (no relay available to continue the connection)"))
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("did not find a relay channel nor a peer connection so give it time to see if relay channel will arrive"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepSendNotify(IICESocketPtr socket)
      {
        String candidatesVersion = socket->getLocalCandidatesVersion();

        if (candidatesVersion == mLastCandidateVersionSent) {
          ZS_LOG_TRACE(log("candidates have not changed - nothing to notify at this time"))
          return true;
        }

        ZS_LOG_DEBUG(log("candidates have changed since last reported (thus notify another peer location find)") + ZS_PARAM("candidates version", candidatesVersion) + ZS_PARAM("last send version", mLastCandidateVersionSent))


        UseAccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        UseLocationPtr selfLocation = UseLocation::getForLocal(Account::convert(outer));
        LocationInfoPtr selfLocationInfo = selfLocation->getLocationInfo();

        // local candidates are now preparerd, the request can be answered
        PeerLocationFindNotifyPtr notify = PeerLocationFindNotify::create(mFindRequest);

        notify->context(mLocalContext);
        notify->iceUsernameFrag(socket->getUsernameFrag());
        notify->icePassword(socket->getPassword());
        notify->final(selfLocationInfo->mCandidatesFinal);
        notify->locationInfo(selfLocationInfo);
        notify->peerFiles(outer->getPeerFiles());
        notify->validated(mFindRequest->didVerifySignature());

        mLastCandidateVersionSent = selfLocationInfo->mCandidatesVersion;

        send(notify);

        ZS_LOG_TRACE(log("step send notify complete") + ZS_PARAM("final", selfLocationInfo->mCandidatesFinal))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepCheckIncomingIdentify()
      {
        if (!isCreatedFromIncomingFind()) {
          ZS_LOG_TRACE(log("only need to check identify when this received a find request"))
          return true;
        }

        if (Time() != mIdentifyTime) {
          ZS_LOG_TRACE(log("already received identify request (or not expecting remote party to identify)"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting for an incoming connection and identification"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIdentify()
      {
        if (!isCreatedFromOutgoingFind()) {
          ZS_LOG_TRACE(log("only need to send out identify if this location issued the find request"))
          return true;
        }

        if (mIdentifyMonitor) {
          ZS_LOG_TRACE(log("waiting for identify to complete"))
          return false;
        }

        if (Time() != mIdentifyTime) {
          ZS_LOG_TRACE(log("identify already sent (or remote party validated this peer already)"))
          return true;
        }

        if (!mSocketSession) {
          ZS_LOG_TRACE(log("waiting to know if should identify self after receiving incoming peer-location find notification"))
          return false;
        }

        UseAccountPtr outer = mOuter.lock();

        ZS_LOG_DETAIL(log("Peer Location connected, sending identity..."))

        // we have connected, perform the identity request since we made the connection outgoing...
        PeerIdentifyRequestPtr request = PeerIdentifyRequest::create();
        request->domain(outer->getDomain());

        IPeerFilePublicPtr remotePeerFilePublic = mPeer->getPeerFilePublic();
        if (remotePeerFilePublic) {
          request->findSecret(remotePeerFilePublic->getFindSecret());
        }

        UseLocationPtr selfLocation = UseLocation::getForLocal(Account::convert(outer));
        LocationInfoPtr selfLocationInfo = selfLocation->getLocationInfo();
        selfLocationInfo->mCandidates.clear();

        request->location(selfLocationInfo);
        request->peerFiles(outer->getPeerFiles());

        mIdentifyMonitor = sendRequest(IMessageMonitorResultDelegate<PeerIdentifyResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS));
        return false;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::setState(IAccount::AccountStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("old state", IAccount::toString(mCurrentState)) + ZS_PARAM("new state", IAccount::toString(state)))

        mCurrentState = state;

        if (isShutdown()) {
          UseMessageMonitorManager::notifyMessageSenderObjectGone(mID);
        }

        if (!mDelegate) return;

        AccountPeerLocationPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            mDelegate->onAccountPeerLocationStateChanged(mThisWeak.lock(), state);
          } catch (IAccountPeerLocationDelegateProxy::Exceptions::DelegateGone &) {
          }
        }
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::handleMessage(MessagePtr message)
      {
        // this is something new/incoming from the remote server...
        UseAccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("peer message is ignored since account is now gone"))
          return;
        }

        // scope: handle incoming keep alives
        {
          PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::convert(message);
          if (request) {
            ZS_LOG_DEBUG(log("handling incoming peer keep alive request"))

            PeerKeepAliveResultPtr result = PeerKeepAliveResult::create(request);
            send(result);
            return;
          }
        }

        bool expectingIdentify = ((isCreatedFromIncomingFind()) &&
                                  (Time() == mIdentifyTime));

        if (!expectingIdentify) {
          // how can this happen we don't have peer files yet we were not expecting identify?
          ZS_THROW_BAD_STATE_IF(!((bool)mPeer->getPeerFilePublic()))
        }

        if (expectingIdentify) {
          if (!isLegalDuringPreIdentify(message)) {
            ZS_LOG_WARNING(Detail, log("message is not legal when expecting to receive identify request"))

            MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_Forbidden);
            if (result) {
              send(result);
            }
            return;
          }

          // scope: handle identify request
          {
            PeerIdentifyRequestPtr request = PeerIdentifyRequest::convert(message);
            if (!request) {
              ZS_LOG_ERROR(Detail, log("unable to forward message to account since message arrived before peer was identified"))
              goto identify_failure;
            }

            ZS_LOG_DEBUG(log("handling peer identify message"))

            IPeerFilesPtr peerFiles = outer->getPeerFiles();
            if (!peerFiles) {
              ZS_LOG_ERROR(Detail, log("failed to obtain local peer files"))
              goto identify_failure;
            }

            IPeerFilePublicPtr peerFilePublic = peerFiles->getPeerFilePublic();
            if (!peerFiles) {
              ZS_LOG_ERROR(Detail, log("failed to obtain local public peer file"))
              goto identify_failure;
            }

            if (request->findSecret() != peerFilePublic->getFindSecret()) {
              ZS_LOG_ERROR(Detail, log("find secret did not match") + ZS_PARAM("request find secret", request->findSecret()) + IPeerFilePublic::toDebug(peerFilePublic))
              goto identify_failure;
            }

            UseLocationPtr location = Location::convert(mLocationInfo->mLocation);

            if (location->getPeerURI() != mPeer->getPeerURI()) {
              ZS_LOG_ERROR(Detail, log("peer file does not match expecting peer URI") + UseLocation::toDebug(location) + UsePeer::toDebug(mPeer))
              goto identify_failure;
            }

            mIdentifyTime = zsLib::now();

            PeerIdentifyResultPtr result = PeerIdentifyResult::create(request);

            UseLocationPtr selfLocation = UseLocation::getForLocal(Account::convert(outer));

            LocationInfoPtr info = selfLocation->getLocationInfo();
            info->mCandidates.clear();

            result->locationInfo(info);
            send(result);

            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
            return;
          }

        identify_failure:
          {
            MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_Forbidden);
            if (result) {
              send(result);
            }
          }
          return;
        }

        ZS_LOG_DEBUG(log("unknown message, will forward message to account"))

        // scope: send the message to the outer (asynchronously)
        try {
          mDelegate->onAccountPeerLocationMessageIncoming(mThisWeak.lock(), message);
        } catch (IAccountPeerLocationDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::isLegalDuringPreIdentify(MessagePtr message) const
      {
        if (Time() != mIdentifyTime) return true;

        {
          if (message->isResult()) {
            // all results are legal
            return true;
          }
        }

        {
          PeerKeepAliveRequestPtr converted = PeerKeepAliveRequest::convert(message);
          if (converted) return true;
        }
        {
          PeerIdentifyRequestPtr converted = PeerIdentifyRequest::convert(message);
          if (converted) return true;
        }
        {
          PeerLocationFindNotifyPtr converted = PeerLocationFindNotify::convert(message);
          if (converted) return true;
        }

        return false;
      }
      
      //-----------------------------------------------------------------------
      void AccountPeerLocation::connectLocation(
                                                const char *remoteICEUsernameFrag,
                                                const char *remoteICEPassword,
                                                const CandidateList &candidates,
                                                bool candidatesFinal
                                                )
      {
        AutoRecursiveLock lock(*this);
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("received request to connect but location is already shutting down/shutdown"))
          return;
        }

        IICESocketPtr socket = getSocket();
        if (!socket) {
          ZS_LOG_WARNING(Detail, log("no RUDP ICE socket found for peer location"))
          return;
        }

        get(mCandidatesFinal) = candidatesFinal;

        if (!mDHRemotePublicKey) {
          ZS_LOG_ERROR(Detail, log("remote DH public key was never obtained from remote party (thus shutting down)"))
          cancel();
          return;
        }

        ZS_LOG_DETAIL(debug("creating/updating session from remote candidates") + ZS_PARAM("total candidates", candidates.size()))

        // filter out only ICE candidates with a transport type that is understood
        IICESocket::CandidateList iceCandidates;
        for (CandidateList::const_iterator iter = candidates.begin(); iter != candidates.end(); ++iter) {
          const Candidate &candidate = (*iter);
          if ((candidate.mNamespace == OPENPEER_STACK_CANDIDATE_NAMESPACE_ICE_CANDIDATES) &&
              (candidate.mTransport == OPENPEER_STACK_TRANSPORT_JSON_MLS_RUDP)) {
            iceCandidates.push_back(candidate);
          }
        }

        if (!mSocketSession) {
          mSocketSession = IICESocketSession::create(
                                                     IICESocketSessionDelegatePtr(),
                                                     socket,
                                                     remoteICEUsernameFrag,
                                                     remoteICEPassword,
                                                     iceCandidates,
                                                     isCreatedFromOutgoingFind() ? IICESocket::ICEControl_Controlling : IICESocket::ICEControl_Controlled
                                                     );

          if (mSocketSession) {
            ZS_LOG_DEBUG(log("setting keep alive properties for socket session"))
            mSocketSession->setKeepAliveProperties(
                                                   Seconds(OPENPEER_STACK_ACCOUNT_PEER_LOCATION_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS),
                                                   Seconds(OPENPEER_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS),
                                                   Duration(),
                                                   Seconds(OPENPEER_STACK_ACCOUNT_PEER_LOCATION_BACKGROUNDING_TIMEOUT_IN_SECONDS)
                                                   );
            if (candidatesFinal) {
              mSocketSession->endOfRemoteCandidates();
            }
          } else {
            ZS_LOG_ERROR(Detail, log("failed to create socket session"))
          }
        } else {
          mSocketSession->updateRemoteCandidates(iceCandidates);
          if (candidatesFinal) {
            mSocketSession->endOfRemoteCandidates();
          }
        }

        if ((mSocketSession) &&
            (!mRUDPSocketSession)) {
          ZS_LOG_TRACE(log("listing for incoming RUDP channels"))

          mRUDPSocketSession = IRUDPTransport::listen(
                                                      UseStack::queueServices(),
                                                      mSocketSession,
                                                      mThisWeak.lock()
                                                      );
        }

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountPeerLocationFactory &IAccountPeerLocationFactory::singleton()
      {
        return AccountPeerLocationFactory::singleton();
      }

      //-----------------------------------------------------------------------
      IAccountPeerLocationForAccount::ForAccountPtr IAccountPeerLocationFactory::createFromIncomingPeerLocationFind(
                                                                                                                    IAccountPeerLocationDelegatePtr delegate,
                                                                                                                    AccountPtr outer,
                                                                                                                    PeerLocationFindRequestPtr request,
                                                                                                                    IDHPrivateKeyPtr localPrivateKey,
                                                                                                                    IDHPublicKeyPtr localPublicKey
                                                                                                                    )
      {
        if (this) {}
        return AccountPeerLocation::createFromIncomingPeerLocationFind(delegate, outer, request, localPrivateKey, localPublicKey);
      }

      //-----------------------------------------------------------------------
      IAccountPeerLocationForAccount::ForAccountPtr IAccountPeerLocationFactory::createFromPeerLocationFindResult(
                                                                                                                  IAccountPeerLocationDelegatePtr delegate,
                                                                                                                  AccountPtr outer,
                                                                                                                  PeerLocationFindRequestPtr request,
                                                                                                                  LocationInfoPtr locationInfo
                                                                                                                  )
      {
        if (this) {}
        return AccountPeerLocation::createFromPeerLocationFindResult(delegate, outer, request, locationInfo);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
