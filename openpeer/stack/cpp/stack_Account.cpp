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
#include <openpeer/stack/internal/stack_AccountPeerLocation.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_MessageMonitor.h>
#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_PeerSubscription.h>
#include <openpeer/stack/internal/stack_PublicationRepository.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>

#include <openpeer/stack/message/MessageResult.h>

#include <openpeer/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <openpeer/stack/message/peer-finder/PeerLocationFindResult.h>
//#include <openpeer/stack/message/peer-finder/PeerLocationFindNotify.h>
#include <openpeer/stack/message/peer-finder/ChannelMapNotify.h>

#include <openpeer/stack/message/bootstrapped-finder/FindersGetRequest.h>

#include <openpeer/stack/ICache.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPublicationRepository.h>

#include <openpeer/services/IBackgrounding.h>
#include <openpeer/services/IHelper.h>
#include <openpeer/services/IDHKeyDomain.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

#include <algorithm>

#define OPENPEER_STACK_PEER_LOCATION_FIND_TIMEOUT_IN_SECONDS (60*2)
#define OPENPEER_STACK_PEER_LOCATION_FIND_RETRY_IN_SECONDS (30)
#define OPENPEER_STACK_PEER_LOCATION_INACTIVITY_TIMEOUT_IN_SECONDS (10*60)
#define OPENPEER_STACK_PEER_LOCATION_KEEP_ALIVE_TIME_IN_SECONDS    (5*60)

#define OPENPEER_STACK_FINDERS_GET_TOTAL_SERVERS_TO_GET (2)
#define OPENPEER_STACK_FINDERS_GET_TIMEOUT_IN_SECONDS (60)

#define OPENPEER_STACK_ACCOUNT_TIMER_FIRES_IN_SECONDS (15)
#define OPENPEER_STACK_ACCOUNT_FINDER_STARTING_RETRY_AFTER_IN_SECONDS (1)
#define OPENPEER_STACK_ACCOUNT_FINDER_MAX_RETRY_AFTER_TIME_IN_SECONDS (60)

#define OPENPEER_STACK_ACCOUNT_RUDP_TRANSPORT_PROTOCOL_TYPE "rudp/udp"
#define OPENPEER_STACK_ACCOUNT_MULTIPLEXED_JSON_TCP_TRANSPORT_PROTOCOL_TYPE "multiplexed-json/tcp"

#define OPENPEER_STACK_ACCOUNT_DEFAULT_PRECOMPILED_DH_DOMAIN_KEY (IDHKeyDomain::KeyDomainPrecompiledType_2048)

