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

#include <openpeer/stack/internal/stack_PeerFiles.h>
#include <openpeer/stack/internal/stack_PeerFilePublic.h>
#include <openpeer/stack/internal/stack_PeerFilePrivate.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Cache.h>

#include <openpeer/stack/IPeer.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IRSAPublicKey.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>

#include <cryptopp/sha.h>

#define OPENPEER_STACK_PEER_FILE_PUBLIC_STORE_CACHE_DURATION_IN_HOURS ((24)*365)
#define OPENPEER_STACK_PEER_FILE_PUBLIC_STORE_CACHE_NAMESPACE "https://meta.openpeer.org/caching/peer-file-public/"


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_TYPEDEF_PTR(ICacheForServices, UseCache)

      using services::IHelper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      typedef CryptoPP::SHA256 SHA256;


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "PeerFilePublic");
      }

      //-----------------------------------------------------------------------
      static String getCookieName(const String &peerURI)
      {
        if (!peerURI.hasData()) return String();

        String domain;
        String contactID;
        if (!IPeer::splitURI(peerURI, domain, contactID)) {
          ZS_LOG_WARNING(Detail, slog("invalid peer URI") + ZS_PARAM("uri", peerURI))
          return String();
        }

        String domainHashed = IHelper::convertToHex(*IHelper::hash(domain));

        String cookieName = OPENPEER_STACK_PEER_FILE_PUBLIC_STORE_CACHE_NAMESPACE + domainHashed + "/" + contactID;
        return cookieName;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePublicForPeerFiles
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerFilePublicForPeerFiles::toDebug(ForPeerFilesPtr peerFilePublic)
      {
        return PeerFilePublic::toDebug(PeerFilePublic::convert(peerFilePublic));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePublicForPeerFilePrivate
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerFilePublicForPeerFilePrivate::toDebug(ForPeerFilePrivatePtr peerFilePublic)
      {
        return PeerFilePublic::toDebug(PeerFilePublic::convert(peerFilePublic));
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicForPeerFilePrivate::createFromPublicKey(
                                                                               PeerFilesPtr peerFiles,
                                                                               DocumentPtr publicDoc,
                                                                               IRSAPublicKeyPtr publicKey,
                                                                               const String &peerURI
                                                                               )
      {
        return IPeerFilePublicFactory::singleton().createFromPublicKey(peerFiles, publicDoc, publicKey, peerURI);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicForPeerFilePrivate::loadFromElement(
                                                                           PeerFilesPtr peerFiles,
                                                                           DocumentPtr publicDoc
                                                                           )
      {
        return IPeerFilePublicFactory::singleton().loadFromElement(peerFiles, publicDoc);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePublic
      #pragma mark

      //-----------------------------------------------------------------------
      PeerFilePublic::PeerFilePublic()
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void PeerFilePublic::init()
      {
      }

      //-----------------------------------------------------------------------
      PeerFilePublic::~PeerFilePublic()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::convert(IPeerFilePublicPtr peerFilePublic)
      {
        return dynamic_pointer_cast<PeerFilePublic>(peerFilePublic);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::convert(ForPeerFilesPtr peerFilePublic)
      {
        return dynamic_pointer_cast<PeerFilePublic>(peerFilePublic);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::convert(ForPeerFilePrivatePtr peerFilePublic)
      {
        return dynamic_pointer_cast<PeerFilePublic>(peerFilePublic);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePublic => IPeerFilePublic
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PeerFilePublic::toDebug(IPeerFilePublicPtr peerFilePublic)
      {
        if (!peerFilePublic) return ElementPtr();
        return PeerFilePublic::convert(peerFilePublic)->toDebug();
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::loadFromElement(ElementPtr publicPeerRootElement)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!publicPeerRootElement)

        PeerFilePublicPtr pThis(new PeerFilePublic);
        pThis->mThisWeak = pThis;
        pThis->mDocument = Document::create();
        pThis->mDocument->adoptAsFirstChild(publicPeerRootElement->clone());
        pThis->init();

        if (!pThis->load()) {
          ZS_LOG_WARNING(Detail, pThis->log("failed to load public peer file"))
          return PeerFilePublicPtr();
        }

        return pThis;
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::loadFromCache(const char *inPeerURI)
      {
        String peerURI(inPeerURI);

        String cookieName = getCookieName(peerURI);
        if (!cookieName.hasData()) return PeerFilePublicPtr();

        String peerFilePublicStr = UseCache::fetch(cookieName);

        if (!peerFilePublicStr.hasData()) {
          ZS_LOG_TRACE(slog("peer file was not cached") + ZS_PARAM("cookie", cookieName) + ZS_PARAM("uri", peerURI))
          return PeerFilePublicPtr();
        }

        ElementPtr rootEl = IHelper::toJSON(peerFilePublicStr);
        if (!rootEl) {
          ZS_LOG_TRACE(slog("peer file previously cached was not valid") + ZS_PARAM("cookie", cookieName) + ZS_PARAM("uri", peerURI))
          UseCache::clear(cookieName);
          return PeerFilePublicPtr();
        }

        PeerFilePublicPtr pThis(new PeerFilePublic);
        pThis->mThisWeak = pThis;
        pThis->mDocument = Document::create();
        pThis->mDocument->adoptAsFirstChild(rootEl);
        pThis->init();

        if (!pThis->load()) {
          ZS_LOG_WARNING(Detail, pThis->log("failed to load public peer file"))
          UseCache::clear(cookieName);
          return PeerFilePublicPtr();
        }

        return pThis;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr PeerFilePublic::saveToElement() const
      {
        if (!mDocument) return ElementPtr();
        ElementPtr rootEl = mDocument->getFirstChildElement();
        if (!rootEl) return ElementPtr();
        return rootEl->clone()->toElement();
      }

      //-----------------------------------------------------------------------
      String PeerFilePublic::getPeerURI() const
      {
        return mPeerURI;
      }

      //-----------------------------------------------------------------------
      Time PeerFilePublic::getCreated() const
      {
        ElementPtr sectionAEl = findSection("A");
        if (!sectionAEl) {
          ZS_LOG_WARNING(Detail, log("failed to find section A in public peer file") + ZS_PARAM("peer URI", mPeerURI))
          return Time();
        }
        try {
          return IHelper::stringToTime(sectionAEl->findFirstChildElementChecked("created")->getTextDecoded());
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("failed to find created") + ZS_PARAM("peer URI", mPeerURI))
        }
        return Time();
      }

      //-----------------------------------------------------------------------
      Time PeerFilePublic::getExpires() const
      {
        ElementPtr sectionAEl = findSection("A");
        if (!sectionAEl) {
          ZS_LOG_WARNING(Detail, log("failed to find section A in public peer file") + ZS_PARAM("peer URI", mPeerURI))
          return Time();
        }
        try {
          return IHelper::stringToTime(sectionAEl->findFirstChildElementChecked("expires")->getTextDecoded());
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("failed to find expires") + ZS_PARAM("peer URI", mPeerURI))
        }
        return Time();
      }

      //-----------------------------------------------------------------------
      String PeerFilePublic::getFindSecret() const
      {
        ElementPtr sectionBEl = findSection("B");
        if (!sectionBEl) {
          ZS_LOG_WARNING(Detail, log("failed to find section B in public peer file") + ZS_PARAM("peer URI", mPeerURI))
          return String();
        }

        try {
          return sectionBEl->findFirstChildElementChecked("findSecret")->getTextDecoded();
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("failed to obtain find secret") + ZS_PARAM("peer URI", mPeerURI))
        }
        return String();
      }

      //-----------------------------------------------------------------------
      ElementPtr PeerFilePublic::getSignedSaltBundle() const
      {
        ElementPtr sectionAEl = findSection("A");
        if (!sectionAEl) {
          ZS_LOG_WARNING(Detail, log("failed to find section A in public peer file") + ZS_PARAM("peer URI", mPeerURI))
          return ElementPtr();
        }

        try {
          return sectionAEl->findFirstChildElementChecked("saltBundle")->clone()->toElement();
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("failed to obtain salt bundle") + ZS_PARAM("peer URI", mPeerURI))
        }
        return ElementPtr();
      }

      //-----------------------------------------------------------------------
      IdentityBundleElementListPtr PeerFilePublic::getIdentityBundles() const
      {
        IdentityBundleElementListPtr result(new IdentityBundleElementList);

        ElementPtr sectionCEl = findSection("C");
        if (!sectionCEl) {
          ZS_LOG_WARNING(Detail, log("failed to find section C in public peer file") + ZS_PARAM("peer URI", mPeerURI))
          return result;
        }

        try {
          ElementPtr identitiesEl = sectionCEl->findFirstChildElement("identities");
          if (!identitiesEl) return result;

          ElementPtr identityBundleEl = identitiesEl->findFirstChildElement("identityBundle");
          while (identityBundleEl) {
            result->push_back(identityBundleEl->clone()->toElement());
            identityBundleEl = identityBundleEl->findNextSiblingElement("identityBundle");
          }
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("failed to obtain salt bundle") + ZS_PARAM("peer URI", mPeerURI))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr PeerFilePublic::getAssociatedPeerFiles() const
      {
        return mOuter.lock();
      }

      //-----------------------------------------------------------------------
      IPeerFilePrivatePtr PeerFilePublic::getAssociatedPeerFilePrivate() const
      {
        IPeerFilesPtr peerFiles = mOuter.lock();
        if (!peerFiles) return IPeerFilePrivatePtr();
        return peerFiles->getPeerFilePrivate();
      }

      //-----------------------------------------------------------------------
      IRSAPublicKeyPtr PeerFilePublic::getPublicKey() const
      {
        return mPublicKey;
      }

      //-----------------------------------------------------------------------
      bool PeerFilePublic::verifySignature(ElementPtr signedEl) const
      {
        ZS_THROW_BAD_STATE_IF(!mPublicKey)
        ZS_THROW_INVALID_ARGUMENT_IF(!signedEl)

        String peerURI;
        String fullPublicKey;
        String fingerprint;

        ElementPtr signatureEl;
        signedEl = stack::IHelper::getSignatureInfo(signedEl, &signatureEl, &peerURI, NULL, NULL, NULL, &fullPublicKey, &fingerprint);

        if (!signedEl) {
          ZS_LOG_WARNING(Detail, log("signature validation failed because no signed element found"))
          return false;
        }

        if (peerURI.hasData()) {
          if (peerURI != mPeerURI) {
            ZS_LOG_WARNING(Detail, log("signature validation failed since was not signed by this peer file") + ZS_PARAM("signature's URI", peerURI) + ZS_PARAM("peer file URI", mPeerURI))
            return false;
          }
        }

        if (fingerprint.hasData()) {
          if (fingerprint != mPublicKey->getFingerprint()) {
            ZS_LOG_WARNING(Detail, log("signature validation failed since was not signed by this peer file") + ZS_PARAM("signature's fingerprint", fingerprint) + ZS_PARAM("peer fingerprint", mPublicKey->getFingerprint()) + ZS_PARAM("peer file URI", mPeerURI))
            return false;
          }
        }

        if (!mPublicKey->verifySignature(signedEl)) {
          ZS_LOG_WARNING(Detail, log("signature failed to validate") + ZS_PARAM("peer URI", peerURI) + ZS_PARAM("fingerprint", fingerprint) + ZS_PARAM("full public key", fullPublicKey))
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr PeerFilePublic::encrypt(const SecureByteBlock &buffer) const
      {
        return mPublicKey->encrypt(buffer);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePublic => IPeerFiles
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PeerFilePublic::toDebug() const
      {
        ElementPtr resultEl = Element::create("PeerFilePublic");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "peer uri", mPeerURI);
        IHelper::debugAppend(resultEl, "created", getCreated());
        IHelper::debugAppend(resultEl, "expires", getExpires());
        IHelper::debugAppend(resultEl, "find secret", getFindSecret());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePublic => IPeerFilePublicForPeerFilePrivate
      #pragma mark

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::createFromPublicKey(
                                                            PeerFilesPtr peerFiles,
                                                            DocumentPtr publicDoc,
                                                            IRSAPublicKeyPtr publicKey,
                                                            const String &peerURI
                                                            )
      {
        PeerFilePublicPtr pThis(new PeerFilePublic);
        pThis->mThisWeak = pThis;
        pThis->mOuter = peerFiles;
        pThis->mDocument = publicDoc;
        pThis->mPublicKey = publicKey;
        pThis->mPeerURI = peerURI;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr PeerFilePublic::loadFromElement(
                                                        PeerFilesPtr peerFiles,
                                                        DocumentPtr publicDoc
                                                        )
      {
        PeerFilePublicPtr pThis(new PeerFilePublic);
        pThis->mThisWeak = pThis;
        pThis->mOuter = peerFiles;
        pThis->mDocument = publicDoc;
        pThis->init();
        if (!pThis->load()) {
          ZS_LOG_ERROR(Detail, pThis->log("public peer file failed to parse"))
          return PeerFilePublicPtr();
        }

        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePublic => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PeerFilePublic::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PeerFilePublic");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      bool PeerFilePublic::load(bool loadedFromCache)
      {
        ElementPtr sectionAEl = findSection("A");
        if (!sectionAEl) {
          ZS_LOG_ERROR(Detail, log("unable to find section A in the public peer file"))
          return false;
        }

        try {
          String cipher = sectionAEl->findFirstChildElementChecked("algorithm")->getTextDecoded();
          if (OPENPEER_STACK_PEER_FILE_CIPHER != cipher) {
            ZS_LOG_WARNING(Detail, log("cipher suite is not understood") + ZS_PARAM("cipher suite", + cipher) + ZS_PARAM("expecting", OPENPEER_STACK_PEER_FILE_CIPHER))
            return false;
          }

          GeneratorPtr generator = Generator::createJSONGenerator();

          String contactID;
          {
            size_t length = 0;
            ElementPtr bundleAEl = sectionAEl->getParentElementChecked();
            ElementPtr canonicalBundleAEl = services::IHelper::cloneAsCanonicalJSON(bundleAEl);
            boost::shared_array<char> sectionAsString = generator->write(canonicalBundleAEl, &length);

            SHA256 sha256;
            SecureByteBlock bundleHash(sha256.DigestSize());

            sha256.Update((const BYTE *)"contact:", strlen("contact:"));
            sha256.Update((const BYTE *)(sectionAsString.get()), length);
            sha256.Final(bundleHash);

            contactID = services::IHelper::convertToHex(bundleHash);
          }

          ElementPtr domainEl;
          try {
            ElementPtr saltSignatureEl;
            if (!IHelper::getSignatureInfo(sectionAEl->findFirstChildElementChecked("saltBundle")->findFirstChildElementChecked("salt"), &saltSignatureEl)) {
              ZS_LOG_ERROR(Basic, log("failed to find salt signature"))
            }
            domainEl = saltSignatureEl->findFirstChildElementChecked("key")->findFirstChildElementChecked("domain");
          } catch (CheckFailed &) {
            ZS_LOG_ERROR(Basic, log("failed to obtain signature domain from signed salt"))
            return false;
          }
          String domain = domainEl->getTextDecoded();
          if (domain.length() < 1) {
            ZS_LOG_ERROR(Basic, log("domain from signed salt was empty"))
            return false;
          }

          mPeerURI = IPeer::joinURI(domain, contactID);
          if (!mPeerURI) {
            ZS_LOG_ERROR(Basic, log("failed to generate a proper peer URI"))
            return false;
          }

          ElementPtr signatureEl;
          if (!IHelper::getSignatureInfo(sectionAEl, &signatureEl)) {
            ZS_LOG_ERROR(Basic, log("failed to obtain signature domain from signed salt"))
            return false;
          }

          SecureByteBlockPtr x509Certificate = services::IHelper::convertFromBase64(signatureEl->findFirstChildElementChecked("key")->findFirstChildElementChecked("x509Data")->getTextDecoded());

          mPublicKey = IRSAPublicKey::load(*x509Certificate);
          if (!mPublicKey) {
            ZS_LOG_ERROR(Detail, log("failed to load public key") + ZS_PARAM("peer URI", mPeerURI))
            return false;
          }

          {
            size_t length = 0;
            ElementPtr canonicalSectionAEl = services::IHelper::cloneAsCanonicalJSON(sectionAEl);
            boost::shared_array<char> sectionAAsString = generator->write(canonicalSectionAEl, &length);

            SecureByteBlockPtr sectionHash = services::IHelper::hash((const char *)(sectionAAsString.get()), services::IHelper::HashAlgorthm_SHA1);
            String algorithm = signatureEl->findFirstChildElementChecked("algorithm")->getTextDecoded();
            if (OPENPEER_STACK_PEER_FILE_SIGNATURE_ALGORITHM != algorithm) {
              ZS_LOG_WARNING(Detail, log("signature algorithm was not understood") + ZS_PARAM("peer URI", mPeerURI) + ZS_PARAM("algorithm", algorithm) + ZS_PARAM("expecting", OPENPEER_STACK_PEER_FILE_SIGNATURE_ALGORITHM))
              return false;
            }

            SecureByteBlockPtr digestValue = services::IHelper::convertFromBase64(signatureEl->findFirstChildElementChecked("digestValue")->getTextDecoded());

            if (0 != services::IHelper::compare(*sectionHash, *digestValue)) {
              ZS_LOG_ERROR(Detail, log("digest value does not match on section A signature on public peer file") + ZS_PARAM("peer URI", mPeerURI) +  ZS_PARAM("calculated digest", IHelper::convertToBase64(*sectionHash)) + ZS_PARAM("signature digest", + IHelper::convertToBase64(*digestValue)))
              return false;
            }

            SecureByteBlockPtr digestSigned =  services::IHelper::convertFromBase64(signatureEl->findFirstChildElementChecked("digestSigned")->getTextDecoded());
            if (!mPublicKey->verify(*sectionHash, *digestSigned)) {
              ZS_LOG_ERROR(Detail, log("signature on section A of public peer file failed to validate") + ZS_PARAM("peer URI", mPeerURI))
              return false;
            }
          }
        } catch(CheckFailed &) {
          ZS_LOG_ERROR(Detail, log("unable to parse the public peer file"))
          return false;
        }

        if (!loadedFromCache) {
          String publicFile = IHelper::toString(mDocument->getFirstChildElement());

          if (publicFile.hasData()) {
            // successfully loaded so cache it immediately
            String cookieName = getCookieName(mPeerURI);
            if (cookieName.hasData()) {
              ZS_LOG_TRACE(log("storing peer file public in cache") + ZS_PARAM("cookie", cookieName) + ZS_PARAM("uri", mPeerURI))
              UseCache::store(getCookieName(mPeerURI), zsLib::now() + Hours(OPENPEER_STACK_PEER_FILE_PUBLIC_STORE_CACHE_DURATION_IN_HOURS), publicFile);
            }
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      ElementPtr PeerFilePublic::findSection(const char *sectionID) const
      {
        if (!mDocument) return ElementPtr();

        ElementPtr peerRootEl = mDocument->getFirstChildElement();
        if (!peerRootEl) return ElementPtr();

        ElementPtr sectionBundleEl = peerRootEl->getFirstChildElement();
        while (sectionBundleEl) {
          ElementPtr sectionEl = sectionBundleEl->getFirstChildElement();
          if (sectionEl) {
            String id = sectionEl->getAttributeValue("id");
            if (id == sectionID) return sectionEl;
          }
          sectionBundleEl = sectionBundleEl->getNextSiblingElement();
        }
        return ElementPtr();
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPeerFilePublic
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPeerFilePublic::toDebug(IPeerFilePublicPtr peerFilePublic)
    {
      return internal::PeerFilePublic::toDebug(peerFilePublic);
    }

    //-------------------------------------------------------------------------
    IPeerFilePublicPtr IPeerFilePublic::loadFromElement(ElementPtr publicPeerRootElement)
    {
      return internal::IPeerFilePublicFactory::singleton().loadFromElement(publicPeerRootElement);
    }
    //-------------------------------------------------------------------------
    IPeerFilePublicPtr IPeerFilePublic::loadFromCache(const char *peerURI)
    {
      return internal::IPeerFilePublicFactory::singleton().loadFromCache(peerURI);
    }
  }
}
