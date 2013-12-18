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

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using services::IHelper;

      typedef ILocationForAccount::ForAccountPtr ForAccountPtr;
      typedef ILocationForMessages::ForMessagesPtr ForMessagesPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ILocationForAccount::toDebug(ForAccountPtr location)
      {
        return ILocation::toDebug(Location::convert(location));
      }

      //-----------------------------------------------------------------------
      ForAccountPtr ILocationForAccount::getForLocal(AccountPtr account)
      {
        return ILocationFactory::singleton().getForLocal(account);
      }

      //-----------------------------------------------------------------------
      ForAccountPtr ILocationForAccount::getForFinder(AccountPtr account)
      {
        return ILocationFactory::singleton().getForFinder(account);
      }

      //-----------------------------------------------------------------------
      ForAccountPtr ILocationForAccount::getForPeer(
                                                    PeerPtr peer,
                                                    const char *locationID
                                                    )
      {
        return ILocationFactory::singleton().getForPeer(peer, locationID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      ForMessagesPtr ILocationForMessages::convert(IMessageSourcePtr messageSource)
      {
        return Location::convert(messageSource);
      }

      //-----------------------------------------------------------------------
      ElementPtr ILocationForMessages::toDebug(ForMessagesPtr location)
      {
        return Location::toDebug(Location::convert(location));
      }

      //-----------------------------------------------------------------------
      ForMessagesPtr ILocationForMessages::getForLocal(AccountPtr account)
      {
        return ILocationFactory::singleton().getForLocal(account);
      }

      //-----------------------------------------------------------------------
      ForMessagesPtr ILocationForMessages::create(
                                                  IMessageSourcePtr messageSource,
                                                  const char *peerURI,
                                                  const char *locationID
                                                  )
      {
        return ILocationFactory::singleton().create(messageSource, peerURI, locationID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForPublication
      #pragma mark

      //-----------------------------------------------------------------------
      int ILocationForPublication::locationCompare(
                                                   const ILocationPtr &left,
                                                   const ILocationPtr &right,
                                                   const char * &outReason
                                                   )
      {
        if (left) {
          if (right) {
            if (left->getLocationType() < right->getLocationType()) {
              outReason = "location type L < location type R";
              return -1;
            }
            if (left->getLocationType() > right->getLocationType()) {
              outReason = "location type L > location type R";
              return 1;
            }

            switch (left->getLocationType())
            {
              case ILocation::LocationType_Finder:  break;  // match
              case ILocation::LocationType_Local:
              case ILocation::LocationType_Peer:
              {
                IPeerPtr leftPeer = left->getPeer();
                IPeerPtr rightPeer = right->getPeer();
                if (leftPeer) {
                  if (rightPeer) {
                    // both set, now compare
                    String leftPeerURI = leftPeer->getPeerURI();
                    String rightPeerURI = rightPeer->getPeerURI();
                    if (leftPeerURI < rightPeerURI) {
                      outReason = "location L peer URI < R peer URI";
                      return -1;
                    }
                    if (leftPeerURI > rightPeerURI) {
                      outReason = "location L peer URI > R peer URI";
                      return 1;
                    }
                    String leftLocationID = left->getLocationID();
                    String rightLocationID = right->getLocationID();
                    if (leftLocationID < rightLocationID) {
                      outReason = "L location ID < R location ID";
                      return -1;
                    }
                    if (leftLocationID > rightLocationID) {
                      outReason = "L location ID > R location ID";
                      return 1;
                    }
                    break;
                  }
                  // left > right
                  outReason = "location peer L set but R not set";
                  return 1;
                }
                if (rightPeer) {
                  // left < right
                  outReason = "location peer R set but L not set";
                  return -1;
                }
                break;
              }
            }
            return 0;
          }

          // left > right
          outReason = "location L set but R not set";
          return 1;
        }
        if (right) {
          // left < right
          outReason = "location R set but L not set";
          return -1;
        }
        return 0;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      int ILocationForPublicationRepository::locationCompare(
                                                             const ILocationPtr &left,
                                                             const ILocationPtr &right,
                                                             const char * &outReason
                                                             )
      {
        return ILocationForPublication::locationCompare(left, right, outReason);
      }

      //-----------------------------------------------------------------------
      LocationPtr ILocationForPublicationRepository::getForLocal(AccountPtr account)
      {
        return ILocationFactory::singleton().getForLocal(account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location
      #pragma mark

      //-----------------------------------------------------------------------
      Location::Location(
                         UseAccountPtr account,
                         LocationTypes type,
                         UsePeerPtr peer,
                         const char *locationID
                         ) :
        mID(zsLib::createPUID()),
        mAccount(account),
        mType(type),
        mPeer(peer),
        mLocationID(locationID ? String(locationID) : String())
      {
        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void Location::init()
      {
      }

      //-----------------------------------------------------------------------
      Location::~Location()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))

        UseAccountPtr account = mAccount.lock();

        if (account) {
          account->notifyDestroyed(*this);
        }
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(ILocationPtr location)
      {
        return dynamic_pointer_cast<Location>(location);
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(ForAccountPtr location)
      {
        return dynamic_pointer_cast<Location>(location);
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(ForMessagesPtr location)
      {
        return dynamic_pointer_cast<Location>(location);
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(ForPeerSubscriptionPtr location)
      {
        return dynamic_pointer_cast<Location>(location);
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(ForPublicationRepositoryPtr location)
      {
        return dynamic_pointer_cast<Location>(location);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => ILocation
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Location::toDebug(ILocationPtr location)
      {
        if (!location) return ElementPtr();
        return Location::convert(location)->toDebug();
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::getForLocal(IAccountPtr inAccount)
      {
        UseAccountPtr account = Account::convert(inAccount);

        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        String locationID = account->getLocationID();
        PeerPtr peerLocal = account->getPeerForLocal();

        LocationPtr existing = account->getLocationForLocal();
        if (existing) {
          ZS_LOG_DEBUG(existing->log("using existing local location"))
          return existing;
        }

        LocationPtr pThis(new Location(account, LocationType_Local, peerLocal, locationID));
        pThis->mThisWeak = pThis;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::getForFinder(IAccountPtr inAccount)
      {
        UseAccountPtr account = Account::convert(inAccount);

        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)

        LocationPtr existing = account->getLocationForFinder();
        if (existing) {
          ZS_LOG_DEBUG(existing->log("using existing finder location"))
          return existing;
        }

        LocationPtr pThis(new Location(account, LocationType_Finder, PeerPtr(), NULL));
        pThis->mThisWeak = pThis;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::Location::getForPeer(
                                                 IPeerPtr inPeer,
                                                 const char *inLocationID
                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inPeer)
        ZS_THROW_INVALID_ARGUMENT_IF(!inLocationID)

        String locationID(inLocationID);

        UsePeerPtr peer = Peer::convert(inPeer);
        UseAccountPtr account = peer->getAccount();

        LocationPtr existing = account->findExisting(peer->getPeerURI(), locationID);
        if (existing) {
          ZS_LOG_TRACE(existing->log("already have existing location for this peer") + ZS_PARAM("peer uri", peer->getPeerURI()) + ZS_PARAM("location", locationID))
          return existing;
        }

        LocationPtr pThis(new Location(account, LocationType_Peer, peer, locationID));
        pThis->mThisWeak = pThis;
        pThis->init();

        LocationPtr useThis = account->findExistingOrUse(pThis);
        if (useThis != pThis) {
          // small window outside lock in which object could have been created but instead re-using existing - do not inform the account of destruction since it was not used
          ZS_LOG_DEBUG(pThis->log("discarding object since one exists already"))
          pThis->mAccount.reset();
        }
        return useThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr Location::convert(IMessageSourcePtr messageSource)
      {
        return dynamic_pointer_cast<Location>(messageSource);
      }

      //-----------------------------------------------------------------------
      ILocation::LocationTypes Location::getLocationType() const
      {
        return mType;
      }

      //-----------------------------------------------------------------------
      String Location::getPeerURI() const
      {
        if (!mPeer) return String();

        return mPeer->getPeerURI();
      }

      //-----------------------------------------------------------------------
      String Location::getLocationID() const
      {
        return mLocationID;
      }

      //-----------------------------------------------------------------------
      LocationInfoPtr Location::getLocationInfo() const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) return LocationInfoPtr();
        return account->getLocationInfo(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      IPeerPtr Location::getPeer() const
      {
        return Peer::convert(mPeer);
      }

      //-----------------------------------------------------------------------
      bool Location::isConnected() const
      {
        return LocationConnectionState_Connected == getConnectionState();
      }

      //-----------------------------------------------------------------------
      ILocation::LocationConnectionStates Location::getConnectionState() const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("location is disconnected as account is gone"))
          return LocationConnectionState_Disconnected;
        }
        return account->getConnectionState(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      bool Location::sendMessage(message::MessagePtr message) const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("send message failed as account is gone"))
          return false;
        }
        return account->send(mThisWeak.lock(), message);
      }

      //-----------------------------------------------------------------------
      void Location::hintNowAvailable()
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("send message failed as account is gone"))
          return;
        }
        return account->hintNowAvailable(mThisWeak.lock());
      }

      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => ILocationForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => ILocationForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      LocationPtr Location::create(
                                   IMessageSourcePtr messageSource,
                                   const char *inPeerURI,
                                   const char *inLocationID
                                   )
      {
        LocationPtr sourceLocation = convert(messageSource);
        if (!sourceLocation) return LocationPtr();

        UseAccountPtr account = sourceLocation->mAccount.lock();
        if (!account) return LocationPtr();

        String peerURI(inPeerURI ? inPeerURI : "");
        String locationID(inLocationID ? inLocationID : "");

        peerURI.toLower();

        if ((peerURI.isEmpty()) &&
            (locationID.isEmpty())) {
          return getForFinder(Account::convert(account));
        }

        if ((peerURI.isEmpty()) ||
            (locationID.isEmpty())) {
          ZS_LOG_DEBUG(slog("cannot create location as missing peer URI or location ID") + ZS_PARAM("peer URI", + peerURI) + ZS_PARAM("location ID", locationID))
          return LocationPtr();
        }

        LocationPtr selfLocation = getForLocal(Account::convert(account));
        UsePeerPtr selfPeer = Peer::convert(selfLocation->getPeer());
        String selfPeerURI = selfPeer->getPeerURI();

        if ((selfLocation->getLocationID() == locationID) &&
            (selfPeerURI == peerURI)) {
          return selfLocation;
        }

        PeerPtr peer = IPeerForLocation::create(Account::convert(account), peerURI);
        if (!peer) {
          ZS_LOG_DEBUG(slog("cannot create location as peer failed to create"))
          return LocationPtr();
        }

        return getForPeer(peer, locationID);
      }

      //-----------------------------------------------------------------------
      PeerPtr Location::getPeer(bool) const
      {
        return Peer::convert(mPeer);
      }

      //-----------------------------------------------------------------------
      AccountPtr Location::getAccount() const
      {
        return Account::convert(mAccount.lock());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => ILocationForPeerSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Location::toDebug() const
      {
        ElementPtr resultEl = Element::create("Location");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "type", toString(mType));
        IHelper::debugAppend(resultEl, UsePeer::toDebug(mPeer));
        IHelper::debugAppend(resultEl, "location id(s)", mLocationID);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => ILocationForMessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      Location::SentViaObjectID Location::sendMessageFromMonitor(message::MessagePtr message) const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("send message failed as account is gone"))
          return false;
        }

        SentViaObjectID sentViaObjectID = 0;

        bool result = account->send(mThisWeak.lock(), message, &sentViaObjectID);

        return (result ? sentViaObjectID : 0);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Location::slog(const char *message)
      {
        return Log::Params(message, "Location");
      }

      //-----------------------------------------------------------------------
      Log::Params Location::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("Location");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Location::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocation
    #pragma mark

    //-------------------------------------------------------------------------
    const char *ILocation::toString(LocationTypes type)
    {
      switch (type)
      {
        case LocationType_Local:  return "Local";
        case LocationType_Finder: return "Finder";
        case LocationType_Peer:   return "Peer";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *ILocation::toString(LocationConnectionStates state)
    {
      switch (state)
      {
        case LocationConnectionState_Pending:       return "Pending";
        case LocationConnectionState_Connected:     return "Connected";
        case LocationConnectionState_Disconnecting: return "Disconnecting";
        case LocationConnectionState_Disconnected:  return "Disconnected";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    ElementPtr ILocation::toDebug(ILocationPtr location)
    {
      return internal::Location::toDebug(location);
    }

    //-------------------------------------------------------------------------
    ILocationPtr ILocation::getForLocal(IAccountPtr account)
    {
      return internal::ILocationFactory::singleton().getForLocal(account);
    }

    //-------------------------------------------------------------------------
    ILocationPtr ILocation::getForFinder(IAccountPtr account)
    {
      return internal::ILocationFactory::singleton().getForFinder(account);
    }

    //-------------------------------------------------------------------------
    ILocationPtr ILocation::getForPeer(
                                       IPeerPtr peer,
                                       const char *locationID
                                       )
    {
      return internal::ILocationFactory::singleton().getForPeer(peer, locationID);
    }

    //-------------------------------------------------------------------------
    ILocationPtr ILocation::convert(IMessageSourcePtr messageSource)
    {
      return internal::Location::convert(messageSource);
    }
  }
}