#define OPENPEER_STACK_ACCOUNT_COOKIE_DH_KEY_DOMAIN_CACHE_LIFETIME_HOURS (365*(24))

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;
      using services::IBackgrounding;

      using services::IWakeDelegateProxy;

      using message::peer_finder::PeerLocationFindRequest;
      using message::peer_finder::PeerLocationFindRequestPtr;
      using message::peer_finder::PeerLocationFindResult;
      using message::peer_finder::PeerLocationFindResultPtr;
      using message::peer_finder::ChannelMapNotify;
      using message::peer_finder::ChannelMapNotifyPtr;

      using message::bootstrapped_finder::FindersGetRequest;
      using message::bootstrapped_finder::FindersGetRequestPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      ILocation::LocationConnectionStates toLocationConnectionState(IAccount::AccountStates state)
      {
        switch (state) {
          case IAccount::AccountState_Pending:      return ILocation::LocationConnectionState_Pending;
          case IAccount::AccountState_Ready:        return ILocation::LocationConnectionState_Connected;
          case IAccount::AccountState_ShuttingDown: return ILocation::LocationConnectionState_Disconnecting;
          case IAccount::AccountState_Shutdown:     return ILocation::LocationConnectionState_Disconnected;
        }
        return ILocation::LocationConnectionState_Disconnected;
      }

      //-----------------------------------------------------------------------
      static String getMultiplexedJsonTCPTransport(const Finder &finder)
      {
        for (Finder::ProtocolList::const_iterator iter = finder.mProtocols.begin(); iter != finder.mProtocols.end(); ++iter)
        {
          const Finder::Protocol &protocol = (*iter);

          if (OPENPEER_STACK_ACCOUNT_MULTIPLEXED_JSON_TCP_TRANSPORT_PROTOCOL_TYPE == protocol.mTransport) {
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
      #pragma mark Account
      #pragma mark

      //-----------------------------------------------------------------------
      Account::Account(
                       IMessageQueuePtr queue,
                       IAccountDelegatePtr delegate,
                       ServiceLockboxSessionPtr lockboxSession
                       ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mLocationID(IHelper::randomString(32)),
        mCurrentState(IAccount::AccountState_Pending),
        mLastError(0),
        mDelegate(IAccountDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mLockboxSession(lockboxSession),
        mFinderRetryAfter(zsLib::now()),
        mLastRetryFinderAfterDuration(Seconds(OPENPEER_STACK_ACCOUNT_FINDER_STARTING_RETRY_AFTER_IN_SECONDS))
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      void Account::init()
      {
        ZS_LOG_DEBUG(log("inited"))

        String instanceID = services::ISettings::getString(OPENPEER_COMMON_SETTING_INSTANCE_ID);
        String authorizedAppId = services::ISettings::getString(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID);

        ZS_LOG_FORCED(Informational, Basic, log("instance information") + ZS_PARAM("instance id", instanceID) + ZS_PARAM("authorized application id", authorizedAppId) + UseStack::agentInfo().toDebug())

        AutoRecursiveLock lock(*this);

        mBackgroundingSubscription = IBackgrounding::subscribe(mThisWeak.lock(), services::ISettings::getUInt(OPENPEER_STACK_SETTING_BACKGROUNDING_ACCOUNT_PHASE));

        mLockboxSession->attach(mThisWeak.lock());

        step();
      }

      //-----------------------------------------------------------------------
      Account::~Account()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(IAccountPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForAccountFinderPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }
      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForRelayChannelPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForAccountPeerLocationPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForLocationPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForMessagesPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForPeerPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(ForPublicationRepositoryPtr account)
      {
        return dynamic_pointer_cast<Account>(account);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Account::toDebug(IAccountPtr account)
      {
        if (!account) return ElementPtr();
        AccountPtr pThis = Account::convert(account);
        return pThis->toDebug();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::create(
                                 IAccountDelegatePtr delegate,
                                 IServiceLockboxSessionPtr peerContactSession
                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate);
        ZS_THROW_INVALID_ARGUMENT_IF(!peerContactSession);

        AccountPtr pThis(new Account(UseStack::queueStack(), delegate, ServiceLockboxSession::convert(peerContactSession)));
        pThis->mThisWeak = pThis;
        pThis->mDelegate = IAccountDelegateProxy::createWeak(UseStack::queueDelegate(), delegate);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates Account::getState(
                                                WORD *outLastErrorCode,
                                                String *outLastErrorReason
                                                ) const
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(debug("get account state"))
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      IServiceLockboxSessionPtr Account::getLockboxSession() const
      {
        AutoRecursiveLock lock(*this);
        return ServiceLockboxSession::convert(mLockboxSession);
      }

      //-----------------------------------------------------------------------
      void Account::getNATServers(
                                  IICESocket::TURNServerInfoList &outTURNServers,
                                  IICESocket::STUNServerInfoList &outSTUNServers
                                  ) const
      {
        AutoRecursiveLock lock(*this);

        outTURNServers.clear();
        outSTUNServers.clear();

        if (mTURN) {
          for (Service::MethodList::iterator iter = mTURN->begin(); iter != mTURN->end(); ++iter)
          {
            Service::MethodPtr &method = (*iter);
            if (!method) continue;

            IICESocket::TURNServerInfoPtr turnInfo = IICESocket::TURNServerInfo::create();
            turnInfo->mTURNServer = method->mURI;
            turnInfo->mTURNServerUsername = method->mUsername;
            turnInfo->mTURNServerPassword = method->mPassword;

            if (turnInfo->hasData()) {
              outTURNServers.push_back(turnInfo);
            }
          }
        }

        if (mSTUN) {
          for (Service::MethodList::iterator iter = mSTUN->begin(); iter != mSTUN->end(); ++iter)
          {
            Service::MethodPtr &method = (*iter);
            if (!method) continue;

            IICESocket::STUNServerInfoPtr stunInfo = IICESocket::STUNServerInfo::create();
            stunInfo->mSTUNServer = method->mURI;

            if (stunInfo->hasData()) {
              outSTUNServers.push_back(stunInfo);
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void Account::shutdown()
      {
        ZS_LOG_DEBUG(log("requested to shutdown"))
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForAccountFinder
      #pragma mark

      //-----------------------------------------------------------------------
      String Account::getDomain() const
      {
        AutoRecursiveLock lock(*this);

        ZS_THROW_BAD_STATE_IF(!mLockboxSession)

        UseBootstrappedNetworkPtr network = mLockboxSession->getBootstrappedNetwork();

        ZS_THROW_BAD_STATE_IF(!network)

        return network->getDomain();
      }

      //-----------------------------------------------------------------------
      bool Account::extractNextFinder(
                                      Finder &outFinder,
                                      IPAddress &outFinderIP
                                      )
      {
        const char *reason = NULL;

        if ((mAvailableFinders.size() < 1) ||
            (!mAvailableFinderSRVResult)) {
          reason = "no finders available";
          goto extract_failure;
        }

        if (!IDNS::extractNextIP(mAvailableFinderSRVResult, outFinderIP)) {
          reason = "unable to extract next IP (no more IPs available)";
          goto extract_failure;
        }
        if (outFinderIP.isAddressEmpty()) {
          reason = "extracted IP address is empty";
          goto extract_failure;
        }

        outFinder = mAvailableFinders.front();
        return true;

      extract_failure:
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        mAvailableFinderSRVResult.reset();
        ZS_LOG_WARNING(Detail, log("extract next IP failed") + ZS_PARAM("reason", reason))
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForAccountPeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      IICESocketPtr Account::getSocket() const
      {
        AutoRecursiveLock lock(*this);
        return mSocket;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForLocation
      #pragma mark

      //-----------------------------------------------------------------------
      LocationPtr Account::findExisting(
                                        const String &peerURI,
                                        const String &locationID
                                        ) const
      {
        return Location::convert(mLocationsDB.findExisting(mThisWeak.lock(), peerURI, locationID));
      }

      //-----------------------------------------------------------------------
      LocationPtr Account::findExistingOrUse(LocationPtr inLocation)
      {
        return Location::convert(mLocationsDB.findExistingOrUse(mThisWeak.lock(), inLocation));
      }

      //-----------------------------------------------------------------------
      LocationPtr Account::getLocationForLocal() const
      {
        return Location::convert(mLocationsDB.getLocal());
      }

      //-----------------------------------------------------------------------
      LocationPtr Account::getLocationForFinder() const
      {
        return Location::convert(mLocationsDB.getFinder());
      }

      //-----------------------------------------------------------------------
      void Account::notifyDestroyed(Location &inLocation)
      {
        UseLocation &location = inLocation;

        String locationID = location.getLocationID();
        String peerURI = location.getPeerURI();

        if (mLocationsDB.remove(locationID, peerURI)) {
          ZS_LOG_DEBUG(log("notified location is destroyed") + location.toDebug())
        } else {
          ZS_LOG_WARNING(Detail, log("existing location in map was already gone (probably okay)") + location.toDebug())
        }
      }

      //-----------------------------------------------------------------------
      const String &Account::getLocationID() const
      {
        return mLocationID;
      }

      //-----------------------------------------------------------------------
      PeerPtr Account::getPeerForLocal() const
      {
        UsePeerPtr peer = mPeersDB.getLocal();
        if (!peer) {
          ZS_LOG_WARNING(Detail, log("obtained peer for local before peer was ready"))
        }
        return Peer::convert(peer);
      }

      //-----------------------------------------------------------------------
      LocationInfoPtr Account::getLocationInfo(LocationPtr inLocation) const
      {
        UseLocationPtr location = inLocation;

        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        AutoRecursiveLock lock(*this);

        LocationInfoPtr info = LocationInfo::create();
        info->mLocation = Location::convert(location);

        if (location == getLocationForLocal()) {
          if (mSocket) {
            get(info->mCandidatesFinal) = IICESocket::ICESocketState_Ready == mSocket->getState();

            IICESocket::CandidateList candidates;
            mSocket->getLocalCandidates(candidates, &(info->mCandidatesVersion));

            for (IICESocket::CandidateList::iterator iter = candidates.begin(); iter != candidates.end(); ++iter) {
              IICESocket::Candidate &candidate = (*iter);
              if (IICESocket::Type_Local == candidate.mType) {
                info->mIPAddress = (*iter).mIPAddress;
              }
              Candidate convert(candidate);
              convert.mNamespace = OPENPEER_STACK_CANDIDATE_NAMESPACE_ICE_CANDIDATES;
              convert.mTransport = OPENPEER_STACK_TRANSPORT_JSON_MLS_RUDP;
              info->mCandidates.push_back(convert);
            }

          }

          char buffer[256];
          memset(&(buffer[0]), 0, sizeof(buffer));

          gethostname(&(buffer[0]), (sizeof(buffer)*sizeof(char))-sizeof(char));

          info->mDeviceID = services::ISettings::getString(OPENPEER_COMMON_SETTING_DEVICE_ID);
          info->mUserAgent = services::ISettings::getString(OPENPEER_COMMON_SETTING_USER_AGENT);
          info->mOS = services::ISettings::getString(OPENPEER_COMMON_SETTING_OS);
          info->mSystem = services::ISettings::getString(OPENPEER_COMMON_SETTING_SYSTEM);
          info->mHost = &(buffer[0]);

          ZS_LOG_TRACE(log("self location info") + info->toDebug())
          return info;
        }

        if (location == getLocationForFinder()) {
          if (!mFinder) {
            ZS_LOG_WARNING(Detail, log("obtaining finder info before finder created"))
            return info;
          }

          Finder finder = mFinder->getCurrentFinder(&(info->mUserAgent), &(info->mIPAddress));

          info->mDeviceID = finder.mID;
          info->mHost = getMultiplexedJsonTCPTransport(finder);
          return info;
        }

        PeerInfoMap::const_iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_WARNING(Detail, log("could not find location information for non-connected peer") + UseLocation::toDebug(location))
          return info;
        }

        PeerInfoPtr peerInfo = (*found).second;

        PeerInfo::PeerLocationMap::const_iterator foundLocation = peerInfo->mLocations.find(location->getLocationID());
        if (foundLocation == peerInfo->mLocations.end()) {
          ZS_LOG_WARNING(Detail, log("could not find peer location information for non-connected peer location") + UseLocation::toDebug(location))
          return info;
        }

        UseAccountPeerLocationPtr accountPeerLocation = (*foundLocation).second;

        LocationInfoPtr sourceInfo = accountPeerLocation->getLocationInfo();

        info->mDeviceID = sourceInfo->mDeviceID;
        info->mIPAddress = sourceInfo->mIPAddress;
        info->mUserAgent = sourceInfo->mUserAgent;
        info->mOS = sourceInfo->mOS;
        info->mSystem = sourceInfo->mSystem;
        info->mHost = sourceInfo->mHost;
        info->mCandidates = sourceInfo->mCandidates;

        return info;
      }

      //-----------------------------------------------------------------------
      ILocation::LocationConnectionStates Account::getConnectionState(LocationPtr inLocation) const
      {
        UseLocationPtr location = inLocation;

        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        AutoRecursiveLock lock(*this);

        if (location == getLocationForLocal()) {
          return toLocationConnectionState(getState());
        }

        if (location == getLocationForFinder()) {
          if (!mFinder) {
            if ((isShuttingDown()) ||
                (isShutdown())) return ILocation::LocationConnectionState_Disconnected;

            return ILocation::LocationConnectionState_Pending;
          }

          return toLocationConnectionState(mFinder->getState());
        }

        PeerInfoMap::const_iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_DEBUG(log("peer is not connected") + UseLocation::toDebug(location))
          return ILocation::LocationConnectionState_Disconnected;
        }

        PeerInfoPtr peerInfo = (*found).second;

        PeerInfo::PeerLocationMap::const_iterator foundLocation = peerInfo->mLocations.find(location->getLocationID());
        if (foundLocation == peerInfo->mLocations.end()) {
          ZS_LOG_WARNING(Detail, log("could not find peer location information for non-connected peer location") + UseLocation::toDebug(location))
          return ILocation::LocationConnectionState_Disconnected;
        }

        UseAccountPeerLocationPtr accountPeerLocation = (*foundLocation).second;

        return toLocationConnectionState(accountPeerLocation->getState());
      }

      //-----------------------------------------------------------------------
      bool Account::send(
                         LocationPtr inLocation,
                         MessagePtr message,
                         PUID *outSentViaObjectID
                         ) const
      {
        UseLocationPtr location = inLocation;

        if (outSentViaObjectID) *outSentViaObjectID = 0;

        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        AutoRecursiveLock lock(*this);

        if (location == getLocationForLocal()) {
          ZS_LOG_ERROR(Detail, log("attempting to send message to self") + UseLocation::toDebug(location))
          return false;
        }

        if (location == getLocationForFinder()) {
          if (!mFinder) {
            ZS_LOG_WARNING(Detail, log("attempting to send to finder") + UseLocation::toDebug(location))
            return false;
          }

          if (outSentViaObjectID) *outSentViaObjectID = mFinder->getID();

          return mFinder->send(message);
        }

        PeerInfoMap::const_iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_DEBUG(log("peer is not connected") + UseLocation::toDebug(location))
          return false;
        }

        PeerInfoPtr peerInfo = (*found).second;

        PeerInfo::PeerLocationMap::const_iterator foundLocation = peerInfo->mLocations.find(location->getLocationID());
        if (foundLocation == peerInfo->mLocations.end()) {
          ZS_LOG_WARNING(Detail, log("could not find peer location information for non-connected peer location") + UseLocation::toDebug(location))
          return ILocation::LocationConnectionState_Disconnected;
        }

        UseAccountPeerLocationPtr accountPeerLocation = (*foundLocation).second;

        if (outSentViaObjectID) *outSentViaObjectID = accountPeerLocation->getID();

        return accountPeerLocation->send(message);
      }

      //-----------------------------------------------------------------------
      void Account::hintNowAvailable(LocationPtr inLocation)
      {
        UseLocationPtr location = inLocation;

        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        ZS_LOG_DEBUG(log("received hint about peer location") + UseLocation::toDebug(location))

        AutoRecursiveLock lock(*this);
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_WARNING(Detail, log("hint about new location when shutting down/shutdown is ignored") + UseLocation::toDebug(location))
          return;
        }

        PeerInfoMap::iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_WARNING(Detail, log("received hint about peer location that is not being subscribed") + UseLocation::toDebug(location))
          return;  // ignore the hint since there is no subscription to this location
        }

        String locationID = location->getLocationID();

        PeerInfoPtr &peerInfo = (*found).second;
        PeerInfo::PeerLocationMap::iterator locationFound = peerInfo->mLocations.find(locationID);
        if (locationFound != peerInfo->mLocations.end()) {
          ZS_LOG_WARNING(Detail, log("received hint about peer location that is already known") + PeerInfo::toDebug(peerInfo) + UseLocation::toDebug(location))
          return;  // thanks for the tip but we already know about this location...
        }

        // scope: see if we are in the middle of already searching for this peer location
        {
          PeerInfo::FindingBecauseOfLocationIDMap::iterator findingBecauseOfFound = peerInfo->mPeerFindBecauseOfLocations.find(locationID);
          if (findingBecauseOfFound != peerInfo->mPeerFindBecauseOfLocations.end()) {
            ZS_LOG_DEBUG(log("received hint about peer location for location that is already being searched because of a previous hint") + PeerInfo::toDebug(peerInfo))
            return; // we've already received this tip...
          }
        }

        // scope: see if we already will redo a search because of this peer location
        {
          PeerInfo::FindingBecauseOfLocationIDMap::iterator findingBecauseOfFound = peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.find(locationID);
          if (findingBecauseOfFound != peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.end()) {
            ZS_LOG_WARNING(Detail, log("received hint about peer location for location that has already been given a hint") + PeerInfo::toDebug(peerInfo))
            return; // we've already received this tip...
          }
        }

        // we will redo the search after this has completed because there are more locations that need to be found - THANKS FOR THE TIP!!
        peerInfo->mPeerFindNeedsRedoingBecauseOfLocations[locationID] = locationID;
        peerInfo->findTimeReset();

        ZS_LOG_DEBUG(log("received hint about peer location that will be added to hint search") + PeerInfo::toDebug(peerInfo))
        (IWakeDelegateProxy::create(mThisWeak.lock()))->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForMessageIncoming
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::notifyMessageIncomingResponseNotSent(MessageIncoming &inMessageIncoming)
      {
        UseMessageIncoming &messageIncoming = inMessageIncoming;

        AutoRecursiveLock lock(*this);

        LocationPtr location = messageIncoming.getLocation();
        MessagePtr message = messageIncoming.getMessage();

        if (message->isNotify()) {
          ZS_LOG_TRACE(log("no auto-message response is needed for notifications") + ZS_PARAM("message id", message->messageID()))
          return;
        }

        MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_NotFound);
        if (!result) {
          ZS_LOG_WARNING(Detail, log("automatic reply to incoming message could not be created") + messageIncoming.toDebug())
          return;
        }

        send(location, result);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFilesPtr Account::getPeerFiles() const
      {
        AutoRecursiveLock lock(*this);

        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, debug("peer files are not available on account as peer contact session does not exist"))
          return IPeerFilesPtr();
        }

        return mLockboxSession->getPeerFiles();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForPeer
      #pragma mark

      //-----------------------------------------------------------------------
      PeerPtr Account::findExisting(const String &peerURI) const
      {
        return Peer::convert(mPeersDB.findExisting(mThisWeak.lock(), peerURI));
      }

      //-----------------------------------------------------------------------
      PeerPtr Account::findExistingOrUse(PeerPtr inPeer)
      {
        return Peer::convert(mPeersDB.findExistingOrUse(mThisWeak.lock(), inPeer));
      }

      //-----------------------------------------------------------------------
      void Account::notifyDestroyed(Peer &inPeer)
      {
        UsePeer &peer = inPeer;

        String peerURI = peer.getPeerURI();

        if (mPeersDB.remove(peerURI)) {
          ZS_LOG_DEBUG(log("notified peer is destroyed") + peer.toDebug())
        } else {
          ZS_LOG_WARNING(Detail, log("existing location in map was already gone (probably okay)") + peer.toDebug())
        }
      }

      //-----------------------------------------------------------------------
      IPeer::PeerFindStates Account::getPeerState(const String &peerURI) const
      {
        AutoRecursiveLock lock(*this);
        PeerInfoMap::const_iterator found = mPeerInfos.find(peerURI);
        if (found == mPeerInfos.end()) {
          ZS_LOG_DEBUG(log("no state to get as peer URI was not found") + ZS_PARAM("peer URI", peerURI))
          return IPeer::PeerFindState_Idle;
        }

        PeerInfoPtr peerInfo = (*found).second;
        return peerInfo->mCurrentFindState;
      }

      //-----------------------------------------------------------------------
      LocationListPtr Account::getPeerLocations(
                                                const String &peerURI,
                                                bool includeOnlyConnectedLocations
                                                ) const
      {
        AutoRecursiveLock lock(*this);

        LocationListPtr result(new LocationList);

        PeerInfoMap::const_iterator found = mPeerInfos.find(peerURI);
        if (found == mPeerInfos.end()) {
          ZS_LOG_DEBUG(log("no peer locations as peer was not found") + ZS_PARAM("peer URI", peerURI))
          return result;
        }

        const PeerInfoPtr &peerInfo = (*found).second;
        for (PeerInfo::PeerLocationMap::const_iterator iter = peerInfo->mLocations.begin(); iter != peerInfo->mLocations.end(); ++iter) {
          UseAccountPeerLocationPtr peerLocation = (*iter).second;
          if ((!includeOnlyConnectedLocations) ||
              (peerLocation->isConnected())) {
            // only push back sessions that are actually connected (unless all are desired)
            ZS_LOG_TRACE(log("returning location") + UseAccountPeerLocation::toDebug(peerLocation))
            result->push_back(peerLocation->getLocation());
          }
        }

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForPeerSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::subscribe(PeerSubscriptionPtr inSubscription)
      {
        UsePeerSubscriptionPtr subscription = inSubscription;

        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Debug, log("subscription happened during shutdown") + ZS_PARAM("subscription", UsePeerSubscription::toDebug(subscription)))
          // during the graceful shutdown or post shutdown process, new subscriptions must not be created...
          subscription->notifyShutdown();
          return;
        }

        mPeerSubscriptions[subscription->getID()] = subscription;

        UseLocationPtr finderLocation = mLocationsDB.getFinder();

        if ((mFinder) &&
            (finderLocation)) {
          subscription->notifyLocationConnectionStateChanged(Location::convert(finderLocation), toLocationConnectionState(mFinder->getState()));
        }

        for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter) {
          PeerInfoPtr &peer = (*iter).second;

          subscription->notifyFindStateChanged(Peer::convert(peer->mPeer), peer->mCurrentFindState);

          for (PeerInfo::PeerLocationMap::iterator iterLocation = peer->mLocations.begin(); iterLocation != peer->mLocations.end(); ++iterLocation) {
            UseAccountPeerLocationPtr &peerLocation = (*iterLocation).second;
            LocationPtr location = peerLocation->getLocation();

            if (location) {
              subscription->notifyLocationConnectionStateChanged((location), toLocationConnectionState(peerLocation->getState()));
            }
          }
        }

        UsePeerPtr subscribingToPeer = Peer::convert(subscription->getSubscribedToPeer());
        if (!subscribingToPeer) {
          ZS_LOG_DEBUG(log("subscription was for all peers and not a specific peer"))
          return;
        }

        PeerURI uri = subscribingToPeer->getPeerURI();

        PeerInfoPtr peerInfo;

        PeerInfoMap::iterator found = mPeerInfos.find(uri);

        if (mPeerInfos.end() == found) {
          // need to create a new peer from scratch and do the find
          peerInfo = PeerInfo::create();
          peerInfo->mPeer = subscribingToPeer;
          ZS_LOG_DEBUG(log("subscribing to new peer") + PeerInfo::toDebug(peerInfo) + UsePeerSubscription::toDebug(subscription))
          mPeerInfos[uri] = peerInfo;
        } else {
          peerInfo = (*found).second;
          ZS_LOG_DEBUG(log("subscribing to existing peer") + PeerInfo::toDebug(peerInfo) + UsePeerSubscription::toDebug(subscription))
        }

        ++(peerInfo->mTotalSubscribers);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void Account::notifyDestroyed(PeerSubscription &inSubscription)
      {
        UsePeerSubscription &subscription = inSubscription;

        PeerSubscriptionMap::iterator found = mPeerSubscriptions.find(subscription.getID());
        if (found == mPeerSubscriptions.end()) {
          ZS_LOG_WARNING(Detail, log("notification of destruction of unknown subscription (probably okay)") + subscription.toDebug())
          return;
        }

        ZS_LOG_DEBUG(log("notification of destruction of subscription") + subscription.toDebug())

        UsePeerPtr subscribingToPeer = Peer::convert(subscription.getSubscribedToPeer());
        if (!subscribingToPeer) {
          ZS_LOG_DEBUG(log("subscription was for all peers and not a specific peer"))
          return;
        }

        PeerURI uri = subscribingToPeer->getPeerURI();

        PeerInfoPtr peerInfo;

        PeerInfoMap::iterator foundPeerInfo = mPeerInfos.find(uri);

        if (mPeerInfos.end() != foundPeerInfo) {
          peerInfo = (*foundPeerInfo).second;
          ZS_LOG_DEBUG(log("unsubscribing from existing peer") + PeerInfo::toDebug(peerInfo) + subscription.toDebug())

          --(peerInfo->mTotalSubscribers);
        }

        mPeerSubscriptions.erase(found);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr Account::getRepository() const
      {
        AutoRecursiveLock lock(*this);
        if (!mRepository) return PublicationRepositoryPtr();
        return PublicationRepository::convert(mRepository);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountFinderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onAccountFinderStateChanged(
                                                AccountFinderPtr finder,
                                                AccountStates state
                                                )
      {
        ZS_THROW_BAD_STATE_IF(!finder)

        ZS_LOG_DETAIL(log("received notification finder state changed") + ZS_PARAM("notified state", toString(state)) + UseAccountFinder::toDebug(finder))
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("notification of finder state changed after shutdown"))
          return;
        }

        if (finder != mFinder) {
          ZS_LOG_WARNING(Detail, log("received state change on obsolete finder") + UseAccountFinder::toDebug(finder))
          return;
        }

        notifySubscriptions(mLocationsDB.getFinder(), toLocationConnectionState(state));

        if (IAccount::AccountState_Ready == state) {
          mLastRetryFinderAfterDuration = Seconds(OPENPEER_STACK_ACCOUNT_FINDER_STARTING_RETRY_AFTER_IN_SECONDS);
        }

        if (IAccount::AccountState_Shutdown == state) {

          mFinder.reset();

          if (!mBackgroundingEnabled) {
            if (!isShuttingDown()) {
              ZS_LOG_WARNING(Detail, log("did not expect finder to shutdown") + toDebug())

              handleFinderRelatedFailure();
            }
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      void Account::onAccountFinderMessageIncoming(
                                                   AccountFinderPtr finder,
                                                   MessagePtr message
                                                   )
      {
        AutoRecursiveLock lock(*this);

        if (!message) {
          ZS_LOG_DEBUG(log("ignoring incoming message as message is NULL"))
          return;
        }

        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("ignoring incoming finder request because shutting down or shutdown"))
          return;
        }

        if (finder != mFinder) {
          ZS_LOG_DEBUG(log("finder does not match current finder (ignoring request)"))
          return;
        }

        PeerLocationFindRequestPtr peerLocationFindRequest = PeerLocationFindRequest::convert(message);

        if (peerLocationFindRequest) {
          ZS_LOG_DEBUG(log("receiving incoming find peer location request (will respond later)"))

          mIncomingFindRequests.push_back(peerLocationFindRequest);
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        ChannelMapNotifyPtr channelMapNotify = ChannelMapNotify::convert(message);
        if (channelMapNotify) {

          IPAddress finderIP;
          String relayAccessToken;
          String relayAccessSecret;
          mFinder->getFinderRelayInformation(finderIP, relayAccessToken, relayAccessSecret);

          for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter)
          {
            const PeerURI &uri = (*iter).first;
            PeerInfoPtr peerInfo = (*iter).second;

            PeerInfo::PeerLocationMap::iterator foundLocation = peerInfo->mLocations.find(channelMapNotify->remoteContext());
            if (foundLocation == peerInfo->mLocations.end()) {
              ZS_LOG_TRACE(log("peer location was not found on this peer") + ZS_PARAM("peer uri", uri) + ZS_PARAM("location", channelMapNotify->remoteContext()))
              continue;
            }

            UseAccountPeerLocationPtr peerLocation = (*foundLocation).second;
            if (!peerLocation->handleIncomingChannelMapNotify(channelMapNotify, relayAccessToken, relayAccessSecret)) {
              ZS_LOG_WARNING(Detail, log("this location did not handle this channel map notification") + ZS_PARAM("location", channelMapNotify->remoteContext()))
            }

            ZS_LOG_DEBUG(log("successfully handled channel map notification"))
            return;
          }
          ZS_LOG_WARNING(Detail, log("did not find any peer location that can handle the channel map notification") + ZS_PARAM("location", channelMapNotify->remoteContext()))
          return;
        }

        UseMessageIncomingPtr messageIncoming = UseMessageIncoming::create(mThisWeak.lock(), Location::convert(mLocationsDB.getFinder()), message);
        notifySubscriptions(messageIncoming);
      }

      //-----------------------------------------------------------------------
      void Account::onAccountFinderIncomingRelayChannel(
                                                        AccountFinderPtr inFinder,
                                                        IFinderRelayChannelPtr relayChannel,
                                                        ITransportStreamPtr receiveStream,
                                                        ITransportStreamPtr sendStream,
                                                        ChannelNumber channelNumber
                                                        )
      {
        UseAccountFinderPtr finder = inFinder;

        ZS_THROW_INVALID_ARGUMENT_IF(!relayChannel)
        ZS_THROW_INVALID_ARGUMENT_IF(!receiveStream)
        ZS_THROW_INVALID_ARGUMENT_IF(!sendStream)

        ZS_LOG_DEBUG(log("on account finder incoming relay channel") + ZS_PARAM("finder ID", finder->getID()) + ZS_PARAM("relay channel ID", relayChannel->getID()) + ZS_PARAM("incoming channel", channelNumber))

        // scope: check to see if relay channel can be used
        {
          AutoRecursiveLock lock(*this);
          
          if ((isShutdown()) ||
              (isShuttingDown())) {
            ZS_LOG_DEBUG(log("ignoring incoming finder relay channel information because shutting down or shutdown"))
            goto relay_channel_invalid;
          }

          if (finder != mFinder) {
            ZS_LOG_DEBUG(log("finder does not match current finder (thus shutting down incoming relay channel)"))
            goto relay_channel_invalid;
          }

          for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter)
          {
            PeerInfoPtr &peerInfo = (*iter).second;

            for (PeerInfo::PeerLocationMap::iterator iterLocation = peerInfo->mLocations.begin(); iterLocation != peerInfo->mLocations.end(); ++iterLocation)
            {
              const LocationID &locationID = (*iterLocation).first;
              UseAccountPeerLocationPtr peerLocation = (*iterLocation).second;

              ChannelNumber peerChannelNumber = peerLocation->getIncomingRelayChannelNumber();
              if (peerChannelNumber != channelNumber) {
                ZS_LOG_TRACE(log("this location is not responsible for this incoming channel") + ZS_PARAM("location", locationID) + ZS_PARAM("peer using channel", peerChannelNumber) + ZS_PARAM("incoming channel", channelNumber))
                continue;
              }

              ZS_LOG_DEBUG(log("found location is responsible for this incoming channel") + ZS_PARAM("location", locationID) + ZS_PARAM("peer using channel", peerChannelNumber) + ZS_PARAM("incoming channel", channelNumber))
              peerLocation->notifyIncomingRelayChannel(relayChannel, receiveStream, sendStream);
              return;
            }
          }

          ZS_LOG_WARNING(Detail, log("did not find any location responsible for this incoming channel") + ZS_PARAM("incoming channel", channelNumber))
          goto relay_channel_invalid;
        }

      relay_channel_invalid:
        relayChannel->cancel();
        relayChannel.reset();

        receiveStream->cancel();
        receiveStream.reset();

        sendStream->cancel();
        sendStream.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountPeerLocationDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onAccountPeerLocationStateChanged(
                                                      AccountPeerLocationPtr inPeerLocation,
                                                      AccountStates state
                                                      )
      {
        UseAccountPeerLocationPtr peerLocation = inPeerLocation;

        ZS_THROW_BAD_STATE_IF(!peerLocation)

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("notified account peer location state changed when account was shutdown") + ZS_PARAM("notified state", toString(state)) + UseAccountPeerLocation::toDebug(peerLocation))
          return;
        }

        ZS_LOG_DEBUG(log("notified account peer location state changed") + ZS_PARAM("notified state", toString(state)) + UseAccountPeerLocation::toDebug(peerLocation))

        UseLocationPtr location = peerLocation->getLocation();

        ZS_LOG_DEBUG(log("notified about peer location") + UseLocation::toDebug(location))

        PeerInfoMap::iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_WARNING(Debug, log("notified peer location state changed but was not found but peer was not found"))
          return;
        }

        PeerInfoPtr &peerInfo = (*found).second;
        PeerInfo::PeerLocationMap::iterator foundLocation = peerInfo->mLocations.find(location->getLocationID());
        if (foundLocation == peerInfo->mLocations.end()) {
          ZS_LOG_WARNING(Debug, log("could not find peer location") + PeerInfo::toDebug(peerInfo) + UseLocation::toDebug(location))
          return;
        }

        UseAccountPeerLocationPtr &foundPeerLocation = (*foundLocation).second;
        if (foundPeerLocation != peerLocation) {
          ZS_LOG_WARNING(Detail, log("notification of peer state on obsolete peer location") + PeerInfo::toDebug(peerInfo) + UseLocation::toDebug(location))
          return;
        }

        notifySubscriptions(location, toLocationConnectionState(state));

        switch (state) {
          case IAccount::AccountState_Shutdown:
          {
            bool findAgain = peerLocation->shouldRefindNow();

            // found the peer location, clear it out...
            peerInfo->mLocations.erase(foundLocation);
            ZS_LOG_DEBUG(log("peer location is shutdown") + PeerInfo::toDebug(peerInfo) + UseLocation::toDebug(location))

            if (findAgain) {
              ZS_LOG_DETAIL(log("allowed to refind peer at next opportunity") + PeerInfo::toDebug(peerInfo) + UseLocation::toDebug(location))
              peerInfo->findTimeReset();
            }
            break;
          }
          default:  break;
        }

        step();
      }

      //-----------------------------------------------------------------------
      void Account::onAccountPeerLocationMessageIncoming(
                                                         AccountPeerLocationPtr inPeerLocation,
                                                         MessagePtr message
                                                         )
      {
        UseAccountPeerLocationPtr peerLocation = inPeerLocation;

        AutoRecursiveLock lock(*this);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("message incoming ignored as account shutdown"))
          return;
        }

        UseLocationPtr location = peerLocation->getLocation();

        PeerInfoMap::iterator found = mPeerInfos.find(location->getPeerURI());
        if (found == mPeerInfos.end()) {
          ZS_LOG_WARNING(Detail, log("incoming message coming in from unknown peer location") + UseAccountPeerLocation::toDebug(peerLocation) + UseLocation::toDebug(location))
          return;
        }

        PeerInfoPtr peerInfo = (*found).second;

        ZS_LOG_DEBUG(log("handling message") + PeerInfo::toDebug(peerInfo))

        UseMessageIncomingPtr messageIncoming = UseMessageIncoming::create(mThisWeak.lock(), Location::convert(location), message);
        notifySubscriptions(messageIncoming);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForServiceLockboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::notifyServiceLockboxSessionStateChanged()
      {
        // WARNING: DO NOT LOCK HERE
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      IKeyGeneratorPtr Account::takeOverRSAGeyGeneration()
      {
        AutoRecursiveLock lock(*this);

        IKeyGeneratorPtr result = mRSAKeyGenerator;

        ZS_LOG_DEBUG(log("lockbox is taking over RSA key generation") + IKeyGenerator::toDebug(result))

        get(mBlockRSAKeyGeneration) = true; // prevent key generation from occuring from without account since it's now the responsibility of the lockbox

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IDNSDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onLookupCompleted(IDNSQueryPtr query)
      {
        AutoRecursiveLock lock(*this);
        if (query != mFinderDNSLookup) {
          ZS_LOG_DEBUG(log("notified about obsolete DNS query"))
          return;
        }

        mAvailableFinderSRVResult = query->getSRV();

        mFinderDNSLookup->cancel();
        mFinderDNSLookup.reset();

        if (mFinder) {
          ZS_LOG_DEBUG(log("notifying existing finder DNS lookup is complete"))
          mFinder->notifyFinderDNSComplete();
        }

        if (!mAvailableFinderSRVResult) {
          ZS_LOG_ERROR(Detail, log("SRV DNS lookoup failed to return result"))

          mAvailableFinders.pop_front();
          if (mAvailableFinders.size() < 1) {
            ZS_LOG_ERROR(Detail, log("all finders failed to resolve or connect"))
            handleFinderRelatedFailure();
          }
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IICESocketDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onICESocketStateChanged(
                                            IICESocketPtr socket,
                                            ICESocketStates state
                                            )
      {
        ZS_LOG_DEBUG(log("on ice socket state changed"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onICESocketCandidatesChanged(IICESocketPtr socket)
      {
        ZS_LOG_DEBUG(log("on ice socket candidates changed"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<PeerLocationFindResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool Account::handleMessageMonitorResultReceived(
                                                       IMessageMonitorPtr monitor,
                                                       PeerLocationFindResultPtr findResult
                                                       )
      {
        ZS_LOG_DEBUG(log("peer location find received result") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("received response after already shutdown"))
          return false;
        }

        PeerInfoPtr peerInfo;

        for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter) {
          PeerInfoPtr &checkPeerInfo = (*iter).second;

          if (monitor != checkPeerInfo->mPeerFindMonitor) continue;

          peerInfo = checkPeerInfo;
          break;
        }

        if (!peerInfo) {
          ZS_LOG_WARNING(Detail, log("did not find any peers monitoring this request (ignoring obsolete result)") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        PeerLocationFindRequestPtr request = PeerLocationFindRequest::convert(monitor->getMonitoredMessage());
        ZS_THROW_INVALID_ASSUMPTION_IF(!request)

        // prepare all the locations in the result hat do not yet exist
        const LocationInfoList &locations = findResult->locations();

        ZS_LOG_DEBUG(debug("peer location find result received") + ZS_PARAM("size", locations.size()))

        for (LocationInfoList::const_iterator iterLocation = locations.begin(); iterLocation != locations.end(); ++iterLocation) {
          LocationInfoPtr locationInfo = (*iterLocation);

          UseLocationPtr fromLocation = Location::convert(locationInfo->mLocation);

          if (!fromLocation) {
            ZS_LOG_WARNING(Detail, log("location info missing location object"))
            continue;
          }

          PeerInfo::PeerLocationMap::iterator foundLocation = peerInfo->mLocations.find(fromLocation->getLocationID());
          if (foundLocation != peerInfo->mLocations.end()) {
            ZS_LOG_WARNING(Detail, log("received result for peer location that already exists thus will shutdown this existing location in favour whatever might be found") + PeerInfo::toDebug(peerInfo))

            UseAccountPeerLocationPtr peerLocation = (*foundLocation).second;

            peerInfo->mLocations.erase(foundLocation);
            peerLocation->shutdown();

            notifySubscriptions(fromLocation, ILocation::LocationConnectionState_Disconnected);

            foundLocation = peerInfo->mLocations.end();
            peerLocation.reset();
          }

          ZS_LOG_DEBUG(log("receiced find result for location") + UseLocation::toDebug(fromLocation))

          // scope: see if this would cause a seach redo later (if so, stop it from happening)
          {
            PeerInfo::FindingBecauseOfLocationIDMap::iterator redoFound = peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.find(fromLocation->getLocationID());
            if (redoFound != peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.end()) {
              ZS_LOG_DEBUG(log("receiced find result for location that would be searched later thus removing from later search"))
              peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.erase(redoFound);
            }
          }

          // don't know this location, remember it for later
          UseAccountPeerLocationPtr peerLocation = UseAccountPeerLocation::createFromPeerLocationFindResult(
                                                                                                            mThisWeak.lock(),
                                                                                                            mThisWeak.lock(),
                                                                                                            request,
                                                                                                            locationInfo
                                                                                                            );

          peerInfo->mLocations[fromLocation->getLocationID()] = peerLocation;
          get(peerInfo->mFindAgainAfterBackgrounded) = false; // no need to continue finding now that a location has been found

          // the act of finding a peer does not cause notification to the subscribers as only the establishment of a peer connection notifies the subscribers
        }

        ZS_LOG_DEBUG(debug("peer location find result processed"))

        handleFindRequestComplete(monitor);

        // process any pending incoming find requests that were waiting for the find to complete
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::handleMessageMonitorErrorResultReceived(
                                                            IMessageMonitorPtr monitor,
                                                            PeerLocationFindResultPtr ignore, // will always be NULL
                                                            MessageResultPtr result
                                                            )
      {
        ZS_LOG_DEBUG(log("peer location find received error result") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("received response after already shutdown"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("received requester received response error") + Message::toDebug(result))
        handleFindRequestComplete(monitor);
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AccountPeerLocation => IMessageMonitorResultDelegate<FindersGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool Account::handleMessageMonitorResultReceived(
                                                       IMessageMonitorPtr monitor,
                                                       FindersGetResultPtr result
                                                       )
      {
        ZS_LOG_DEBUG(log("finders get received result") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        if (monitor != mFindersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("received finders get result on obsolete monitor") + ZS_PARAM("received from", IMessageMonitor::toDebug(monitor)) + ZS_PARAM("expecting", IMessageMonitor::toDebug(mFindersGetMonitor)))
          return false;
        }

        mFindersGetMonitor->cancel();
        mFindersGetMonitor.reset();

        mAvailableFinders = result->finders();
        if (mAvailableFinders.size() < 1) {
          ZS_LOG_ERROR(Detail, log("finders get failed to return any finders"))
          handleFinderRelatedFailure();
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::handleMessageMonitorErrorResultReceived(
                                                            IMessageMonitorPtr monitor,
                                                            FindersGetResultPtr ignore,
                                                            MessageResultPtr result
                                                            )
      {
        ZS_LOG_ERROR(Detail, log("finders get failed, will try later") + Message::toDebug(result))

        AutoRecursiveLock lock(*this);

        if (monitor != mFindersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("received finders get error result on obsolete monitor") + ZS_PARAM("received from", IMessageMonitor::toDebug(monitor)) + ZS_PARAM("expecting", IMessageMonitor::toDebug(mFindersGetMonitor)))
          return false;
        }

        mFindersGetMonitor->cancel();
        mFindersGetMonitor.reset();

        handleFinderRelatedFailure();
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onWake()
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
      #pragma mark Account => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onTimer(TimerPtr timer)
      {
        ZS_LOG_TRACE(log("tick"))
        AutoRecursiveLock lock(*this);

        if (timer != mTimer) {
          ZS_LOG_WARNING(Detail, log("received timer notification on obsolete timer (probably okay)") + ZS_PARAM("timer ID", timer->getID()))
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onBackgroundingGoingToBackground(
                                                     IBackgroundingSubscriptionPtr subscription,
                                                     IBackgroundingNotifierPtr notifier
                                                     )
      {
        ZS_LOG_DEBUG(log("going to background"))

        AutoRecursiveLock lock(*this);

        get(mBackgroundingEnabled) = true;

        mBackgroundingNotifier = notifier;

        step();
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("going to background now"))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier.reset();
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("returning from background"))

        AutoRecursiveLock lock(*this);

        get(mBackgroundingEnabled) = false;
        mBackgroundingNotifier.reset();

        step();
      }

      //-----------------------------------------------------------------------
      void Account::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("application will quit"))

        AutoRecursiveLock lock(*this);

        setError(IHTTP::HTTPStatusCode_ClientClosedRequest, "application is quitting");
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IKeyGeneratorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onKeyGenerated(IKeyGeneratorPtr generator)
      {
        ZS_LOG_DEBUG(log("on key generated") + ZS_PARAM("generator id", generator->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Account::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::Account");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Account::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("stack::Account");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "shutdown reference", (bool)mGracefulShutdownReference);

        IHelper::debugAppend(resultEl, "state", IAccount::toString(mCurrentState));
        IHelper::debugAppend(resultEl, "location ID", mLocationID);
        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "backgrounding", mBackgroundingEnabled);
        IHelper::debugAppend(resultEl, "backgrounding subscription", (bool)mBackgroundingSubscription);
        IHelper::debugAppend(resultEl, "backgrounding notifier", (bool)mBackgroundingNotifier);

        IHelper::debugAppend(resultEl, "delegate", (bool)mTimer);

        IHelper::debugAppend(resultEl, "lockbox session id", mLockboxSession ? mLockboxSession->getID() : 0);
        IHelper::debugAppend(resultEl, "turn method list", mTURN ? mTURN->size() : 0);
        IHelper::debugAppend(resultEl, "stun method list", mSTUN ? mSTUN->size() : 0);

        IHelper::debugAppend(resultEl, "ice socket id", mSocket ? mSocket->getID() : 0);

        IHelper::debugAppend(resultEl, "location id", mLocationID);
        IHelper::debugAppend(resultEl, "self peer", UsePeer::toDebug(mPeersDB.getLocal()));
        IHelper::debugAppend(resultEl, "self location", UseLocation::toDebug(mLocationsDB.getLocal()));
        IHelper::debugAppend(resultEl, "finder location", UseLocation::toDebug(mLocationsDB.getFinder()));

        IHelper::debugAppend(resultEl, "repository id", mRepository ? mRepository->getID() : 0);

        IHelper::debugAppend(resultEl, "dh key pair templates", mDHKeyPairTemplates.size());

        IHelper::debugAppend(resultEl, "rsa key generator blocked", mBlockRSAKeyGeneration);
        IHelper::debugAppend(resultEl, "rsa key generator id", mRSAKeyGenerator ? mRSAKeyGenerator->getID() : 0);
        IHelper::debugAppend(resultEl, "rsa key generator complete", mRSAKeyGenerator ? mRSAKeyGenerator->isComplete() : false);

        IHelper::debugAppend(resultEl, "finder id", mFinder ? mFinder->getID() : 0);

        IHelper::debugAppend(resultEl, "finder retry after", mFinderRetryAfter);
        IHelper::debugAppend(resultEl, "last retry fidner after (ms)", mLastRetryFinderAfterDuration);

        IHelper::debugAppend(resultEl, "available finders", mAvailableFinders.size());
        IHelper::debugAppend(resultEl, "available finders srv result", (bool)mAvailableFinderSRVResult);
        IHelper::debugAppend(resultEl, "finders get monitor", (bool)mFindersGetMonitor);
        IHelper::debugAppend(resultEl, "finders dns lookup", (bool)mFinderDNSLookup);

        IHelper::debugAppend(resultEl, "peer infos", mPeerInfos.size());

        IHelper::debugAppend(resultEl, "subscribers", mPeerSubscriptions.size());

        IHelper::debugAppend(resultEl, "peer infos", mPeersDB.size());

        IHelper::debugAppend(resultEl, "locations", mLocationsDB.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void Account::cancel()
      {
        ZS_LOG_DEBUG(debug("cancel called"))

        AutoRecursiveLock lock(*this);  // just in case

        if (isShutdown()) return;

        setState(IAccount::AccountState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mRepository) {
          mRepository->cancel();
          mRepository.reset();
        }

        if (mFindersGetMonitor) {
          mFindersGetMonitor->cancel();
          mFindersGetMonitor.reset();
        }

        if (mFinderDNSLookup) {
          mFinderDNSLookup->cancel();
          mFinderDNSLookup.reset();
        }

        // scope: kill all the connection subscriptions
        {
          for (PeerSubscriptionMap::iterator iter_doNotUse = mPeerSubscriptions.begin(); iter_doNotUse != mPeerSubscriptions.end(); )
          {
            PeerSubscriptionMap::iterator current = iter_doNotUse;
            ++iter_doNotUse;

            PUID subscriptionID = (*current).first;
            UsePeerSubscriptionPtr subscriber = (*current).second.lock();

            if (!subscriber) {
              ZS_LOG_WARNING(Trace, log("peer subscription already gone thus removing (likely okay)") + ZS_PARAM("subscriber", subscriptionID))
              mPeerSubscriptions.erase(current);
              continue;
            }
            subscriber->notifyShutdown();
          }
        }

        // scope: kill all the subscriptions and the find requests since they are not needed anymore
        {
          for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter) {
            PeerInfoPtr &peerInfo = (*iter).second;

            if (peerInfo->mPeerFindMonitor) {
              setFindState(*peerInfo, IPeer::PeerFindState_Completed);
              setFindState(*peerInfo, IPeer::PeerFindState_Idle);
            }

            // after this point all subscriptions are considered "dead" and no new ones are allowed to be created
            peerInfo->mTotalSubscribers = 0;

            if (peerInfo->mPeerFindMonitor) {
              ZS_LOG_DEBUG(log("cancelling / stopping find request for peer") + PeerInfo::toDebug(peerInfo))

              // we have to kill all requests to find more peer locations (simply ignore the results from the requests)
              peerInfo->mPeerFindMonitor->cancel();
              peerInfo->mPeerFindMonitor.reset();
            }
          }
        }

        if (mRSAKeyGenerator) {
          mRSAKeyGenerator->cancel();
          mRSAKeyGenerator.reset();
        }

        if (mGracefulShutdownReference) {

          if (mPeerInfos.size() > 0) {
            for (PeerInfoMap::iterator peerIter = mPeerInfos.begin(); peerIter != mPeerInfos.end(); ) {
              PeerInfoMap::iterator current = peerIter;
              ++peerIter;

              PeerInfoPtr peerInfo = (*current).second;

              if (peerInfo->mLocations.size() > 0) {
                for (PeerInfo::PeerLocationMap::iterator iterLocation = peerInfo->mLocations.begin(); iterLocation != peerInfo->mLocations.end(); ++iterLocation) {
                  // send a shutdown request to each outstanding location
                  UseAccountPeerLocationPtr peerLocation = (*iterLocation).second;

                  ZS_LOG_DEBUG(log("cancel shutting down peer location") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
                  peerLocation->shutdown();
                }
              } else {
                // we don't need to remember this connection anymore since no locations were found or established
                ZS_LOG_DEBUG(log("cancel closing down peer") + PeerInfo::toDebug(peerInfo))
                mPeerInfos.erase(current);
              }
            }
          }

          if (mFinder) {
            ZS_LOG_DEBUG(log("shutting down peer finder") + UseAccountFinder::toDebug(mFinder))
            mFinder->shutdown();
          }

          if (mPeerInfos.size() > 0) {
            ZS_LOG_DEBUG(log("shutdown still waiting for all peer locations to shutdown..."))
            return;
          }

          if (mFinder) {
            if (IAccount::AccountState_Shutdown != mFinder->getState()) {
              ZS_LOG_DEBUG(log("shutdown still waiting for finder to shutdown"))
              return;
            }
          }

          if (mSocket) {
            ZS_LOG_DEBUG(log("shutting down socket"))
            mSocket->shutdown();

            if (IICESocket::ICESocketState_Shutdown != mSocket->getState()) {
              ZS_LOG_DEBUG(log("shutdown still waiting for ICE socket to shutdown"))
              return;
            }
          }

        }

        setState(IAccount::AccountState_Shutdown);

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        mGracefulShutdownReference.reset();

        mBackgroundingNotifier.reset();

        // scope: clear out peers that have not had their locations shutdown
        {
          for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter) {
            PeerInfoPtr &peerInfo = (*iter).second;

            for (PeerInfo::PeerLocationMap::iterator iterLocation = peerInfo->mLocations.begin(); iterLocation != peerInfo->mLocations.end(); ++iterLocation) {
              UseAccountPeerLocationPtr peerLocation = (*iterLocation).second;
              ZS_LOG_DEBUG(log("hard shutdown of peer location") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
              peerLocation->shutdown();
            }
            peerInfo->mLocations.clear();
          }

          mPeerInfos.clear();
        }

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }

        if (mFinder) {
          ZS_LOG_DEBUG(log("hard shutdown of peer finder"))
          mFinder->shutdown();
          mFinder.reset();
        }

        if (mSocket) {
          ZS_LOG_DEBUG(log("hard shutdown of socket"))
          mSocket->shutdown();
          mSocket.reset();
        }

        ZS_LOG_DEBUG(log("service lockbox disconnected"))
        mLockboxSession.reset();

        ZS_LOG_DEBUG(log("shutdown complete"))
      }

      //-----------------------------------------------------------------------
      void Account::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepTimer()) return;
        if (!stepRepository()) return;
        if (!stepRSAKeyGeneration()) return;
        if (!stepDefaultDHKeyTemplate()) return;
        if (!stepLockboxSession()) return;
        if (!stepLocations()) return;
        if (!stepSocket()) return;
        if (!stepRespondToPeerLocationFindReqests()) return;
        if (!stepFinderDNS()) return;
        if (!stepFinder()) return;

        setState(AccountState_Ready);

        if (!stepPeers()) return;
        if (!stepFinderBackgrounding()) return;

        ZS_LOG_TRACE(debug("step complete"))
      }

      //-----------------------------------------------------------------------
      bool Account::stepTimer()
      {
        if (mTimer) {
          ZS_LOG_TRACE(log("already have timer"))
          return true;
        }

        mTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_STACK_ACCOUNT_TIMER_FIRES_IN_SECONDS));
        ZS_LOG_TRACE(log("created timer") + ZS_PARAM("timer ID", mTimer->getID()))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepRepository()
      {
        if (mRepository) {
          ZS_LOG_TRACE(log("already have repository"))
          return true;
        }

        mRepository = IPublicationRepositoryForAccount::create(mThisWeak.lock());
        ZS_LOG_TRACE(log("repository created") + ZS_PARAM("respository ID", mRepository->getID()))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepRSAKeyGeneration()
      {
        // To speed things up generate an RSA key in the background for later use...

        if (mBlockRSAKeyGeneration) {
          ZS_LOG_TRACE(log("RSA key generation prevented (as lockbox session has taken over key generation)"))
          return true;
        }

        if (mRSAKeyGenerator) {
          ZS_LOG_TRACE(log("already have an RSA key generator in progress") + IKeyGenerator::toDebug(mRSAKeyGenerator))
          return true;
        }

        mRSAKeyGenerator = IKeyGenerator::generateRSA(mThisWeak.lock());

        ZS_LOG_DEBUG(log("generating RSA key (just in case)") + IKeyGenerator::toDebug(mRSAKeyGenerator))

        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepDefaultDHKeyTemplate()
      {
        if (mDHKeyPairTemplates.size() > 0) {
          ZS_LOG_TRACE(log("already have a DH key template"))
          return true;
        }

        DHKeyPair keyPair = getDHKeyPairTemplate(OPENPEER_STACK_ACCOUNT_DEFAULT_PRECOMPILED_DH_DOMAIN_KEY);

        if ((!keyPair.first) ||
            (!keyPair.second)) {
          ZS_LOG_ERROR(Detail, debug("could not load or generate default DH key pair template") + ZS_PARAM("namespace", IDHKeyDomain::toNamespace(OPENPEER_STACK_ACCOUNT_DEFAULT_PRECOMPILED_DH_DOMAIN_KEY)))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, (String("could not load or generate default DH key pair template for namespace") + IDHKeyDomain::toNamespace(OPENPEER_STACK_ACCOUNT_DEFAULT_PRECOMPILED_DH_DOMAIN_KEY)).c_str());
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("generated or loaded default size DH public / private key pair") + ZS_PARAM("private key id", keyPair.first->getID()) + ZS_PARAM("public key id", keyPair.second->getID()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxSession()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession)

        if ((mSTUN) ||
            (mTURN)) {
          ZS_LOG_TRACE(log("lockbox session is already setup"))
          return true;
        }

        WORD errorCode = 0;
        String reason;
        IServiceLockboxSession::SessionStates state = mLockboxSession->getState(&errorCode, &reason);
        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration:
          {
            ZS_LOG_TRACE(log("lockbox session pending"))
            return false;
          }
          case IServiceLockboxSession::SessionState_Shutdown:
          {
            ZS_LOG_ERROR(Detail, log("lockbox session is shutdown thus account must shutdown"))
            setError(errorCode, reason);
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready:  {
            ZS_LOG_TRACE(log("lockbox session is ready"))
            break;
          }
        }

        if (!mTURN) {
          ZS_LOG_DEBUG(log("creating TURN session"))
          mTURN = mLockboxSession->findServiceMethods("turn", "turn");
        }
        if (!mSTUN) {
          mSTUN = mLockboxSession->findServiceMethods("stun", "stun");
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLocations()
      {
        if (mPeersDB.getLocal()) {
          ZS_LOG_TRACE(log("already have a self peer"))
          ZS_THROW_BAD_STATE_IF(!mLocationsDB.getLocal())
          ZS_THROW_BAD_STATE_IF(!mLocationsDB.getFinder())
          return true;
        }

        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_ERROR(Detail, log("peer files are missing"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "Peer files are missing");
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("peer files are ready"))

        IPeerFilePublicPtr peerFilePublic = peerFiles->getPeerFilePublic();
        IPeerFilePrivatePtr peerFilePrivate = peerFiles->getPeerFilePrivate();

        ZS_THROW_BAD_STATE_IF(!peerFilePublic)
        ZS_THROW_BAD_STATE_IF(!peerFilePrivate)

        mPeersDB.setLocal(IPeerForAccount::create(mThisWeak.lock(), peerFilePublic));
        mLocationsDB.setLocal(UseLocation::getForLocal(mThisWeak.lock()));
        mLocationsDB.setFinder(UseLocation::getForFinder(mThisWeak.lock()));
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepSocket()
      {
        if (mSocket) {
          IICESocket::ICESocketStates socketState = mSocket->getState();

          if (IICESocket::ICESocketState_Shutdown == socketState) {
            ZS_LOG_ERROR(Debug, log("notified ICE socket is shutdown unexpected"))
            mSocket.reset();

            if (services::ISettings::getBool(OPENPEER_STACK_SETTING_ACCOUNT_SHUTDOWN_ON_ICE_SOCKET_FAILURE)) {
              setError(IHTTP::HTTPStatusCode_Networkconnecttimeouterror, "ICE socket shutdown unexpectedly");
              cancel();
            }
            return false;
          }

          if ((IICESocket::ICESocketState_Ready != socketState) &&
              (IICESocket::ICESocketState_Sleeping != socketState)) {
            ZS_LOG_TRACE(log("waiting for the socket to wake up or to go to sleep"))
            return true;
          }

          ZS_LOG_TRACE(log("sockets are ready"))
          return true;
        }

        IICESocket::TURNServerInfoList turnServers;
        IICESocket::STUNServerInfoList stunServers;
        getNATServers(turnServers, stunServers);

        ZS_LOG_DEBUG(log("creating ICE socket") + ZS_PARAM("turn servers", turnServers.size()) + ZS_PARAM("stun servers", stunServers.size()))

        mSocket = IICESocket::create(
                                     UseStack::queueServices(),
                                     mThisWeak.lock(),
                                     turnServers,
                                     stunServers
                                     );
        if (mSocket) {
          ZS_LOG_TRACE(log("socket created thus need to wait for socket to be ready"))
          return true;
        }

        ZS_LOG_ERROR(Detail, log("failed to create ICE socket thus shutting down"))
        setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create RUDP ICE Socket");
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepRespondToPeerLocationFindReqests()
      {
        ZS_LOG_TRACE(log("responding to incoming find requests") + ZS_PARAM("size", mIncomingFindRequests.size()))

        for (IncomingFindRequestList::iterator iter = mIncomingFindRequests.begin(); iter != mIncomingFindRequests.end(); )
        {
          IncomingFindRequestList::iterator current = iter; ++iter;

          PeerLocationFindRequestPtr &peerLocationFindRequest = (*current);

          LocationInfoPtr fromLocationInfo = peerLocationFindRequest->locationInfo();
          UseLocationPtr fromLocation;
          if (fromLocationInfo) {
            if (fromLocationInfo->mLocation) {
              fromLocation = Location::convert(fromLocationInfo->mLocation);
            }
          }

          UsePeerPtr fromPeer;
          if (fromLocation) {
            fromPeer = fromLocation->getPeer();
          }

          if ((!fromLocation) ||
              (!fromPeer)){
            ZS_LOG_WARNING(Detail, log("invalid request received") + UseLocation::toDebug(fromLocation))
            MessageResultPtr result = MessageResult::create(peerLocationFindRequest, IHTTP::HTTPStatusCode_BadRequest);
            send(Location::convert(mLocationsDB.getFinder()), result);

            mIncomingFindRequests.erase(current);
            continue;
          }

          ZS_LOG_DEBUG(log("received incoming peer find request") + UseLocation::toDebug(fromLocation))

          if (mBackgroundingEnabled) {
            ZS_LOG_WARNING(Detail, log("all incoming requests must be rejected because of backgrounding") + UseLocation::toDebug(fromLocation))

            MessageResultPtr result = MessageResult::create(peerLocationFindRequest, IHTTP::HTTPStatusCode_Gone);
            send(Location::convert(mLocationsDB.getFinder()), result);

            mIncomingFindRequests.erase(current);
            continue;
          }

          PeerInfoPtr peerInfo;

          PeerInfoMap::iterator foundPeer = mPeerInfos.find(fromLocation->getPeerURI());
          if (foundPeer != mPeerInfos.end()) {
            peerInfo = (*foundPeer).second;
            ZS_LOG_DEBUG(log("received incoming peer find request from known peer") + PeerInfo::toDebug(peerInfo))

            if (peerInfo->mPeerFindMonitor) {
              ZS_LOG_TRACE(log("waiting for outgoing peer location find to complete before responding to incoming peer location find request"))
              continue;
            }

          } else {
            peerInfo = PeerInfo::create();
            peerInfo->mPeer = fromPeer;
            mPeerInfos[fromPeer->getPeerURI()] = peerInfo;
            ZS_LOG_DEBUG(log("received incoming peer find request from unknown peer") + PeerInfo::toDebug(peerInfo))
          }

          UseAccountPeerLocationPtr peerLocation;
          PeerInfo::PeerLocationMap::iterator foundLocation = peerInfo->mLocations.find(fromLocation->getLocationID());
          if (foundLocation != peerInfo->mLocations.end()) {
            ZS_LOG_WARNING(Detail, log("received incoming peer find request from existing peer location (will resolve conflict to which location should remain alive)") + PeerInfo::toDebug(peerInfo))

            peerLocation = (*foundLocation).second;

            bool resetExistingLocation = true;

            if (!peerLocation->wasCreatedFromIncomingFind()) {
              Time existingRequestTime = peerLocation->getCreationFindRequestTimestamp();
              Time newRequestTime = peerLocationFindRequest->created();

              if (Time() == newRequestTime) {
                newRequestTime = existingRequestTime;
              }

              if (existingRequestTime > newRequestTime) {
                ZS_LOG_DEBUG(log("incoming find request is older than the find request that was issued (thus favouring existing location)") + ZS_PARAM("existing time", existingRequestTime) + ZS_PARAM("incoming find request time", newRequestTime))
                resetExistingLocation = false;
              } else if (existingRequestTime < newRequestTime) {
                ZS_LOG_DEBUG(log("incoming find request is newer than the find request that was previously issued (thus favouring incoming find request)") + ZS_PARAM("existing time", existingRequestTime) + ZS_PARAM("incoming find request time", newRequestTime))
                resetExistingLocation = true;
              } else {
                String existingContext = peerLocation->getFindRequestContext();
                String incomingFindLocationID = peerLocationFindRequest->context();

                ZS_LOG_DEBUG(log("incoming find request is the same time as the new find reqest (will resolve conflict by location ID)") + ZS_PARAM("existing time", existingRequestTime) + ZS_PARAM("incoming find request time", newRequestTime) + ZS_PARAM("this location id", mLocationID) + ZS_PARAM("incoming find location id", incomingFindLocationID))

                if (existingContext > incomingFindLocationID) {
                  ZS_LOG_DEBUG(log("this location is favoured to the incoming find request"))
                  resetExistingLocation = false;
                } else {
                  ZS_LOG_DEBUG(log("incoming find location is favoured vs this location"))
                  resetExistingLocation = true;
                }
              }
            }

            if (!resetExistingLocation) {
              ZS_LOG_WARNING(Detail, log("intentionally ignoring incoming find request because the existing location is favoured to the new location described in the incoming find request (this location won the conflict resolution)"))
              mIncomingFindRequests.erase(current);
              continue;
            }

            ZS_LOG_WARNING(Detail, log("shutting down existing account peer location in favour of new incoming find request (this location lost conflict resolution)"))

            peerInfo->mLocations.erase(foundLocation);
            peerLocation->shutdown();

            notifySubscriptions(fromLocation, ILocation::LocationConnectionState_Disconnected);

            foundLocation = peerInfo->mLocations.end();
            peerLocation.reset();
          }

          // we must create a DH private / public key pair for use with this location based upon the key domain of the remote public key

          IDHKeyDomainPtr remoteKeyDomain = peerLocationFindRequest->dhKeyDomain();
          IDHPublicKeyPtr remotePublicKey = peerLocationFindRequest->dhPublicKey();

          if ((!remoteKeyDomain) ||
              (!remotePublicKey)) {
            ZS_LOG_ERROR(Detail, log("remote party did not include remote public key and key domain (thus ignoring incoming find request)"))
            mIncomingFindRequests.erase(current);
            continue;
          }

          DHKeyPair templateKeyPair = getDHKeyPairTemplate(remoteKeyDomain->getPrecompiledType());
          if ((!templateKeyPair.first) ||
              (!templateKeyPair.second)) {
            ZS_LOG_ERROR(Detail, log("remote party's key domain does not have a precompiled domain key template (thus ignoring incoming find request)"))
            mIncomingFindRequests.erase(current);
            continue;
          }

          IDHPublicKeyPtr newPublicKey;
          IDHPrivateKeyPtr newPrivateKey = IDHPrivateKey::loadAndGenerateNewEphemeral(templateKeyPair.first, templateKeyPair.second, newPublicKey);
          ZS_THROW_INVALID_ASSUMPTION_IF(!newPrivateKey)
          ZS_THROW_INVALID_ASSUMPTION_IF(!newPublicKey)

          peerLocation = UseAccountPeerLocation::createFromIncomingPeerLocationFind(
                                                                                    mThisWeak.lock(),
                                                                                    mThisWeak.lock(),
                                                                                    peerLocationFindRequest,
                                                                                    newPrivateKey,
                                                                                    newPublicKey
                                                                                    );

          peerInfo->mLocations[fromLocation->getLocationID()] = peerLocation;
          get(peerInfo->mFindAgainAfterBackgrounded) = false;

          ZS_LOG_DEBUG(log("received incoming peer find request from peer location") + PeerInfo::toDebug(peerInfo) + ZS_PARAM("peer location id", peerLocation->getID()))

          mIncomingFindRequests.erase(current);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepFinderDNS()
      {
        if (mFindersGetMonitor) {
          ZS_LOG_TRACE(log("waiting for finders get monitor to complete"))
          return false;
        }

        if (mFinderDNSLookup) {
          ZS_LOG_TRACE(log("waiting for finder DNS lookup to complete"))
          return false;
        }

        Time tick = zsLib::now();

        if (mFinderRetryAfter > tick) {
          ZS_LOG_TRACE(log("waiting a bit before retrying finder connection..."))
          return false;
        }

        if (mAvailableFinders.size() < 1) {
          ZS_THROW_BAD_STATE_IF(!mLockboxSession)

          BootstrappedNetworkPtr network = mLockboxSession->getBootstrappedNetwork();

          ZS_THROW_BAD_STATE_IF(!network)

          FindersGetRequestPtr request = FindersGetRequest::create();
          request->domain(getDomain());
          request->totalFinders(OPENPEER_STACK_FINDERS_GET_TOTAL_SERVERS_TO_GET);

          mFindersGetMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<FindersGetResult>::convert(mThisWeak.lock()), network, "bootstrapped-finders", "finders-get", request, Seconds(OPENPEER_STACK_FINDERS_GET_TIMEOUT_IN_SECONDS));

          ZS_LOG_DEBUG(log("attempting to get finders"))
          return false;
        }

        if (!mAvailableFinderSRVResult) {
          Finder &finder = mAvailableFinders.front();
          String srv = getMultiplexedJsonTCPTransport(finder);
          if (srv.isEmpty()) {
            ZS_LOG_ERROR(Detail, log("finder missing SRV name"))
            mAvailableFinders.pop_front();
            if (mAvailableFinders.size() < 1) {
              handleFinderRelatedFailure();
            }
            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
            return false;
          }

          mFinderDNSLookup = IDNS::lookupSRV(mThisWeak.lock(), srv, "finder", "tcp");
          ZS_LOG_DEBUG(log("performing DNS lookup on finder"))
          return false;
        }

        ZS_LOG_TRACE(log("step - finder DNS ready"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepFinder()
      {
        if (mBackgroundingEnabled) {
          ZS_LOG_TRACE(log("do not create finder while backgrounding"))
          return true;
        }

        if (mFinder) {
          if (IAccount::AccountState_Ready != mFinder->getState()) {
            ZS_LOG_TRACE(log("waiting for the finder to connect"))
            return false;
          }
          ZS_LOG_TRACE(log("finder already created"))
          return true;
        }

        ZS_LOG_DEBUG(log("creating finder instance"))
        mFinder = UseAccountFinder::create(mThisWeak.lock(), mThisWeak.lock());

        if (mFinder) {
          ZS_LOG_TRACE(log("waiting for finder to be ready"))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("step failed to create finder thus shutting down"))
        setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create Account Finder");
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepPeers()
      {
        if (mPeerInfos.size() < 1) {
          ZS_LOG_TRACE(log("step peers complete as no peers exist"))
          return true;
        }

        bool done = true;

        for (PeerInfoMap::iterator peerIter = mPeerInfos.begin(); peerIter != mPeerInfos.end(); )
        {
          PeerInfoMap::iterator current = peerIter;
          ++peerIter;

          const String &peerURI = (*current).first;
          PeerInfoPtr &peerInfo = (*current).second;

          if (mBackgroundingEnabled) {
            setFindState(*peerInfo, IPeer::PeerFindState_Completed);

            shutdownAllLocationsDueToBackgrounding(peerURI, peerInfo);

            if (peerInfo->mLocations.size() > 0) {
              ZS_LOG_TRACE(log("some location are still connected thus do not allow backgrounding yet") + PeerInfo::toDebug(peerInfo))
              done = false;
            }
            continue;
          }

          if (shouldShutdownInactiveLocations(peerURI, peerInfo)) {

            shutdownPeerLocationsNotNeeded(peerURI, peerInfo);

            if (peerInfo->mLocations.size() > 0) {
              ZS_LOG_TRACE(log("some location are still connected thus do not shutdown the peer yet") + PeerInfo::toDebug(peerInfo))
              continue;
            }

            // erase the peer now...
            ZS_LOG_DEBUG(log("no locations at this peer thus shutting down now") + PeerInfo::toDebug(peerInfo))

            setFindState(*peerInfo, IPeer::PeerFindState_Completed);
            mPeerInfos.erase(current);
            continue;
          }

          sendPeerKeepAlives(peerURI, peerInfo);

          performPeerFind(peerURI, peerInfo);
        }

        ZS_LOG_TRACE(log("step peers complete") + ZS_PARAM("done", done))
        return done;
      }

      //-----------------------------------------------------------------------
      bool Account::stepFinderBackgrounding()
      {
        if (!mBackgroundingEnabled) {
          ZS_LOG_TRACE(log("backgrounding not enabled (skipping step)"))
          return true;
        }

        if (!mFinder) {
          mBackgroundingNotifier.reset();
          ZS_LOG_TRACE(log("finder already shutdown due to backgrounding"))
          return true;
        }

        ZS_LOG_TRACE(log("telling finder to shutdown now"))

        // safe to shutdown finder now
        mFinder->shutdown();
        return true;
      }

      //-----------------------------------------------------------------------
      void Account::setState(IAccount::AccountStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("old state", toString(mCurrentState)) + ZS_PARAM("new state", toString(state)))

        mCurrentState = state;

        if (!mDelegate) return;

        AccountPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            mDelegate->onAccountStateChanged(pThis, mCurrentState);
          } catch(IAccountDelegateProxy::Exceptions::DelegateGone &) {
          }
        }

        notifySubscriptions(mLocationsDB.getLocal(), toLocationConnectionState(mCurrentState));
      }

      //-----------------------------------------------------------------------
      void Account::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_ERROR(Basic, debug("account error"))
      }

      //-----------------------------------------------------------------------
      CandidatePtr Account::getRelayCandidate(const String &localContext) const
      {
        if (!mFinder) return CandidatePtr();

        IPAddress finderIP;
        String relayAccessToken;
        String relayAccessSecret;

        mFinder->getFinderRelayInformation(finderIP, relayAccessToken, relayAccessSecret);

        if ((relayAccessToken.isEmpty()) ||
            (relayAccessSecret.isEmpty())) return CandidatePtr();

        CandidatePtr candidate(new Candidate);

        candidate->mNamespace = OPENPEER_STACK_CANDIDATE_NAMESPACE_FINDER_RELAY;
        candidate->mTransport = OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_MLS_TCP;
        candidate->mAccessToken = relayAccessToken;
        candidate->mIPAddress = finderIP;

        // hex(hmac(<relay-access-secret>, "finder-relay-access-validate:" + <relay-access-token> + ":" + <local-context> + ":channel-map")
        String hashString = "finder-relay-access-validate:" + relayAccessToken + ":" + localContext + ":channel-map";
        String proofHash = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(relayAccessSecret), hashString));

        ZS_LOG_TRACE(log("get relay candidate hash calculation") + ZS_PARAM("relay acceess secret", relayAccessSecret) + ZS_PARAM("hmac input", hashString) + ZS_PARAM("relay access secret proof", proofHash))

        candidate->mAccessSecretProof = proofHash;
        return candidate;
      }

      //-----------------------------------------------------------------------
      Account::DHKeyPair Account::getDHKeyPairTemplate(IDHKeyDomain::KeyDomainPrecompiledTypes type)
      {
        if (IDHKeyDomain::KeyDomainPrecompiledType_Unknown == type) return DHKeyPair();

        DHKeyPairTemplates::iterator found = mDHKeyPairTemplates.find(type);
        if (found != mDHKeyPairTemplates.end()) {
          DHKeyPair &result = (*found).second;
          ZS_LOG_DEBUG(log("found existing DH public / private key pair template") + ZS_PARAM("private key id", result.first->getID()) + ZS_PARAM("public key id", result.second->getID()))
          return result;
        }

        // attempt to load from cache
        String namespaceStr = IDHKeyDomain::toNamespace(type);

        IDHKeyDomainPtr keyDomain = IDHKeyDomain::loadPrecompiled(type);
        if (!keyDomain) {
          ZS_LOG_WARNING(Detail, log("do not have a precompiled DH key domain") + ZS_PARAM("namespace", namespaceStr))
          return DHKeyPair();
        }

        String staticPrivateCacheStr = namespaceStr + "/" + "static-private";
        String staticPublicCacheStr = namespaceStr + "/" + "static-public";

        String ephemeralPrivateCacheStr = namespaceStr + "/" + "ephemeral-private";
        String ephemeralPublicCacheStr = namespaceStr + "/" + "ephemeral-public";

        IDHPrivateKeyPtr privateTemplate;
        IDHPublicKeyPtr publicTemplate;

        // scope: load existing key template
        {
          SecureByteBlockPtr staticPrivateKey = IHelper::convertFromBase64(ICache::fetch(staticPrivateCacheStr));
          SecureByteBlockPtr staticPublicKey = IHelper::convertFromBase64(ICache::fetch(staticPublicCacheStr));

          SecureByteBlockPtr ephemeralPrivateKey = IHelper::convertFromBase64(ICache::fetch(ephemeralPrivateCacheStr));
          SecureByteBlockPtr ephemeralPublicKey = IHelper::convertFromBase64(ICache::fetch(ephemeralPublicCacheStr));

          if ((IHelper::hasData(staticPrivateKey)) &&
              (IHelper::hasData(staticPublicKey)) &&
              (IHelper::hasData(ephemeralPrivateKey)) &&
              (IHelper::hasData(ephemeralPublicKey))) {
            privateTemplate = IDHPrivateKey::load(keyDomain, publicTemplate, *staticPrivateKey, *ephemeralPrivateKey, *staticPublicKey, *ephemeralPublicKey);

            if (privateTemplate) {
              ZS_LOG_DEBUG(log("loaded DH public / private key pair template") + ZS_PARAM("private key id", privateTemplate->getID()) + ZS_PARAM("public key id", publicTemplate->getID()))
              DHKeyPair result = DHKeyPair(privateTemplate, publicTemplate);
              mDHKeyPairTemplates[type] = result;
              return result;
            }

            // this previous public / private key pair failed to load thus clear out the cache
            ZS_LOG_ERROR(Detail, log("failed to load existing DH public / private key pair template"))

            ICache::clear(staticPrivateCacheStr);
            ICache::clear(staticPublicCacheStr);
            ICache::clear(ephemeralPrivateCacheStr);
            ICache::clear(ephemeralPublicCacheStr);
          }
        }

        privateTemplate = IDHPrivateKey::generate(keyDomain, publicTemplate);

        if ((!privateTemplate) ||
            (!publicTemplate)) {
          ZS_LOG_ERROR(Detail, debug("failed to create DH public / private key pair template"))
          return DHKeyPair();
        }

        // scope: store key template
        {
          SecureByteBlock staticPrivateKey;
          SecureByteBlock staticPublicKey;

          SecureByteBlock ephemeralPrivateKey;
          SecureByteBlock ephemeralPublicKey;

          privateTemplate->save(&staticPrivateKey, &ephemeralPrivateKey);
          publicTemplate->save(&staticPublicKey, &ephemeralPublicKey);

          zsLib::Time expires = zsLib::now() + Hours(OPENPEER_STACK_ACCOUNT_COOKIE_DH_KEY_DOMAIN_CACHE_LIFETIME_HOURS);

          ICache::store(staticPrivateCacheStr, expires, IHelper::convertToBase64(staticPrivateKey));
          ICache::store(staticPublicCacheStr, expires, IHelper::convertToBase64(staticPublicKey));
          ICache::store(ephemeralPrivateCacheStr, expires, IHelper::convertToBase64(ephemeralPrivateKey));
          ICache::store(ephemeralPublicCacheStr, expires, IHelper::convertToBase64(ephemeralPublicKey));
        }

        ZS_LOG_DEBUG(log("sucessfully generated DH public / private key pair template") + ZS_PARAM("private key id", privateTemplate->getID()) + ZS_PARAM("public key id", publicTemplate->getID()))

        DHKeyPair result = DHKeyPair(privateTemplate, publicTemplate);
        mDHKeyPairTemplates[type] = result;
        return result;
      }

      //-----------------------------------------------------------------------
      void Account::setFindState(
                                 PeerInfo &peerInfo,
                                 IPeer::PeerFindStates state
                                 )
      {
        if (peerInfo.mCurrentFindState == state) return;

        ZS_LOG_DEBUG(log("find state changed") + ZS_PARAM("old state", IPeer::toString(peerInfo.mCurrentFindState)) + ZS_PARAM("new state", IPeer::toString(state)) + peerInfo.toDebug())

        peerInfo.mCurrentFindState = state;

        notifySubscriptions(peerInfo.mPeer, state);
      }

      //-----------------------------------------------------------------------
      bool Account::shouldFind(
                               const String &peerURI,
                               const PeerInfoPtr &peerInfo
                               ) const
      {
        typedef zsLib::Time Time;

        Time tick = zsLib::now();

        if (peerInfo->mPeerFindMonitor) {
          ZS_LOG_TRACE(log("peer has peer find in progress thus no need to conduct new search") + PeerInfo::toDebug(peerInfo))
          return false;
        }

        if (peerInfo->mFindAgainAfterBackgrounded) {
          if (peerInfo->mLocations.size() > 0) {
            ZS_LOG_TRACE(log("cannot perform find request while locations are still attached from before backgrounding"))
            return false;
          }
        }

        if (!peerInfo->mFindAgainAfterBackgrounded) {
          if (peerInfo->mTotalSubscribers < 1) {
            ZS_LOG_TRACE(log("no subscribers required so no need to subscribe to this location"))
            return false;
          }
        }

        if (peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.size() > 0) {
          ZS_LOG_TRACE(log("peer has hints of new locations thus search needs redoing") + PeerInfo::toDebug(peerInfo))
          return true;
        }

        if (peerInfo->mLocations.size() > 0) {
          ZS_LOG_TRACE(log("peer has locations and no hints to suggest new locations thus no need to conduct new search") + PeerInfo::toDebug(peerInfo))
          return false;
        }

        // we have subscriptions but no locations, see if it is okay to find again "now"...
        if (tick < peerInfo->mNextScheduledFind) {
          ZS_LOG_TRACE(log("not time yet to conduct a new search") + PeerInfo::toDebug(peerInfo))
          return false;
        }

        if (!peerInfo->mPeer->getPeerFilePublic()) {
          ZS_LOG_WARNING(Detail, log("cannot find a peer where the public peer file is not known"))
          return false;
        }

        ZS_LOG_TRACE(log("peer search should be conducted") + PeerInfo::toDebug(peerInfo))
        return true;
      }

      //-----------------------------------------------------------------------
      void Account::shutdownAllLocationsDueToBackgrounding(
                                                           const String &peerURI,
                                                           PeerInfoPtr &peerInfo
                                                           )
      {
        get(peerInfo->mFindAgainAfterBackgrounded) = true;

        if (peerInfo->mPeerFindMonitor) {

          ZS_LOG_DEBUG(log("must cancel find request as it cannot complete while in background"))
          peerInfo->mPeerFindMonitor->cancel();
          peerInfo->mPeerFindMonitor.reset();

          for (PeerInfo::FindingBecauseOfLocationIDMap::iterator iter = peerInfo->mPeerFindBecauseOfLocations.begin(); iter != peerInfo->mPeerFindBecauseOfLocations.end(); ++iter)
          {
            const LocationID &locationID = (*iter).first;
            peerInfo->mPeerFindNeedsRedoingBecauseOfLocations[locationID] = locationID;
          }

          peerInfo->mPeerFindBecauseOfLocations.clear();
        }

        // scope: the peer is not incoming and all subscriptions are gone therefor it is safe to shutdown the peer locations entirely
        if (peerInfo->mLocations.size() < 1) return;

        ZS_LOG_TRACE(log("attempting to shutdown peer location") + PeerInfo::toDebug(peerInfo))

        for (PeerInfo::PeerLocationMap::iterator locationIter = peerInfo->mLocations.begin(); locationIter != peerInfo->mLocations.end(); ) {
          PeerInfo::PeerLocationMap::iterator locationCurrentIter = locationIter;
          ++locationIter;

          const String &locationID = (*locationCurrentIter).first;
          UseAccountPeerLocationPtr &peerLocation = (*locationCurrentIter).second;

          ZS_LOG_DEBUG(log("shutting down peer location immediately due to backgrounding") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))

          peerInfo->mPeerFindNeedsRedoingBecauseOfLocations[locationID] = locationID;

          // signal the shutdown now...
          peerLocation->shutdown();
        }
      }
      
      //-----------------------------------------------------------------------
      bool Account::shouldShutdownInactiveLocations(
                                                    const String &peerURI,
                                                    const PeerInfoPtr &peerInfo
                                                    ) const
      {
        if (peerInfo->mPeerFindMonitor) {
          ZS_LOG_TRACE(log("peer has peer active find in progress thus its location should not be shutdown") + ZS_PARAM("peer", peerInfo->mID) + ZS_PARAM("peer URI", peerURI))
          return false;
        }

        if (peerInfo->mFindAgainAfterBackgrounded) {
          ZS_LOG_TRACE(log("peer is returning from backgrounding thus do not shut it down") + PeerInfo::toDebug(peerInfo))
          return false;
        }

        if (peerInfo->mTotalSubscribers > 0) {
          ZS_LOG_TRACE(log("peer has subscriptions thus no need to shutdown") + PeerInfo::toDebug(peerInfo))
          return false;
        }

        ZS_LOG_TRACE(log("this peer should shutdown non-active peer locations") + PeerInfo::toDebug(peerInfo))
        return true;
      }

      //-----------------------------------------------------------------------
      void Account::shutdownPeerLocationsNotNeeded(
                                                   const String &peerURI,
                                                   PeerInfoPtr &peerInfo
                                                   )
      {
        Time tick = zsLib::now();

        // scope: the peer is not incoming and all subscriptions are gone therefor it is safe to shutdown the peer locations entirely
        if (peerInfo->mLocations.size() > 0) {
          ZS_LOG_TRACE(log("checking to see which locations for this peer should be shutdown due to inactivity") + PeerInfo::toDebug(peerInfo))

          for (PeerInfo::PeerLocationMap::iterator locationIter = peerInfo->mLocations.begin(); locationIter != peerInfo->mLocations.end(); ) {
            PeerInfo::PeerLocationMap::iterator locationCurrentIter = locationIter;
            ++locationIter;

            // const String &locationID = (*locationCurrentIter).first;
            UseAccountPeerLocationPtr &peerLocation = (*locationCurrentIter).second;

            Time lastActivityTime = peerLocation->getTimeOfLastActivity();

            if (lastActivityTime + Seconds(OPENPEER_STACK_PEER_LOCATION_INACTIVITY_TIMEOUT_IN_SECONDS) > tick) {
              ZS_LOG_TRACE(log("peer location is still considered active at this time (thus keeping connection alive)") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
              continue;
            }

            ZS_LOG_DEBUG(log("shutting down non incoming peer location that does not have a subscription") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))

            // signal the shutdown now...
            peerLocation->shutdown();
          }
        }
      }

      //-----------------------------------------------------------------------
      void Account::sendPeerKeepAlives(
                                       const String &peerURI,
                                       PeerInfoPtr &peerInfo
                                       )
      {
        if (peerInfo->mLocations.size() < 1) return;

        Time tick = zsLib::now();

        ZS_LOG_TRACE(log("checking to see which locations should fire a keep alive timer") + PeerInfo::toDebug(peerInfo))

        for (PeerInfo::PeerLocationMap::iterator locationIter = peerInfo->mLocations.begin(); locationIter != peerInfo->mLocations.end(); ) {
          PeerInfo::PeerLocationMap::iterator locationCurrentIter = locationIter;
          ++locationIter;

          // const String &locationID = (*locationCurrentIter).first;
          UseAccountPeerLocationPtr &peerLocation = (*locationCurrentIter).second;

          Time lastActivityTime = peerLocation->getTimeOfLastActivity();

          if (lastActivityTime + Seconds(OPENPEER_STACK_PEER_LOCATION_KEEP_ALIVE_TIME_IN_SECONDS) > tick) {
            ZS_LOG_TRACE(log("peer location is not requiring a keep alive yet") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
            continue;
          }

          ZS_LOG_DEBUG(log("peer location is still needed thus sending keep alive now (if possible)...") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
          peerLocation->sendKeepAlive();
        }
      }

      //-----------------------------------------------------------------------
      void Account::performPeerFind(
                                    const String &peerURI,
                                    PeerInfoPtr &peerInfo
                                    )
      {
        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
        ZS_THROW_BAD_STATE_IF(!peerFiles)

        if (!shouldFind(peerURI, peerInfo)) {
          ZS_LOG_TRACE(log("peer find should not be conducted at this time") + PeerInfo::toDebug(peerInfo))
          return;
        }

        mSocket->wakeup();

        ZS_LOG_DEBUG(log("peer is conducting a peer find search for locations") + PeerInfo::toDebug(peerInfo))

        // remember which hints caused this search to happen
        peerInfo->mPeerFindBecauseOfLocations = peerInfo->mPeerFindNeedsRedoingBecauseOfLocations;
        peerInfo->mPeerFindNeedsRedoingBecauseOfLocations.clear();  // we no longer need to redo because of these hints since we are now doing the search

        PeerLocationFindRequestPtr request = PeerLocationFindRequest::create();
        request->domain(getDomain());

        PeerLocationFindRequest::ExcludedLocationList exclude;
        for (PeerInfo::PeerLocationMap::iterator iter = peerInfo->mLocations.begin(); iter != peerInfo->mLocations.end(); ++iter) {

          const LocationID &locationID = (*iter).first;
          UseAccountPeerLocationPtr peerLocation = (*iter).second;

          // do not conduct a search for locations that already are connected or in the process of connecting...
          ZS_LOG_DEBUG(log("peer find will exclude location in search since location is already connecting or connected") + PeerInfo::toDebug(peerInfo) + UseAccountPeerLocation::toDebug(peerLocation))
          exclude.push_back(locationID);
        }

        LocationInfoPtr locationInfo = getLocationInfo(Location::convert(mLocationsDB.getLocal()));

        request->findPeer(Peer::convert(peerInfo->mPeer));
        request->context(IHelper::randomString(20*8/5));
        request->peerSecret(IHelper::randomString(32*8/5+1));
        request->iceUsernameFrag(mSocket->getUsernameFrag());
        request->icePassword(mSocket->getPassword());
        ZS_LOG_WARNING(Trace, log("peer location find request final should be set"))
        request->final(locationInfo->mCandidatesFinal);
        request->excludeLocations(exclude);

        DHKeyPair keyPair = getDHKeyPairTemplate(OPENPEER_STACK_ACCOUNT_DEFAULT_PRECOMPILED_DH_DOMAIN_KEY);
        ZS_THROW_INVALID_ASSUMPTION_IF((!keyPair.first) || (!keyPair.second))

        IDHPublicKeyPtr publicKey;
        IDHPrivateKeyPtr privateKey = IDHPrivateKey::loadAndGenerateNewEphemeral(keyPair.first, keyPair.second, publicKey);
        ZS_THROW_INVALID_ASSUMPTION_IF((!privateKey) ||  (!publicKey))

        // remember the private keys used for this request as it will be used in the context of the relay and data channel
        request->dhPrivateKey(privateKey);
        request->dhPublicKey(publicKey);

        CandidatePtr relayCandidate = getRelayCandidate(request->context());
        if (relayCandidate) {
          locationInfo->mCandidates.push_front(*relayCandidate);
        }

        request->locationInfo(locationInfo);
        request->peerFiles(peerFiles);

        peerInfo->mPeerFindMonitor = mFinder->sendRequest(IMessageMonitorResultDelegate<PeerLocationFindResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_PEER_LOCATION_FIND_TIMEOUT_IN_SECONDS));
        peerInfo->findTimeScheduleNext(); // schedule the next find after this one (in case this find fails)

        setFindState(*peerInfo, IPeer::PeerFindState_Finding);
      }

      //-----------------------------------------------------------------------
      void Account::handleFindRequestComplete(IMessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(*this);

        for (PeerInfoMap::iterator iter = mPeerInfos.begin(); iter != mPeerInfos.end(); ++iter) {
          PeerInfoPtr peerInfo = (*iter).second;

          if (monitor != peerInfo->mPeerFindMonitor) continue;

          ZS_LOG_DEBUG(log("find request is now complete") + PeerInfo::toDebug(peerInfo))

          // the search is now complete, stop looking for new searches
          peerInfo->mPeerFindMonitor.reset();
          peerInfo->mPeerFindBecauseOfLocations.clear();  // all hints are effectively destroyed now since we've completed the search

          setFindState(*peerInfo, IPeer::PeerFindState_Completed);
          setFindState(*peerInfo, IPeer::PeerFindState_Idle);
        }
      }

      //-----------------------------------------------------------------------
      void Account::handleFinderRelatedFailure()
      {
        ZS_LOG_TRACE(log("handling finder related failure"))

        Time tick = zsLib::now();

        mFinderRetryAfter = tick + mLastRetryFinderAfterDuration;
        mLastRetryFinderAfterDuration = mLastRetryFinderAfterDuration * 2;

        if (mLastRetryFinderAfterDuration > Seconds(OPENPEER_STACK_ACCOUNT_FINDER_MAX_RETRY_AFTER_TIME_IN_SECONDS)) {
          mLastRetryFinderAfterDuration = Seconds(OPENPEER_STACK_ACCOUNT_FINDER_MAX_RETRY_AFTER_TIME_IN_SECONDS);
        }
      }

      //-----------------------------------------------------------------------
      void Account::notifySubscriptions(
                                        UseLocationPtr location,
                                        ILocation::LocationConnectionStates state
                                        )
      {
        if (!location) {
          ZS_LOG_WARNING(Detail, log("location was NULL thus cannot notify about location state") + ZS_PARAM("state", ILocation::toString(state)))
          return;
        }

        for (PeerSubscriptionMap::iterator iter = mPeerSubscriptions.begin(); iter != mPeerSubscriptions.end(); )
        {
          PeerSubscriptionMap::iterator current = iter;
          ++iter;

          PUID subscriptionID = (*current).first;
          UsePeerSubscriptionPtr subscription = (*current).second.lock();
          if (!subscription) {
            ZS_LOG_WARNING(Detail, log("peer subscription is gone") + ZS_PARAM("subscription ID", subscriptionID))
            mPeerSubscriptions.erase(current);
            continue;
          }

          ZS_LOG_DEBUG(log("notifying subscription peer locations changed") + UsePeerSubscription::toDebug(subscription) + ZS_PARAM("state", ILocation::toString(state)) + UseLocation::toDebug(location))
          subscription->notifyLocationConnectionStateChanged(Location::convert(location), state);
        }
      }

      //-----------------------------------------------------------------------
      void Account::notifySubscriptions(
                                        UsePeerPtr peer,
                                        IPeer::PeerFindStates state
                                        )
      {
        for (PeerSubscriptionMap::iterator iter = mPeerSubscriptions.begin(); iter != mPeerSubscriptions.end(); )
        {
          PeerSubscriptionMap::iterator current = iter;
          ++iter;

          PUID subscriptionID = (*current).first;
          UsePeerSubscriptionPtr subscription = (*current).second.lock();
          if (!subscription) {
            ZS_LOG_WARNING(Detail, log("peer subscription is gone") + ZS_PARAM("subscription ID", subscriptionID))
            mPeerSubscriptions.erase(current);
            continue;
          }

          ZS_LOG_DEBUG(log("notifying subscription peer find state changed") + UsePeerSubscription::toDebug(subscription) + ZS_PARAM("state", IPeer::toString(state)) + UsePeer::toDebug(peer))
          subscription->notifyFindStateChanged(Peer::convert(peer), state);
        }
      }

      //-----------------------------------------------------------------------
      void Account::notifySubscriptions(UseMessageIncomingPtr messageIncoming)
      {
        for (PeerSubscriptionMap::iterator iter = mPeerSubscriptions.begin(); iter != mPeerSubscriptions.end(); )
        {
          PeerSubscriptionMap::iterator current = iter;
          ++iter;

          PUID subscriptionID = (*current).first;
          UsePeerSubscriptionPtr subscription = (*current).second.lock();
          if (!subscription) {
            ZS_LOG_WARNING(Detail, log("peer subscription is gone") + ZS_PARAM("subscription ID", subscriptionID))
            mPeerSubscriptions.erase(current);
            continue;
          }

          ZS_LOG_DEBUG(log("notifying subscription of incoming message") + UsePeerSubscription::toDebug(subscription) + UseMessageIncoming::toDebug(messageIncoming))
          subscription->notifyMessageIncoming(MessageIncoming::convert(messageIncoming));
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::PeerInfo
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Account::PeerInfo::toDebug(PeerInfoPtr peerInfo)
      {
        if (!peerInfo) return ElementPtr();
        return peerInfo->toDebug();
      }

      //-----------------------------------------------------------------------
      Account::PeerInfoPtr Account::PeerInfo::create()
      {
        PeerInfoPtr pThis(new PeerInfo);
        pThis->findTimeReset();
        pThis->mCurrentFindState = IPeer::PeerFindState_Pending;
        pThis->mTotalSubscribers = 0;
        return pThis;
      }

      //-----------------------------------------------------------------------
      Account::PeerInfo::~PeerInfo()
      {
      }

      //-----------------------------------------------------------------------
      void Account::PeerInfo::findTimeReset()
      {
        mNextScheduledFind = zsLib::now();
        mLastScheduleFindDuration = Seconds(OPENPEER_STACK_PEER_LOCATION_FIND_RETRY_IN_SECONDS/2);
        if (mLastScheduleFindDuration < Seconds(1))
          mLastScheduleFindDuration = Seconds(1);
      }

      //-----------------------------------------------------------------------
      void Account::PeerInfo::findTimeScheduleNext()
      {
        mLastScheduleFindDuration = mLastScheduleFindDuration * 2;
        mNextScheduledFind = zsLib::now() + mLastScheduleFindDuration;
      }

      //-----------------------------------------------------------------------
      Log::Params Account::PeerInfo::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::Account::PeerInfo");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::PeerInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("stack::Account::PeerInfo");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePeer::toDebug(mPeer));
        IHelper::debugAppend(resultEl, "locations", mLocations.size());
        IHelper::debugAppend(resultEl, "find monitor", (bool)mPeerFindMonitor);
        IHelper::debugAppend(resultEl, "find because", mPeerFindBecauseOfLocations.size());
        IHelper::debugAppend(resultEl, "find redo because", mPeerFindNeedsRedoingBecauseOfLocations.size());
        IHelper::debugAppend(resultEl, "find state", IPeer::toString(mCurrentFindState));
        IHelper::debugAppend(resultEl, "subscribers", mTotalSubscribers);
        IHelper::debugAppend(resultEl, "next find", mNextScheduledFind);
        IHelper::debugAppend(resultEl, "last duration (ms)", mLastScheduleFindDuration.total_milliseconds());
        IHelper::debugAppend(resultEl, "find again", mFindAgainAfterBackgrounded);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::PeersDB
      #pragma mark

      //-----------------------------------------------------------------------
      Account::UsePeerPtr Account::PeersDB::getLocal() const
      {
        AutoRecursiveLock lock(*this);
        return mSelf;
      }

      //-----------------------------------------------------------------------
      void Account::PeersDB::setLocal(UsePeerPtr peer)
      {
        AutoRecursiveLock lock(*this);
        mSelf = peer;
      }

      //-----------------------------------------------------------------------
      Account::UsePeerPtr Account::PeersDB::findExisting(
                                                         AccountPtr account,
                                                         const String &peerURI
                                                         ) const
      {
        AutoRecursiveLock lock(*this);
        PeerMap::const_iterator found = mPeers.find(peerURI);

        if (found == mPeers.end()) {
          ZS_LOG_TRACE(account->log("did not find existing peer") + ZS_PARAM("uri", peerURI))
          return PeerPtr();
        }

        UsePeerPtr existingPeer = (*found).second.lock();
        if (!existingPeer) {
          ZS_LOG_WARNING(Debug, account->log("existing peer object was destroyed") + ZS_PARAM("uri", peerURI))
          return PeerPtr();
        }

        ZS_LOG_TRACE(account->log("found existing peer") + UsePeer::toDebug(existingPeer))
        return existingPeer;
      }

      //-----------------------------------------------------------------------
      Account::UsePeerPtr Account::PeersDB::findExistingOrUse(
                                                              AccountPtr account,
                                                              UsePeerPtr peer
                                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peer)

        AutoRecursiveLock lock(*this);

        String peerURI = peer->getPeerURI();

        PeerMap::iterator found = mPeers.find(peerURI);

        if (found != mPeers.end()) {
          UsePeerPtr existingPeer = (*found).second.lock();
          if (existingPeer) {
            ZS_LOG_DEBUG(account->log("found existing peer to use") + UsePeer::toDebug(existingPeer))
            return Peer::convert(existingPeer);
          }
          ZS_LOG_WARNING(Detail, account->log("existing peer in map is now gone") + ZS_PARAM("peer URI", peerURI))
        }

        ZS_LOG_DEBUG(account->log("using newly created peer") + UsePeer::toDebug(peer))
        mPeers[peerURI] = peer;
        return peer;
      }

      //-----------------------------------------------------------------------
      bool Account::PeersDB::remove(const String &peerURI)
      {
        AutoRecursiveLock lock(*this);

        PeerMap::iterator found = mPeers.find(peerURI);
        if (found == mPeers.end()) return false;

        mPeers.erase(found);
        return true;
      }

      //-----------------------------------------------------------------------
      size_t Account::PeersDB::size() const
      {
        AutoRecursiveLock lock(*this);
        return mPeers.size();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationsDB
      #pragma mark

      //-----------------------------------------------------------------------
      Account::UseLocationPtr Account::LocationsDB::findExisting(
                                                                 AccountPtr account,
                                                                 const String &peerURI,
                                                                 const String &locationID
                                                                 ) const
      {
        AutoRecursiveLock lock(*this);

        PeerLocationIDPair index(locationID, peerURI);

        LocationMap::const_iterator found = mLocations.find(index);

        if (found == mLocations.end()) {
          ZS_LOG_TRACE(account->log("did not find existing location") + ZS_PARAM("peer uri", peerURI) + ZS_PARAM("location id", locationID))
          return UseLocationPtr();
        }

        UseLocationPtr location = (*found).second.lock();

        ZS_LOG_TRACE(account->log("found existing location") + UseLocation::toDebug(location))
        return location;
      }

      //-----------------------------------------------------------------------
      Account::UseLocationPtr Account::LocationsDB::findExistingOrUse(
                                                                      AccountPtr account,
                                                                      UseLocationPtr location
                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!location)

        AutoRecursiveLock lock(*this);

        String locationID = location->getLocationID();
        String peerURI = location->getPeerURI();

        PeerLocationIDPair index(locationID, peerURI);

        LocationMap::iterator found = mLocations.find(index);

        if (found != mLocations.end()) {
          UseLocationPtr existingLocation = (*found).second.lock();
          if (existingLocation) {
            ZS_LOG_DEBUG(account->log("found existing location to use") + UseLocation::toDebug(existingLocation))
            return Location::convert(existingLocation);
          }
          ZS_LOG_WARNING(Detail, account->log("existing location in map is now gone") + UseLocation::toDebug(location))
        }

        ZS_LOG_DEBUG(account->log("using newly created location") + UseLocation::toDebug(location))
        mLocations[index] = location;
        return Location::convert(location);
      }

      //-----------------------------------------------------------------------
      Account::UseLocationPtr Account::LocationsDB::getLocal() const
      {
        AutoRecursiveLock lock(*this);
        return mSelf;
      }

      //-----------------------------------------------------------------------
      Account::UseLocationPtr Account::LocationsDB::getFinder() const
      {
        AutoRecursiveLock lock(*this);
        return mFinder;
      }

      //-----------------------------------------------------------------------
      void Account::LocationsDB::setLocal(UseLocationPtr location)
      {
        AutoRecursiveLock lock(*this);
        mSelf = location;
      }

      //-----------------------------------------------------------------------
      void Account::LocationsDB::setFinder(UseLocationPtr location)
      {
        AutoRecursiveLock lock(*this);
        mFinder = location;
      }

      //-----------------------------------------------------------------------
      bool Account::LocationsDB::remove(
                                        const String &locationID,
                                        const String &peerURI
                                        )
      {
        AutoRecursiveLock lock(*this);

        PeerLocationIDPair index(locationID, peerURI);

        LocationMap::iterator found = mLocations.find(index);
        if (found == mLocations.end()) return false;

        mLocations.erase(found);
        return true;
      }
      
      //-----------------------------------------------------------------------
      size_t Account::LocationsDB::size() const
      {
        AutoRecursiveLock lock(*this);
        return mLocations.size();
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IAccount
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IAccount::toString(AccountStates state)
    {
      switch (state) {
        case AccountState_Pending:      return "Pending";
        case AccountState_Ready:        return "Ready";
        case AccountState_ShuttingDown: return "Shutting down";
        case AccountState_Shutdown:     return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    ElementPtr IAccount::toDebug(IAccountPtr account)
    {
      return internal::Account::toDebug(account);
    }

    //-------------------------------------------------------------------------
    IAccountPtr IAccount::create(
                                 IAccountDelegatePtr delegate,
                                 IServiceLockboxSessionPtr peerContactSession
                                 )
    {
      return internal::IAccountFactory::singleton().create(delegate, peerContactSession);
    }
  }
}
