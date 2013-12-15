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

#include <openpeer/stack/internal/stack_ServiceCertificatesValidateQuery.h>
#include <openpeer/stack/internal/stack_PeerFilePrivate.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>

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
      typedef IStackForInternal UseStack;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceCertificatesValidateQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceCertificatesValidateQuery::ServiceCertificatesValidateQuery(
                                                                         IMessageQueuePtr queue,
                                                                         IServiceCertificatesValidateQueryDelegatePtr delegate,
                                                                         ElementPtr signedElement
                                                                         ) :
        zsLib::MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mDelegate(IServiceCertificatesValidateQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate))
      {
        ZS_LOG_DEBUG(log("created"))

        ElementPtr signatureEl;
        String id;
        String domain;
        String service;

        signedElement = stack::IHelper::getSignatureInfo(signedElement, &signatureEl, NULL, &id, &domain, &service);

        if (!signedElement) {
          ZS_LOG_WARNING(Detail, log("signature validation failed because no signed element found"))
          return;
        }

        if (!IHelper::isValidDomain(domain)) {
          ZS_LOG_WARNING(Detail, log("signature domain is not valid") + ZS_PARAM("domain", domain))
          return;
        }

        // found the signature reference, now check if the peer URIs match - they must...
        try {
          String algorithm = signatureEl->findFirstChildElementChecked("algorithm")->getTextDecoded();
          if (algorithm != OPENPEER_STACK_PEER_FILE_SIGNATURE_ALGORITHM) {
            ZS_LOG_WARNING(Detail, log("signature validation algorithm is not understood") + ZS_PARAM("algorithm", algorithm))
            return;
          }

          String signatureDigestAsString = signatureEl->findFirstChildElementChecked("digestValue")->getTextDecoded();

          GeneratorPtr generator = Generator::createJSONGenerator();
          boost::shared_array<char> signedElAsJSON = generator->write(signedElement);

          SecureByteBlockPtr actualDigest = IHelper::hash((const char *)(signedElAsJSON.get()), IHelper::HashAlgorthm_SHA1);

          if (0 != IHelper::compare(*actualDigest, *IHelper::convertFromBase64(signatureDigestAsString))) {
            ZS_LOG_WARNING(Detail, log("digest values did not match") + ZS_PARAM("signature digest", signatureDigestAsString) + ZS_PARAM("actual digest", IHelper::convertToBase64(*actualDigest)))
            return;
          }

          mCertificateID = id;
          mDomain = domain;
          mService = service;
          mDigest = actualDigest;
          mDigestSigned = IHelper::convertFromBase64(signatureEl->findFirstChildElementChecked("digestSigned")->getTextDecoded());
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("signature failed to validate due to missing signature element") + ZS_PARAM("signature id", id) + ZS_PARAM("signature domain", domain) + ZS_PARAM("signature service", service))
        }
      }

      //-----------------------------------------------------------------------
      ServiceCertificatesValidateQuery::~ServiceCertificatesValidateQuery()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceCertificatesValidateQuery::init()
      {
        if (!mDigestSigned) {
          cancel();
          return;
        }

        mBootstrappedNetwork = UseBootstrappedNetwork::prepare(mDomain, mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ServiceCertificatesValidateQueryPtr ServiceCertificatesValidateQuery::convert(IServiceCertificatesValidateQueryPtr query)
      {
        return dynamic_pointer_cast<ServiceCertificatesValidateQuery>(query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceCertificatesValidateQuery => IServiceCertificatesValidateQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServiceCertificatesValidateQuery::toDebug(IServiceCertificatesValidateQueryPtr query)
      {
        if (!query) return ElementPtr();
        return ServiceCertificatesValidateQuery::convert(query)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServiceCertificatesValidateQueryPtr ServiceCertificatesValidateQuery::queryIfValidSignature(
                                                                                                IServiceCertificatesValidateQueryDelegatePtr delegate,
                                                                                                ElementPtr signedElement
                                                                                                )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ServiceCertificatesValidateQueryPtr pThis(new ServiceCertificatesValidateQuery(UseStack::queueStack(), delegate, signedElement));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceCertificatesPtr ServiceCertificatesValidateQuery::getService() const
      {
        AutoRecursiveLock lock(getLock());
        return BootstrappedNetwork::convert(mBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      String ServiceCertificatesValidateQuery::getSignatureDomain() const
      {
        AutoRecursiveLock lock(getLock());
        return mDomain;
      }

      //-----------------------------------------------------------------------
      String ServiceCertificatesValidateQuery::getSignatureService() const
      {
        AutoRecursiveLock lock(getLock());
        return mService;
      }

      //-----------------------------------------------------------------------
      bool ServiceCertificatesValidateQuery::isComplete() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mBootstrappedNetwork) return true;

        return mBootstrappedNetwork->isPreparationComplete();
      }

      //-----------------------------------------------------------------------
      bool ServiceCertificatesValidateQuery::isValidSignature(
                                                              WORD *outErrorCode,
                                                              String *outErrorReason
                                                              ) const
      {
        AutoRecursiveLock lock(getLock());
        if (!mBootstrappedNetwork) return false;

        if (!mBootstrappedNetwork->isPreparationComplete()) {
          ZS_LOG_WARNING(Detail, log("certificate is not valid because of bootstrapper isn't ready"))
          return false;
        }

        if (!mBootstrappedNetwork->wasSuccessful(outErrorCode, outErrorReason)) {
          ZS_LOG_WARNING(Detail, log("certificate is not valid because of bootstrapper failure"))
          return false;
        }

        return mBootstrappedNetwork->isValidSignature(mCertificateID, mDomain, mService, mDigest, mDigestSigned);
      }

      //-----------------------------------------------------------------------
      void ServiceCertificatesValidateQuery::cancel()
      {
        ZS_LOG_DEBUG(log("cancelling certificate validation"))

        AutoRecursiveLock lock(getLock());
        if (!mDelegate) {
          ZS_LOG_DEBUG(log("certificate query already cancelled"))
          return;
        }

        ServiceCertificatesValidateQueryPtr pThis = mThisWeak.lock();
        if (pThis) {
          try {
            mDelegate->onServiceCertificatesValidateQueryCompleted(pThis);
          } catch(IServiceCertificatesValidateQueryDelegateProxy::Exceptions::DelegateGone &) {
          }
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceCertificatesValidateQuery => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceCertificatesValidateQuery::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(getLock());
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceCertificatesValidateQuery => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ServiceCertificatesValidateQuery::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      Log::Params ServiceCertificatesValidateQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServiceCertificatesValidateQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr ServiceCertificatesValidateQuery::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("ServiceCertificatesValidateQuery");

        IHelper::debugAppend(resultEl, "certificate ID", mCertificateID);
        IHelper::debugAppend(resultEl, "domain", mDomain);
        IHelper::debugAppend(resultEl, "domain", mService);
        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "digest", mDigest ? IHelper::convertToBase64(*mDigest) : String());
        IHelper::debugAppend(resultEl, "digest signed", mDigestSigned ? IHelper::convertToBase64(*mDigestSigned) : String());
        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));

        return resultEl;
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
    #pragma mark IServiceCertificatesValidateQuery
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IServiceCertificatesValidateQuery::toDebug(IServiceCertificatesValidateQueryPtr query)
    {
      return internal::ServiceCertificatesValidateQuery::toDebug(query);
    }

    //-------------------------------------------------------------------------
    IServiceCertificatesValidateQueryPtr IServiceCertificatesValidateQuery::queryIfValidSignature(
                                                                                                IServiceCertificatesValidateQueryDelegatePtr delegate,
                                                                                                ElementPtr signedElement
                                                                                                )
    {
      return internal::IServiceCertificatesValidateQueryFactory::singleton().queryIfValidSignature(delegate, signedElement);
    }
  }
}
