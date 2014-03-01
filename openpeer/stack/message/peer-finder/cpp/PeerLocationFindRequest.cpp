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

#include <openpeer/stack/message/peer-finder/PeerLocationFindRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Account.h>

#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IHelper.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IDHKeyDomain.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>


#define OPENPEER_STACK_MESSAGE_PEER_LOCATION_FIND_REQUEST_LIFETIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;
      typedef services::IHelper::SplitMap SplitMap;

      namespace peer_finder
      {
        ZS_DECLARE_TYPEDEF_PTR(stack::internal::IAccountForMessages, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(stack::internal::ILocationForMessages, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(stack::internal::IPeerForMessages, UsePeer)

        using zsLib::Seconds;
        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;
        using namespace stack::internal;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "PeerLocationFindRequest");
        }

        //---------------------------------------------------------------------
        PeerLocationFindRequestPtr PeerLocationFindRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<PeerLocationFindRequest>(message);
        }

        //---------------------------------------------------------------------
        PeerLocationFindRequest::PeerLocationFindRequest() :
          mCreated(zsLib::timeSinceEpoch(Seconds(zsLib::timeSinceEpoch(zsLib::now()).total_seconds())))
        {
        }

        //---------------------------------------------------------------------
        PeerLocationFindRequestPtr PeerLocationFindRequest::create(
                                                                   ElementPtr root,
                                                                   IMessageSourcePtr messageSource
                                                                   )
        {
          PeerLocationFindRequestPtr ret(new PeerLocationFindRequest);
          IMessageHelper::fill(*ret, root, messageSource);

          UseLocationPtr messageLocation = UseLocation::convert(messageSource);
          if (!messageLocation) {
            ZS_LOG_ERROR(Detail, slog("message source was not a known location"))
            return PeerLocationFindRequestPtr();
          }

          UseAccountPtr account = messageLocation->getAccount();
          if (!account) {
            ZS_LOG_ERROR(Detail, slog("account object is gone"))
            return PeerLocationFindRequestPtr();
          }

          IPeerFilesPtr peerFiles = account->getPeerFiles();
          if (!peerFiles) {
            ZS_LOG_ERROR(Detail, slog("peer files not found in account"))
            return PeerLocationFindRequestPtr();
          }

          IPeerFilePrivatePtr peerFilePrivate = peerFiles->getPeerFilePrivate();
          if (!peerFilePrivate) {
            ZS_LOG_ERROR(Detail, slog("peer file private was null"))
            return PeerLocationFindRequestPtr();
          }
          IPeerFilePublicPtr peerFilePublic = peerFiles->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_ERROR(Detail, slog("peer file public was null"))
            return PeerLocationFindRequestPtr();
          }

          UseLocationPtr localLocation = UseLocation::getForLocal(Account::convert(account));
          if (!localLocation) {
            ZS_LOG_ERROR(Detail, slog("could not obtain local location"))
            return PeerLocationFindRequestPtr();
          }

          try {
            ElementPtr findProofEl = root->findFirstChildElementChecked("findProofBundle")->findFirstChildElementChecked("findProof");

            String clientNonce = findProofEl->findFirstChildElementChecked("nonce")->getText();

#define WARNING_CREATED_SHOULD_BE_MANDITORY 1
#define WARNING_CREATED_SHOULD_BE_MANDITORY 2

            ElementPtr createdEl = findProofEl->findFirstChildElement("created");
            if (createdEl) {
              ret->mCreated = IHelper::stringToTime(createdEl->getText());
            } else {
              ret->mCreated = Time();
            }

            String peerURI = findProofEl->findFirstChildElementChecked("find")->getText();

            if (peerURI != localLocation->getPeerURI()) {
              ZS_LOG_ERROR(Detail, slog("find was not intended for this peer") + ZS_PARAM("find peer URI", peerURI) + UseLocation::toDebug(localLocation))
              return PeerLocationFindRequestPtr();
            }

            String findSecretProof = findProofEl->findFirstChildElementChecked("findSecretProof")->getText();
            Time expires = IHelper::stringToTime(findProofEl->findFirstChildElementChecked("findSecretProofExpires")->getText());

            String findSecret = peerFilePublic->getFindSecret();
            String calculatedFindSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(findSecret), "proof:" + clientNonce + ":" + IHelper::timeToString(expires)));

            if (calculatedFindSecretProof != findSecretProof) {
              ZS_LOG_WARNING(Detail, slog("calculated find secret proof did not match request") + ZS_PARAM("calculated", calculatedFindSecretProof) + ZS_PARAM("request", findSecretProof))
              return PeerLocationFindRequestPtr();
            }

            if (zsLib::now() > expires) {
              ZS_LOG_WARNING(Detail, slog("request expired") + ZS_PARAM("expires", expires) + ZS_PARAM("now", ::zsLib::now()))
              return PeerLocationFindRequestPtr();
            }

            ret->mContext = IMessageHelper::getElementTextAndDecode(findProofEl->findFirstChildElementChecked("context"));

            String peerSecretEncrypted = IMessageHelper::getElementTextAndDecode(findProofEl->findFirstChildElement("peerSecretEncrypted"));

            if (peerSecretEncrypted.hasData()) {
              SplitMap splits;

              IHelper::split(peerSecretEncrypted, splits, ':');

              String namespaceStr;
              String passPhraseEncrypted;

              if (splits.size() == 1) {
                passPhraseEncrypted = (*(splits.find(0))).second;
              }
              if (splits.size() > 1) {
                namespaceStr = (*(splits.find(0))).second;
                passPhraseEncrypted = (*(splits.find(1))).second;
              }
              if (passPhraseEncrypted) {
                ret->mPeerSecret = IHelper::convertToString(*peerFilePrivate->decrypt(*IHelper::convertFromBase64(passPhraseEncrypted)));
                ZS_LOG_TRACE(slog("decrypted peer secret") + ZS_PARAM("passphrase", ret->mPeerSecret))
              }
              if (splits.size() >= 4) {
                IDHKeyDomain::KeyDomainPrecompiledTypes precompiledKey = IDHKeyDomain::fromNamespace(IHelper::convertToString(*IHelper::convertFromBase64(namespaceStr)));
                if (precompiledKey != IDHKeyDomain::KeyDomainPrecompiledType_Unknown) {
                  ret->mDHKeyDomain = IDHKeyDomain::loadPrecompiled(precompiledKey);
                  ZS_LOG_TRACE(slog("found DH key domain") + ZS_PARAM("namespace", IDHKeyDomain::toNamespace(precompiledKey)))
                }

                String staticPublicKeyStr = (*(splits.find(2))).second;
                String ephemeralPublicKeyStr = (*(splits.find(3))).second;

                SecureByteBlockPtr staticPublicKey = IHelper::convertFromBase64(staticPublicKeyStr);
                SecureByteBlockPtr ephemeralPublicKey = IHelper::convertFromBase64(ephemeralPublicKeyStr);

                if ((IHelper::hasData(staticPublicKey)) &&
                    (IHelper::hasData(ephemeralPublicKey))) {
                  ret->mDHPublicKey = IDHPublicKey::load(*staticPublicKey, *ephemeralPublicKey);
                  ZS_LOG_TRACE(slog("found DH public key") + IDHPublicKey::toDebug(ret->mDHPublicKey))
                }
              }
            }

            if (ret->mPeerSecret.isEmpty()) {
              ZS_LOG_WARNING(Detail, slog("peer secret failed to decrypt"))
              return PeerLocationFindRequestPtr();
            }

            ZS_LOG_TRACE(slog("decrypted peer secret") + ZS_PARAM("secret", ret->mPeerSecret))

            ret->mICEUsernameFrag = IMessageHelper::getElementTextAndDecode(findProofEl->findFirstChildElementChecked("iceUsernameFrag"));
            String icePasswordEncrypted = IMessageHelper::getElementTextAndDecode(findProofEl->findFirstChildElementChecked("icePasswordEncrypted"));
            if (icePasswordEncrypted.hasData()) {
              SecureByteBlockPtr icePassword = stack::IHelper::splitDecrypt(*IHelper::hash(ret->mPeerSecret, IHelper::HashAlgorthm_SHA256), icePasswordEncrypted);
              if (icePassword) {
                ret->mICEPassword = IHelper::convertToString(*icePassword);
              }
            }

            ElementPtr locationEl = findProofEl->findFirstChildElement("location");
            if (locationEl) {
              ret->mLocationInfo = internal::MessageHelper::createLocation(locationEl, messageSource, ret->mPeerSecret);
            }

            UseLocationPtr location;
            if (ret->mLocationInfo) {
              location = Location::convert(ret->mLocationInfo->mLocation);
            }

            if (!location) {
              ZS_LOG_ERROR(Detail, slog("missing location information in find request"))
              return PeerLocationFindRequestPtr();
            }

            if (!location->getPeer()) {
              ZS_LOG_ERROR(Detail, slog("missing location peer information in find request"))
              return PeerLocationFindRequestPtr();
            }

            UsePeerPtr peer = IPeerForMessages::getFromSignature(Account::convert(account), findProofEl);
            if (!peer) {
              ZS_LOG_WARNING(Detail, slog("peer object failed to create from signature"))
              return PeerLocationFindRequestPtr();
            }

            if (location->getPeerURI() != peer->getPeerURI()) {
              ZS_LOG_WARNING(Detail, slog("location peer is not same as signature peer"))
              return PeerLocationFindRequestPtr();
            }

            ElementPtr signatureEl;
            IHelper::getSignatureInfo(findProofEl, &signatureEl);

            if (signatureEl) {
              ret->mRequestfindProofBundleDigestValue = signatureEl->findFirstChildElementChecked("digestValue")->getTextDecoded();
            }

            if (peer->getPeerFilePublic()) {
              // know the peer file public so this should verify the signature
              if (!peer->verifySignature(findProofEl)) {
                ZS_LOG_WARNING(Detail, slog("signature on request did not verify"))
                return PeerLocationFindRequestPtr();
              }
              get(ret->mDidVerifySignature) = true;
            }

            ElementPtr excludeEl = root->findFirstChildElement("exclude");
            if (excludeEl) {
              ElementPtr locationsEl = excludeEl->findFirstChildElement("locations");
              if (locationsEl) {
                ExcludedLocationList excludeList;
                ElementPtr locationEl = locationsEl->findFirstChildElement("location");
                while (locationEl)
                {
                  String id = IMessageHelper::getAttributeID(locationEl);
                  excludeList.push_back(id);

                  locationEl = locationEl->findNextSiblingElement("location");
                }

                if (excludeList.size() > 0)
                  ret->mExcludedLocations = excludeList;
              }
            }
          } catch(CheckFailed &) {
            ZS_LOG_WARNING(Detail, slog("expected element is missing"))
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool PeerLocationFindRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_RequestfindProofBundleDigestValue: return mRequestfindProofBundleDigestValue.hasData();
            case AttributeType_CreatedTimestamp:                  return Time() != mCreated;
            case AttributeType_FindPeer:                          return (bool)mFindPeer;
            case AttributeType_Context:                           return mContext.hasData();
            case AttributeType_PeerSecret:                        return mPeerSecret.hasData();
            case AttributeType_DHKeyDomain:                       return (((bool)mDHKeyDomain) || ((bool)mDHPrivateKey));
            case AttributeType_DHPrivateKey:                      return (bool)mDHPrivateKey;
            case AttributeType_DHPublicKey:                       return (bool)mDHPublicKey;
            case AttributeType_ICEUsernameFrag:                   return mICEUsernameFrag.hasData();
            case AttributeType_ICEPassword:                       return mICEPassword.hasData();
            case AttributeType_LocationInfo:                      return mLocationInfo ? mLocationInfo->hasData() : false;
            case AttributeType_ExcludedLocations:                 return (mExcludedLocations.size() > 0);
            case AttributeType_PeerFiles:                         return (bool)mPeerFiles;
            default:                                              break;
          }
          return false;
        }
        //---------------------------------------------------------------------
        IDHKeyDomainPtr PeerLocationFindRequest::dhKeyDomain() const
        {
          if (mDHKeyDomain) return mDHKeyDomain;
          if (!mDHPrivateKey) return IDHKeyDomainPtr();
          return mDHPrivateKey->getKeyDomain();
        }

        //---------------------------------------------------------------------
        DocumentPtr PeerLocationFindRequest::encode()
        {
          if (!mPeerFiles) {
            ZS_LOG_ERROR(Detail, slog("peer files was null"))
            return DocumentPtr();
          }

          IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();
          if (!peerFilePrivate) {
            ZS_LOG_ERROR(Detail, slog("peer file private was null"))
            return DocumentPtr();
          }
          IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_ERROR(Detail, slog("peer file public was null"))
            return DocumentPtr();
          }

          String clientNonce = IHelper::convertToHex(*IHelper::random(16));

          Time expires = zsLib::now() + Duration(Seconds(OPENPEER_STACK_MESSAGE_PEER_LOCATION_FIND_REQUEST_LIFETIME_IN_SECONDS));

          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr root = ret->getFirstChildElement();

          ElementPtr findProofBundleEl = Element::create("findProofBundle");
          ElementPtr findProofEl = Element::create("findProof");

          findProofEl->adoptAsLastChild(IMessageHelper::createElementWithText("nonce", clientNonce));

