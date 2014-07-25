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

#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_BootstrappedNetworkManager.h>
#include <openpeer/stack/internal/stack_Cache.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_PeerFilePrivate.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/bootstrapper/ServicesGetRequest.h>
#include <openpeer/stack/message/certificates/CertificatesGetRequest.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/ISettings.h>
#include <openpeer/services/IHelper.h>
#include <openpeer/services/IRSAPublicKey.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_STACK_BOOTSTRAPPED_NETWORK_MAX_REDIRECTION_ATTEMPTS (5)

#define OPENPEER_STACK_BOOTSTRAPPED_NETWORK_DEFAULT_MIME_TYPE "application/json"

#define OPENPEER_STACK_BOOSTRAPPER_DEFAULT_REQUEST_TIMEOUT_SECONDS (60)

#define OPENPEER_STACK_BOOTSTRAPPER_SERVICES_GET_CACHING_TIME_SECONDS (24*60*60)
#define OPENPEER_STACK_BOOTSTRAPPER_CERTIFICATES_GET_CACHING_TIME_SECONDS (24*60*60)

#define OPENPEER_STACK_BOOTSTRAPPER_CACHING_NAMESPACE_PREFIX "https://meta.openpeer.org/caching/boostrapper/"

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;
      using services::IHelper;
      using services::IHTTPQuery;
      using services::IHTTPQueryDelegateProxy;
      using services::ISettings;

      using namespace stack::message;
      using namespace stack::message::bootstrapper;
      using namespace stack::message::certificates;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      typedef IBootstrappedNetworkForServices::ForServicesPtr ForServicesPtr;


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      enum BootstrapperRequestTypes
      {
        BootstrapperRequestType_ServicesGet,
        BootstrapperRequestType_CertificatesGet,
      };

      //-----------------------------------------------------------------------
      static const char *toName(BootstrapperRequestTypes type)
      {
        switch (type) {
          case BootstrapperRequestType_ServicesGet:     return "services-get";
          case BootstrapperRequestType_CertificatesGet: return "certificates-get";
        }
        return "unknown";
      }

      //-----------------------------------------------------------------------
      static String getCookieName(
                                  BootstrapperRequestTypes type,
                                  const String &domain,
                                  ULONG attempt = 0
                                  )
      {
        return String(OPENPEER_STACK_BOOTSTRAPPER_CACHING_NAMESPACE_PREFIX) + domain + "/" + toName(type) + "/" + (attempt > 0 ? string(attempt) : String());
      }

      //-----------------------------------------------------------------------
      static Time getCacheTime(BootstrapperRequestTypes type)
      {
        Time expires = zsLib::now();

        switch (type) {
          case BootstrapperRequestType_ServicesGet:     return expires + Seconds(OPENPEER_STACK_BOOTSTRAPPER_SERVICES_GET_CACHING_TIME_SECONDS);
          case BootstrapperRequestType_CertificatesGet: return expires + Seconds(OPENPEER_STACK_BOOTSTRAPPER_CERTIFICATES_GET_CACHING_TIME_SECONDS);
        }
        return Time();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark FakeHTTPQuery
      #pragma mark

      ZS_DECLARE_CLASS_PTR(FakeHTTPQuery)

      class FakeHTTPQuery : public IHTTPQuery
      {
      protected:
        FakeHTTPQuery() {}

      public:
        ~FakeHTTPQuery() {}

        static FakeHTTPQueryPtr create() {return FakeHTTPQueryPtr(new FakeHTTPQuery);}
        static FakeHTTPQueryPtr convert(IHTTPQueryPtr query)
        {
          return dynamic_pointer_cast<FakeHTTPQuery>(query);
        }

        MessagePtr result() const               {return mResult;}
        void result(MessagePtr result)          {mResult = result;}

        SecureByteBlockPtr buffer() const       {return mBuffer;}
        void buffer(SecureByteBlockPtr buffer)  {mBuffer = buffer;}

      protected:
        typedef IHTTP::HTTPStatusCodes HTTPStatusCodes;

        virtual PUID getID() const {return mID;}

        virtual void cancel() {}

        virtual bool isComplete() const {return true;}
        virtual bool wasSuccessful() const {return true;}
        virtual HTTPStatusCodes getStatusCode() const {return IHTTP::HTTPStatusCode_OK;}
        virtual long getResponseCode() const {return 0;}

        virtual size_t getHeaderReadSizeAvailableInBytes() const {return 0;}
        virtual size_t readHeader(
                                  BYTE *outResultData,
                                  size_t bytesToRead
                                  ) {return 0;}

        virtual size_t readHeaderAsString(String &outHeader) {return 0;}

        virtual size_t getReadDataAvailableInBytes() const {return 0;}

        virtual size_t readData(
                                BYTE *outResultData,
                                size_t bytesToRead
                                ) {return 0;}

        virtual size_t readDataAsString(String &outResultData) {return 0;}

      protected:
        AutoPUID mID;
        MessagePtr mResult;

        SecureByteBlockPtr mBuffer;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkForServices
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IBootstrappedNetworkForServices::toDebug(ForServicesPtr network)
      {
        return IBootstrappedNetwork::toDebug(BootstrappedNetwork::convert(network));
      }

      //-----------------------------------------------------------------------
      ForServicesPtr IBootstrappedNetworkForServices::prepare(
                                                              const char *domain,
                                                              IBootstrappedNetworkDelegatePtr delegate
                                                              )
      {
        return IBootstrappedNetworkFactory::singleton().prepare(domain, delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork
      #pragma mark

      //-----------------------------------------------------------------------
      const char *BootstrappedNetwork::toString(ErrorCodes errorCode)
      {
        return IHTTP::toString(IHTTP::toStatusCode(errorCode));
      }

      //-----------------------------------------------------------------------
      BootstrappedNetwork::BootstrappedNetwork(
                                               const SharedRecursiveLock &inLock,
                                               IMessageQueuePtr queue,
                                               const char *domain
                                               ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(inLock),
        mManager(UseBootstrappedNetworkManager::singleton()),
        mDomain(domain),
        mCompleted(false),
        mErrorCode(0),
        mRedirectionAttempts(0)
      {
        ZS_LOG_DEBUG(debug("created"))
        mDomain.toLower();
      }

      //-----------------------------------------------------------------------
      BootstrappedNetwork::~BootstrappedNetwork()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(debug("destroyed"))

        cancel();
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::init()
      {
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IBootstrappedNetworkPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IServiceCertificatesPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IServiceIdentityPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IServiceLockboxPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IServicePushMailboxPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(IServiceSaltPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(ForServicesPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::convert(ForBootstrappedNetworkManagerPtr network)
      {
        return dynamic_pointer_cast<BootstrappedNetwork>(network);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IBootstrappedNetwork
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr BootstrappedNetwork::toDebug(IBootstrappedNetworkPtr network)
      {
        if (!network) return ElementPtr();
        return BootstrappedNetwork::convert(network)->toDebug();
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetwork::prepare(
                                                          const char *inDomain,
                                                          IBootstrappedNetworkDelegatePtr inDelegate
                                                          )
      {
        UseStack::logInstanceInformation();

        ZS_THROW_INVALID_ARGUMENT_IF(!inDomain)

        String domain(inDomain ? String(inDomain) : String());
        ZS_THROW_INVALID_ARGUMENT_IF(domain.isEmpty())

        domain.toLower();

        UseBootstrappedNetworkManagerPtr manager = UseBootstrappedNetworkManager::singleton();

        BootstrappedNetworkPtr pThis(new BootstrappedNetwork(manager ? manager->getLock() : SharedRecursiveLock(SharedRecursiveLock::create()), UseStack::queueStack(), domain));
        pThis->mThisWeak = pThis;
        pThis->init();

        AutoRecursiveLock lock(*pThis);

        BootstrappedNetworkPtr useThis = manager ? manager->findExistingOrUse(pThis) : pThis;

        if (pThis->getID() != useThis->getID()) {
          ZS_LOG_DEBUG(useThis->log("reusing existing object") + useThis->toDebug())
          useThis->reuse();
          pThis->dontUse();
        }

        if (inDelegate) {
          if (!manager) {
            IBootstrappedNetworkDelegatePtr delegate = IBootstrappedNetworkDelegateProxy::createWeak(UseStack::queueDelegate(), inDelegate);

            useThis->setFailure(IHTTP::HTTPStatusCode_Gone, "BootstrappedNetworkManager singleton is gone");
            useThis->cancel();

            try {
              delegate->onBootstrappedNetworkPreparationCompleted(useThis);
            } catch(IBootstrappedNetworkDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, slog("delegate gone"))
            }

            return useThis;
          }

          manager->registerDelegate(useThis, inDelegate);
        }

        if (pThis->getID() == useThis->getID()) {
          ZS_LOG_DEBUG(useThis->log("preparing new object") + pThis->toDebug())
          pThis->go();
        }
        return useThis;
      }

      //-----------------------------------------------------------------------
      String BootstrappedNetwork::getDomain() const
      {
        return mDomain;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::isPreparationComplete() const
      {
        AutoRecursiveLock lock(*this);
        return mCompleted;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::wasSuccessful(
                                              WORD *outErrorCode,
                                              String *outErrorReason
                                              ) const
      {
        AutoRecursiveLock lock(*this);

        if (outErrorCode) *outErrorCode = mErrorCode;
        if (outErrorReason) *outErrorReason = mErrorReason;

        return (0 == mErrorCode);
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::sendServiceMessage(
                                                   const char *serviceType,
                                                   const char *serviceMethodName,
                                                   message::MessagePtr message,
                                                   const char *cachedCookieNameForResult,
                                                   Time cacheExpires
                                                   )
      {
        ZS_LOG_DEBUG(log("sending message to service") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", serviceMethodName) + Message::toDebug(message))
        AutoRecursiveLock lock(*this);
        if (!mCompleted) {
          ZS_LOG_WARNING(Detail, log("bootstrapper isn't complete and thus cannot send message"))
          return false;
        }

        if (!message) {
          ZS_LOG_WARNING(Detail, log("bootstrapped was asked to send a null message"))
          return false;
        }

        const Service::Method *service = findServiceMethod(serviceType, serviceMethodName);
        if (!service) {
          ZS_LOG_WARNING(Detail, log("failed to find service to send to") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", serviceMethodName))
          if ((message->isRequest()) ||
              (message->isNotify())) {
            MessageResultPtr result = MessageResult::create(message, ErrorCode_NotFound, toString(ErrorCode_NotFound));
            if (!result) {
              ZS_LOG_WARNING(Detail, log("failed to create result for message"))
              return false;
            }
            IMessageMonitor::handleMessageReceived(result);
          }
          return false;
        }

        if (service->mURI.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("failed to find service URI to send to") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", serviceMethodName))
          return false;
        }

        IHTTPQueryPtr query = post(service->mURI, message, cachedCookieNameForResult, cacheExpires);
        if (!query) {
          ZS_LOG_WARNING(Detail, log("failed to create query for message"))
          if ((message->isRequest()) ||
              (message->isNotify())) {
            MessageResultPtr result = MessageResult::create(message, ErrorCode_InternalServerError, toString(ErrorCode_InternalServerError));
            if (!result) {
              ZS_LOG_WARNING(Detail, log("failed to create result for message"))
              return false;
            }
            IMessageMonitor::handleMessageReceived(result);
          }
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IServiceCertificates
      #pragma mark

      //-------------------------------------------------------------------------
      IBootstrappedNetworkPtr BootstrappedNetwork::getBootstrappedNetwork() const
      {
        return mThisWeak.lock();
      }
      
      //-----------------------------------------------------------------------
      IServiceCertificatesPtr BootstrappedNetwork::createServiceCertificatesFrom(IBootstrappedNetworkPtr preparedBootstrappedNetwork)
      {
        return BootstrappedNetwork::convert(preparedBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::isValidSignature(ElementPtr signedElement) const
      {
        ElementPtr signatureEl;
        String id;
        String domain;
        String service;
        signedElement = stack::IHelper::getSignatureInfo(signedElement, &signatureEl, NULL, &id, &domain, &service);

        bool skipValidation = ISettings::getBool(OPENPEER_STACK_SETTING_BOOTSTREPPER_SKIP_SIGNATURE_VALIDATION);
        if (skipValidation) {
          ZS_LOG_WARNING(Basic, log("signature validation was intentionally skipped (skipping signature validation can compromise security)"))
          return true;
        }

        if (!signedElement) {
          ZS_LOG_WARNING(Detail, log("signature validation failed because no signed element found"))
          return false;
        }

        if (domain != mDomain) {
          ZS_LOG_WARNING(Detail, log("signature is not valid as domains do not match") + ZS_PARAM("signature domain", domain) + ZS_PARAM("domain", mDomain))
          return false;
        }

        CertificateMap::const_iterator found = mCertificates.find(id);
        if (found == mCertificates.end()) {
          ZS_LOG_WARNING(Detail, log("no signature with the id specified found") + ZS_PARAM("signature id", id))
          return false;
        }

        const Certificate &certificate = (*found).second;
        if (certificate.mService != service) {
          ZS_LOG_WARNING(Detail, log("signature is not valid as services do not match") + ZS_PARAM("signature service", service) + certificate.toDebug())
          return false;
        }

        if (!certificate.mPublicKey) {
          ZS_LOG_WARNING(Detail, log("certificate missing public key") + certificate.toDebug())
          return false;
        }

        // found the signature reference, now check if the peer URIs match - they must...
        try {
          String algorithm = signatureEl->findFirstChildElementChecked("algorithm")->getTextDecoded();
          if (algorithm != OPENPEER_STACK_PEER_FILE_SIGNATURE_ALGORITHM) {
            ZS_LOG_WARNING(Detail, log("signature validation algorithm is not understood") + ZS_PARAM("algorithm", algorithm))
            return false;
          }

          String signatureDigestAsString = signatureEl->findFirstChildElementChecked("digestValue")->getTextDecoded();

          GeneratorPtr generator = Generator::createJSONGenerator();
          boost::shared_array<char> signedElAsJSON = generator->write(signedElement);

          SecureByteBlockPtr actualDigest = IHelper::hash((const char *)(signedElAsJSON.get()), IHelper::HashAlgorthm_SHA1);

          if (0 != IHelper::compare(*actualDigest, *IHelper::convertFromBase64(signatureDigestAsString)))
          {
            ZS_LOG_WARNING(Detail, log("digest values did not match") + ZS_PARAM("signature digest", signatureDigestAsString) + ZS_PARAM("actual digest", IHelper::convertToBase64(*actualDigest)))
            return false;
          }

          SecureByteBlockPtr signatureDigestSigned = IHelper::convertFromBase64(signatureEl->findFirstChildElementChecked("digestSigned")->getTextDecoded());

          if (!certificate.mPublicKey->verify(*actualDigest, *signatureDigestSigned)) {
            ZS_LOG_WARNING(Detail, log("signature failed to validate") + certificate.toDebug())
            return false;
          }
        } catch(CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("signature failed to validate due to missing signature element") + certificate.toDebug())
          return false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IServiceIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceIdentityPtr BootstrappedNetwork::createServiceIdentityFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        return BootstrappedNetwork::convert(bootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IServiceLockbox
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceLockboxPtr BootstrappedNetwork::createServiceLockboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        return BootstrappedNetwork::convert(bootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IServicePushMailbox
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxPtr BootstrappedNetwork::createServicePushMailboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        return BootstrappedNetwork::convert(bootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IServiceSalt
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceSaltPtr BootstrappedNetwork::createServiceSaltFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        return BootstrappedNetwork::convert(bootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IBootstrappedNetworkForServices
      #pragma mark

      //-----------------------------------------------------------------------
      String BootstrappedNetwork::getServiceURI(
                                                const char *serviceType,
                                                const char *serviceMethodName
                                                ) const
      {
        AutoRecursiveLock lock(*this);
        if (!mCompleted) return String();

        const Service::Method *service = findServiceMethod(serviceType, serviceMethodName);
        if (!service) return String();

        return service->mURI;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::isValidSignature(
                                                 const String &id,
                                                 const String &domain,
                                                 const String &service,
                                                 SecureByteBlockPtr buffer,
                                                 SecureByteBlockPtr bufferSigned
                                                 ) const
      {
        AutoRecursiveLock lock(*this);

        if (domain != mDomain) {
          ZS_LOG_WARNING(Detail, log("signature is not valid as domains do not match") + ZS_PARAM("signature domain", domain) + ZS_PARAM("domain", mDomain))
          return false;
        }

        CertificateMap::const_iterator found = mCertificates.find(id);
        if (found == mCertificates.end()) {
          ZS_LOG_WARNING(Detail, log("no signature with the id specified found") + ZS_PARAM("signature id", id))
          return false;
        }

        const Certificate &certificate = (*found).second;
        if (certificate.mService != service) {
          ZS_LOG_WARNING(Detail, log("signature is not valid as services do not match") + ZS_PARAM("signature service", service) + certificate.toDebug())
          return false;
        }

        if (!certificate.mPublicKey) {
          ZS_LOG_WARNING(Detail, log("certificate missing public key") + certificate.toDebug())
          return false;
        }

        if (!certificate.mPublicKey->verify(*buffer, *bufferSigned)) {
          ZS_LOG_WARNING(Detail, log("signature failed to validate") + certificate.toDebug())
          return false;
        }

        ZS_LOG_DEBUG(log("signature validated") + certificate.toDebug())
        return true;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::supportsRolodex() const
      {
        String serviceURL = getServiceURI("rolodex", "rolodex-access");
        return serviceURL.hasData();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork =>IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::onWake()
      {
        ZS_LOG_DEBUG(log("on wake"))
        AutoRecursiveLock lock(*this);
        step();
      }

      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => IHTTPQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::onHTTPReadDataAvailable(IHTTPQueryPtr query)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::onHTTPCompleted(IHTTPQueryPtr query)
      {
        ZS_LOG_DEBUG(log("on http complete") + ZS_PARAM("query ID", query->getID()))

        AutoRecursiveLock lock(*this);

        PendingRequestMap::iterator found = mPendingRequests.find(query);
        if (found == mPendingRequests.end()) {
          ZS_LOG_WARNING(Detail, log("could not find as pending request (during shutdown?)"))
          return;
        }

        MessagePtr originalMessage = (*found).second;

        mPendingRequests.erase(found);

        ZS_LOG_DEBUG(log("found pending request"))

        PendingRequestCookieMap::iterator foundCookie = mPendingRequestCookies.find(query);
        bool willAttemptStore = (foundCookie != mPendingRequestCookies.end());

        SecureByteBlockPtr output;

        MessagePtr resultMessage = getMessageFromQuery(
                                                       query,
                                                       originalMessage,
                                                       willAttemptStore ? &output : NULL
                                                       );

        if (willAttemptStore) {
          const CookieName &cookieName = (*foundCookie).second.first;
          const CookieExpires &cookieExpires = (*foundCookie).second.second;

          if (resultMessage) {
            MessageResultPtr actualResultMessage = MessageResult::convert(resultMessage);
            if (!actualResultMessage->hasError()) {
              UseCache::store(cookieName, cookieExpires, (const char *)(output->BytePtr()));
            } else {
              if (mServicesGetQuery == query) {
                if (IHTTP::isRedirection(IHTTP::toStatusCode(actualResultMessage->errorCode()))) {
                  ZS_LOG_DEBUG(log("putting result into cache") + ZS_PARAM("name", cookieName) + ZS_PARAM("expires", cookieExpires) + ZS_PARAM("json", (const char *)(output->BytePtr())))
                  UseCache::store(cookieName, cookieExpires, (const char *)(output->BytePtr()));
                } else {
                  UseCache::clear(cookieName); // do not store error results
                }
              } else {
                UseCache::clear(cookieName); // do not store error results
              }
            }
          } else {
            UseCache::clear(cookieName);
          }

          mPendingRequestCookies.erase(foundCookie);
        }

        if (resultMessage) {
          if (IMessageMonitor::handleMessageReceived(resultMessage)) {
            ZS_LOG_DEBUG(log("http result was handled by message monitor"))
            return;
          }
          ZS_LOG_WARNING(Detail, log("no message handler registered for this request"))
        } else {
          ZS_LOG_WARNING(Detail, log("failed to create message from pending query completion"))
        }

        MessageResultPtr result = MessageResult::create(originalMessage, IHTTP::HTTPStatusCode_BadRequest);
        if (!result) {
          ZS_LOG_WARNING(Detail, log("failed to create result for message"))
          return;
        }
        IMessageMonitor::handleMessageReceived(result);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => IMessageMonitorResultDelegate<ServicesGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   ServicesGetResultPtr result
                                                                   )
      {
        ZS_LOG_DEBUG(log("services get result received") + ZS_PARAM("monitor", monitor->getID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mServicesGetMonitor) {
          ZS_LOG_WARNING(Detail, log("services get result received from obsolete monitor") + ZS_PARAM("monitor", monitor->getID()))
          return false;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();   // force a step to perform next action after an HTTP result

        mServiceTypeMap = result->servicesByType();
        if (mServiceTypeMap.size() < 1) {
          ZS_LOG_WARNING(Detail, log("services get failed to return any services"))
          setFailure(ErrorCode_NotFound, "Failed to obtain services");
          cancel();
          return true;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        ServicesGetResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        ZS_LOG_DEBUG(log("services get result error received") + ZS_PARAM("monitor", monitor->getID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mServicesGetMonitor) {
          ZS_LOG_WARNING(Detail, log("services get result received from obsolete monitor") + ZS_PARAM("monitor", monitor->getID()))
          return false;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();   // force a step to perform next action after an HTTP result

        mServicesGetQuery.reset();  // forget the services query - only needed to apply message ID

        bool isRedirection = IHTTP::isRedirection(IHTTP::toStatusCode(result->errorCode()));

        String redirectionURL;

        if (isRedirection) {
          try {
            ElementPtr errorRootEl = result->errorRoot();
            if (errorRootEl) {
              redirectionURL = errorRootEl->findFirstChildElementChecked("location")->getTextDecoded();
            }
          } catch (CheckFailed &) {
            ZS_LOG_WARNING(Detail, log("redirection specified but no redirection location found"))
          }
        }

        if (redirectionURL.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("services get result has an error"))
          setFailure(result->errorCode(), result->errorReason());
          cancel();
          return true;
        }

        ++mRedirectionAttempts;
        if (mRedirectionAttempts >= OPENPEER_STACK_BOOTSTRAPPED_NETWORK_MAX_REDIRECTION_ATTEMPTS) {
          ZS_LOG_WARNING(Detail, debug("too many redirection attempts"))
          setFailure(ErrorCode_LoopDetected, "too many redirection attempts");
          cancel();
          return true;
        }

        ZS_LOG_DEBUG(log("redirecting services get to a new URL") + ZS_PARAM("url", redirectionURL))

        ServicesGetRequestPtr request = ServicesGetRequest::create();
        request->domain(mDomain);

        mServicesGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<ServicesGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_BOOSTRAPPER_DEFAULT_REQUEST_TIMEOUT_SECONDS));
        mServicesGetQuery = post(redirectionURL, request, getCookieName(BootstrapperRequestType_ServicesGet, mDomain, mRedirectionAttempts), getCacheTime(BootstrapperRequestType_ServicesGet));
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Identity => IMessageMonitorResultDelegate<ServicesGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::handleMessageMonitorResultReceived(
                                                                   IMessageMonitorPtr monitor,
                                                                   CertificatesGetResultPtr result
                                                                   )
      {
        ZS_LOG_DEBUG(log("certificates get result received") + ZS_PARAM("monitor", monitor->getID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mCertificatesGetMonitor) {
          ZS_LOG_WARNING(Detail, log("certificates get result received from obsolete monitor") + ZS_PARAM("monitor", monitor->getID()))
          return false;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();   // force a step to perform next action after an HTTP result

        mCertificates = result->certificates();
        if (mCertificates.size() < 1) {
          ZS_LOG_WARNING(Detail, log("certificates get failed to return any certificates"))
          setFailure(ErrorCode_NotFound);
          cancel();
          return true;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool BootstrappedNetwork::handleMessageMonitorErrorResultReceived(
                                                                        IMessageMonitorPtr monitor,
                                                                        CertificatesGetResultPtr ignore, // will always be NULL
                                                                        MessageResultPtr result
                                                                        )
      {
        ZS_LOG_DEBUG(log("certificates get result error received") + ZS_PARAM("monitor", monitor->getID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mCertificatesGetMonitor) {
          ZS_LOG_WARNING(Detail, log("certificates get result received from obsolete monitor") + ZS_PARAM("monitor", monitor->getID()))
          return false;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();   // force a step to perform next action after an HTTP result

        ZS_LOG_WARNING(Detail, log("certificates get result has an error"))
        setFailure(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params BootstrappedNetwork::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("BootstrappedNetwork");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params BootstrappedNetwork::slog(const char *message)
      {
        return Log::Params(message, "BootstrappedNetwork");
      }

      //-----------------------------------------------------------------------
      Log::Params BootstrappedNetwork::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr BootstrappedNetwork::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("BootstrappedNetwork");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "domain", mDomain);

        IHelper::debugAppend(resultEl, "complete", mCompleted);

        IHelper::debugAppend(resultEl, "error code", mErrorCode);
        IHelper::debugAppend(resultEl, "error reason", mErrorReason);

        IHelper::debugAppend(resultEl, "services get HTTP query", mServicesGetQuery ? mServicesGetQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "services get monitor", mServicesGetMonitor ? mServicesGetMonitor->getID() : 0);
        IHelper::debugAppend(resultEl, "certificates get monitor", mCertificatesGetMonitor ? mCertificatesGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "redirection attempts", mRedirectionAttempts);

        IHelper::debugAppend(resultEl, "service types", mServiceTypeMap.size());
        IHelper::debugAppend(resultEl, "certificates", mCertificates.size());

        IHelper::debugAppend(resultEl, "pending HTTP requests", mPendingRequests.size());
        IHelper::debugAppend(resultEl, "pending request cookies", mPendingRequestCookies.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::go()
      {
        step();
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::reuse()
      {
        ZS_LOG_DEBUG(log("reuse called"))

        AutoRecursiveLock lock(*this);

        if (!isPreparationComplete()) {
          ZS_LOG_DEBUG(log("preparation is still pending"))
          return;
        }

        if (wasSuccessful()) {
          ZS_LOG_DEBUG(log("preparation is complete and was successful"))
          return;
        }

        ZS_LOG_DEBUG(log("reuse called"))

        mCompleted = false;

        mErrorCode = 0;
        mErrorReason.clear();

        if (mServicesGetMonitor) {
          mServicesGetMonitor->cancel();
          mServicesGetMonitor.reset();
        }

        if (mCertificatesGetMonitor) {
          mCertificatesGetMonitor->cancel();
          mCertificatesGetMonitor.reset();
        }

        mRedirectionAttempts = 0;

        mServiceTypeMap.clear();
        mServiceTypeMap.clear();

        mCertificates.clear();

        go();
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::dontUse()
      {
        ZS_LOG_DEBUG(log("not using this object (will self destruct)"))
        mCompleted = true;
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::step()
      {
        if (mCompleted) {
          ZS_LOG_DEBUG(log("step - already completed"))
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        // now we have the DNS service name...
        if (!mServicesGetMonitor) {
          bool forceOverHTTP = ISettings::getBool(OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_OVER_INSECURE_HTTP);

          String serviceURL = (forceOverHTTP ? "http://" : "https://") + mDomain + "/.well-known/" + OPENPEER_STACK_SETTING_BOOSTRAPPER_SERVICES_GET_URL_METHOD_NAME;
          ZS_LOG_DEBUG(log("step - performing services get request") + ZS_PARAM("services-get URL", serviceURL))

          if (forceOverHTTP) {
            ZS_LOG_WARNING(Basic, log("/.well-known/" OPENPEER_STACK_SETTING_BOOSTRAPPER_SERVICES_GET_URL_METHOD_NAME " being forced over insecure http connection"))
          }

          ServicesGetRequestPtr request = ServicesGetRequest::create();
          request->domain(mDomain);

          bool sendAsGetRequest = !ISettings::getBool(OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_USING_POST);

          if (!sendAsGetRequest) {
            ZS_LOG_WARNING(Basic, log("/.well-known/" OPENPEER_STACK_SETTING_BOOSTRAPPER_SERVICES_GET_URL_METHOD_NAME " being forced as an HTTP POST request"))
          }

          mServicesGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<ServicesGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_BOOSTRAPPER_DEFAULT_REQUEST_TIMEOUT_SECONDS));
          mServicesGetQuery = post(serviceURL, request, getCookieName(BootstrapperRequestType_ServicesGet, mDomain), getCacheTime(BootstrapperRequestType_ServicesGet), sendAsGetRequest);
        }

        if (!mServicesGetMonitor->isComplete()) {
          ZS_LOG_DEBUG(log("waiting for services get query to complete"))
          return;
        }

        if (mServiceTypeMap.size() < 1) {
          ZS_LOG_DEBUG(log("waiting for services get query to read data"))
          return;
        }

        if (!mCertificatesGetMonitor) {
          const Service::Method *method = findServiceMethod("certificates", "certificates-get");
          if (!method) {
            ZS_LOG_WARNING(Detail, log("failed to obtain certificate service information"))
            setFailure(ErrorCode_ServiceUnavailable, "Certificate service is not available");
            return;
          }

          if (method->mURI.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("failed to obtain certificate service URI information"))
            setFailure(ErrorCode_ServiceUnavailable, "Certificate service URI is not available");
            return;
          }

          CertificatesGetRequestPtr request = CertificatesGetRequest::create();
          request->domain(mDomain);

          mCertificatesGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<CertificatesGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_BOOSTRAPPER_DEFAULT_REQUEST_TIMEOUT_SECONDS));
          post(method->mURI, request, getCookieName(BootstrapperRequestType_CertificatesGet, mDomain), getCacheTime(BootstrapperRequestType_CertificatesGet));
        }

        if (!mCertificatesGetMonitor->isComplete()) {
          ZS_LOG_DEBUG(log("waiting for certificates get query to complete"))
          return;
        }

        if (mCertificates.size() < 1) {
          ZS_LOG_DEBUG(log("waiting for certificates get query to read data"))
          return;
        }


        mCompleted = true;

        // these are no longer needed...

        mServicesGetMonitor->cancel();
        mServicesGetMonitor.reset();

        mCertificatesGetMonitor->cancel();
        mCertificatesGetMonitor.reset();

        mRedirectionAttempts = 0;

        UseBootstrappedNetworkManagerPtr manager = mManager.lock();
        if (manager) {
          manager->notifyComplete(mThisWeak.lock());
        }
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::cancel()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("cancel called"))

        BootstrappedNetworkPtr pThis = mThisWeak.lock();

        if (!mCompleted) {
          setFailure(ErrorCode_UserCancelled);
        }
        mCompleted = true;

        if (mServicesGetMonitor) {
          mServicesGetMonitor->cancel();
          mServicesGetMonitor.reset();
        }
        mServicesGetQuery.reset();  // no need to cancel as the pending request will cause it to cancel

        if (mCertificatesGetMonitor) {
          mCertificatesGetMonitor->cancel();
          mCertificatesGetMonitor.reset();
        }

        mRedirectionAttempts = 0;

        mServiceTypeMap.clear();

        mCertificates.clear();

        // scope: cancel all pending requests
        {
          for (PendingRequestMap::iterator iter = mPendingRequests.begin(); iter != mPendingRequests.end(); ++iter) {
            IHTTPQueryPtr query = (*iter).first;

            query->cancel();
            query.reset();
          }
        }

        mPendingRequests.clear();
        mPendingRequestCookies.clear();

        if (pThis) {
          UseBootstrappedNetworkManagerPtr manager = mManager.lock();
          if (manager) {
            manager->notifyComplete(pThis);
          }
        }
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetwork::setFailure(
                                           WORD errorCode,
                                           const char *reason
                                           )
      {
        if (0 == errorCode) {
          errorCode = ErrorCode_InternalServerError;
        }
        if (!reason) {
          reason = toString((ErrorCodes)errorCode);
        }

        if (0 != mErrorCode) {
          ZS_LOG_WARNING(Detail, debug("failure reason already set") + ZS_PARAM("requesting error", errorCode) + ZS_PARAM("requesting reason", reason))
          return;
        }

        mCompleted = true;

        mErrorCode = errorCode;
        mErrorReason = reason;

        ZS_LOG_WARNING(Detail, debug("failure set"))
      }

      //-----------------------------------------------------------------------
      const Service::Method *BootstrappedNetwork::findServiceMethod(
                                                                    const char *inServiceType,
                                                                    const char *inMethod
                                                                    ) const
      {
        String serviceType(inServiceType);
        String method(inMethod);

        if ((serviceType.isEmpty()) ||
            (method.isEmpty())) {
          ZS_LOG_WARNING(Detail, log("missing service type or method") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", method))
          return NULL;
        }

        ServiceTypeMap::const_iterator found = mServiceTypeMap.find(serviceType);
        if (found == mServiceTypeMap.end()) {
          ZS_LOG_WARNING(Debug, log("service type not found") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", method))
          return NULL;
        }

        const ServiceMap &serviceMap = (*found).second;
        if (serviceMap.size() < 1) {
          ZS_LOG_WARNING(Debug, log("service method not found") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", method))
          return NULL;
        }

        // this only finds the first service rather than the entire array since most cases we only have one service per type
        const Service::MethodMap &methodMap = (*(serviceMap.begin())).second.mMethods;

        Service::MethodMap::const_iterator foundMethod = methodMap.find(method);
        if (foundMethod == methodMap.end()) {
          ZS_LOG_WARNING(Debug, log("service method not found") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", method))
          return NULL;
        }

        ZS_LOG_TRACE(log("service method found") + ZS_PARAM("type", serviceType) + ZS_PARAM("method", method))
        return &((*foundMethod).second);
      }

      //-----------------------------------------------------------------------
      MessagePtr BootstrappedNetwork::getMessageFromQuery(
                                                          IHTTPQueryPtr query,
                                                          MessagePtr originalMesssage,
                                                          SecureByteBlockPtr *outRawData
                                                          )
      {
        size_t size = query->getReadDataAvailableInBytes();

        MessagePtr message;

        SecureByteBlockPtr rawBuffer;

        FakeHTTPQueryPtr fakeQuery = FakeHTTPQuery::convert(query);
        if (fakeQuery) {
          message = fakeQuery->result();
          rawBuffer = fakeQuery->buffer();
        } else {
          size_t size = query->getReadDataAvailableInBytes();
          rawBuffer = SecureByteBlockPtr(new SecureByteBlock(size));
          query->readData(*rawBuffer, size);

          if (size > 0) {
            DocumentPtr doc = Document::createFromAutoDetect((const char *)((const BYTE *)(*rawBuffer)));
            if (query == mServicesGetQuery) {
              ElementPtr rootEl = doc->getFirstChildElement();
              // This is the only request in the system which could have come
              // from a HTTP GET request where no data was posted. As a result,
              // the returned message might not contain the message ID. Thus
              // force the message to have the same message ID as the request.
              if (originalMesssage) {
                IMessageHelper::setAttributeID(rootEl, originalMesssage->messageID());
              }
              IMessageHelper::setAttributeTimestamp(rootEl, zsLib::now());
            }
            message = Message::create(doc, mThisWeak.lock());
          }
        }

        if (ZS_IS_LOGGING(Detail)) {
          SecureByteBlock &buffer = (*rawBuffer);

          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("<----<----<----<----<----<---- HTTP RECEIVED DATA START <----<----<----<----<----<----<----"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + ZS_PARAM("from cache", (bool)fakeQuery) + ZS_PARAM("override message ID", fakeQuery ? (originalMesssage ? originalMesssage->messageID() : String()) : String()) + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          if (buffer.SizeInBytes() > 0) {
            ZS_LOG_BASIC(log("HTTP RECEIVED") + ZS_PARAM("size", buffer.SizeInBytes()) + ZS_PARAM("json in", ((const char *)(buffer.BytePtr()))))
          } else {
            ZS_LOG_BASIC(log("HTTP RECEIVED") + ZS_PARAM("size", size) + ZS_PARAM("json in", ""))
          }
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("<----<----<----<----<----<---- HTTP RECEIVED DATA END   <----<----<----<----<----<----<----"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        if (outRawData) {
          *outRawData = rawBuffer;
        }

        return message;
      }

      //-----------------------------------------------------------------------
      IHTTPQueryPtr BootstrappedNetwork::post(
                                              const char *url,
                                              MessagePtr message,
                                              const char *cachedCookieNameForResult,
                                              Time cacheExpires,
                                              bool forceAsGetRequest
                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!url)

        SecureByteBlockPtr buffer;

        if (message) {
          DocumentPtr doc = message->encode();
          buffer = IHelper::writeAsJSON(doc);
        }

        if (ZS_IS_LOGGING(Detail)) {
          const char *output = (buffer ? (const char *)buffer->BytePtr(): NULL);
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">---->---->---->---->---->----   HTTP SEND DATA START   >---->---->---->---->---->---->----"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("URL") + ZS_PARAM("url", url))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("HTTP SEND") + ZS_PARAM("json out", output))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">---->---->---->---->---->----   HTTP SEND DATA START   >---->---->---->---->---->---->----"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        SecureByteBlockPtr cacheBuffer;
        MessagePtr cacheResult = UseCache::getFromCache(
                                                        cachedCookieNameForResult,
                                                        message,
                                                        cacheBuffer,
                                                        mThisWeak.lock()
                                                        );

        FakeHTTPQueryPtr fakeQuery;
        IHTTPQueryPtr query;

        if (cacheResult) {
          fakeQuery = FakeHTTPQuery::create();

          fakeQuery->result(cacheResult);
          fakeQuery->buffer(cacheBuffer);

          // treat this query as having been handled immediately
          IHTTPQueryDelegateProxy::create(mThisWeak.lock())->onHTTPCompleted(fakeQuery);

          query = fakeQuery;
        } else {
          if ((IHelper::hasData(buffer) &&
               (!forceAsGetRequest))) {
            query = IHTTP::post(mThisWeak.lock(), ISettings::getString(OPENPEER_COMMON_SETTING_USER_AGENT), url, buffer->BytePtr(), buffer->SizeInBytes(), OPENPEER_STACK_BOOTSTRAPPED_NETWORK_DEFAULT_MIME_TYPE);
          } else {
            query = IHTTP::get(mThisWeak.lock(), ISettings::getString(OPENPEER_COMMON_SETTING_USER_AGENT), url);
          }
        }

        if (!query) {
          ZS_LOG_ERROR(Detail, log("failed to create HTTP query"))
          MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_BadRequest);
          if (!result) {
            ZS_LOG_WARNING(Detail, log("failed to create result for message"))
            return query;
          }
          IMessageMonitor::handleMessageReceived(result);
          return query;
        }

        mPendingRequests[query] = message;

        if ((!fakeQuery) &&
            (cachedCookieNameForResult)) {
          mPendingRequestCookies[query] = CookiePair(String(cachedCookieNameForResult), cacheExpires);
        }

        return query;
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
    #pragma mark IBootstrappedNetwork
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IBootstrappedNetwork::toDebug(IBootstrappedNetworkPtr network)
    {
      return internal::BootstrappedNetwork::toDebug(network);
    }

    //-------------------------------------------------------------------------
    IBootstrappedNetworkPtr IBootstrappedNetwork::prepare(
                                                          const char *domain,
                                                          IBootstrappedNetworkDelegatePtr delegate
                                                          )
    {
      return internal::IBootstrappedNetworkFactory::singleton().prepare(domain, delegate);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceCertificates
    #pragma mark

    //-------------------------------------------------------------------------
    IServiceCertificatesPtr IServiceCertificates::createServiceCertificatesFrom(IBootstrappedNetworkPtr preparedBootstrappedNetwork)
    {
      return internal::BootstrappedNetwork::createServiceCertificatesFrom(preparedBootstrappedNetwork);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceIdentity
    #pragma mark

    //-------------------------------------------------------------------------
    IServiceIdentityPtr IServiceIdentity::createServiceIdentityFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
    {
      return internal::BootstrappedNetwork::createServiceIdentityFrom(bootstrappedNetwork);
    }

    //-------------------------------------------------------------------------
    IServiceIdentityPtr IServiceIdentity::createServiceIdentityFromIdentityProofBundle(ElementPtr identityProofBundleEl)
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)
      String domain;
      stack::IHelper::getSignatureInfo(
                                       identityProofBundleEl,
                                       NULL,
                                       NULL,
                                       NULL,
                                       &domain,
                                       NULL
                                       );

      if (domain.isEmpty()) {
        ZS_LOG_WARNING(Detail, Log::Params("domain missing from signture", "IServiceIdentity"))
        return IServiceIdentityPtr();
      }

      if (!services::IHelper::isValidDomain(domain)) {
        ZS_LOG_WARNING(Detail, Log::Params("domain from signture is not valid", "IServiceIdentity") + ZS_PARAM("domain", domain))
        return IServiceIdentityPtr();
      }

      return createServiceIdentityFrom(IBootstrappedNetwork::prepare(domain));
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockbox
    #pragma mark

    //-------------------------------------------------------------------------
    IServiceLockboxPtr IServiceLockbox::createServiceLockboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
    {
      return internal::BootstrappedNetwork::createServiceLockboxFrom(bootstrappedNetwork);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailbox
    #pragma mark

    //-------------------------------------------------------------------------
    IServicePushMailboxPtr IServicePushMailbox::createServicePushMailboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
    {
      return internal::BootstrappedNetwork::createServicePushMailboxFrom(bootstrappedNetwork);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceSalt
    #pragma mark

    //-------------------------------------------------------------------------
    IServiceSaltPtr IServiceSalt::createServiceSaltFrom(IBootstrappedNetworkPtr bootstrappedNetwork)
    {
      return internal::BootstrappedNetwork::createServiceSaltFrom(bootstrappedNetwork);
    }
  }
}
