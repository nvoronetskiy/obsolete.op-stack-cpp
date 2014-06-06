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

#pragma once

#include <openpeer/stack/internal/types.h>

#include <openpeer/stack/message/bootstrapper/ServicesGetResult.h>
#include <openpeer/stack/message/certificates/CertificatesGetResult.h>

#include <openpeer/stack/message/types.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServiceCertificates.h>
#include <openpeer/stack/IServiceIdentity.h>
#include <openpeer/stack/IServiceLockbox.h>
#include <openpeer/stack/IServiceNamespaceGrant.h>
#include <openpeer/stack/IServicePushMailbox.h>
#include <openpeer/stack/IServiceSalt.h>
#include <openpeer/stack/IMessageSource.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IDNS.h>
#include <openpeer/services/IHTTP.h>
#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

#define OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_OVER_INSECURE_HTTP   "openpeer/stack/bootstrapper-force-well-known-over-insecure-http"
#define OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_USING_POST           "openpeer/stack/bootstrapper-force-well-known-using-post"
#define OPENPEER_STACK_SETTING_BOOTSTREPPER_SKIP_SIGNATURE_VALIDATION                     "openpeer/stack/bootstrapper-skip-signature-validation"

#define OPENPEER_STACK_SETTING_BOOSTRAPPER_SERVICES_GET_URL_METHOD_NAME "openpeer-services-get"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkManagerForBootstrappedNetwork;
      interaction ICacheForServices;

      using stack::message::bootstrapper::ServicesGetResult;
      using stack::message::bootstrapper::ServicesGetResultPtr;
      using stack::message::certificates::CertificatesGetResult;
      using stack::message::certificates::CertificatesGetResultPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkForAccount
      #pragma mark

      interaction IBootstrappedNetworkForAccount
      {
        virtual String getDomain() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkForServices
      #pragma mark

      interaction IBootstrappedNetworkForServices
      {
        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, ForServices)

        static ElementPtr toDebug(ForServicesPtr network);

        static ForServicesPtr prepare(
                                      const char *domain,
                                      IBootstrappedNetworkDelegatePtr delegate = IBootstrappedNetworkDelegatePtr()
                                      );

        virtual String getDomain() const = 0;

        virtual bool isPreparationComplete() const = 0;

        virtual bool wasSuccessful(
                                   WORD *outErrorCode = NULL,
                                   String *outErrorReason = NULL
                                   ) const = 0;

        virtual bool sendServiceMessage(
                                        const char *serviceType,
                                        const char *serviceMethodName,
                                        message::MessagePtr message,
                                        const char *cachedCookieNameForResult = NULL,
                                        Time cacheExpires = Time()
                                        ) = 0;

        virtual String getServiceURI(
                                     const char *serviceType,
                                     const char *serviceMethodName
                                     ) const = 0;

        virtual bool supportsRolodex() const = 0;

        virtual bool isValidSignature(ElementPtr signedElement) const = 0;

        virtual bool isValidSignature(
                                      const String &id,
                                      const String &domain,
                                      const String &service,
                                      SecureByteBlockPtr buffer,
                                      SecureByteBlockPtr bufferSigned
                                      ) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkForBootstrappedNetworkManager
      #pragma mark

      interaction IBootstrappedNetworkForBootstrappedNetworkManager
      {
        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForBootstrappedNetworkManager, ForBootstrappedNetworkManager)

        virtual PUID getID() const = 0;

        virtual String getDomain() const = 0;

        virtual bool isPreparationComplete() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetwork
      #pragma mark

      class BootstrappedNetwork : public Noop,
                                  public MessageQueueAssociator,
                                  public SharedRecursiveLock,
                                  public IBootstrappedNetwork,
                                  public IServiceCertificates,
                                  public IServiceIdentity,
                                  public IServiceLockbox,
                                  public IServicePushMailbox,
                                  public IServiceSalt,
                                  public IBootstrappedNetworkForAccount,
                                  public IBootstrappedNetworkForServices,
                                  public IBootstrappedNetworkForBootstrappedNetworkManager,
                                  public IWakeDelegate,
                                  public IHTTPQueryDelegate,
                                  public IMessageSource,
                                  public IMessageMonitorResultDelegate<ServicesGetResult>,
                                  public IMessageMonitorResultDelegate<CertificatesGetResult>
      {
      public:
        friend interaction IBootstrappedNetworkFactory;
        friend interaction IBootstrappedNetwork;
        friend interaction IServiceCertificates;
        friend interaction IServiceIdentity;
        friend interaction IServiceLockbox;
        friend interaction IServiceSalt;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkManagerForBootstrappedNetwork, UseBootstrappedNetworkManager)
        ZS_DECLARE_TYPEDEF_PTR(ICacheForServices, UseCache)

        typedef zsLib::IMessageQueuePtr IMessageQueuePtr;
        typedef message::ServiceMap ServiceMap;
        typedef message::Service Service;
        typedef message::MessagePtr MessagePtr;
        typedef message::CertificateMap CertificateMap;
        typedef message::ServiceTypeMap ServiceTypeMap;

        enum ErrorCodes
        {
          ErrorCode_BadRequest =          IHTTP::HTTPStatusCode_BadRequest,
          ErrorCode_NotFound =            IHTTP::HTTPStatusCode_NotFound,
          ErrorCode_InternalServerError = IHTTP::HTTPStatusCode_InternalServerError,
          ErrorCode_ServiceUnavailable =  IHTTP::HTTPStatusCode_ServiceUnavailable,
          ErrorCode_UserCancelled =       IHTTP::HTTPStatusCode_ClientClosedRequest,
          ErrorCode_LoopDetected =        IHTTP::HTTPStatusCode_LoopDetected,
        };

        const char *toString(ErrorCodes errorCode);

      protected:
        BootstrappedNetwork(
                            const SharedRecursiveLock &inLock,
                            IMessageQueuePtr queue,
                            const char *domain
                            );
        
        BootstrappedNetwork(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}
        
        void init();

      public:
        ~BootstrappedNetwork();

        static BootstrappedNetworkPtr convert(IBootstrappedNetworkPtr network);
        static BootstrappedNetworkPtr convert(IServiceCertificatesPtr network);
        static BootstrappedNetworkPtr convert(IServiceIdentityPtr network);
        static BootstrappedNetworkPtr convert(IServiceLockboxPtr network);
        static BootstrappedNetworkPtr convert(IServicePushMailboxPtr network);
        static BootstrappedNetworkPtr convert(IServiceSaltPtr network);
        static BootstrappedNetworkPtr convert(ForServicesPtr network);
        static BootstrappedNetworkPtr convert(ForBootstrappedNetworkManagerPtr network);

        typedef std::map<IHTTPQueryPtr, message::MessagePtr> PendingRequestMap;

        typedef String CookieName;
        typedef Time CookieExpires;
        typedef std::pair<CookieName, CookieExpires> CookiePair;

        typedef std::map<IHTTPQueryPtr, CookiePair> PendingRequestCookieMap;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IBootstrappedNetwork
        #pragma mark

        static ElementPtr toDebug(IBootstrappedNetworkPtr network);

        static BootstrappedNetworkPtr prepare(
                                              const char *domain,
                                              IBootstrappedNetworkDelegatePtr delegate
                                              );

        virtual PUID getID() const {return mID;}

        virtual String getDomain() const;

        virtual bool isPreparationComplete() const;
        virtual bool wasSuccessful(
                                   WORD *outErrorCode = NULL,
                                   String *outErrorReason = NULL
                                   ) const;

        // (duplicate) virtual void cancel();

        // use IMessageMonitor to monitor the result (if result is important)
        virtual bool sendServiceMessage(
                                        const char *serviceType,
                                        const char *serviceMethodName,
                                        message::MessagePtr message,
                                        const char *cachedCookieNameForResult = NULL,
                                        Time cacheExpires = Time()
                                        );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IServiceCertificates
        #pragma mark

        static IServiceCertificatesPtr createServiceCertificatesFrom(IBootstrappedNetworkPtr preparedBootstrappedNetwork);

        // (duplicate) virtual PUID getID() const;

        virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const;

        virtual bool isValidSignature(ElementPtr signedElement) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IServiceIdentity
        #pragma mark

        static IServiceIdentityPtr createServiceIdentityFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const;

        // (duplicate) virtual String getDomain() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IServiceLockbox
        #pragma mark

        static IServiceLockboxPtr createServiceLockboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IServicePushMailbox
        #pragma mark

        static IServicePushMailboxPtr createServicePushMailboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IServiceSalt
        #pragma mark

        static IServiceSaltPtr createServiceSaltFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IBootstrappedNetworkForAccount
        #pragma mark

        // (duplicate) virtual String getDomain() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IBootstrappedNetworkForServices
        #pragma mark

        // (duplicate) virtual String getDomain() const;

        // (duplicate) static BootstrappedNetworkPtr prepare(
        //                                                   const char *domain,
        //                                                   IBootstrappedNetworkDelegatePtr delegate
        //                                                   );

        // (duplicate) virtual bool isPreparationComplete() const;

        // (duplicate) virtual bool wasSuccessful(
        //                                        WORD *outErrorCode = NULL,
        //                                        String *outErrorReason = NULL
        //                                        ) const;

        // (duplicate) virtual bool sendServiceMessage(
        //                                             const char *serviceType,
        //                                             const char *serviceMethodName,
        //                                             message::MessagePtr message
        //                                             const char *cachedCookieNameForResult = NULL,
        //                                             Time cacheExpires = Time()
        //                                             );

        // (duplicate) virtual bool isValidSignature(ElementPtr signedElement) const;

        virtual String getServiceURI(
                                     const char *serviceType,
                                     const char *serviceMethodName
                                     ) const;

        virtual bool supportsRolodex() const;

        virtual bool isValidSignature(
                                      const String &id,
                                      const String &domain,
                                      const String &service,
                                      SecureByteBlockPtr buffer,
                                      SecureByteBlockPtr bufferSigned
                                      ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IBootstrappedNetworkForBootstrappedNetworkManager
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual String getDomain() const;

        // (duplicate) virtual bool isPreparationComplete() const;
        // (duplicate) virtual bool wasSuccessful(
        //                                        WORD *outErrorCode = NULL,
        //                                        String *outErrorReason = NULL
        //                                        ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork =>IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => IHTTPQueryDelegate
        #pragma mark

        virtual void onHTTPReadDataAvailable(IHTTPQueryPtr query);
        virtual void onHTTPCompleted(IHTTPQueryPtr query);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Identity => IMessageMonitorResultDelegate<ServicesGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        ServicesGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             ServicesGetResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Identity => IMessageMonitorResultDelegate<ServicesGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        CertificatesGetResultPtr result
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             CertificatesGetResultPtr ignore, // will always be NULL
                                                             MessageResultPtr result
                                                             );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => (internal)
        #pragma mark

        RecursiveLock &getLock() const;

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void go();
        void reuse();
        void dontUse();

        void step();
        void cancel();
        void setFailure(
                        WORD errorCode,
                        const char *reason = NULL
                        );

        const Service::Method *findServiceMethod(
                                                 const char *serviceType,
                                                 const char *method
                                                 ) const;

        MessagePtr getMessageFromQuery(
                                       IHTTPQueryPtr query,
                                       MessagePtr originalMesssage,
                                       SecureByteBlockPtr *outRawData = NULL
                                       );

        IHTTPQueryPtr post(
                           const char *url,
                           MessagePtr message,
                           const char *cachedCookieNameForResult = NULL,
                           Time cacheExpires = Time(),
                           bool forceAsGetRequest = false
                           );

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetwork => (data)
        #pragma mark

        AutoPUID mID;
        BootstrappedNetworkWeakPtr mThisWeak;
        String mDomain;

        UseBootstrappedNetworkManagerWeakPtr mManager;

        bool mCompleted;

        WORD mErrorCode;
        String mErrorReason;

        IHTTPQueryPtr mServicesGetQuery;
        IMessageMonitorPtr mServicesGetMonitor;
        IMessageMonitorPtr mCertificatesGetMonitor;

        ULONG mRedirectionAttempts;

        ServiceTypeMap mServiceTypeMap;
        CertificateMap mCertificates;

        PendingRequestMap mPendingRequests;
        PendingRequestCookieMap mPendingRequestCookies;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkFactory
      #pragma mark

      interaction IBootstrappedNetworkFactory
      {
        static IBootstrappedNetworkFactory &singleton();

        virtual BootstrappedNetworkPtr prepare(
                                               const char *domain,
                                               IBootstrappedNetworkDelegatePtr delegate
                                               );
      };

    }
  }
}

