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

#include <openpeer/stack/internal/stack.h>
#include <openpeer/stack/stack.h>

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IRSAPublicKey.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Numeric.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_IMPLEMENT_SUBSYSTEM(openpeer_stack) } }
namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    ZS_DECLARE_TYPEDEF_PTR(internal::Helper, UseHelper)
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

    ZS_DECLARE_USING_PTR(services, IRSAPublicKey)

    using zsLib::string;
    using namespace zsLib::XML;
    using message::IMessageHelper;
    using zsLib::Numeric;
    using internal::Location;

    typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

    ZS_DECLARE_TYPEDEF_PTR(stack::internal::ILocationForMessages, UseLocation)
    ZS_DECLARE_TYPEDEF_PTR(stack::internal::IPeerForMessages, UsePeer)

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark (helpers)
    #pragma mark

    //-----------------------------------------------------------------------
    static void merge(String &result, const String &source, bool overwrite)
    {
      if (source.isEmpty()) return;
      if (result.hasData()) {
        if (!overwrite) return;
      }
      result = source;
    }

    //-----------------------------------------------------------------------
    static void merge(Time &result, const Time &source, bool overwrite)
    {
      if (Time() == source) return;
      if (Time() != result) {
        if (!overwrite) return;
      }
      result = source;
    }

    //---------------------------------------------------------------------
    static Log::Params LocationInfo_slog(const char *message)
    {
      return Log::Params(message, "LocationInfo");
    }

    //---------------------------------------------------------------------
    static Log::Params Server_slog(const char *message)
    {
      return Log::Params(message, "message::Server");
    }

    //---------------------------------------------------------------------
    static Log::Params Token_slog(const char *message)
    {
      return Log::Params(message, "message::Token");
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark Candidate
    #pragma mark

    //-------------------------------------------------------------------------
    Candidate::Candidate() :
      IICESocket::Candidate()
    {
    }

    //-------------------------------------------------------------------------
    Candidate::Candidate(const Candidate &candidate) :
      IICESocket::Candidate(candidate)
    {
      mNamespace = candidate.mNamespace;
      mTransport = candidate.mTransport;

      mToken = candidate.mToken;
    }

    //-------------------------------------------------------------------------
    Candidate::Candidate(const IICESocket::Candidate &candidate) :
      IICESocket::Candidate(candidate)
    {
    }

    //-------------------------------------------------------------------------
    bool Candidate::hasData() const
    {
      bool hasData = IICESocket::Candidate::hasData();
      if (hasData) return true;

      return ((mNamespace.hasData()) ||
              (mTransport.hasData()) ||
              (mProtocols.size() > 0) ||
              (mToken.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::toDebug() const
    {
      ElementPtr resultEl = Element::create("stack::Candidate");

      UseServicesHelper::debugAppend(resultEl, IICESocket::Candidate::toDebug());
      UseServicesHelper::debugAppend(resultEl, "class", mNamespace);

      UseServicesHelper::debugAppend(resultEl, "transport", mTransport);
      UseServicesHelper::debugAppend(resultEl, toDebug(mProtocols));
      UseServicesHelper::debugAppend(resultEl, mToken.toDebug());

      return resultEl;
    }

    //---------------------------------------------------------------------
    Candidate Candidate::create(
                                ElementPtr elem,
                                const char *encryptionPassphrase
                                )
    {
      Candidate ret;
      if (!elem) return ret;

      ElementPtr namespaceEl = elem->findFirstChildElement("namespace");
      ElementPtr transportEl = elem->findFirstChildElement("transport");
      ElementPtr protocolsEl = elem->findFirstChildElement("protocols");
      ElementPtr typeEl = elem->findFirstChildElement("type");
      ElementPtr foundationEl = elem->findFirstChildElement("foundation");
      ElementPtr componentEl = elem->findFirstChildElement("component");
      ElementPtr hostEl = elem->findFirstChildElement("host");
      ElementPtr ipEl = elem->findFirstChildElement("ip");
      ElementPtr portEl = elem->findFirstChildElement("port");
      ElementPtr priorityEl = elem->findFirstChildElement("priority");
      ElementPtr tokenEl = elem->findFirstChildElement("token");
      ElementPtr relatedEl = elem->findFirstChildElement("related");
      ElementPtr relatedIPEl;
      ElementPtr relatedPortEl;
      if (relatedEl) {
        relatedIPEl = relatedEl->findFirstChildElement("ip");
        relatedPortEl = relatedEl->findFirstChildElement("port");
      }

      ret.mNamespace = IMessageHelper::getElementTextAndDecode(namespaceEl);
      ret.mTransport = IMessageHelper::getElementTextAndDecode(transportEl);

      if (protocolsEl) {
      }

      String type = IMessageHelper::getElementTextAndDecode(typeEl);
      if ("host" == type) {
        ret.mType = IICESocket::Type_Local;
      } else if ("srflx" == type) {
        ret.mType = IICESocket::Type_ServerReflexive;
      } else if ("prflx" == type) {
        ret.mType = IICESocket::Type_PeerReflexive;
      } else if ("relay" == type) {
        ret.mType = IICESocket::Type_Relayed;
      }

      ret.mFoundation = IMessageHelper::getElementTextAndDecode(foundationEl);

      if (componentEl) {
        try {
          ret.mComponentID = Numeric<decltype(ret.mComponentID)>(IMessageHelper::getElementText(componentEl));
        } catch(Numeric<decltype(ret.mComponentID)>::ValueOutOfRange &) {
        }
      }

      if ((ipEl) ||
          (hostEl))
      {
        WORD port = 0;
        if (portEl) {
          try {
            port = Numeric<WORD>(IMessageHelper::getElementText(portEl));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
          }
        }

        if (ipEl)
          ret.mIPAddress = IPAddress(IMessageHelper::getElementText(ipEl), port);
        if (hostEl)
          ret.mIPAddress = IPAddress(IMessageHelper::getElementText(hostEl), port);

        ret.mIPAddress.setPort(port);
      }

      if (priorityEl) {
        try {
          ret.mPriority = Numeric<decltype(ret.mPriority)>(IMessageHelper::getElementText(priorityEl));
        } catch(Numeric<decltype(ret.mPriority)>::ValueOutOfRange &) {
        }
      }

      if (relatedIPEl) {
        WORD port = 0;
        if (relatedPortEl) {
          try {
            port = Numeric<WORD>(IMessageHelper::getElementText(relatedPortEl));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
          }
        }

        ret.mRelatedIP = IPAddress(IMessageHelper::getElementText(ipEl), port);
        ret.mRelatedIP.setPort(port);
      }

      ret.mToken = Token::create(tokenEl);

      if ((ret.mToken.mSecretEncrypted.hasData()) &&
          (encryptionPassphrase)) {

        SecureByteBlockPtr accessSeretProof = stack::IHelper::splitDecrypt(*UseServicesHelper::hash(encryptionPassphrase, UseServicesHelper::HashAlgorthm_SHA256), ret.mToken.mSecretEncrypted);
        if (accessSeretProof) {
          ret.mToken.mSecret = UseServicesHelper::convertToString(*accessSeretProof);
          ret.mToken.mSecretEncrypted.clear();
        }
      }

      return ret;
    }

    //---------------------------------------------------------------------
    ElementPtr Candidate::createElement(const char *encryptionPassphrase) const
    {
      ElementPtr candidateEl = IMessageHelper::createElement("candidate");

      if (mNamespace.hasData()) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("namespace", mNamespace));
      }

      if (mTransport.hasData()) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("transport", mTransport));
      }

      ElementPtr protocolsEl = createElement(mProtocols);
      if ((protocolsEl->hasChildren()) ||
          (protocolsEl->hasAttributes())) {
        candidateEl->adoptAsLastChild(protocolsEl);
      }

      const char *typeAsString = NULL;
      switch (mType) {
        case IICESocket::Type_Unknown:          break;
        case IICESocket::Type_Local:            typeAsString = "host"; break;
        case IICESocket::Type_ServerReflexive:  typeAsString = "srflx";break;
        case IICESocket::Type_PeerReflexive:    typeAsString = "prflx"; break;
        case IICESocket::Type_Relayed:          typeAsString = "relay"; break;
      }

      if (typeAsString) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("type", typeAsString));
      }

      if (mFoundation.hasData()) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("foundation", mFoundation));
      }

      if (0 != mComponentID) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("component", string(mIPAddress.getPort())));
      }

      if (!mIPAddress.isEmpty()) {
        if (mToken.hasData()) {
          candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("host", mIPAddress.string(false)));
        } else {
          candidateEl->adoptAsLastChild(IMessageHelper::createElementWithText("ip", mIPAddress.string(false)));
        }
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("port", string(mIPAddress.getPort())));
      }

      if (0 != mPriority) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("priority", string(mPriority)));
      }

      if (!mRelatedIP.isEmpty()) {
        ElementPtr relatedEl = Element::create("related");
        candidateEl->adoptAsLastChild(relatedEl);
        relatedEl->adoptAsLastChild(IMessageHelper::createElementWithText("ip", mRelatedIP.string(false)));
        relatedEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("port", string(mRelatedIP.getPort())));
      }

      if (mToken.hasData()) {
        Token tempToken(mToken);
        if (encryptionPassphrase) {
          tempToken.mSecretEncrypted = stack::IHelper::splitEncrypt(*UseServicesHelper::hash(encryptionPassphrase, UseServicesHelper::HashAlgorthm_SHA256), *UseServicesHelper::convertToBuffer(mToken.mSecret));
          tempToken.mSecret.clear();
        }
        candidateEl->adoptAsLastChild(tempToken.createElement());
      }

      return candidateEl;
    }

    //-------------------------------------------------------------------------
    Candidate::ProtocolList Candidate::createProtocolList(ElementPtr protocolsEl)
    {
      ProtocolList result;
      if (!protocolsEl) return result;

      ElementPtr protocolEl = protocolsEl->findFirstChildElement("protocol");
      while (protocolEl) {
        Protocol protocol = Protocol::create(protocolEl);

        if (protocol.hasData()) {
          result.push_back(protocol);
        }

        protocolEl = protocolEl->findNextSiblingElement("protocol");
      }

      return result;
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::createElement(const ProtocolList &protocols)
    {
      ElementPtr protocolsEl = Element::create("protocols");

      for (ProtocolList::const_iterator iter = protocols.begin(); iter != protocols.end(); ++iter)
      {
        const Protocol &protocol = (*iter);
        if (!protocol.hasData()) continue;

        ElementPtr protocolEl = protocol.createElement();

        if ((!protocolEl->hasChildren()) &&
            (!protocolEl->hasAttributes())) continue;

        protocolsEl->adoptAsLastChild(protocolEl);
      }

      return protocolsEl;
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::toDebug(
                                  const ProtocolList &protocols,
                                  const char *groupingElementName
                                  )
    {
      if (!groupingElementName) groupingElementName = "stack::Candidate::ProtocolList";

      if (protocols.size() < 1) return ElementPtr();

      ElementPtr resultEl = Element::create(groupingElementName);

      for (ProtocolList::const_iterator iter = protocols.begin(); iter != protocols.end(); ++iter)
      {
        const Protocol &protocol = (*iter);
        UseServicesHelper::debugAppend(resultEl, protocol.toDebug());
      }

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark Candidate::Protocol
    #pragma mark

    //-------------------------------------------------------------------------
    bool Candidate::Protocol::hasData() const
    {
      return ((mTransport.hasData()) ||
              (mHost.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::Protocol::toDebug() const
    {
      ElementPtr resultEl = Element::create("stack::Candidate::Protocol");

      UseServicesHelper::debugAppend(resultEl, "transport", mTransport);
      UseServicesHelper::debugAppend(resultEl, "host", mHost);

      return resultEl;
    }

    //---------------------------------------------------------------------
    Candidate::Protocol Candidate::Protocol::create(ElementPtr elem)
    {
      Candidate::Protocol info;
      if (!elem) return info;

      info.mTransport = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("transport"));
      info.mHost = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("host"));

      return info;
    }

    //---------------------------------------------------------------------
    ElementPtr Candidate::Protocol::createElement() const
    {
      ElementPtr protocolEl = IMessageHelper::createElement("protocol");

      if (mTransport.hasData()) {
        protocolEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("transport", mTransport));
      }
      if (mHost.hasData()) {
        protocolEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("host", mHost));
      }

      return protocolEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark LocationInfo
    #pragma mark

    //-------------------------------------------------------------------------
    LocationInfoPtr LocationInfo::create()
    {
      return LocationInfoPtr(new LocationInfo);
    }

    //-------------------------------------------------------------------------
    bool LocationInfo::hasData() const
    {
      return (((bool)mLocation) ||
              (!mIPAddress.isEmpty()) ||
              (mDeviceID.hasData()) ||
              (mUserAgent.hasData()) ||
              (mOS.hasData()) ||
              (mSystem.hasData()) ||
              (mHost.hasData()) ||
              (mCandidatesFinal) ||
              (mCandidatesVersion.hasData()) ||
              (mCandidates.size() > 0));
    }

    //-------------------------------------------------------------------------
    ElementPtr LocationInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("LocationInfo");

      UseServicesHelper::debugAppend(resultEl, ILocation::toDebug(mLocation));
      UseServicesHelper::debugAppend(resultEl, "IP address", !mIPAddress.isEmpty() ? mIPAddress.string() : String());
      UseServicesHelper::debugAppend(resultEl, "device ID", mDeviceID);
      UseServicesHelper::debugAppend(resultEl, "user agent", mUserAgent);
      UseServicesHelper::debugAppend(resultEl, "os", mOS);
      UseServicesHelper::debugAppend(resultEl, "system", mSystem);
      UseServicesHelper::debugAppend(resultEl, "host", mHost);
      UseServicesHelper::debugAppend(resultEl, "candidates final", mCandidatesFinal);
      UseServicesHelper::debugAppend(resultEl, "candidates version", mCandidatesVersion);
      UseServicesHelper::debugAppend(resultEl, "candidates", mCandidates.size());

      return resultEl;
    }

    //---------------------------------------------------------------------
    LocationInfoPtr LocationInfo::create(
                                         ElementPtr elem,
                                         IMessageSourcePtr messageSource,
                                         const char *encryptionPassphrase
                                         )
    {
      if (!elem) return LocationInfoPtr();

      LocationInfoPtr ret = LocationInfo::create();

      String id = IMessageHelper::getAttributeID(elem);

      ElementPtr contact = elem->findFirstChildElement("contact");
      if (contact)
      {
        String peerURI = IMessageHelper::getElementTextAndDecode(contact);
        ret->mLocation = Location::convert(UseLocation::create(messageSource, peerURI, id));
      }

      ElementPtr candidates = elem->findFirstChildElement("candidates");
      if (candidates)
      {
        CandidateList candidateLst;
        ElementPtr candidateEl = candidates->findFirstChildElement("candidate");
        while (candidateEl)
        {
          Candidate c = Candidate::create(candidateEl, encryptionPassphrase);
          candidateLst.push_back(c);

          candidateEl = candidateEl->getNextSiblingElement();
        }

        if (candidateLst.size() > 0)
          ret->mCandidates = candidateLst;
      }

      if (elem->getValue() == "location")
        elem = elem->findFirstChildElement("details");

      if (elem)
      {
        ElementPtr device = elem->findFirstChildElement("device");
        ElementPtr ip = elem->findFirstChildElement("ip");
        ElementPtr ua = elem->findFirstChildElement("userAgent");
        ElementPtr os = elem->findFirstChildElement("os");
        ElementPtr system = elem->findFirstChildElement("system");
        ElementPtr host = elem->findFirstChildElement("host");
        if (device) {
          ret->mDeviceID = IMessageHelper::getAttribute(device, "id");
        }
        if (ip) {
          IPAddress ipOriginal(IMessageHelper::getElementText(ip), 0);

          ret->mIPAddress.mIPAddress = ipOriginal.mIPAddress;
        }
        if (ua) ret->mUserAgent = IMessageHelper::getElementTextAndDecode(ua);
        if (os) ret->mOS = IMessageHelper::getElementTextAndDecode(os);
        if (system) ret->mSystem = IMessageHelper::getElementTextAndDecode(system);
        if (host) ret->mHost = IMessageHelper::getElementTextAndDecode(host);
      }
      return ret;
    }

    //---------------------------------------------------------------------
    ElementPtr LocationInfo::createElement(const char *encryptionPassphrase) const
    {
      if (!mLocation) {
        ZS_LOG_WARNING(Detail, LocationInfo_slog("missing location object in location info"))
        return ElementPtr();
      }

      UseLocationPtr location = Location::convert(mLocation);

      ElementPtr locationEl = IMessageHelper::createElementWithID("location", location->getLocationID());
      ElementPtr detailEl = IMessageHelper::createElement("details");

      if (!mDeviceID.isEmpty()) {
        detailEl->adoptAsLastChild(IMessageHelper::createElementWithID("device", mDeviceID));
      }

      if (!mIPAddress.isAddressEmpty())
      {
        ElementPtr ipEl = IMessageHelper::createElementWithText("ip", mIPAddress.string(false));
        detailEl->adoptAsLastChild(ipEl);
      }

      if (!mUserAgent.isEmpty())
        detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("userAgent", mUserAgent));

      if (!mOS.isEmpty())
        detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("os", mOS));

      if (!mSystem.isEmpty())
        detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("system", mSystem));

      if (!mHost.isEmpty())
        detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("host", mHost));

      UsePeerPtr peer = location->getPeer();
      if (peer) {
        locationEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("contact", peer->getPeerURI()));
      }

      if (detailEl->hasChildren()) {
        locationEl->adoptAsLastChild(detailEl);
      }

      if (mCandidates.size() > 0)
      {
        ElementPtr candidates = IMessageHelper::createElement("candidates");
        locationEl->adoptAsLastChild(candidates);

        for(CandidateList::const_iterator iter = mCandidates.begin(); iter != mCandidates.end(); ++iter)
        {
          const Candidate &candidate = (*iter);
          candidates->adoptAsLastChild(candidate.createElement(encryptionPassphrase));
        }

        locationEl->adoptAsLastChild(candidates);
      }

      return locationEl;
    }

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark message::Server
    #pragma mark

    //-----------------------------------------------------------------------
    bool Server::hasData() const
    {
      return (mID.hasData()) ||
      (mType.hasData()) ||
      (mProtocols.size() > 0) ||
      ((bool)mPublicKey) ||
      (mPriority != 0) ||
      (mWeight != 0) ||
      (mRegion.hasData()) ||
      (Time() != mCreated) ||
      (Time() != mExpires);
    }

    //-----------------------------------------------------------------------
    ElementPtr Server::toDebug() const
    {
      ElementPtr resultEl = Element::create("message::Server");

      UseServicesHelper::debugAppend(resultEl, "id", mID);
      UseServicesHelper::debugAppend(resultEl, "type", mType);
      UseServicesHelper::debugAppend(resultEl, Candidate::toDebug(mProtocols, "message::Server::ProtocolList"));
      UseServicesHelper::debugAppend(resultEl, "public key", (bool)mPublicKey);
      UseServicesHelper::debugAppend(resultEl, "priority", mPriority);
      UseServicesHelper::debugAppend(resultEl, "weight", mWeight);
      UseServicesHelper::debugAppend(resultEl, "region", mRegion);
      UseServicesHelper::debugAppend(resultEl, "created", mCreated);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

      return resultEl;
    }

    //-----------------------------------------------------------------------
    Server Server::create(ElementPtr elem)
    {
      Server ret;

      if (!elem) return ret;

      ret.mID = IMessageHelper::getAttributeID(elem);
      ret.mType = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("type"));

      ElementPtr protocolsEl = elem->findFirstChildElement("protocols");
      ret.mProtocols = Candidate::createProtocolList(protocolsEl);
      ret.mRegion = IMessageHelper::getElementText(elem->findFirstChildElement("region"));
      ret.mCreated = UseServicesHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("created")));
      ret.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("expires")));

      try
      {
        ret.mPublicKey = IRSAPublicKey::load(*UseServicesHelper::convertFromBase64(IMessageHelper::getElementText(elem->findFirstChildElementChecked("key")->findFirstChildElementChecked("x509Data"))));
        try {
          ret.mPriority = Numeric<decltype(ret.mPriority)>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("priority")));
        } catch(Numeric<decltype(ret.mPriority)>::ValueOutOfRange &) {
        }
        try {
          ret.mWeight = Numeric<decltype(ret.mWeight)>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("weight")));
        } catch(Numeric<decltype(ret.mWeight)>::ValueOutOfRange &) {
        }
      }
      catch(CheckFailed &) {
        ZS_LOG_BASIC(Server_slog("check failure"))
      }

      return ret;
    }

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark message::Token
    #pragma mark

    //-----------------------------------------------------------------------
    bool Token::hasData() const
    {
      return ((mID.hasData()) ||
              (mSecret.hasData()) ||
              (mSecretEncrypted.hasData()) ||
              (Time() != mExpires) ||

              (mProof.hasData()) ||
              (mNonce.hasData()) ||
              (mResource.hasData()));
    }

    //-----------------------------------------------------------------------
    ElementPtr Token::toDebug() const
    {
      ElementPtr resultEl = Element::create("message::Token");

      UseServicesHelper::debugAppend(resultEl, "id", mID);
      UseServicesHelper::debugAppend(resultEl, "secret", mSecret);
      UseServicesHelper::debugAppend(resultEl, "secret encrypted", mSecretEncrypted);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

      UseServicesHelper::debugAppend(resultEl, "proof", mProof);
      UseServicesHelper::debugAppend(resultEl, "nonce", mNonce);
      UseServicesHelper::debugAppend(resultEl, "resource", mResource);

      return resultEl;
    }

    //-----------------------------------------------------------------------
    void Token::mergeFrom(
                          const Token &source,
                          bool overwriteExisting
                          )
    {
      merge(mID, source.mID, overwriteExisting);
      merge(mSecret, source.mSecret, overwriteExisting);
      merge(mExpires, source.mExpires, overwriteExisting);

      merge(mProof, source.mProof, overwriteExisting);
      merge(mNonce, source.mNonce, overwriteExisting);
      merge(mResource, source.mResource, overwriteExisting);
    }

    //-----------------------------------------------------------------------
    Token Token::create(ElementPtr elem)
    {
      Token info;

      if (!elem) return info;

      info.mID = IMessageHelper::getAttributeID(elem);
      info.mSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("secret"));
      info.mSecretEncrypted = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("secretEncrypted"));
      info.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("expires")));

      info.mProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("proof"));
      info.mNonce = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("nonce"));
      info.mResource = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("resource"));

      return info;
    }

    //-----------------------------------------------------------------------
    Token Token::create(
                        const String &masterSecret,
                        const String &associatedID,
                        Duration validDuration
                        )
    {
      Token info;
      info.mExpires = zsLib::now() + validDuration;

      String id = UseServicesHelper::randomString(32*8/5);
      String expires = UseServicesHelper::timeToString(info.mExpires);
      String verification = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(masterSecret), "validation:" + id + ":" + expires + ":" + associatedID));
      String secret = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(masterSecret), "secret:" + id + ":" + expires + ":" + associatedID));

      info.mID = id + "-" + associatedID + "-" + expires + "-" + verification;
      info.mSecret = secret;

      ZS_LOG_TRACE(Token_slog("created token from master secret") + ZS_PARAM("master", masterSecret) + ZS_PARAM("verification", verification) + ZS_PARAM("secret", secret) + info.toDebug())

      return info;
    }

    //-----------------------------------------------------------------------
    Token Token::createProof(
                             const char *resource,
                             Duration validDuration
                             )
    {
      Token info;

      if (!hasData()) return info;

      info.mID = mID;
      info.mResource = String(resource);
      info.mNonce = UseServicesHelper::randomString(16*8/5);
      info.mExpires = zsLib::now() + validDuration;

      // `<proof>` = hex(hmac(`<token-secret>`, "proof:" + `<token-id>` + ":" + `<token-nonce>` + ":" + `<token-expires>` + ":" + `<token-resource>`))
      info.mProof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mSecret), "proof:" + info.mID + ":" + info.mNonce + ":" + UseServicesHelper::timeToString(info.mExpires) + ":" + info.mResource));

      return info;
    }

    //-----------------------------------------------------------------------
    ElementPtr Token::createElement() const
    {
      ElementPtr tokenEl = IMessageHelper::createElementWithID("token", mID);

      if (mSecret.hasData()) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("secret", mSecret));
      }

      if (mSecretEncrypted.hasData()) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("secretEncrypted", mSecret));
      }

      if (mProof.hasData()) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("proof", mProof));
      }

      if (mNonce.hasData()) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("nonce", mProof));
      }

      if (mResource.hasData()) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("resource", mResource));
      }

      if (Time() != mExpires) {
        tokenEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
      }

      return tokenEl;
    }

    //-----------------------------------------------------------------------
    bool Token::validate(const Token &proofToken) const
    {
      Time now = zsLib::now();

      if ((Time() != mExpires) &&
          (now > mExpires)) {
        ZS_LOG_WARNING(Trace, Token_slog("token has expired") + ZS_PARAM("expires", mExpires) + ZS_PARAM("now", now) + toDebug() + proofToken.toDebug())
        return false;
      }

      if ((Time() != proofToken.mExpires) &&
          (now > proofToken.mExpires)) {
        ZS_LOG_WARNING(Trace, Token_slog("token id has expired") + ZS_PARAM("expires", proofToken.mExpires) + ZS_PARAM("now", now) + toDebug() + proofToken.toDebug())
        return false;
      }

      // `<proof>` = hex(hmac(`<token-secret>`, "proof:" + `<token-id>` + ":" + `<token-nonce>` + ":" + `<token-expires>` + ":" + `<token-resource>`))
      String proof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mSecret), "proof:" + mID + ":" + mNonce + ":" + UseServicesHelper::timeToString(mExpires) + ":" + mResource));

      if (proof != proofToken.mProof) {
        ZS_LOG_WARNING(Trace, Token_slog("token proof did not validate") + ZS_PARAM("calculated proof", proof) + toDebug() + proofToken.toDebug())
        return false;
      }

      return true;
    }

    //-----------------------------------------------------------------------
    bool Token::validate(
                         const String &masterSecret,
                         String &outAssociatedID
                         ) const
    {
      outAssociatedID.clear();

      UseServicesHelper::SplitMap split;

      UseServicesHelper::split(mID, split, '-');
      if (split.size() != 4) {
        ZS_LOG_WARNING(Trace, Token_slog("token did not split") + toDebug())
        return false;
      }

      String id = split[0];
      String associatedID = split[1];
      String expires = split[2];
      String verification = split[3];

      Time actualExpires = UseServicesHelper::stringToTime(expires);

      String calculatedVerification = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(masterSecret), "validation:" + id + ":" + expires + ":" + associatedID));
      String calculatedSecret = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(masterSecret), "secret:" + id + ":" + expires + ":" + associatedID));

      if (verification != calculatedVerification) {
        ZS_LOG_WARNING(Trace, Token_slog("token id did not validate") + ZS_PARAM("master", masterSecret) + ZS_PARAM("calculated verification", calculatedVerification) + ZS_PARAM("calculated secret", calculatedSecret) + toDebug())
        return false;
      }

      Token master;
      master.mID = mID;
      master.mSecret = calculatedSecret;
      master.mExpires = actualExpires;

      bool validated = master.validate(*this);
      if (!validated) return false;

      outAssociatedID = associatedID;
      return true;
    }
  }
}
