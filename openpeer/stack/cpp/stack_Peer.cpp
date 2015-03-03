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

#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/XML.h>

#include <regex>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerForAccount::toDebug(ForAccountPtr peer)
      {
        return Peer::toDebug(Peer::convert(peer));
      }

      //-----------------------------------------------------------------------
      IPeerForAccount::ForAccountPtr IPeerForAccount::create(
                                                             AccountPtr account,
                                                             IPeerFilePublicPtr peerFilePublic
                                                             )
      {
        return IPeerFactory::singleton().create(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForLocation
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerForLocation::toDebug(ForLocationPtr peer)
      {
        return Peer::toDebug(Peer::convert(peer));
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerForLocation::create(
                                       AccountPtr account,
                                       const char *peerURI
                                       )
      {
        return IPeerFactory::singleton().create(account, peerURI);
      }

      //-----------------------------------------------------------------------
      bool IPeerForLocation::isValid(const char *peerURI)
      {
        return Peer::isValid(peerURI);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForLocationDatabases
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerForLocationDatabases::ForLocationDatabasesPtr IPeerForLocationDatabases::create(
                                                                                           AccountPtr account,
                                                                                           const char *peerURI
                                                                                           )
      {
        return IPeerFactory::singleton().create(account, peerURI);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerForMessages::toDebug(ForMessagesPtr peer)
      {
        return Peer::toDebug(Peer::convert(peer));
      }

      //-----------------------------------------------------------------------
      IPeerForMessages::ForMessagesPtr IPeerForMessages::create(
                                                                AccountPtr account,
                                                                IPeerFilePublicPtr peerFilePublic
                                                                )
      {
        return IPeerFactory::singleton().create(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      IPeerForMessages::ForMessagesPtr IPeerForMessages::getFromSignature(
                                                                          AccountPtr account,
                                                                          ElementPtr signedElement
                                                                          )
      {
        return IPeerFactory::singleton().getFromSignature(account, signedElement);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForPeerSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerForPeerSubscription::toDebug(ForPeerSubscriptionPtr peer)
      {
        return Peer::toDebug(Peer::convert(peer));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer
      #pragma mark

      //-----------------------------------------------------------------------
      Peer::Peer(
                 AccountPtr account,
                 IPeerFilePublicPtr peerFilePublic,
                 const String &peerURI
                 ) :
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mAccount(account),
        mPeerFilePublic(peerFilePublic),
        mPeerURI(peerURI)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void Peer::init()
      {
        AutoRecursiveLock lock(*this);

        if (!mPeerFilePublic) {
          ZS_LOG_TRACE(log("attempting to load peer file public from cache") + ZS_PARAM("uri", mPeerURI))
          mPeerFilePublic = IPeerFilePublic::loadFromCache(mPeerURI);
          if (mPeerFilePublic) {
            ZS_LOG_TRACE(log("loaded peer file public from cache") + ZS_PARAM("uri", mPeerURI))
          }
        }
      }

      //-----------------------------------------------------------------------
      Peer::~Peer()
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
      PeerPtr Peer::convert(IPeerPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::convert(ForAccountPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::convert(ForMessagesPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::convert(ForLocationPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::convert(ForLocationDatabasesPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::convert(ForPeerSubscriptionPtr peer)
      {
        return ZS_DYNAMIC_PTR_CAST(Peer, peer);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer => IPeer
      #pragma mark

      //-----------------------------------------------------------------------
      bool Peer::isValid(const char *peerURI)
      {
        if (!peerURI) {
          ZS_LOG_WARNING(Detail, slog("peer URI is not valid as it is NULL") + ZS_PARAM("uri", "(null)"))
          return false;
        }

        String domain;
        String contactID;

        bool result = splitURI(peerURI, domain, contactID);

        if (!result) {
          ZS_LOG_WARNING(Detail, slog("peer URI is not valid") + ZS_PARAM("peer uri", peerURI));
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Peer::splitURI(
                          const char *inPeerURI,
                          String &outDomain,
                          String &outContactID
                          )
      {
        String peerURI(inPeerURI);

        peerURI.toLower();

        std::regex e1("^peer:\\/\\/..*\\/([a-f0-9][a-f0-9])+$");
        if (!std::regex_match(peerURI, e1)) {
          ZS_LOG_WARNING(Detail, slog("peer URI is not valid") + ZS_PARAM("uri", peerURI))
          return false;
        }

        size_t startPos = strlen("peer://");
        size_t slashPos = peerURI.find('/', startPos);

        ZS_THROW_BAD_STATE_IF(slashPos == String::npos)

        outDomain = peerURI.substr(startPos, slashPos - startPos);
        outContactID = peerURI.substr(slashPos + 1);

        if (!IHelper::isValidDomain(outDomain)) {
          ZS_LOG_WARNING(Detail, slog("peer URI domain is not valid") + ZS_PARAM("uri", peerURI) + ZS_PARAM("domain", outDomain))

          outDomain.clear();
          outContactID.clear();

          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      String Peer::joinURI(
                           const char *inDomain,
                           const char *inContactID
                           )
      {
        String domain(inDomain ? inDomain : "");
        String contactID(inContactID ? inContactID : "");

        domain.trim();
        contactID.trim();

        domain.toLower();
        contactID.toLower();

        String result = "peer://" + domain + "/" + contactID;
        if (!IPeer::isValid(result)) {
          ZS_LOG_WARNING(Detail, slog("invalid peer URI createtd after join") + ZS_PARAM("peer URI", result))
          return String();
        }
        return result;
      }

      //-----------------------------------------------------------------------
      ElementPtr Peer::toDebug(IPeerPtr peer)
      {
        if (!peer) return ElementPtr();
        return Peer::convert(peer)->toDebug();
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::create(
                           AccountPtr inAccount,
                           IPeerFilePublicPtr peerFilePublic
                           )
      {
        UseAccountPtr account = inAccount;

        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!peerFilePublic)

        String peerURI = peerFilePublic->getPeerURI();

        PeerPtr existing = account->findExisting(peerURI);
        if (existing) {
          AutoRecursiveLock lock(*existing);

          ZS_LOG_TRACE(existing->log("found existing peer object to re-use") + ZS_PARAM("uri", peerURI))
          if (!existing->mPeerFilePublic) {
            existing->mPeerFilePublic = peerFilePublic;
          }

          return existing;
        }

        PeerPtr pThis(new Peer(inAccount, peerFilePublic, String()));
        pThis->mThisWeak = pThis;
        pThis->init();

        // check if it already exists in the account
        PeerPtr useThis = pThis->mAccount.lock()->findExistingOrUse(pThis);

        AutoRecursiveLock lock(*pThis);

        if (!(useThis->mPeerFilePublic)) {
          useThis->mPeerFilePublic = peerFilePublic;
        }

        if (pThis != useThis) {
          // small window in which could have created new object outside of lock - do not inform account of destruction since it was not used
          ZS_LOG_DEBUG(pThis->log("discarding object since one exists already"))
          pThis->mAccount.reset();
        }

        return useThis;
      }

      //-----------------------------------------------------------------------
      PeerPtr Peer::getFromSignature(
                                     AccountPtr account,
                                     ElementPtr inSignedElement
                                     )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!inSignedElement)

        ElementPtr signature;
        ElementPtr signedElement = IHelper::getSignatureInfo(inSignedElement, &signature);

        if (!signedElement) {
          ZS_LOG_WARNING(Detail, slog("could not find signature information"))
          return PeerPtr();
        }

        ElementPtr keyEl = signature->findFirstChildElement("key");
        if (!keyEl) {
          ZS_LOG_WARNING(Detail, slog("could not find key in signature"))
          return PeerPtr();
        }

        ElementPtr uriEl = keyEl->findFirstChildElement("uri");
        if (!uriEl) {
          ZS_LOG_WARNING(Detail, slog("could not find key in signature"))
          return PeerPtr();
        }

        String peerURI = uriEl->getTextDecoded();
        if (!isValid(peerURI)) {
          ZS_LOG_WARNING(Detail, slog("URI in key is not valid peer URI") + ZS_PARAM("peer uri", peerURI))
          return PeerPtr();
        }

        PeerPtr peer = create(account, peerURI);
        if (!peer) {
          ZS_LOG_ERROR(Detail, slog("could not create peer for given peer") + ZS_PARAM("peer uri", peerURI))
          return PeerPtr();
        }
        return peer;
      }

      //-----------------------------------------------------------------------
      bool Peer::verifySignature(ElementPtr inSignedElement) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inSignedElement)

        ElementPtr signature;
        ElementPtr signedElement = IHelper::getSignatureInfo(inSignedElement, &signature);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot validate signature aas account object is gone"))
          return false;
        }

        PeerPtr peer = getFromSignature(Account::convert(account), inSignedElement);
        if (!peer) {
          ZS_LOG_ERROR(Detail, log("could not create peer for given signature"))
          return false;
        }

        String peerURI = peer->getPeerURI();
        if (peerURI != getPeerURI()) {
          ZS_LOG_WARNING(Detail, debug("peer URI in signature does not match peer") + ZS_PARAM("signature", IPeer::toDebug(peer)))
          return false;
        }

        IPeerFilePublicPtr peerFilePublic = peer->getPeerFilePublic();
        if (!peerFilePublic) {
          ZS_LOG_WARNING(Detail, log("public peer file is not known yet for peer") + IPeer::toDebug(peer))
          return false;
        }

        if (!peerFilePublic->verifySignature(signedElement)) {
          ZS_LOG_WARNING(Detail, peer->log("signature failed to validate") + IPeer::toDebug(peer))
          return false;
        }

        ZS_LOG_DEBUG(log("signature passed for peer") + IPeer::toDebug(peer))
        return true;
      }

      //-----------------------------------------------------------------------
      String Peer::getPeerURI() const
      {
        AutoRecursiveLock lock(*this);
        if (mPeerFilePublic) {
          return mPeerFilePublic->getPeerURI();
        }
        return mPeerURI;
      }

      //-----------------------------------------------------------------------
      IPeerFilePublicPtr Peer::getPeerFilePublic() const
      {
        AutoRecursiveLock lock(*this);
        return mPeerFilePublic;
      }

      //-----------------------------------------------------------------------
      IPeer::PeerFindStates Peer::getFindState() const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("get find state account gone"))
          return PeerFindState_Completed;
        }
        return account->getPeerState(getPeerURI());
      }

      //-----------------------------------------------------------------------
      LocationListPtr Peer::getLocationsForPeer(bool includeOnlyConnectedLocations) const
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("locations are not available as account is gone"))
          LocationListPtr locations(new LocationList);
          return locations;
        }
        return account->getPeerLocations(getPeerURI(), includeOnlyConnectedLocations);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer => IPeerForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      void Peer::setPeerFilePublic(IPeerFilePublicPtr peerFilePublic)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerFilePublic)

        AutoRecursiveLock lock(*this);
        mPeerFilePublic = peerFilePublic;
        mPeerURI.clear();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer => IPeerForLocation
      #pragma mark

      //-----------------------------------------------------------------------
      PeerPtr Peer::create(
                           AccountPtr inAccount,
                           const char *inPeerURI
                           )
      {
        String peerURI(inPeerURI);
        UseAccountPtr account = inAccount;

        if (!account) return PeerPtr();
        if (!peerURI) return PeerPtr();

        if (!isValid(peerURI)) {
          ZS_LOG_DEBUG(slog("cannot create peer as URI is not valid") + ZS_PARAM("uri", peerURI))
          return PeerPtr();
        }

        PeerPtr existing = account->findExisting(peerURI);
        if (existing) {
          ZS_LOG_TRACE(existing->log("found existing peer object to re-use") + ZS_PARAM("uri", peerURI))
          return existing;
        }

        PeerPtr pThis(new Peer(inAccount, IPeerFilePublicPtr(), peerURI));
        pThis->mThisWeak = pThis;
        pThis->init();

        // check if it already exists in the account
        PeerPtr useThis = account->findExistingOrUse(pThis);
        if (useThis != pThis) {
          // small window in which could have created new object outside of lock - do not inform account of destruction since it was not used
          ZS_LOG_DEBUG(pThis->log("discarding object since one exists already"))
          pThis->mAccount.reset();
        }
        return useThis;
      }

      //-----------------------------------------------------------------------
      AccountPtr Peer::getAccount() const
      {
        return Account::convert(mAccount.lock());
      }

      //-----------------------------------------------------------------------
      ElementPtr Peer::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("Peer");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "peer uri", getPeerURI());
        IHelper::debugAppend(resultEl, "peer file public", (bool)mPeerFilePublic);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Peer::slog(const char *message)
      {
        return Log::Params(message, "Peer");
      }

      //-----------------------------------------------------------------------
      Log::Params Peer::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("Peer");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Peer::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFactory &IPeerFactory::singleton()
      {
        return PeerFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::create(
                                   AccountPtr account,
                                   IPeerFilePublicPtr peerFilePublic
                                   )
      {
        if (this) {}
        return Peer::create(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::getFromSignature(
                                             AccountPtr account,
                                             ElementPtr signedElement
                                             )
      {
        if (this) {}
        return Peer::getFromSignature(account, signedElement);
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::create(
                                   AccountPtr account,
                                   const char *peerURI
                                   )
      {
        if (this) {}
        return Peer::create(account, peerURI);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPeer
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IPeer::toString(PeerFindStates state)
    {
      switch (state)
      {
        case PeerFindState_Pending:   return "Pending";
        case PeerFindState_Idle:      return "Idle";
        case PeerFindState_Finding:   return "Finding";
        case PeerFindState_Completed: return "Complete";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    bool IPeer::isValid(const char *peerURI)
    {
      return internal::Peer::isValid(peerURI);
    }

    //-------------------------------------------------------------------------
    bool IPeer::splitURI(
                         const char *peerURI,
                         String &outDomain,
                         String &outContactID
                         )
    {
      return internal::Peer::splitURI(peerURI, outDomain, outContactID);
    }

    //-------------------------------------------------------------------------
    String IPeer::joinURI(
                          const char *domain,
                          const char *contactID
                          )
    {
      return internal::Peer::joinURI(domain, contactID);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPeer::toDebug(IPeerPtr peer)
    {
      return internal::Peer::toDebug(peer);
    }

    //-------------------------------------------------------------------------
    IPeerPtr IPeer::create(
                           IAccountPtr account,
                           IPeerFilePublicPtr peerFilePublic
                           )
    {
      return internal::IPeerFactory::singleton().create(internal::Account::convert(account), peerFilePublic);
    }

    //-------------------------------------------------------------------------
    IPeerPtr IPeer::create(
                           IAccountPtr account,
                           const char *peerURI
                           )
    {
      return internal::IPeerFactory::singleton().create(internal::Account::convert(account), peerURI);
    }

    //-------------------------------------------------------------------------
    IPeerPtr IPeer::getFromSignature(
                                     IAccountPtr account,
                                     ElementPtr signedElement
                                     )
    {
      return internal::IPeerFactory::singleton().getFromSignature(internal::Account::convert(account), signedElement);
    }
  }
}
