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
#include <openpeer/stack/internal/stack_AccountPeerLocation.h>
#include <openpeer/stack/internal/stack_AccountFinder.h>
#include <openpeer/stack/internal/stack_MessageMonitor.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/stack/message/peer-to-peer/PeerIdentifyRequest.h>
#include <openpeer/stack/message/peer-to-peer/PeerKeepAliveRequest.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindResult.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindNotify.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/Log.h>
#include <zsLib/helpers.h>

#ifndef _WIN32
#include <unistd.h>
#endif //_WIN32

#define OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO "text/x-openpeer-json-plain"
#define OPENPEER_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS (2*60)
#define OPENPEER_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS (2*60)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_SEND_ICE_KEEP_ALIVE_INDICATIONS_IN_SECONDS (15)
#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS (50)

#define OPENPEER_STACK_ACCOUNT_PEER_LOCATION_BACKGROUNDING_TIMEOUT_IN_SECONDS (OPENPEER_STACK_ACCOUNT_PEER_LOCATION_EXPECT_SESSION_DATA_IN_SECONDS + 40)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using services::IHelper;
      using services::IMessageLayerSecurityChannel;

      using message::peer_to_peer::PeerIdentifyRequest;
      using message::peer_to_peer::PeerIdentifyRequestPtr;
      using message::peer_to_peer::PeerKeepAliveRequest;
      using message::peer_to_peer::PeerKeepAliveRequestPtr;
      using message::peer_finder::PeerLocationFindNotify;
      using message::peer_finder::PeerLocationFindNotifyPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPeerLocationPtr IAccountPeerLocationForAccount::create(
                                                                    IAccountPeerLocationDelegatePtr delegate,
                                                                    AccountPtr outer,
                                                                    const LocationInfo &locationInfo
                                                                    )
      {
        return IAccountPeerLocationFactory::singleton().create(delegate, outer, locationInfo);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPeerLocation::AccountPeerLocation(
                                               IMessageQueuePtr queue,
                                               IAccountPeerLocationDelegatePtr delegate,
                                               AccountPtr outer,
                                               const LocationInfo &locationInfo
                                               ) :
        MessageQueueAssociator(queue),
        mDelegate(IAccountPeerLocationDelegateProxy::createWeak(IStackForInternal::queueStack(), delegate)),
        mOuter(outer),
        mCurrentState(IAccount::AccountState_Pending),
        mShouldRefindNow(false),
        mLastActivity(zsLib::now()),
        mLocationInfo(locationInfo),
        mLocation(Location::convert(locationInfo.mLocation)),
        mPeer(mLocation->forAccount().getPeer()),
        mIncoming(false)
      {
        ZS_LOG_BASIC(log("created"))
        ZS_THROW_BAD_STATE_IF(!mPeer)
      }

      //---------------------------------------------------------------------
      void AccountPeerLocation::init()
      {
        ZS_LOG_DEBUG(debug("initialized"))
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
      AccountPeerLocationPtr AccountPeerLocation::create(
                                                         IAccountPeerLocationDelegatePtr delegate,
                                                         AccountPtr outer,
                                                         const LocationInfo &locationInfo
                                                         )
      {
        AccountPeerLocationPtr pThis(new AccountPeerLocation(IStackForInternal::queueStack(), delegate, outer, locationInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr AccountPeerLocation::getLocation() const
      {
        AutoRecursiveLock lock(getLock());
        return mLocation;
      }

      //-----------------------------------------------------------------------
      const LocationInfo &AccountPeerLocation::getLocationInfo() const
      {
        AutoRecursiveLock lock(getLock());
        return mLocationInfo;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::shutdown()
      {
        AutoRecursiveLock lock(getLock());
        cancel();
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates AccountPeerLocation::getState() const
      {
        AutoRecursiveLock lock(getLock());
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::shouldRefindNow() const
      {
        AutoRecursiveLock lock(getLock());
        bool refind = mShouldRefindNow;
        mShouldRefindNow = false;
        return refind;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::isConnected() const
      {
        AutoRecursiveLock lock(getLock());
        return isReady();
      }

      //-----------------------------------------------------------------------
      Time AccountPeerLocation::getTimeOfLastActivity() const
      {
        AutoRecursiveLock lock(getLock());
        return mLastActivity;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::connectLocation(
                                                const char *remoteContextID,
                                                const char *remotePeerSecret,
                                                const char *remoteICEUsernameFrag,
                                                const char *remoteICEPassword,
                                                const CandidateList &candidates,
                                                bool candidatesFinal,
                                                IICESocket::ICEControls control
                                                )
      {
        AutoRecursiveLock lock(getLock());
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

        mRemoteContextID = String(remoteContextID);
        mRemotePeerSecret = String(remotePeerSecret);
        get(mCandidatesFinal) = candidatesFinal;

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
          mSocketSession = socket->createSessionFromRemoteCandidates(IICESocketSessionDelegatePtr(), remoteICEUsernameFrag, remoteICEPassword, iceCandidates, control);

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

        if (!mRUDPSocketSession) {
          if (mSocketSession) {
            mRUDPSocketSession = IRUDPICESocketSession::listen(IStackForInternal::queueServices(), mSocketSession, mThisWeak.lock());
          }
        }

        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::incomingRespondWhenCandidatesReady(PeerLocationFindRequestPtr request)
      {
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        mIncoming = true;

        mPendingRequests.push_back(request);
        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::hasReceivedCandidateInformation() const
      {
        AutoRecursiveLock lock(getLock());
        return (mSocketSession) && (mCandidatesFinal);
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::send(MessagePtr message) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("attempted to send a message but the location is shutdown"))
          return false;
        }

        if (!isReady()) {
          PeerIdentifyRequestPtr identifyRequest = PeerIdentifyRequest::convert(message);
          PeerIdentifyResultPtr identifyResult = PeerIdentifyResult::convert(message);

          PeerLocationFindNotifyPtr peerLocationFindNotify = PeerLocationFindNotify::convert(message);

          if ((!identifyRequest) &&
              (!identifyResult) &&
              (!peerLocationFindNotify)) {
            ZS_LOG_WARNING(Detail, log("attempted to send a message as the location is not ready"))
            return false;
          }
        }

        DocumentPtr document = message->encode();

        boost::shared_array<char> output;
        size_t length = 0;
        output = document->writeAsJSON(&length);

        if (ZS_IS_LOGGING(Detail)) {
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + ZS_PARAM("message info", Message::toDebug(message)))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("PEER SEND MESSAGE") + ZS_PARAM("json out", ((CSTR)(output.get()))))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("> > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > > >"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        mLastActivity = zsLib::now();

        if (mMLSChannel) {
          if (IMessageLayerSecurityChannel::SessionState_Connected == mMLSChannel->getState()) {
            ZS_LOG_TRACE(log("message sent via RUDP/MLS"))

            mMLSSendStream->write((const BYTE *)(output.get()), length);
            return true;
          }
        }

        if (mOutgoingRelayChannel) {
          if (IFinderRelayChannel::SessionState_Connected == mOutgoingRelayChannel->getState()) {
            ZS_LOG_TRACE(log("message send via outgoing relay"))
            mRelaySendStream->write((const BYTE *)(output.get()), length);
            return true;
          }
        }

        AccountPtr outer = mOuter.lock();
        if ((outer) &&
            (mPeer)) {
          bool result = outer->forAccountPeerLocation().sendViaRelay(mPeer->forAccount().getPeerURI(), mThisWeak.lock(), (const BYTE *)(output.get()), length);
          if (result) {
            ZS_LOG_TRACE(log("message send via incoming relay"))
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
          MessageMonitor::convert(monitor)->forAccountPeerLocation().notifyMessageSendFailed();
          return monitor;
        }

        ZS_LOG_DEBUG(log("request successfully created"))
        return monitor;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::sendKeepAlive()
      {
        AutoRecursiveLock lock(getLock());

        if (mKeepAliveMonitor) {
          ZS_LOG_WARNING(Detail, log("keep alive requester is already in progress"))
          return;
        }

        AccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_ERROR(Debug, log("stack account appears to be gone thus cannot keep alive"))
          return;
        }

        PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::create();
        request->domain(outer->forAccountPeerLocation().getDomain());

        mKeepAliveMonitor = sendRequest(IMessageMonitorResultDelegate<PeerKeepAliveResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_PEER_KEEP_ALIVE_REQUEST_TIMEOUT_IN_SECONDS));
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::handleMessage(
                                              MessagePtr message,
                                              bool viaRelay
                                              )
      {
        bool hasPeerFile = (bool)mPeer->forAccount().getPeerFilePublic();

        // this is something new/incoming from the remote server...
        AccountPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("peer message is ignored since account is now gone"))
          return;
        }

        // can't process requests until the incoming is received, drop the message
        if (((!hasPeerFile) ||
             (mIncoming)) &&
            (Time() == mIdentifyTime)) {

          // scope: handle identify request
          {
            PeerIdentifyRequestPtr request = PeerIdentifyRequest::convert(message);
            if (!request) {
              ZS_LOG_WARNING(Detail, log("unable to forward message to account since message arrived before peer was identified"))
              goto identify_failure;
            }

            ZS_LOG_DEBUG(log("handling peer identify message"))

            IPeerFilesPtr peerFiles = outer->forAccountPeerLocation().getPeerFiles();
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

            LocationPtr location = Location::convert(mLocationInfo.mLocation);

            if (location->forAccount().getPeerURI() != mPeer->forAccount().getPeerURI()) {
              ZS_LOG_ERROR(Detail, log("peer file does not match expecting peer URI") + ILocation::toDebug(location) + IPeer::toDebug(mPeer))
              goto identify_failure;
            }

            mIdentifyTime = zsLib::now();

            PeerIdentifyResultPtr result = PeerIdentifyResult::create(request);

            LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);

            LocationInfoPtr info = selfLocation->forAccount().getLocationInfo();
            info->mCandidates.clear();

            result->locationInfo(*info);
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

        // handle incoming keep alives
        {
          PeerKeepAliveRequestPtr request = PeerKeepAliveRequest::convert(message);
          if (request) {
            ZS_LOG_DEBUG(log("handling incoming peer keep alive request"))

            PeerKeepAliveResultPtr result = PeerKeepAliveResult::create(request);
            send(result);
            return;
          }
        }

        ZS_LOG_DEBUG(log("unknown message, will forward message to account"))

        // send the message to the outer (asynchronously)
        try {
          mDelegate->onAccountPeerLocationMessageIncoming(mThisWeak.lock(), message);
        } catch (IAccountPeerLocationDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::notifyIncomingRelayChannel(IFinderRelayChannelPtr channel)
      {
        ZS_LOG_DEBUG(log("notify incoming relay channel") + IFinderRelayChannel::toDebug(channel))

        AutoRecursiveLock lock(getLock());
        if (mIncomingRelayChannel == channel) {
          ZS_LOG_DEBUG(log("already notified about this incoming relay channel (thus ignoring)"))
          return;
        }
        mIncomingRelayChannel = channel;
        mRelayChannelSubscription = mIncomingRelayChannel->subscribe(mThisWeak.lock());
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

        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onFinderRelayChannelNeedsContext(IFinderRelayChannelPtr channel)
      {
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onICESocketCandidatesChanged(IICESocketPtr socket)
      {
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IRUDPICESocketSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onRUDPICESocketSessionStateChanged(
                                                                   IRUDPICESocketSessionPtr session,
                                                                   RUDPICESocketSessionStates state
                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!session)

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("received socket session state after already shutdown") + ZS_PARAM("session ID", session->getID()))
          return;
        }

        if (session != mRUDPSocketSession) {
          ZS_LOG_WARNING(Detail, log("received socket session state changed from an obsolete session") + ZS_PARAM("session ID", session->getID()))
          return;
        }

        if ((IRUDPICESocketSession::RUDPICESocketSessionState_ShuttingDown == state) ||
            (IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown == state))
        {
          WORD errorCode = 0;
          String reason;
          session->getState(&errorCode, &reason);

          ZS_LOG_WARNING(Detail, log("notified RUDP ICE socket session is shutdown") + ZS_PARAM("error", IICESocketSession::toString(static_cast<IICESocketSession::ICESocketSessionShutdownReasons>(errorCode))) + ZS_PARAM("reason", reason))
          if ((IICESocketSession::ICESocketSessionShutdownReason_Timeout == errorCode) ||
              (IICESocketSession::ICESocketSessionShutdownReason_BackgroundingTimeout == errorCode)) {
            mShouldRefindNow = true;
          }
          cancel();
          return;
        }

        step();
      }

      //---------------------------------------------------------------------
      void AccountPeerLocation::onRUDPICESocketSessionChannelWaiting(IRUDPICESocketSessionPtr session)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!session)

        ZS_LOG_DEBUG(log("received RUDP channel waiting") + ZS_PARAM("sessionID", session->getID()))

        AutoRecursiveLock lock(getLock());

        if (isShutdown()) return;
        if (isShuttingDown()) {
          ZS_LOG_WARNING(Debug, log("incoming RUDP session ignored during shutdown process"))
          return; // ignore incoming channels during the shutdown
        }

        if (session != mRUDPSocketSession) return;

        if (mMessaging) {
          if (IRUDPMessaging::RUDPMessagingState_Connected != mMessaging->getState())
          {
            ZS_LOG_WARNING(Debug, log("incoming RUDP session replacing existing RUDP session since the outgoing RUDP session was not connected"))
            // out messaging was not connected so use this one instead since it is connected

            ITransportStreamPtr receiveStream = ITransportStream::create();
            ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

            IRUDPMessagingPtr messaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mRUDPSocketSession, mThisWeak.lock(), receiveStream, sendStream);
            if (!messaging) return;

            if (OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO != messaging->getRemoteConnectionInfo()) {
              ZS_LOG_WARNING(Detail, log("received unknown incoming connection type thus shutting down incoming connection") + ZS_PARAM("type", messaging->getRemoteConnectionInfo()))
              messaging->shutdown();
              return;
            }

            if (mIdentifyMonitor) {
              mIdentifyMonitor->cancel();
              mIdentifyMonitor.reset();
            }

            mMessagingReceiveStream->cancel();
            mMessagingReceiveStream.reset();

            mMessagingSendStream->cancel();
            mMessagingSendStream.reset();

            mMessaging->shutdown();
            mMessaging.reset();

            mMessaging = messaging;
            mMessagingReceiveStream = receiveStream->getReader();
            mMessagingSendStream = sendStream->getWriter();

            mMessagingReceiveStream->notifyReaderReadyToRead();

            mIncoming = true;

            step();
            return;
          }

          ZS_LOG_WARNING(Detail, log("incoming RUDP session ignored since an outgoing RUDP session is already established"))

          ITransportStreamPtr receiveStream = ITransportStream::create();
          ITransportStreamPtr sendStream = ITransportStream::create();

          // we already have a connected channel, so dump this one...
          IRUDPMessagingPtr messaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mRUDPSocketSession, mThisWeak.lock(), receiveStream, sendStream);
          messaging->shutdown();

          receiveStream->cancel();
          sendStream->cancel();

          ZS_LOG_WARNING(Detail, log("sending keep alive as it is likely the remote party connection request is valid and our current session is stale"))
          sendKeepAlive();
          return;
        }

        ZS_LOG_DEBUG(log("incoming RUDP session being answered"))

        ITransportStreamPtr receiveStream = ITransportStream::create();
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        // no messaging present, accept this incoming channel
        mMessaging = IRUDPMessaging::acceptChannel(IStackForInternal::queueServices(), mRUDPSocketSession, mThisWeak.lock(), receiveStream, sendStream);
        mIncoming = true;

        mMessagingReceiveStream = receiveStream->getReader();
        mMessagingSendStream = sendStream->getWriter();

        mMessagingReceiveStream->notifyReaderReadyToRead();

        if (OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO != mMessaging->getRemoteConnectionInfo()) {
          ZS_LOG_WARNING(Detail, log("received unknown incoming connection type thus shutting down incoming connection") + ZS_PARAM("type", mMessaging->getRemoteConnectionInfo()))

          mMessaging->shutdown();
          mMessaging.reset();
          return;
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
        AutoRecursiveLock lock(getLock());
        if (isShutdown()) return;

        if (messaging != mMessaging) {
          ZS_LOG_WARNING(Detail, log("received messaging state changed from an obsolete RUDP messaging") + ZS_PARAM("messaging ID", messaging->getID()))
          return;
        }

        if ((IRUDPMessaging::RUDPMessagingState_ShuttingDown == state) ||
            (IRUDPMessaging::RUDPMessagingState_Shutdown == state))
        {
          WORD errorCode = 0;
          String reason;
          messaging->getState(&errorCode, &reason);
          ZS_LOG_WARNING(Detail, log("notified messaging shutdown") + ZS_PARAM("error", IRUDPMessaging::toString(static_cast<IRUDPMessaging::RUDPMessagingShutdownReasons>(errorCode))) + ZS_PARAM("reason", reason))
          if (IRUDPMessaging::RUDPMessagingShutdownReason_Timeout == errorCode) {
            mShouldRefindNow = true;
          }
          cancel();
          return;
        }

        step();
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
        ZS_LOG_DEBUG(log("on message layer security channel state changed"))
        AutoRecursiveLock lock(getLock());
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
        ZS_LOG_TRACE(log("on stream write ready (ignored)"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void AccountPeerLocation::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        AutoRecursiveLock lock(getLock());

        if (isShutdown()) return;

        if ((reader != mMLSReceiveStream) &&
            (reader != mRelayReceiveStream)) {
          ZS_LOG_WARNING(Debug, log("messaging reader ready arrived for obsolete stream") + ZS_PARAM("stream reader id", reader->getID()))
          return;
        }

        while (true) {
          SecureByteBlockPtr buffer = reader->read();
          if (!buffer) {
            ZS_LOG_TRACE(log("no data to read"))
            return;
          }

          mLastActivity = zsLib::now();

          DocumentPtr document = Document::createFromAutoDetect((CSTR)(buffer->BytePtr()));
          MessagePtr message = Message::create(document, mLocation);
          
          if (ZS_IS_LOGGING(Detail)) {
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("< < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < < <"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("MESSAGE INFO") + ZS_PARAM("message info", Message::toDebug(message)))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("PEER RECEIVED MESSAGE") + ZS_PARAM("via", reader == mRelayReceiveStream ? "RELAY" : "RUDP/MLS") + ZS_PARAM("json in", ((CSTR)(buffer->BytePtr()))))
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

          handleMessage(message, reader == mRelayReceiveStream);
        }
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
        AutoRecursiveLock lock(getLock());
        if (monitor != mIdentifyMonitor) {
          ZS_LOG_WARNING(Detail, log("received a result on an obsolete monitor"))
          return false;
        }

        ZS_LOG_DEBUG(log("received result to peer identity request"))

        mIdentifyMonitor->cancel();
        mIdentifyMonitor.reset();

        LocationPtr location = Location::convert(result->locationInfo().mLocation);
        if (!location) {
          ZS_LOG_ERROR(Detail, log("location not properly identified"))
          cancel();
          return true;
        }

        if (location->forAccount().getPeerURI() != mPeer->forAccount().getPeerURI()) {
          ZS_LOG_ERROR(Detail, log("peer URL does not match expecting peer URI") + ILocation::toDebug(location) + IPeer::toDebug(mPeer))
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
        AutoRecursiveLock lock(getLock());
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
        AutoRecursiveLock lock(getLock());
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
      RecursiveLock &AccountPeerLocation::getLock() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->forAccountPeerLocation().getLock();
      }

      //-----------------------------------------------------------------------
      IICESocketPtr AccountPeerLocation::getSocket() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return IICESocketPtr();
        return outer->forAccountPeerLocation().getSocket();
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
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("AccountPeerLocation");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "graceful reference", (bool)mGracefulShutdownReference);

        IHelper::debugAppend(resultEl, "state", IAccount::toString(mCurrentState));
        IHelper::debugAppend(resultEl, "refind", mShouldRefindNow);

        IHelper::debugAppend(resultEl, "last activity", mLastActivity);

        IHelper::debugAppend(resultEl, "pending requests", mPendingRequests.size());

        IHelper::debugAppend(resultEl, "remote context ID", mRemoteContextID);
        IHelper::debugAppend(resultEl, "remote peer secret", mRemotePeerSecret);

        IHelper::debugAppend(resultEl, "location info", mLocationInfo.toDebug());
        if (mLocation != mLocationInfo.mLocation) {
          IHelper::debugAppend(resultEl, "location", ILocation::toDebug(mLocation));
        }
        IHelper::debugAppend(resultEl, "peer", IPeer::toDebug(mPeer));

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
        IHelper::debugAppend(resultEl, "mls encoding passphrase", mMLSEncodingPassphrase);
        IHelper::debugAppend(resultEl, "mls did connect", mMLSDidConnect);

        IHelper::debugAppend(resultEl, "outgoing relay channel id", mOutgoingRelayChannel ? mOutgoingRelayChannel->getID() : 0);
        IHelper::debugAppend(resultEl, "outgoing relay receive stream id", mRelayReceiveStream ? mRelayReceiveStream->getID() : 0);
        IHelper::debugAppend(resultEl, "outgoing relay send stream id", mRelaySendStream ? mRelaySendStream->getID() : 0);

        IHelper::debugAppend(resultEl, "incoming relay channel id", mIncomingRelayChannel ? mIncomingRelayChannel->getID() : 0);
        IHelper::debugAppend(resultEl, "incoming relay channel subscription", (bool)mRelayChannelSubscription);

        IHelper::debugAppend(resultEl, "incoming", mIncoming);
        IHelper::debugAppend(resultEl, "identify time", mIdentifyTime);

        IHelper::debugAppend(resultEl, "identity monitor", (bool)mIdentifyMonitor);
        IHelper::debugAppend(resultEl, "keep alive monitor", (bool)mKeepAliveMonitor);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::cancel()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("cancel called"))

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("already shutdown"))
          return;
        }

        setState(IAccount::AccountState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mIdentifyMonitor) {
          mIdentifyMonitor->cancel();
          mIdentifyMonitor.reset();
        }

        if (mKeepAliveMonitor) {
          mKeepAliveMonitor->cancel();
          mKeepAliveMonitor.reset();
        }

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

            if (IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown != mRUDPSocketSession->getState()) {
              ZS_LOG_DEBUG(log("waiting for RUDP ICE socket session to shutdown"))
              return;
            }

            mRUDPSocketSession.reset();
          }
        }

        setState(IAccount::AccountState_Shutdown);

        mGracefulShutdownReference.reset();
        mDelegate.reset();

        mPendingRequests.clear();

        if (mOutgoingRelayChannel) {
          mOutgoingRelayChannel->cancel();
          mOutgoingRelayChannel.reset();
        }

        if (mRelayReceiveStream) {
          mRelayReceiveStream->cancel();
          mRelayReceiveStream.reset();
        }
        if (mRelaySendStream) {
          mRelaySendStream->cancel();
          mRelaySendStream.reset();
        }

        if (mIncomingRelayChannel) {
          mIncomingRelayChannel->cancel();
          mIncomingRelayChannel.reset();
        }
        if (mRelayChannelSubscription) {
          mRelayChannelSubscription->cancel();
          mRelayChannelSubscription.reset();
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

        AccountPtr outer = mOuter.lock();
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

        if (!stepAnyConnectionPresent()) return;
        if (!stepPendingRequests(socket)) return;
        if (!stepIncomingIdentify()) return;
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
        if (mPendingRequests.size() < 1) {
          ZS_LOG_TRACE(log("no pending requests thus no need for an outgoing relay connection"))
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

        PeerLocationFindRequestPtr pendingRequest = mPendingRequests.back();

        CandidateList remoteCandidates;
        remoteCandidates = pendingRequest->locationInfo().mCandidates;

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
          ZS_LOG_ERROR(Detail, log("no relay candidates found"))
          cancel();
          return false;
        }

        Candidate relayCandidate = relayCandidates.front();

        AccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        ITransportStreamPtr receiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        String localContext = outer->forAccountPeerLocation().getLocalContextID(mPeer->forAccount().getPeerURI());
        String remoteContext = pendingRequest->context();

        String relayAccessToken = relayCandidate.mAccessToken;
        String relayAccessSecretProof = relayCandidate.mAccessSecretProof;
        String domain = outer->forAccountPeerLocation().getDomain();

        String passphrase = pendingRequest->peerSecret();
        mMLSEncodingPassphrase = passphrase;

        mOutgoingRelayChannel = IFinderRelayChannel::connect(mThisWeak.lock(), outer, receiveStream, sendStream, relayCandidate.mIPAddress, localContext, remoteContext, domain, relayAccessToken, relayAccessSecretProof, passphrase);

        if (!mOutgoingRelayChannel) {
          ZS_LOG_ERROR(Detail, log("failed to create outgoing relay channel"))
          cancel();
          return false;
        }

        mRelayReceiveStream = receiveStream->getReader();
        mRelaySendStream = sendStream->getWriter();

        mRelayReceiveStream->notifyReaderReadyToRead();

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

        if (IRUDPICESocketSession::RUDPICESocketSessionState_Ready != mRUDPSocketSession->getState()) {
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

        if (mIncoming) {
          ZS_LOG_TRACE(log("no need to create messaging since incoming"))
          return true;
        }

        ZS_LOG_TRACE(log("requesting messaging channel open"))
        ITransportStreamPtr receiveStream = ITransportStream::create();
        ITransportStreamPtr sendStream = ITransportStream::create();

        mMessaging = IRUDPMessaging::openChannel(IStackForInternal::queueServices(), mRUDPSocketSession, mThisWeak.lock(), OPENPEER_STACK_PEER_TO_PEER_RUDP_CONNECTION_INFO, receiveStream, sendStream);

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

        AccountPtr outer = mOuter.lock();
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
              IPeerFilesPtr peerFiles = outer->forAccountPeerLocation().getPeerFiles();
              IPeerFilePrivatePtr peerFilePrivate;
              IPeerFilePublicPtr peerFilePublic;
              if (peerFiles) {
                peerFilePrivate = peerFiles->getPeerFilePrivate();
                peerFilePublic = peerFiles->getPeerFilePublic();
              }
              IPeerFilePublicPtr remotePeerFilePublic;
              if (mPeer) {
                ZS_LOG_TRACE(log("remote public peer file is known"))
                remotePeerFilePublic = mPeer->forAccount().getPeerFilePublic();
              }

              if ((mMLSChannel->needsReceiveKeyingDecodingPrivateKey()) &&
                  (peerFilePrivate) &&
                  (peerFilePublic)) {
                ZS_LOG_DEBUG(log("set receive keying decoding by public / private key"))
                mMLSChannel->setReceiveKeyingDecoding(peerFilePrivate->getPrivateKey(), peerFilePublic->getPublicKey());
              }

              if (mMLSChannel->needsReceiveKeyingDecodingPassphrase()) {
                 if (mMLSEncodingPassphrase.hasData()) {
                   ZS_LOG_DEBUG(log("set receive keying decoding by passphrase"))
                   mMLSChannel->setReceiveKeyingDecoding(mMLSEncodingPassphrase);
                 } else if (mPeer) {
                   String calculatedPassphrase = outer->forAccountPeerLocation().getLocalPassword(mPeer->forAccount().getPeerURI());
                   mMLSChannel->setReceiveKeyingDecoding(mMLSEncodingPassphrase);
                   ZS_LOG_DEBUG(log("set receive keying decoding by passphrase (base upon local password for peer URI)") + ZS_PARAM("peer URI", mPeer->forAccount().getPeerURI()) + ZS_PARAM("calculated passphrase", calculatedPassphrase))
                   mMLSChannel->setReceiveKeyingDecoding(calculatedPassphrase);
                 }
              }

              if ((mMLSChannel->needsReceiveKeyingMaterialSigningPublicKey()) &&
                  (remotePeerFilePublic)) {
                ZS_LOG_DEBUG(log("set receive keying signing public key"))
                mMLSChannel->setReceiveKeyingMaterialSigningPublicKey(remotePeerFilePublic->getPublicKey());
              }

              if (mMLSChannel->needsSendKeyingEncodingMaterial()) {
                if (remotePeerFilePublic) {
                  ZS_LOG_DEBUG(log("set send keying encoding public key"))
                  mMLSChannel->setSendKeyingEncoding(remotePeerFilePublic->getPublicKey());
                } else if (mMLSEncodingPassphrase.hasData()) {
                  ZS_LOG_DEBUG(log("set send keying encoding passphrase") + ZS_PARAM("passphrase", mMLSEncodingPassphrase))
                  mMLSChannel->setSendKeyingEncoding(mMLSEncodingPassphrase);
                } else if (mPeer) {
                  String calculatedPassphrase = outer->forAccountPeerLocation().getLocalPassword(mPeer->forAccount().getPeerURI());
                  ZS_LOG_DEBUG(log("set send keying encoding passphrase (base upon local password for peer URI)") + ZS_PARAM("peer URI", mPeer->forAccount().getPeerURI()) + ZS_PARAM("calculated passphrase", calculatedPassphrase))
                  mMLSChannel->setSendKeyingEncoding(calculatedPassphrase);
                }
              }

              if ((mMLSChannel->needsSendKeyingMaterialToeBeSigned()) &&
                  (peerFilePrivate)) {
                DocumentPtr doc;
                ElementPtr signEl;
                mMLSChannel->getSendKeyingMaterialNeedingToBeSigned(doc, signEl);

                if (signEl) {
                  ZS_LOG_DEBUG(log("notified send keying material signed"))
                  peerFilePrivate->signElement(signEl);
                  mMLSChannel->notifySendKeyingMaterialSigned();
                }
              }
              ZS_LOG_TRACE(log("MLS waiting for needed information"))
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

        String localContext = outer->forAccountPeerLocation().getLocalContextID(mPeer->forAccount().getPeerURI());

        ITransportStreamPtr receiveStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());
        ITransportStreamPtr sendStream = ITransportStream::create(mThisWeak.lock(), ITransportStreamReaderDelegatePtr());

        mMLSChannel = IMessageLayerSecurityChannel::create(mThisWeak.lock(), mMessagingReceiveStream->getStream(), receiveStream, sendStream, mMessagingSendStream->getStream(), localContext);

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
      bool AccountPeerLocation::stepAnyConnectionPresent()
      {
        bool found = false;
        bool ready = false;

        WORD error = 0;
        String reason;

        if (mIncomingRelayChannel) {
          switch (mIncomingRelayChannel->getState(&error, &reason)) {
            case IFinderRelayChannel::SessionState_Pending: {
              ZS_LOG_TRACE(log("incoming relay channel pending (channel may still become available)"))
              found = true;
              break;
            }
            case IFinderRelayChannel::SessionState_Connected: {
              ZS_LOG_TRACE(log("incoming relay channel connected (thus communication channel is available)"))
              found = ready = true;
              return true;
            }
            case IFinderRelayChannel::SessionState_Shutdown:  {
              ZS_LOG_WARNING(Trace, log("incoming relay channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              break;
            }
          }
        }

        if (mOutgoingRelayChannel) {
          switch (mOutgoingRelayChannel->getState(&error, &reason))
          {
            case IFinderRelayChannel::SessionState_Pending:   {
              ZS_LOG_TRACE(log("outgoing relay channel pending (channel may still become available)"))
              found = true;
              return false;
            }
            case IFinderRelayChannel::SessionState_Connected: {
              ZS_LOG_TRACE(log("outgoing relay channel connected (thus communication channel is available)"))
              found = ready = true;
              return true;
            }
            case IFinderRelayChannel::SessionState_Shutdown: {
              ZS_LOG_WARNING(Trace, log("outgoing relay channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              return true;
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
              if (!found) {
                mSocketSession->endOfRemoteCandidates();  // no more candidates can possibly arrive
              }
              found = true;
              break;
            }
            case IICESocketSession::ICESocketSessionState_Nominated:
            case IICESocketSession::ICESocketSessionState_Completed:
            {
              ZS_THROW_BAD_STATE_IF(!mRUDPSocketSession)  // how is this possible since it's created with the ICE socket

              // scope: check to make sure RUDP socket is ready too
              {
                switch (mRUDPSocketSession->getState(&error, &reason)) {
                  case IRUDPICESocketSession::RUDPICESocketSessionState_Pending: {
                    ZS_LOG_TRACE(log("rudp ice socket session is pending"))
                    found = true;
                    goto rudp_not_ready;
                  }
                  case IRUDPICESocketSession::RUDPICESocketSessionState_Ready: {
                    ZS_LOG_TRACE(log("rudp ice socket session is ready"))
                    found = true;
                    goto rudp_ready;
                  }
                  case IRUDPICESocketSession::RUDPICESocketSessionState_ShuttingDown:
                  case IRUDPICESocketSession::RUDPICESocketSessionState_Shutdown: {
                    ZS_LOG_WARNING(Trace, log("rudp socket session is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
                    goto rudp_not_ready;
                  }
                }
              }

            rudp_not_ready:
              break;

            rudp_ready:

              if (!mMessaging) {
                ZS_LOG_TRACE(log("waiting for a messaging channel to be setup"))
                found = true;
                break;
              }

              // scope: check if messaging is ready
              {
                switch (mMessaging->getState(&error, &reason)) {
                  case IRUDPMessaging::RUDPMessagingState_Connecting:
                  {
                    ZS_LOG_TRACE(log("Messaging channel is still pending (thus communication channel is not available yet)"))
                    found = true;
                    goto messaging_not_ready;
                  }
                  case IRUDPMessaging::RUDPMessagingState_Connected:
                  {
                    ZS_LOG_TRACE(log("Messaging channel is connected (but must see if MLS channel is ready)"))
                    found = true;
                    goto messaging_ready;
                  }
                  case IRUDPMessaging::RUDPMessagingState_ShuttingDown:
                  case IRUDPMessaging::RUDPMessagingState_Shutdown: {
                    ZS_LOG_WARNING(Trace, log("messaging channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
                    goto messaging_not_ready;
                  }
                }
              }

            messaging_not_ready:
              break;

            messaging_ready:

              if (!mMLSChannel) {
                ZS_LOG_TRACE(log("waiting for an MLS channel to be setup"))
                found = true;
                break;
              }
              switch (mMLSChannel->getState(&error, &reason)) {
                case IMessageLayerSecurityChannel::SessionState_Pending:
                case IMessageLayerSecurityChannel::SessionState_WaitingForNeededInformation: {
                  ZS_LOG_TRACE(log("MLS channel is still pending (thus communication channel is not available yet)"))
                  found = true;
                  break;
                }
                case IMessageLayerSecurityChannel::SessionState_Connected:  {
                  found = ready = true;
                  ZS_LOG_TRACE(log("MLS channel is ready thus communication channel is available"))
                  break;
                }
                case IMessageLayerSecurityChannel::SessionState_Shutdown: {
                  ZS_LOG_WARNING(Trace, log("MLS channel is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
                  break;
                }
              }
              break;
            }
            case IICESocketSession::ICESocketSessionState_Shutdown:
              ZS_LOG_WARNING(Trace, log("rudp socket session is shutdown (thus unuseable)") + ZS_PARAM("error", error) + ZS_PARAM("reason", reason))
              break;
          }
        }

        if (!found) {
          ZS_LOG_ERROR(Detail, log("no possible communication channel open to peer thus must shutdown now"))
          cancel();
          return false;
        }

        if (!ready) {
          ZS_LOG_TRACE(log("waiting for any communication channel to be ready before messages are allowed to be sent/received"))
          return false;
        }

        ZS_LOG_TRACE(log("some communication channel is ready to send messages"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepPendingRequests(IICESocketPtr socket)
      {
        if (mPendingRequests.size() < 1) {
          ZS_LOG_TRACE(log("no pending requests"))
          return true;
        }

        while (mPendingRequests.size() > 0) {
          PeerLocationFindRequestPtr pendingRequest = mPendingRequests.front();
          mPendingRequests.pop_front();

          mLastRequest = pendingRequest;

          stepRespondLastRequest(socket);
        }

        if (!mLastRequest) {
          ZS_LOG_TRACE(log("no last request sent at this time"))
          return true;
        }

        String candidatesVersion = socket->getLocalCandidatesVersion();
        if ((candidatesVersion != mLastCandidateVersionSent) &&
            (!mCandidatesFinal)) {
          ZS_LOG_DEBUG(log("candidates have changed since last reported (thus notify another peer location find)") + ZS_PARAM("candidates version", candidatesVersion) + ZS_PARAM("last send version", mLastCandidateVersionSent))
          stepRespondLastRequest(socket);
        }

        ZS_LOG_TRACE(log("step pending requests complete"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepRespondLastRequest(IICESocketPtr socket)
      {
        AccountPtr outer = mOuter.lock();
        ZS_THROW_BAD_STATE_IF(!outer)

        LocationPtr finderLocation = ILocationForAccount::getForFinder(outer);

        if (!finderLocation->forAccount().isConnected()) {
          ZS_LOG_WARNING(Detail, log("cannot respond to pending find request if the finder is not ready (thus shutting down)"))
          cancel();
          return false;
        }

        LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);
        LocationInfoPtr selfLocationInfo = selfLocation->forAccount().getLocationInfo();
        
        CandidateList remoteCandidates;
        remoteCandidates = mLastRequest->locationInfo().mCandidates;

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

        // local candidates are now preparerd, the request can be answered
        PeerLocationFindNotifyPtr reply = PeerLocationFindNotify::create(mLastRequest);

        reply->context(outer->forAccountPeerLocation().getLocalContextID(mPeer->forAccount().getPeerURI()));
        reply->peerSecret(outer->forAccountPeerLocation().getLocalPassword(mPeer->forAccount().getPeerURI()));
        reply->iceUsernameFrag(socket->getUsernameFrag());
        reply->icePassword(socket->getPassword());
        reply->final(selfLocationInfo->mCandidatesFinal);
        reply->locationInfo(*selfLocationInfo);
        reply->peerFiles(outer->forAccountPeerLocation().getPeerFiles());

        mLastCandidateVersionSent = selfLocationInfo->mCandidatesVersion;

        if (mPendingRequests.size() < 1) {
          // this is the final location, use this location as the connection point as it likely to be the latest of all the requests (let's hope)
          connectLocation(mLastRequest->context(), mLastRequest->peerSecret(), mLastRequest->iceUsernameFrag(), mLastRequest->icePassword(), remoteCandidates, selfLocationInfo->mCandidatesFinal, IICESocket::ICEControl_Controlled);
        }

        send(reply);

        ZS_LOG_DEBUG(log("responded to pending request"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIncomingIdentify()
      {
        if (!mIncoming) {
          ZS_LOG_TRACE(log("session is outgoing (thus not expecting incoming identity)"))
          return true;
        }

        if ((Time() != mIdentifyTime) &&
            (mPeer->forAccount().getPeerFilePublic())) {
          ZS_LOG_TRACE(log("already received identify request"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting for an incoming connection and identification"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool AccountPeerLocation::stepIdentify()
      {
        if (mIncoming) {
          ZS_LOG_TRACE(log("no need to identify"))
          return true;
        }

        if (mIdentifyMonitor) {
          ZS_LOG_TRACE(log("waiting for identify to complete"))
          return false;
        }

        if (Time() != mIdentifyTime) {
          ZS_LOG_TRACE(log("identify already complete"))
          return true;
        }

        AccountPtr outer = mOuter.lock();

        ZS_LOG_DETAIL(log("Peer Location connected, sending identity..."))

        // we have connected, perform the identity request since we made the connection outgoing...
        PeerIdentifyRequestPtr request = PeerIdentifyRequest::create();
        request->domain(outer->forAccountPeerLocation().getDomain());

        IPeerFilePublicPtr remotePeerFilePublic = mPeer->forAccount().getPeerFilePublic();
        if (remotePeerFilePublic) {
          request->findSecret(remotePeerFilePublic->getFindSecret());
        }

        LocationPtr selfLocation = ILocationForAccount::getForLocal(outer);
        LocationInfoPtr selfLocationInfo = selfLocation->forAccount().getLocationInfo();
        selfLocationInfo->mCandidates.clear();

        request->location(*selfLocationInfo);
        request->peerFiles(outer->forAccountPeerLocation().getPeerFiles());

        mIdentifyMonitor = sendRequest(IMessageMonitorResultDelegate<PeerIdentifyResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_CONNECTION_MANAGER_PEER_IDENTIFY_EXPIRES_IN_SECONDS));
        return false;
      }

      //-----------------------------------------------------------------------
      void AccountPeerLocation::setState(IAccount::AccountStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("old state", IAccount::toString(mCurrentState)) + ZS_PARAM("new state", IAccount::toString(state)))

        mCurrentState = state;

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
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }
  }
}
