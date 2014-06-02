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

#include <openpeer/stack/internal/stack.h>
#include <openpeer/stack/stack.h>

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

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
    using internal::Helper;
    typedef services::IHelper OPIHelper;
    using zsLib::string;
    using namespace zsLib::XML;
    using message::IMessageHelper;
    using zsLib::Numeric;
    using internal::Location;

    ZS_DECLARE_TYPEDEF_PTR(stack::internal::ILocationForMessages, UseLocation)
    ZS_DECLARE_TYPEDEF_PTR(stack::internal::IPeerForMessages, UsePeer)

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark (helpers)
    #pragma mark

    //---------------------------------------------------------------------
    static Log::Params LocationInfo_slog(const char *message)
    {
      return Log::Params(message, "LocationInfo");
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

      mAccessToken = candidate.mAccessToken;
      mAccessSecretProof = candidate.mAccessSecretProof;
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
              (mAccessToken.hasData()) ||
              (mAccessSecretProof.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::toDebug() const
    {
      ElementPtr resultEl = Element::create("stack::Candidate");

      OPIHelper::debugAppend(resultEl, IICESocket::Candidate::toDebug());
      OPIHelper::debugAppend(resultEl, "class", mNamespace);

      OPIHelper::debugAppend(resultEl, "transport", mTransport);
      OPIHelper::debugAppend(resultEl, "access token", mAccessToken);
      OPIHelper::debugAppend(resultEl, "access secret proof", mAccessSecretProof);

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
      ElementPtr typeEl = elem->findFirstChildElement("type");
      ElementPtr foundationEl = elem->findFirstChildElement("foundation");
      ElementPtr componentEl = elem->findFirstChildElement("component");
      ElementPtr hostEl = elem->findFirstChildElement("host");
      ElementPtr ipEl = elem->findFirstChildElement("ip");
      ElementPtr portEl = elem->findFirstChildElement("port");
      ElementPtr priorityEl = elem->findFirstChildElement("priority");
      ElementPtr accessTokenEl = elem->findFirstChildElement("accessToken");
      ElementPtr accessSecretProofEncryptedEl = elem->findFirstChildElement("accessSecretProofEncrypted");
      ElementPtr relatedEl = elem->findFirstChildElement("related");
      ElementPtr relatedIPEl;
      ElementPtr relatedPortEl;
      if (relatedEl) {
        relatedIPEl = relatedEl->findFirstChildElement("ip");
        relatedPortEl = relatedEl->findFirstChildElement("port");
      }

      ret.mNamespace = IMessageHelper::getElementTextAndDecode(namespaceEl);
      ret.mTransport = IMessageHelper::getElementTextAndDecode(transportEl);

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

      ret.mAccessToken = IMessageHelper::getElementTextAndDecode(accessTokenEl);

      String accessSecretProofEncrypted = IMessageHelper::getElementTextAndDecode(accessSecretProofEncryptedEl);
      if ((accessSecretProofEncrypted.hasData()) &&
          (encryptionPassphrase)) {

        SecureByteBlockPtr accessSeretProof = stack::IHelper::splitDecrypt(*OPIHelper::hash(encryptionPassphrase, OPIHelper::HashAlgorthm_SHA256), accessSecretProofEncrypted);
        if (accessSeretProof) {
          ret.mAccessSecretProof = OPIHelper::convertToString(*accessSeretProof);
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
        if (mAccessToken.hasData()) {
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

      if (mAccessToken.hasData()) {
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accessToken", mAccessToken));
      }

      if ((mAccessSecretProof.hasData()) &&
          (encryptionPassphrase)) {
        String accessSecretProofEncrypted = stack::IHelper::splitEncrypt(*OPIHelper::hash(encryptionPassphrase, OPIHelper::HashAlgorthm_SHA256), *OPIHelper::convertToBuffer(mAccessSecretProof));
        candidateEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accessSecretProofEncrypted", accessSecretProofEncrypted));
      }

      return candidateEl;
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

      OPIHelper::debugAppend(resultEl, ILocation::toDebug(mLocation));
      OPIHelper::debugAppend(resultEl, "IP address", !mIPAddress.isEmpty() ? mIPAddress.string() : String());
      OPIHelper::debugAppend(resultEl, "device ID", mDeviceID);
      OPIHelper::debugAppend(resultEl, "user agent", mUserAgent);
      OPIHelper::debugAppend(resultEl, "os", mOS);
      OPIHelper::debugAppend(resultEl, "system", mSystem);
      OPIHelper::debugAppend(resultEl, "host", mHost);
      OPIHelper::debugAppend(resultEl, "candidates final", mCandidatesFinal);
      OPIHelper::debugAppend(resultEl, "candidates version", mCandidatesVersion);
      OPIHelper::debugAppend(resultEl, "candidates", mCandidates.size());

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

  }
}