#define WARNING_CREATED_SHOULD_BE_MANDITORY 1
#define WARNING_CREATED_SHOULD_BE_MANDITORY 2
//          if (hasAttribute(AttributeType_CreatedTimestamp)) {
//            findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("created", IHelper::timeToString(mCreated)));
//          }

          if (mFindPeer) {
            findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("find", mFindPeer->getPeerURI()));
          }

          if (mFindPeer) {
            IPeerFilePublicPtr remotePeerFilePublic = mFindPeer->getPeerFilePublic();
            if (remotePeerFilePublic) {
              String findSecret = remotePeerFilePublic->getFindSecret();
              if (findSecret.length() > 0) {
                String findSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(findSecret), "proof:" + clientNonce + ":" + IHelper::timeToString(expires)));
                findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("findSecretProof", findSecretProof));
                findProofEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("findSecretProofExpires", IHelper::timeToString(expires)));
              }

              if (hasAttribute(AttributeType_Context)) {
                findProofEl->adoptAsLastChild(IMessageHelper::createElementWithText("context", mContext));
              }

              if (hasAttribute(AttributeType_PeerSecret)) {

                ZS_LOG_TRACE(slog("encrypting peer secret") + ZS_PARAM("passphrase", mPeerSecret))

                String peerSecretEncrypted = IHelper::convertToBase64(*remotePeerFilePublic->encrypt(* IHelper::convertToBuffer(mPeerSecret)));

                if ((hasAttribute(AttributeType_DHPublicKey)) &&
                    (hasAttribute(AttributeType_DHKeyDomain))) {

                  IDHKeyDomain::KeyDomainPrecompiledTypes type = dhKeyDomain()->getPrecompiledType();
                  if (IDHKeyDomain::KeyDomainPrecompiledType_Unknown != type) {
                    ZS_LOG_TRACE(slog("adding DH key domain") + ZS_PARAM("namespace", IDHKeyDomain::toNamespace(type)))

                    String keyDomainStr = IHelper::convertToBase64(*IHelper::convertToBuffer(IDHKeyDomain::toNamespace(type)));

                    ZS_LOG_TRACE(slog("adding DH public key") + IDHPublicKey::toDebug(mDHPublicKey))

                    SecureByteBlock staticPublicKey;
                    SecureByteBlock ephemeralPublicKey;
                    mDHPublicKey->save(&staticPublicKey, &ephemeralPublicKey);

                    peerSecretEncrypted = keyDomainStr + ":" + peerSecretEncrypted + ":" + IHelper::convertToBase64(staticPublicKey) + ":" + IHelper::convertToBase64(ephemeralPublicKey);
                  }
                }

                findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("peerSecretEncrypted", peerSecretEncrypted));
              }
            }
          }

          if (hasAttribute(AttributeType_ICEUsernameFrag)) {
            findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("iceUsernameFrag", mICEUsernameFrag));
          }
          if (hasAttribute(AttributeType_ICEPassword)) {
            String icePassword = stack::IHelper::splitEncrypt(*IHelper::hash(mPeerSecret, IHelper::HashAlgorthm_SHA256), *IHelper::convertToBuffer(mICEPassword));
            findProofEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("icePasswordEncrypted", icePassword));
          }

          if (hasAttribute(AttributeType_LocationInfo)) {
            ElementPtr locationEl = internal::MessageHelper::createElement(*mLocationInfo, mPeerSecret);
            findProofEl->adoptAsLastChild(locationEl);
          }

          findProofBundleEl->adoptAsLastChild(findProofEl);
          peerFilePrivate->signElement(findProofEl);
          root->adoptAsLastChild(findProofBundleEl);

          ElementPtr signatureEl;
          IHelper::getSignatureInfo(findProofEl, &signatureEl);

          if (signatureEl) {
            mRequestfindProofBundleDigestValue = signatureEl->findFirstChildElementChecked("digestValue")->getTextDecoded();
          }

          if (hasAttribute(AttributeType_ExcludedLocations))
          {
            ElementPtr excludeEl = IMessageHelper::createElement("exclude");
            ElementPtr locationsEl = IMessageHelper::createElement("locations");
            excludeEl->adoptAsLastChild(locationsEl);
            root->adoptAsLastChild(excludeEl);

            for (ExcludedLocationList::const_iterator it = mExcludedLocations.begin(); it != mExcludedLocations.end(); ++it)
            {
              const String &location = (*it);
              locationsEl->adoptAsLastChild(IMessageHelper::createElementWithID("location", location));
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        PeerLocationFindRequestPtr PeerLocationFindRequest::create()
        {
          PeerLocationFindRequestPtr ret(new PeerLocationFindRequest);
          return ret;
        }

      }
    }
  }
}
