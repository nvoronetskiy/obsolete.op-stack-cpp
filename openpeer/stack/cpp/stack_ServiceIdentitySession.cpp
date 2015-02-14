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

#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/message/identity/IdentityAccessWindowRequest.h>
#include <openpeer/stack/message/identity/IdentityAccessWindowResult.h>
#include <openpeer/stack/message/identity/IdentityAccessStartNotify.h>
#include <openpeer/stack/message/identity/IdentityAccessCompleteNotify.h>
#include <openpeer/stack/message/identity/IdentityAccessNamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/identity/IdentityLookupUpdateRequest.h>
#include <openpeer/stack/message/identity/IdentityAccessRolodexCredentialsGetRequest.h>
#include <openpeer/stack/message/identity-lookup/IdentityLookupRequest.h>
#include <openpeer/stack/message/rolodex/RolodexAccessRequest.h>
#include <openpeer/stack/message/rolodex/RolodexNamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/rolodex/RolodexContactsGetRequest.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/message/IMessageHelper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Promise.h>
#include <zsLib/Stringize.h>

#include <regex>

#define OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS (60*2)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_PERCENTAGE_TIME_REMAINING_BEFORE_RESIGN_IDENTITY_REQUIRED (20) // at 20% of the remaining on the certificate before expiry, resign

#define OPENPEER_STACK_SERVICE_IDENTITY_SIGN_CREATE_SHOULD_NOT_BE_BEFORE_NOW_IN_HOURS (72)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_CONSUMED_TIME_PERCENTAGE_BEFORE_IDENTITY_PROOF_REFRESH (80)

#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_DOWNLOAD_FROZEN_VALUE "FREEZE-"
#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS ((60)*2)
#define OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS (((60)*60) * 24)

#define OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_NOT_SUPPORTED_FOR_IDENTITY (404)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    using zsLib::string;
    using zsLib::Hours;
    using stack::message::IMessageHelper;

    namespace internal
    {
      typedef stack::internal::IStackForInternal UseStack;

      typedef IServiceIdentitySessionForServiceLockbox::ForLockboxPtr ForLockboxPtr;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(stack::IHelper, UseStackHelper)

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessWindowRequest)
      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessWindowResult)
      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessStartNotify)
      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessCompleteNotify)
      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessNamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::identity, IdentityLookupUpdateRequest)
      ZS_DECLARE_USING_PTR(message::identity, IdentityAccessRolodexCredentialsGetRequest)

      ZS_DECLARE_USING_PTR(message::identity_lookup, IdentityLookupRequest)

      ZS_DECLARE_USING_PTR(message::rolodex, RolodexAccessRequest)
      ZS_DECLARE_USING_PTR(message::rolodex, RolodexNamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::rolodex, RolodexContactsGetRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-------------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "IServiceIdentity");
      }
      
      //-----------------------------------------------------------------------
      static char getSafeSplitChar(const String &identifier)
      {
        const char *testChars = ",; :./\\*#!$%&@?~+=-_|^<>[]{}()";

        while (*testChars) {
          if (String::npos == identifier.find(*testChars)) {
            return *testChars;
          }

          ++testChars;
        }

        return 0;
      }
      
      //-----------------------------------------------------------------------
      static bool isSame(
                         IPeerFilePublicPtr peerFilePublic1,
                         IPeerFilePublicPtr peerFilePublic2
                         )
      {
        if (!peerFilePublic1) return false;
        if (!peerFilePublic2) return false;

        return peerFilePublic1->getPeerURI() == peerFilePublic2->getPeerURI();
      }

      //-------------------------------------------------------------------------
      static bool extractAndVerifyProof(
                                        ElementPtr identityProofBundleEl,
                                        IPeerFilePublicPtr peerFilePublic,
                                        String *outPeerURI,
                                        String *outIdentityURI,
                                        String *outStableID,
                                        Time *outCreated,
                                        Time *outExpires
                                        )
      {
        if (outPeerURI) {
          *outPeerURI = String();
        }
        if (outIdentityURI) {
          *outIdentityURI = String();
        }
        if (outStableID) {
          *outStableID = String();
        }
        if (outCreated) {
          *outCreated = Time();
        }
        if (outExpires) {
          *outExpires = Time();
        }

        ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)

        try {
          ElementPtr identityProofEl = identityProofBundleEl->findFirstChildElementChecked("identityProof");
          ElementPtr contactProofBundleEl = identityProofEl->findFirstChildElementChecked("contactProofBundle");
          ElementPtr contactProofEl = contactProofBundleEl->findFirstChildElementChecked("contactProof");

          ElementPtr stableIDEl = contactProofEl->findFirstChildElement("stableID");      // optional

          ElementPtr contactEl = contactProofEl->findFirstChildElementChecked("contact");
          ElementPtr uriEl = contactProofEl->findFirstChildElementChecked("uri");
          ElementPtr createdEl = contactProofEl->findFirstChildElementChecked("created");
          ElementPtr expiresEl = contactProofEl->findFirstChildElementChecked("expires");

          Time created = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(createdEl));
          Time expires = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(expiresEl));

          if ((outStableID) &&
              (stableIDEl)) {
            *outStableID = IMessageHelper::getElementTextAndDecode(stableIDEl);
          }
          if (outCreated) {
            *outCreated = created;
          }
          if (outExpires) {
            *outExpires = expires;
          }

          if (outIdentityURI) {
            *outIdentityURI = IMessageHelper::getElementTextAndDecode(uriEl);
          }

          String peerURI = IMessageHelper::getElementTextAndDecode(contactEl);

          if (outPeerURI) {
            *outPeerURI = peerURI;
          }

          if (peerFilePublic) {
            if (peerURI != peerFilePublic->getPeerURI()) {
              ZS_LOG_WARNING(Detail, slog("peer URI check failed") + ZS_PARAM("bundle URI", peerURI) + ZS_PARAM("peer file URI", peerFilePublic->getPeerURI()))
              return false;
            }
            if (peerFilePublic->verifySignature(contactProofEl)) {
              ZS_LOG_WARNING(Detail, slog("signature validation failed") + ZS_PARAM("peer URI", peerURI))
              return false;
            }
          }

          Time tick = zsLib::now();
          if (created < tick + Hours(OPENPEER_STACK_SERVICE_IDENTITY_SIGN_CREATE_SHOULD_NOT_BE_BEFORE_NOW_IN_HOURS)) {
            ZS_LOG_WARNING(Detail, slog("creation date is invalid") + ZS_PARAM("created", created) + ZS_PARAM("now", tick))
            return false;
          }

          if (tick > expires) {
            ZS_LOG_WARNING(Detail, slog("signature expired") + ZS_PARAM("expires", expires) + ZS_PARAM("now", tick))
            return false;
          }
          
        } catch (zsLib::XML::Exceptions::CheckFailed &) {
          ZS_LOG_WARNING(Detail, slog("check failure"))
          return false;
        }

        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionForServiceLockbox
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IServiceIdentitySessionForServiceLockbox::toDebug(ForLockboxPtr session)
      {
        return IServiceIdentitySession::toDebug(ServiceIdentitySession::convert(session));
      }

      //-----------------------------------------------------------------------
      ForLockboxPtr IServiceIdentitySessionForServiceLockbox::reload(
                                                                     BootstrappedNetworkPtr provider,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr existingLockbox,
                                                                     const char *identityURI,
                                                                     const char *reloginKey
                                                                     )
      {
        return IServiceIdentitySessionFactory::singleton().reload(provider, grantSession, existingLockbox, identityURI, reloginKey);
        return ServiceIdentitySessionPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceIdentitySession::ServiceIdentitySession(
                                                     IMessageQueuePtr queue,
                                                     IServiceIdentitySessionDelegatePtr delegate,
                                                     UseBootstrappedNetworkPtr providerNetwork,
                                                     UseBootstrappedNetworkPtr identityNetwork,
                                                     ServiceNamespaceGrantSessionPtr grantSession,
                                                     UseServiceLockboxSessionPtr existingLockbox,
                                                     const char *outerFrameURLUponReload
                                                     ) :
        zsLib::MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(delegate ? IServiceIdentitySessionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate) : IServiceIdentitySessionDelegatePtr()),
        mAssociatedLockbox(existingLockbox),
        mProviderBootstrappedNetwork(providerNetwork),
        mIdentityBootstrappedNetwork(identityNetwork),
        mGrantSession(grantSession),
        mCurrentState(SessionState_Pending),
        mLastReportedState(SessionState_Pending),
        mOuterFrameURLUponReload(outerFrameURLUponReload),
        mFrozenVersion(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_DOWNLOAD_FROZEN_VALUE + UseServicesHelper::randomString(32)),
        mNextRetryAfterFailureTime(Seconds(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS))
      {
        ZS_LOG_BASIC(log("created"))
        mRolodexInfo.mVersion = mFrozenVersion;
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySession::~ServiceIdentitySession()
      {
        if(isNoop()) return;

        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::init()
      {
        AutoRecursiveLock lock(*this);

        UseServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();

        if (lockbox) {
          ServiceIdentitySessionPtr pThis = mThisWeak.lock();
          mAssociatedLockboxSubscription = lockbox->subscribe(pThis);
        }

        if (mIdentityBootstrappedNetwork) {
          UseBootstrappedNetwork::prepare(mIdentityBootstrappedNetwork->getDomain(), mThisWeak.lock());
        }
        if (mProviderBootstrappedNetwork) {
          UseBootstrappedNetwork::prepare(mProviderBootstrappedNetwork->getDomain(), mThisWeak.lock());
        }

        // one or the other must be valid or a login is not possible
        ZS_THROW_BAD_STATE_IF((!mIdentityBootstrappedNetwork) && (!mProviderBootstrappedNetwork))
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::convert(IServiceIdentitySessionPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceIdentitySession, object);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::convert(ForLockboxPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceIdentitySession, object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceIdentitySession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServiceIdentitySession::toDebug(IServiceIdentitySessionPtr session)
      {
        if (!session) return ElementPtr();
        return ServiceIdentitySession::convert(session)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::loginWithIdentity(
                                                                          IServiceIdentitySessionDelegatePtr delegate,
                                                                          IServiceIdentityPtr provider,
                                                                          IServiceNamespaceGrantSessionPtr grantSession,
                                                                          IServiceLockboxSessionPtr existingLockbox,
                                                                          const char *outerFrameURLUponReload,
                                                                          const char *identityURI_or_identityBaseURI
                                                                          )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)

        if (identityURI_or_identityBaseURI) {
          if (!provider) {
            if (IServiceIdentity::isLegacy(identityURI_or_identityBaseURI)) {
              ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
            }
          }

          if ((!IServiceIdentity::isValid(identityURI_or_identityBaseURI)) &&
              (!IServiceIdentity::isValidBase(identityURI_or_identityBaseURI))) {
            ZS_LOG_ERROR(Detail, slog("identity URI specified is not valid") + ZS_PARAM("uri", identityURI_or_identityBaseURI))
            return ServiceIdentitySessionPtr();
          }
        } else {
          ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
        }

        UseBootstrappedNetworkPtr identityNetwork;
        UseBootstrappedNetworkPtr providerNetwork = BootstrappedNetwork::convert(provider);

        if (identityURI_or_identityBaseURI) {
          if (IServiceIdentity::isValid(identityURI_or_identityBaseURI)) {
            if (!IServiceIdentity::isLegacy(identityURI_or_identityBaseURI)) {
              String domain;
              String identifier;
              IServiceIdentity::splitURI(identityURI_or_identityBaseURI, domain, identifier);
              identityNetwork = UseBootstrappedNetwork::prepare(domain);
            }
          }
        }

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(UseStack::queueStack(), delegate, providerNetwork, identityNetwork, ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(existingLockbox), outerFrameURLUponReload));
        pThis->mThisWeak = pThis;
        if (identityURI_or_identityBaseURI) {
          if (IServiceIdentity::isValidBase(identityURI_or_identityBaseURI)) {
            pThis->mIdentityInfo.mBase = identityURI_or_identityBaseURI;
          } else {
            pThis->mIdentityInfo.mURI = identityURI_or_identityBaseURI;
          }
        }
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::loginWithIdentityPreauthorized(
                                                                                       IServiceIdentitySessionDelegatePtr delegate,
                                                                                       IServiceIdentityPtr provider,
                                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                                       IServiceLockboxSessionPtr existingLockbox,
                                                                                       const char *identityURI,
                                                                                       const Token &identityToken
                                                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityToken.hasData())

        if (!provider) {
          if (IServiceIdentity::isLegacy(identityURI)) {
            ZS_THROW_INVALID_ARGUMENT_IF(!provider) // provider can normally be derived from the identity but only if the identity contains a provider
          }
        }

        if (!IServiceIdentity::isValid(identityURI)) {
          ZS_LOG_ERROR(Detail, slog("identity URI specified is not valid") + ZS_PARAM("uri", identityURI))
          return ServiceIdentitySessionPtr();
        }

        UseBootstrappedNetworkPtr identityNetwork;
        UseBootstrappedNetworkPtr providerNetwork = BootstrappedNetwork::convert(provider);

        if (IServiceIdentity::isValid(identityURI)) {
          if (!IServiceIdentity::isLegacy(identityURI)) {
            String domain;
            String identifier;
            IServiceIdentity::splitURI(identityURI, domain, identifier);
            identityNetwork = UseBootstrappedNetwork::prepare(domain);
          }
        }

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(UseStack::queueStack(), delegate, providerNetwork, identityNetwork, ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(existingLockbox), NULL));
        pThis->mThisWeak = pThis;
        pThis->mIdentityInfo.mURI = identityURI;
        pThis->mIdentityInfo.mToken = identityToken;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceIdentityPtr ServiceIdentitySession::getService() const
      {
        AutoRecursiveLock lock(*this);
        if (mIdentityBootstrappedNetwork) {
          return BootstrappedNetwork::convert(mIdentityBootstrappedNetwork);
        }
        return BootstrappedNetwork::convert(mProviderBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      IServiceIdentitySession::SessionStates ServiceIdentitySession::getState(
                                                                              WORD *outLastErrorCode,
                                                                              String *outLastErrorReason
                                                                              ) const
      {
        AutoRecursiveLock lock(*this);
        if (outLastErrorCode) *outLastErrorCode = mLastError;
        if (outLastErrorReason) *outLastErrorReason = mLastErrorReason;
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isDelegateAttached() const
      {
        AutoRecursiveLock lock(*this);
        return ((bool)mDelegate);
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::attachDelegate(
                                                  IServiceIdentitySessionDelegatePtr delegate,
                                                  const char *outerFrameURLUponReload
                                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!outerFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        ZS_LOG_DEBUG(log("attach delegate called") + ZS_PARAM("frame URL", outerFrameURLUponReload))

        AutoRecursiveLock lock(*this);

        mDelegate = IServiceIdentitySessionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate);
        mOuterFrameURLUponReload = (outerFrameURLUponReload ? String(outerFrameURLUponReload) : String());

        try {
          if (mCurrentState != mLastReportedState) {
            mDelegate->onServiceIdentitySessionStateChanged(mThisWeak.lock(), mCurrentState);
            mLastReportedState = mCurrentState;
          }
          if (mPendingMessagesToDeliver.size() > 0) {
            mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
          }
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::attachDelegateAndPreauthorizeLogin(
                                                                      IServiceIdentitySessionDelegatePtr delegate,
                                                                      const Token &identityToken
                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityToken.hasData())

        ZS_LOG_DEBUG(log("attach delegate and preautothorize login called") + identityToken.toDebug())

        AutoRecursiveLock lock(*this);

        mDelegate = IServiceIdentitySessionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate);

        mIdentityInfo.mToken = identityToken;

        try {
          if (mCurrentState != mLastReportedState) {
            mDelegate->onServiceIdentitySessionStateChanged(mThisWeak.lock(), mCurrentState);
            mLastReportedState = mCurrentState;
          }
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getIdentityURI() const
      {
        AutoRecursiveLock lock(*this);
        return mIdentityInfo.mURI.hasData() ? mIdentityInfo.mURI : mIdentityInfo.mBase;
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getIdentityProviderDomain() const
      {
        AutoRecursiveLock lock(*this);
        if (mActiveBootstrappedNetwork) {
          return mActiveBootstrappedNetwork->getDomain();
        }
        return mProviderBootstrappedNetwork->getDomain();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::getIdentityInfo(IdentityInfo &outIdentityInfo) const
      {
        AutoRecursiveLock lock(*this);

        outIdentityInfo = mPreviousLookupInfo;
        outIdentityInfo.mergeFrom(mIdentityInfo, true);
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getInnerBrowserWindowFrameURL() const
      {
        AutoRecursiveLock lock(*this);
        if (!mActiveBootstrappedNetwork) {
          ZS_LOG_WARNING(Detail, log("attempted to get inner browser window frame URL but no active bootstrapper is ready yet"))
          return String();
        }

        String result = mActiveBootstrappedNetwork->getServiceURI("identity", "identity-access-inner-frame");

        ZS_LOG_TRACE(log("obtained inner browser window frame url") + ZS_PARAM("inner frame url", result))
        return result;
      }

      //-----------------------------------------------------------------------
      String ServiceIdentitySession::getBrowserWindowRedirectURL() const
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("obtained browser window redirect url") + ZS_PARAM("url", mNeedsBrowserWindowRedirectURL))
        return mNeedsBrowserWindowRedirectURL;
      }
      
      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyBrowserWindowVisible()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("browser window visible"))
        mBrowserWindowVisible = true;
        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyBrowserWindowRedirected()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("browser window redirected"))
        mNeedsBrowserWindowRedirectURL.clear();
        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyBrowserWindowClosed()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("browser window close called"))
        mBrowserWindowClosed = true;
        step();
      }

      //-----------------------------------------------------------------------
      DocumentPtr ServiceIdentitySession::getNextMessageForInnerBrowerWindowFrame()
      {
        AutoRecursiveLock lock(*this);
        if (mPendingMessagesToDeliver.size() < 1) return DocumentPtr();

        DocumentMessagePair resultPair = mPendingMessagesToDeliver.front();
        mPendingMessagesToDeliver.pop_front();

        DocumentPtr result = resultPair.first;
        MessagePtr message = resultPair.second;

        if (ZS_IS_LOGGING(Detail)) {
          GeneratorPtr generator = Generator::createJSONGenerator();
          std::unique_ptr<char[]> jsonText = generator->write(result);
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MESSAGE TO INNER FRAME (START) >>>>>>>>>>>>>>>>>>>>>>>>>>>>>"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("sending inner frame message") + ZS_PARAM("json out", (CSTR)(jsonText.get())))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  MESSAGE TO INNER FRAME (END)  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        if (mDelegate) {
          if (mPendingMessagesToDeliver.size() > 0) {
            try {
              ZS_LOG_DEBUG(log("notifying about another pending message for the inner frame"))
              mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
            } catch (IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, log("delegate gone"))
            }
          }
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::handleMessageFromInnerBrowserWindowFrame(DocumentPtr unparsedMessage)
      {
        MessagePtr message = Message::create(unparsedMessage, mThisWeak.lock());

        if (ZS_IS_LOGGING(Detail)) {
          GeneratorPtr generator = Generator::createJSONGenerator();
          std::unique_ptr<char[]> jsonText = generator->write(unparsedMessage);
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("<<<<<<<<<<<<<<<<<<<<<<<<<<<< MESSAGE FROM INNER FRAME (START) <<<<<<<<<<<<<<<<<<<<<<<<<<<<<"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("handling message from inner frame") + ZS_PARAM("json in", (CSTR)(jsonText.get())))
          ZS_LOG_BASIC(log("<<<<<<<<<<<<<<<<<<<<<<<<<<<<  MESSAGE FROM INNER FRAME (END)  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        if (IMessageMonitor::handleMessageReceived(message)) {
          ZS_LOG_DEBUG(log("message handled via message monitor"))
          return;
        }

        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("cannot handle message when shutdown"))
          return;
        }

        IdentityAccessWindowRequestPtr windowRequest = IdentityAccessWindowRequest::convert(message);
        if (windowRequest) {
          // send a result immediately
          IdentityAccessWindowResultPtr result = IdentityAccessWindowResult::create(windowRequest);
          sendInnerWindowMessage(result);

          if (windowRequest->ready()) {
            ZS_LOG_DEBUG(log("notified browser window ready"))
            mBrowserWindowReady = true;
          }

          if (windowRequest->visible()) {
            ZS_LOG_DEBUG(log("notified browser window needs to be made visible"))
            mNeedsBrowserWindowVisible = true;
          }
          if (windowRequest->redirectURL().hasData()) {
            ZS_LOG_DEBUG(log("notified browser window needs to be redirected") + ZS_PARAM("url", windowRequest->redirectURL()))
            mNeedsBrowserWindowRedirectURL = windowRequest->redirectURL();
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        IdentityAccessCompleteNotifyPtr completeNotify = IdentityAccessCompleteNotify::convert(message);
        if (completeNotify) {

          const IdentityInfo &identityInfo = completeNotify->identityInfo();
          const LockboxInfo &lockboxInfo = completeNotify->lockboxInfo();

          mAccessChallengeInfo = completeNotify->namespaceGrantChallengeInfo();
          mGrantQueryAccess = mGrantSession->query(mThisWeak.lock(), mAccessChallengeInfo);

          mEncryptedUserSpecificPassphrase = completeNotify->encryptedUserSpecificPassphrase();
          mEncryptionKeyUponGrantProof = completeNotify->encryptionKeyUponGrantProof();
          mEncryptionKeyUponGrantProofHash = completeNotify->encryptionKeyUponGrantProofHash();

          ZS_LOG_DEBUG(log("received complete notification") + identityInfo.toDebug() + lockboxInfo.toDebug())

          mIdentityInfo.mergeFrom(identityInfo, true);
          mLockboxInfo.mergeFrom(lockboxInfo, true);

          if ((!mIdentityInfo.mToken.hasData()) ||
              (mIdentityInfo.mURI.isEmpty())) {
            ZS_LOG_ERROR(Detail, log("failed to obtain access token/secret"))
            setError(IHTTP::HTTPStatusCode_Forbidden, "Login via identity provider failed");
            cancel();
            return;
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        if ((message->isRequest()) ||
            (message->isNotify())) {
          ZS_LOG_WARNING(Debug, log("request was not understood"))
          MessageResultPtr result = MessageResult::create(message, IHTTP::HTTPStatusCode_NotImplemented);
          sendInnerWindowMessage(result);
          return;
        }

        ZS_LOG_WARNING(Detail, log("message result ignored since it was not monitored"))
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::startRolodexDownload(const char *inLastDownloadedVersion)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("allowing rolodex downloading to start"))

        mRolodexInfo.mVersion = String(inLastDownloadedVersion);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::refreshRolodexContacts()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("forcing rolodex server to refresh its contact list"))

        mRolodexInfo.mUpdateNext = Time();  // reset when the next update is allowed to occur
        mForceRefresh = zsLib::now();
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::getDownloadedRolodexContacts(
                                                                bool &outFlushAllRolodexContacts,
                                                                String &outVersionDownloaded,
                                                                IdentityInfoListPtr &outRolodexContacts
                                                                )
      {
        AutoRecursiveLock lock(*this);

        bool refreshed = (Time() != mFreshDownload);
        mFreshDownload = Time();

        if ((mIdentities.size() < 1) &&
            (!refreshed)) {
          ZS_LOG_DEBUG(log("no contacts downloaded"))
          return false;
        }

        ZS_LOG_DEBUG(log("returning downloaded contacts") + ZS_PARAM("total", mIdentities.size()))

        outFlushAllRolodexContacts = refreshed;
        outVersionDownloaded = mRolodexInfo.mVersion;

        outRolodexContacts = IdentityInfoListPtr(new IdentityInfoList);

        (*outRolodexContacts) = mIdentities;
        mIdentities.clear();

        return true;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        mGraciousShutdownReference.reset();

        mPendingMessagesToDeliver.clear();

        if (mIdentityAccessNamespaceGrantChallengeValidateMonitor) {
          mIdentityAccessNamespaceGrantChallengeValidateMonitor->cancel();
          mIdentityAccessNamespaceGrantChallengeValidateMonitor.reset();
        }

        if (mIdentityLookupUpdateMonitor) {
          mIdentityLookupUpdateMonitor->cancel();
          mIdentityLookupUpdateMonitor.reset();
        }

        if (mIdentityAccessRolodexCredentialsGetMonitor) {
          mIdentityAccessRolodexCredentialsGetMonitor->cancel();
          mIdentityAccessRolodexCredentialsGetMonitor.reset();
        }

        if (mIdentityLookupMonitor) {
          mIdentityLookupMonitor->cancel();
          mIdentityLookupMonitor.reset();
        }

        if (mRolodexAccessMonitor) {
          mRolodexAccessMonitor->cancel();
          mRolodexAccessMonitor.reset();
        }
        if (mRolodexNamespaceGrantChallengeValidateMonitor) {
          mRolodexNamespaceGrantChallengeValidateMonitor->cancel();
          mRolodexNamespaceGrantChallengeValidateMonitor.reset();
        }
        if (mRolodexContactsGetMonitor) {
          mRolodexContactsGetMonitor->cancel();
          mRolodexContactsGetMonitor.reset();
        }

        if (mGrantQueryAccess) {
          mGrantQueryAccess->cancel();
          mGrantQueryAccess.reset();
        }

        if (mGrantQueryRolodex) {
          mGrantQueryRolodex->cancel();
          mGrantQueryRolodex.reset();
        }

        if (mGrantWait) {
          mGrantWait->cancel();
          mGrantWait.reset();
        }

        mTimer.reset();

        setState(SessionState_Shutdown);

        if (mAssociatedLockboxSubscription) {
          mAssociatedLockboxSubscription->cancel();
          mAssociatedLockboxSubscription.reset();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageSource
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceIdentitySessionForServiceLockbox
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr ServiceIdentitySession::reload(
                                                               BootstrappedNetworkPtr provider,
                                                               IServiceNamespaceGrantSessionPtr grantSession,
                                                               IServiceLockboxSessionPtr existingLockbox,
                                                               const char *identityURI,
                                                               const char *reloginKey
                                                               )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!provider)
        ZS_THROW_INVALID_ARGUMENT_IF(!existingLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!identityURI)

        UseBootstrappedNetworkPtr identityNetwork;

        if (IServiceIdentity::isValid(identityURI)) {
          if (!IServiceIdentity::isLegacy(identityURI)) {
            String domain;
            String identifier;
            IServiceIdentity::splitURI(identityURI, domain, identifier);
            identityNetwork = UseBootstrappedNetwork::prepare(domain);
          }
        }

        UseServiceLockboxSessionPtr lockbox = ServiceLockboxSession::convert(existingLockbox);

        ServiceIdentitySessionPtr pThis(new ServiceIdentitySession(
                                                                   UseStack::queueStack(),
                                                                   IServiceIdentitySessionDelegatePtr(),
                                                                   provider,
                                                                   identityNetwork,
                                                                   ServiceNamespaceGrantSession::convert(grantSession),
                                                                   ServiceLockboxSession::convert(existingLockbox),
                                                                   NULL
                                                                   ));
        pThis->mThisWeak = pThis;
        pThis->setLock(lockbox->getLock());
        pThis->mIdentityInfo.mURI = identityURI;
        pThis->mIdentityInfo.mReloginKey = String(reloginKey);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::associate(ServiceLockboxSessionPtr inLockbox)
      {
        UseServiceLockboxSessionPtr lockbox = inLockbox;

        // NOTE: will use the service identity session lockbox's lock
        //       temporarily to attach the lock from the associated lockbox
        //       but it's safe because the service identity session will not be
        //       calling into the lockbox at this time (since it wasn't
        //       associated yet). And if for some reason it was associated then
        //       they share the same lock anyway so it's still safe.

        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("associate called"))
        setLock(lockbox->getLock());
        mAssociatedLockbox = lockbox;

        if (lockbox) {
          ServiceIdentitySessionPtr pThis = mThisWeak.lock();
          mAssociatedLockboxSubscription = lockbox->subscribe(pThis);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::killAssociation(ServiceLockboxSessionPtr peerContact)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("kill associate called"))

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("assoication already killed"))
          return;
        }

        // this shutdown must be performed graciously so that there is time to clean out the associations
        mGraciousShutdownReference = mThisWeak.lock();

        mAssociatedLockbox.reset();
        mKillAssociation = true;
        if (mAssociatedLockboxSubscription) {
          mAssociatedLockboxSubscription->cancel();
          mAssociatedLockboxSubscription.reset();
        }

        if (mIdentityLookupUpdateMonitor) {
          mIdentityLookupUpdateMonitor->cancel();
          mIdentityLookupUpdateMonitor.reset();
        }

        if (mIdentityLookupMonitor) {
          mIdentityLookupMonitor->cancel();
          mIdentityLookupMonitor.reset();
        }

        mIdentityLookupUpdated = false;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isAssociated() const
      {
        AutoRecursiveLock lock(*this);
        return (bool)mAssociatedLockbox.lock();
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isLoginComplete() const
      {
        AutoRecursiveLock lock(*this);
        if (!mKeyingPrepared) {
          ZS_LOG_TRACE(log("login not complete as keying not ready"))
          return false;
        }
        if (!mIdentityInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("login not complete as access token not ready"))
          return false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::isShutdown() const
      {
        AutoRecursiveLock lock(*this);
        return SessionState_Shutdown == mCurrentState;
      }

      //-----------------------------------------------------------------------
      IdentityInfo ServiceIdentitySession::getIdentityInfo() const
      {
        AutoRecursiveLock lock(*this);
        return mIdentityInfo;
      }

      //-----------------------------------------------------------------------
      LockboxInfo ServiceIdentitySession::getLockboxInfo() const
      {
        AutoRecursiveLock lock(*this);
        return mLockboxInfo;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceLockboxSessionForInternalDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceLockboxSessionStateChanged(
                                                                       IServiceLockboxSessionPtr session,
                                                                       IServiceLockboxSession::SessionStates state
                                                                       )
      {
        ZS_LOG_DEBUG(log("on service lockbox session state changed") + ZS_PARAM("session", session->getID()) + ZS_PARAM("state", IServiceLockboxSession::toString(state)))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session)
      {
        // ignored
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onWake()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on step"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onTimer(TimerPtr timer)
      {
        ZS_LOG_DEBUG(log("on timer fired"))
        AutoRecursiveLock lock(*this);

        mTimer.reset();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceNamespaceGrantSessionWaitDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session)
      {
        ZS_LOG_DEBUG(log("namespace grant waits have completed, can try again to obtain a wait (if waiting)"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IServiceNamespaceGrantSessionQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                                          IServiceNamespaceGrantSessionQueryPtr query,
                                                                                          ElementPtr namespaceGrantChallengeBundleEl
                                                                                          )
      {
        ZS_LOG_DEBUG(log("namespace grant query completed"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityAccessNamespaceGrantChallengeValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityAccessNamespaceGrantChallengeValidateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityAccessNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityAccessNamespaceGrantChallengeValidateMonitor->cancel();
        mIdentityAccessNamespaceGrantChallengeValidateMonitor.reset();

        mEncryptionKeyUponGrantProof = result->encryptionKeyUponGrantProof();

        String calculatedProof = UseServicesHelper::convertToHex(*UseServicesHelper::hash(mEncryptionKeyUponGrantProof));

        if (mEncryptionKeyUponGrantProofHash != calculatedProof) {
          ZS_LOG_ERROR(Detail, log("encryption key upon grant proof does not match expected value after grant proof issued") + ZS_PARAM("expecting", mEncryptionKeyUponGrantProofHash) + ZS_PARAM("calculated", calculatedProof) + ZS_PARAM("encryption key upon grant proof", mEncryptionKeyUponGrantProof))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "encryption key upon grant proof does not match expected value");
          cancel();
          return false;
        }

        // no longer need this value
        mEncryptionKeyUponGrantProofHash.clear();

        ZS_LOG_DEBUG(log("identity access namespace grant challenge validate complete"))

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityAccessNamespaceGrantChallengeValidateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityAccessNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_ERROR(Detail, log("identity access namespace grant challenge validate failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityLookupUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityLookupUpdateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityLookupUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityLookupUpdateMonitor->cancel();
        mIdentityLookupUpdateMonitor.reset();

        mIdentityLookupUpdated = true;

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityLookupUpdateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityLookupUpdateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity login complete failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityAccessRolodexCredentialsGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityAccessRolodexCredentialsGetResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mIdentityAccessRolodexCredentialsGetMonitor->cancel();
        mIdentityAccessRolodexCredentialsGetMonitor.reset();

        mRolodexInfo.mergeFrom(result->rolodexInfo(), true);

        if (mRolodexInfo.mServerToken.isEmpty()) {
          setError(IHTTP::HTTPStatusCode_MethodFailure, "rolodex credentials gets did not return a server token");
          cancel();
          return true;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityAccessRolodexCredentialsGetResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        if (OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_NOT_SUPPORTED_FOR_IDENTITY == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("identity does not support rolodex even if identity provider supports"))
          mRolodexNotSupportedForIdentity = true;

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }
        
        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity rolodex credentials failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<IdentityLookupResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      IdentityLookupResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityLookupMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        // possible the identity lookup will not return any result, so we need to remember that it did in fact complete
        mPreviousLookupInfo.mURI = mIdentityInfo.mURI;

        const IdentityInfoList &infos = result->identities();
        if (infos.size() > 0) {

          const IdentityInfo &identityInfo = infos.front();

          bool validProof = false;

          if (identityInfo.mIdentityProofBundle) {
            // validate the identity proof bundle...
            String stableID;
            Time created;
            Time expires;
            validProof = IServiceIdentity::isValidIdentityProofBundle(
                                                                      identityInfo.mIdentityProofBundle,
                                                                      identityInfo.mPeerFilePublic,
                                                                      NULL, // outPeerURI
                                                                      NULL, // outIdentityURI
                                                                      &stableID,
                                                                      &created,
                                                                      &expires
                                                                      );

            Time tick = zsLib::now();
            if (tick < created) {
              tick = created; // for calculation safety
            }

            auto consumed = tick - created;
            auto total = (expires - created);
            if (consumed > total) {
              consumed = total; // for calculation safety
            }

            auto percentageUsed = (consumed.count() * 100) / total.count();
            if (percentageUsed > OPENPEER_STACK_SERVICE_IDENTITY_MAX_CONSUMED_TIME_PERCENTAGE_BEFORE_IDENTITY_PROOF_REFRESH) {
              ZS_LOG_WARNING(Detail, log("identity bundle proof too close to expiry, will recreate identity proof") + ZS_PARAM("percentage used", percentageUsed) + ZS_PARAM("consumed (s)", consumed) + ZS_PARAM("total (s)", total))
              validProof = false;
            }

            if (stableID != identityInfo.mStableID) {
              ZS_LOG_WARNING(Detail, log("stabled ID from proof bundle does not match stable ID in identity") + ZS_PARAM("proof stable ID", stableID) + ZS_PARAM("identity stable id", identityInfo.mStableID))
              validProof = false;
            }
          }

          mPreviousLookupInfo.mergeFrom(identityInfo, true);
          if (!validProof) {
            mPreviousLookupInfo.mIdentityProofBundle.reset(); // identity proof bundle isn't valid, later will be forced to recreated if it is missing
          }
        }

        ZS_LOG_DEBUG(log("identity lookup (of self) complete"))

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           IdentityLookupResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mIdentityLookupMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("identity lookup failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexAccessResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexAccessResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexAccessMonitor->cancel();
        mRolodexAccessMonitor.reset();

        mRolodexInfo.mergeFrom(result->rolodexInfo(), true);

        if (!mRolodexInfo.mToken.hasData()) {
          setError(IHTTP::HTTPStatusCode_MethodFailure, "rolodex access did not return a proper access token/secret");
          cancel();
          return true;
        }

        mRolodexChallengeInfo = result->namespaceGrantChallengeInfo();

        if (mRolodexChallengeInfo.mID.hasData()) {
          mGrantQueryRolodex = mGrantSession->query(mThisWeak.lock(), mRolodexChallengeInfo);
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexAccessResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("rolodex access failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexNamespaceGrantChallengeValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexNamespaceGrantChallengeValidateResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexNamespaceGrantChallengeValidateMonitor->cancel();
        mRolodexNamespaceGrantChallengeValidateMonitor.reset();

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexNamespaceGrantChallengeValidateResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        setError(result->errorCode(), result->errorReason());

        ZS_LOG_DEBUG(log("rolodex namespace grant challenge failure"))

        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => IMessageMonitorResultDelegate<RolodexContactsGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorResultReceived(
                                                                      IMessageMonitorPtr monitor,
                                                                      RolodexContactsGetResultPtr result
                                                                      )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexContactsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mRolodexContactsGetMonitor->cancel();
        mRolodexContactsGetMonitor.reset();

        // reset the failure case
        mNextRetryAfterFailureTime = Seconds(OPENPEER_STACK_SERVICE_IDENTITY_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS);
        mFailuresInARow = 0;

        const IdentityInfoList &identities = result->identities();

        mRolodexInfo.mergeFrom(result->rolodexInfo());

        ZS_LOG_DEBUG(log("downloaded contacts") + ZS_PARAM("total", identities.size()))

        for (IdentityInfoList::const_iterator iter = identities.begin(); iter != identities.end(); ++iter)
        {
          const IdentityInfo &identityInfo = (*iter);
          ZS_LOG_TRACE(log("downloaded contact") + identityInfo.toDebug())
          mIdentities.push_back(identityInfo);
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onServiceIdentitySessionRolodexContactsDownloaded(mThisWeak.lock());
        } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::handleMessageMonitorErrorResultReceived(
                                                                           IMessageMonitorPtr monitor,
                                                                           RolodexContactsGetResultPtr ignore, // will always be NULL
                                                                           message::MessageResultPtr result
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mRolodexContactsGetMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("rolodex contacts get failure") + ZS_PARAM("error code", result->errorCode()) + ZS_PARAM("error reason", result->errorReason()))
        ++mFailuresInARow;

        if ((mFailuresInARow < 2) &&
            (result->errorCode() != IHTTP::HTTPStatusCode_MethodFailure)) { // error 424
          ZS_LOG_DEBUG(log("performing complete rolodex refresh"))
          refreshRolodexContacts();
          step();
          return true;
        }

        // try again later
        mRolodexInfo.mUpdateNext = zsLib::now() + mNextRetryAfterFailureTime;

        // double the wait time for the next error
        mNextRetryAfterFailureTime = mNextRetryAfterFailureTime + mNextRetryAfterFailureTime;

        // cap the retry method at a maximum retrial value
        if (mNextRetryAfterFailureTime > Seconds(OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS)) {
          mNextRetryAfterFailureTime = Seconds(OPENPEER_STACK_SERVICE_IDENTITY_MAX_ROLODEX_ERROR_RETRY_TIME_IN_SECONDS);
        }

        step();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceIdentitySession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServiceIdentitySession::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServiceIdentitySession");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServiceIdentitySession::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServiceIdentitySession::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServiceIdentitySession");

        UseServicesHelper::debugAppend(resultEl, "id", mID);

        UseServicesHelper::debugAppend(resultEl, "state", toString(mCurrentState));
        UseServicesHelper::debugAppend(resultEl, "reported", toString(mLastReportedState));

        UseServicesHelper::debugAppend(resultEl, "error code", mLastError);
        UseServicesHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        UseServicesHelper::debugAppend(resultEl, "associated lockbox", (bool)mAssociatedLockbox.lock());
        UseServicesHelper::debugAppend(resultEl, "associated lockbox subscription", (bool)mAssociatedLockboxSubscription);
        UseServicesHelper::debugAppend(resultEl, "kill association", mKillAssociation);

        UseServicesHelper::debugAppend(resultEl, mIdentityInfo.hasData() ? mIdentityInfo.toDebug() : ElementPtr());

        UseServicesHelper::debugAppend(resultEl, "provider", UseBootstrappedNetwork::toDebug(mProviderBootstrappedNetwork));
        UseServicesHelper::debugAppend(resultEl, "identity", UseBootstrappedNetwork::toDebug(mIdentityBootstrappedNetwork));
        UseServicesHelper::debugAppend(resultEl, "active boostrapper", mActiveBootstrappedNetwork ? (mIdentityBootstrappedNetwork == mActiveBootstrappedNetwork ? String("identity") : String("provider")) : String());

        UseServicesHelper::debugAppend(resultEl, "grant session id", mGrantSession ? mGrantSession->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant query access id", mGrantQueryAccess ? mGrantQueryAccess->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant query rolodex id", mGrantQueryRolodex ? mGrantQueryRolodex->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant wait id", mGrantWait ? mGrantWait->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "identity access namespace grant challenge validate monitor", (bool)mIdentityAccessNamespaceGrantChallengeValidateMonitor);
        UseServicesHelper::debugAppend(resultEl, "identity lookup update monitor", (bool)mIdentityLookupUpdateMonitor);
        UseServicesHelper::debugAppend(resultEl, "identity access rolodex credentials get monitor", (bool)mIdentityAccessRolodexCredentialsGetMonitor);
        UseServicesHelper::debugAppend(resultEl, "rolodex access monitor", (bool)mRolodexAccessMonitor);
        UseServicesHelper::debugAppend(resultEl, "rolodex grant monitor", (bool)mRolodexNamespaceGrantChallengeValidateMonitor);
        UseServicesHelper::debugAppend(resultEl, "rolodex contacts get monitor", (bool)mRolodexContactsGetMonitor);

        UseServicesHelper::debugAppend(resultEl, mLockboxInfo.hasData() ? mLockboxInfo.toDebug() : ElementPtr());

        UseServicesHelper::debugAppend(resultEl, "browser window ready", mBrowserWindowReady);
        UseServicesHelper::debugAppend(resultEl, "browser window visible", mBrowserWindowVisible);
        UseServicesHelper::debugAppend(resultEl, "browser closed", mBrowserWindowClosed);

        UseServicesHelper::debugAppend(resultEl, "need browser window visible", mNeedsBrowserWindowVisible);
        UseServicesHelper::debugAppend(resultEl, "need browser window redirect", mNeedsBrowserWindowRedirectURL);

        UseServicesHelper::debugAppend(resultEl, "identity access start notification sent", mIdentityAccessStartNotificationSent);
        UseServicesHelper::debugAppend(resultEl, mAccessChallengeInfo.hasData() ? mAccessChallengeInfo.toDebug() : ElementPtr());
        UseServicesHelper::debugAppend(resultEl, "keying prepared", mKeyingPrepared);
        UseServicesHelper::debugAppend(resultEl, "encrypted user specific passphrase", mEncryptedUserSpecificPassphrase);
        UseServicesHelper::debugAppend(resultEl, "user specific passphrase", mUserSpecificPassphrase);
        UseServicesHelper::debugAppend(resultEl, "encryption key upon grant proof", mEncryptionKeyUponGrantProof);
        UseServicesHelper::debugAppend(resultEl, "encryption key upon grant proof hash", mEncryptionKeyUponGrantProofHash);
        UseServicesHelper::debugAppend(resultEl, "identity lookup updated", mIdentityLookupUpdated);
        UseServicesHelper::debugAppend(resultEl, mPreviousLookupInfo.hasData() ? mPreviousLookupInfo.toDebug() : ElementPtr());

        UseServicesHelper::debugAppend(resultEl, "outer frame url", mOuterFrameURLUponReload);

        UseServicesHelper::debugAppend(resultEl, "pending messages", mPendingMessagesToDeliver.size());

        UseServicesHelper::debugAppend(resultEl, "rolodex not supported", mRolodexNotSupportedForIdentity);
        UseServicesHelper::debugAppend(resultEl, mRolodexInfo.hasData() ? mRolodexInfo.toDebug() : ElementPtr());
        UseServicesHelper::debugAppend(resultEl, "rolodex grant challenge id", mRolodexChallengeInfo.hasData() ? mRolodexChallengeInfo.toDebug() : ElementPtr());

        UseServicesHelper::debugAppend(resultEl, "download timer", (bool)mTimer);

        UseServicesHelper::debugAppend(resultEl, "frozen version", mFrozenVersion);

        UseServicesHelper::debugAppend(resultEl, "last version download", mLastVersionDownloaded);
        UseServicesHelper::debugAppend(resultEl, "force refresh", mForceRefresh);

        UseServicesHelper::debugAppend(resultEl, "fresh download", mFreshDownload);
        UseServicesHelper::debugAppend(resultEl, "pending identities", mIdentities.size());

        UseServicesHelper::debugAppend(resultEl, "failures in a row", mFailuresInARow);
        UseServicesHelper::debugAppend(resultEl, "next retry (seconds)", mNextRetryAfterFailureTime);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!mDelegate) {
          ZS_LOG_DEBUG(log("step waiting for delegate to become attached"))
          setState(SessionState_WaitingAttachmentOfDelegate);
          return;
        }

        if (!stepBootstrapper()) return;
        if (!stepGrantCheck()) return;
        if (!stepLoadBrowserWindow()) return;
        if (!stepIdentityAccessStartNotification()) return;
        if (!stepMakeBrowserWindowVisible()) return;
        if (!stepMakeBrowserWindowRedirect()) return;
        if (!stepIdentityAccessCompleteNotification()) return;
        if (!stepRolodexCredentialsGet()) return;
        if (!stepRolodexAccess()) return;
        if (!stepIdentityLookup()) return;
        if (!stepCloseBrowserWindow()) return;
        if (!stepPreGrantChallenge()) return;
        if (!stepClearGrantWait()) return;
        if (!stepGrantChallengeAccess()) return;
        if (!stepGrantChallengeRolodex()) return;
        if (!stepPrepareKeying()) return;
        if (!stepLockboxAssociation()) return;
        if (!stepLockboxAccessToken()) return;
        if (!stepLockboxReady()) return;
        if (!stepLookupUpdate()) return;
        if (!stepDownloadContacts()) return;

        if (mKillAssociation) {
          ZS_LOG_DEBUG(debug("association is now killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "assocation is now killed");
          cancel();
          return;
        }

        // signal the object is ready
        setState(SessionState_Ready);

        ZS_LOG_TRACE(debug("step complete"))
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepBootstrapper()
      {
        if (mActiveBootstrappedNetwork) {
          ZS_LOG_TRACE(log("already have an active bootstrapper"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        setState(SessionState_Pending);

        if (mIdentityBootstrappedNetwork) {
          if (!mIdentityBootstrappedNetwork->isPreparationComplete()) {
            ZS_LOG_TRACE(log("waiting for preparation of identity bootstrapper to complete"))
            return false;
          }

          WORD errorCode = 0;
          String reason;

          if (mIdentityBootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
            ZS_LOG_DEBUG(log("identity bootstrapper was successful thus using that as the active identity service"))
            mActiveBootstrappedNetwork = mIdentityBootstrappedNetwork;
            return true;
          }

          if (!mProviderBootstrappedNetwork) {
            ZS_LOG_ERROR(Detail, log("bootstrapped network failed for identity and there is no provider identity service specified") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        if (!mProviderBootstrappedNetwork) {
          ZS_LOG_ERROR(Detail, log("provider domain not specified for identity thus identity lookup cannot complete"))
          setError(IHTTP::HTTPStatusCode_BadGateway);
          cancel();
          return false;
        }

        if (!mProviderBootstrappedNetwork->isPreparationComplete()) {
          ZS_LOG_TRACE(log("waiting for preparation of provider bootstrapper to complete"))
          return false;
        }

        WORD errorCode = 0;
        String reason;

        if (mProviderBootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
          ZS_LOG_DEBUG(log("provider bootstrapper was successful thus using that as the active identity service"))
          mActiveBootstrappedNetwork = mProviderBootstrappedNetwork;
          return true;
        }

        ZS_LOG_ERROR(Detail, log("bootstrapped network failed for provider") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

        setError(errorCode, reason);
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepGrantCheck()
      {
        if (mBrowserWindowClosed) {
          ZS_LOG_TRACE(log("already informed browser window closed thus no need to make sure grant wait lock is obtained"))
          return true;
        }

        if (mGrantWait) {
          ZS_LOG_TRACE(log("grant wait lock is already obtained"))
          return true;
        }

        mGrantWait = mGrantSession->obtainWaitToProceed(mThisWeak.lock());

        if (!mGrantWait) {
          ZS_LOG_TRACE(log("waiting to obtain grant wait lock"))
          return false;
        }

        ZS_LOG_TRACE(log("obtained grant wait"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLoadBrowserWindow()
      {
        if (mBrowserWindowReady) {
          ZS_LOG_TRACE(log("browser window is ready"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        if (mIdentityInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("already have access token, no need to load browser"))
          return true;
        }

        String url = getInnerBrowserWindowFrameURL();
        if (!url) {
          ZS_LOG_ERROR(Detail, log("bootstrapper did not return a valid inner window frame URL"))
          setError(IHTTP::HTTPStatusCode_NotFound);
          cancel();
          return false;
        }

        setState(SessionState_WaitingForBrowserWindowToBeLoaded);

        ZS_LOG_TRACE(log("waiting for browser window to report it is loaded/ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityAccessStartNotification()
      {
        if (mIdentityAccessStartNotificationSent) {
          ZS_LOG_TRACE(log("identity access start notification already sent"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        if (mIdentityInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("already have access token, no need to load browser"))
          return true;
        }

        ZS_LOG_DEBUG(log("identity access start notification being sent"))

        setState(SessionState_Pending);

        // make sure the provider domain is set to the active bootstrapper for the identity
        mIdentityInfo.mProvider = mActiveBootstrappedNetwork->getDomain();

        IdentityAccessStartNotifyPtr request = IdentityAccessStartNotify::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());
        request->identityInfo(mIdentityInfo);

        request->browserVisibility(IdentityAccessStartNotify::BrowserVisibility_VisibleOnDemand);
        request->popup(false);

        request->outerFrameURL(mOuterFrameURLUponReload);

        sendInnerWindowMessage(request);

        mIdentityAccessStartNotificationSent = true;
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepMakeBrowserWindowVisible()
      {
        if (mBrowserWindowVisible) {
          ZS_LOG_TRACE(log("browser window is visible"))
          return true;
        }

        if (!mNeedsBrowserWindowVisible) {
          ZS_LOG_TRACE(log("browser window was not requested to become visible"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("waiting for browser window to become visible"))
        setState(SessionState_WaitingForBrowserWindowToBeMadeVisible);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepMakeBrowserWindowRedirect()
      {
        if (mNeedsBrowserWindowRedirectURL.isEmpty()) {
          ZS_LOG_TRACE(log("browser window does not need to be redirected"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("waiting for browser window to be redirected") + ZS_PARAM("url", mNeedsBrowserWindowRedirectURL))
        setState(SessionState_WaitingForBrowserWindowToBeRedirected);
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityAccessCompleteNotification()
      {
        if (mIdentityInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("idenity access complete notification received"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_WARNING(Detail, log("association is killed"))
          setError(IHTTP::HTTPStatusCode_Gone, "association is killed");
          cancel();
          return false;
        }

        setState(SessionState_Pending);

        ZS_LOG_TRACE(log("waiting for identity access complete notification"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepRolodexCredentialsGet()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))
          return true;
        }

        if (mRolodexInfo.mServerToken.hasData()) {
          ZS_LOG_TRACE(log("already have rolodex server token credentials"))
          return true;
        }

        if (mIdentityAccessRolodexCredentialsGetMonitor) {
          ZS_LOG_TRACE(log("rolodex credentials get pending"))
          return false;
        }

        if (!mActiveBootstrappedNetwork->supportsRolodex()) {
          ZS_LOG_WARNING(Detail, log("rolodex service not supported on this domain") + ZS_PARAM("domain", mActiveBootstrappedNetwork->getDomain()))
          mRolodexNotSupportedForIdentity = true;
          return true;
        }

        IdentityAccessRolodexCredentialsGetRequestPtr request = IdentityAccessRolodexCredentialsGetRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());
        request->identityInfo(mIdentityInfo);

        ZS_LOG_DEBUG(log("fetching rolodex credentials"))

        mIdentityAccessRolodexCredentialsGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<IdentityAccessRolodexCredentialsGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS), PromisePtr());
        sendInnerWindowMessage(request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepRolodexAccess()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))
          return true;
        }

        if (mRolodexInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("rolodex access token obtained"))
          return true;
        }

        if (mRolodexAccessMonitor) {
          ZS_LOG_TRACE(log("rolodex access still pending (continuing to next step so other steps can run in parallel)"))
          return true;
        }

        ZS_THROW_BAD_STATE_IF(!mActiveBootstrappedNetwork->supportsRolodex())

        RolodexAccessRequestPtr request = RolodexAccessRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());
        request->rolodexInfo(mRolodexInfo);
        request->identityInfo(mIdentityInfo);
        request->grantID(mGrantSession->getGrantID());

        ZS_LOG_DEBUG(log("accessing rolodex (continuing to next step so other steps can run in parallel)"))

        mRolodexAccessMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<RolodexAccessResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "rolodex", "rolodex-access", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepIdentityLookup()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need to perform identity lookup when lockbox association is being killed"))
          return true;
        }

        if (mPreviousLookupInfo.mURI.hasData()) {
          ZS_LOG_TRACE(log("identity lookup has already completed"))
          return true;
        }

        if (mIdentityLookupMonitor) {
          ZS_LOG_TRACE(log("identity lookup already in progress (but not going to wait for it to complete to continue"))
          return true;
        }

        ZS_LOG_DEBUG(log("performing identity lookup"))

        IdentityLookupRequestPtr request = IdentityLookupRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());

        IdentityLookupRequest::Provider provider;

        String domain;
        String id;

        IServiceIdentity::splitURI(mIdentityInfo.mURI, domain, id);

        provider.mBase = IServiceIdentity::joinURI(domain, NULL);
        char safeChar[2];
        safeChar[0] = getSafeSplitChar(id);;
        safeChar[1] = 0;

        provider.mSeparator = String(&(safeChar[0]));
        provider.mIdentities = id;

        IdentityLookupRequest::ProviderList providers;
        providers.push_back(provider);

        request->providers(providers);

        mIdentityLookupMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityLookupResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "identity-lookup", "identity-lookup", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));

        setState(SessionState_Pending);
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepCloseBrowserWindow()
      {
        if (!mBrowserWindowReady) {
          ZS_LOG_TRACE(log("browser window was never made visible"))
          return true;
        }

        if (mBrowserWindowClosed) {
          ZS_LOG_TRACE(log("browser window is closed"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting for browser window to close"))

        setState(SessionState_WaitingForBrowserWindowToClose);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepPreGrantChallenge()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex service is not supported"))
          return true;
        }

        // before continuing, make sure rolodex access is completed
        if (!mRolodexInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("rolodex access is still pending"))
          return false;
        }

        ZS_LOG_TRACE(log("rolodex access has already completed"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepClearGrantWait()
      {
        if (!mGrantWait) {
          ZS_LOG_TRACE(log("wait already cleared"))
          return true;
        }

        ZS_LOG_DEBUG(log("clearing grant wait"))

        mGrantWait->cancel();
        mGrantWait.reset();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepGrantChallengeAccess()
      {
        if (mIdentityAccessNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_TRACE(log("waiting for identity access namespace grant challenge validate monitor to complete"))
          return true;
        }

        if (!mGrantQueryAccess) {
          ZS_LOG_TRACE(log("no access related grant challenge query thus continuing..."))
          return true;
        }

        if (!mGrantQueryAccess->isComplete()) {
          ZS_LOG_TRACE(log("waiting for the access grant query to complete"))
          return true;
        }

        ElementPtr bundleEl = mGrantQueryAccess->getNamespaceGrantChallengeBundle();
        if (!bundleEl) {
          ZS_LOG_ERROR(Detail, log("namespaces were no granted in access challenge"))
          setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access identity");
          cancel();
          return false;
        }

        for (NamespaceInfoMap::iterator iter = mAccessChallengeInfo.mNamespaces.begin(); iter != mAccessChallengeInfo.mNamespaces.end(); ++iter)
        {
          NamespaceInfo &namespaceInfo = (*iter).second;

          if (!mGrantSession->isNamespaceURLInNamespaceGrantChallengeBundle(bundleEl, namespaceInfo.mURL)) {
            ZS_LOG_WARNING(Detail, log("access was not granted required namespace") + ZS_PARAM("namespace", namespaceInfo.mURL))
            setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access identity");
            cancel();
            return false;
          }
        }

        mGrantQueryAccess->cancel();
        mGrantQueryAccess.reset();

        ZS_LOG_DEBUG(log("all namespaces required were correctly granted, notify the identity access of the newly created access"))

        IdentityAccessNamespaceGrantChallengeValidateRequestPtr request = IdentityAccessNamespaceGrantChallengeValidateRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());

        request->identityInfo(mIdentityInfo);
        request->namespaceGrantChallengeBundle(bundleEl);

        mIdentityAccessNamespaceGrantChallengeValidateMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityAccessNamespaceGrantChallengeValidateResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "identity", "identity-access-namespace-grant-challenge-validate", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepGrantChallengeRolodex()
      {
        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex service is not supported"))
          return true;
        }

        if (mRolodexNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_TRACE(log("waiting for rolodex namespace grant challenge validate monitor to complete"))
          return true;
        }

        if (!mGrantQueryRolodex) {
          ZS_LOG_TRACE(log("no rolodex grant challenge query thus continuing..."))
          return true;
        }

        if (!mGrantQueryRolodex->isComplete()) {
          ZS_LOG_TRACE(log("waiting for the rolodex grant query to complete"))
          return true;
        }

        ElementPtr bundleEl = mGrantQueryRolodex->getNamespaceGrantChallengeBundle();
        if (!bundleEl) {
          ZS_LOG_ERROR(Detail, log("namespaces were no granted in challenge"))
          setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access rolodex");
          cancel();
          return false;
        }

        for (NamespaceInfoMap::iterator iter = mRolodexChallengeInfo.mNamespaces.begin(); iter != mRolodexChallengeInfo.mNamespaces.end(); ++iter)
        {
          NamespaceInfo &namespaceInfo = (*iter).second;

          if (!mGrantSession->isNamespaceURLInNamespaceGrantChallengeBundle(bundleEl, namespaceInfo.mURL)) {
            ZS_LOG_WARNING(Detail, log("rolodex was not granted required namespace") + ZS_PARAM("namespace", namespaceInfo.mURL))
            setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access rolodex");
            cancel();
            return false;
          }
        }

        mGrantQueryRolodex->cancel();
        mGrantQueryRolodex.reset();

        ZS_LOG_DEBUG(log("all namespaces required were correctly granted, notify the rolodex of the newly created access"))

        RolodexNamespaceGrantChallengeValidateRequestPtr request = RolodexNamespaceGrantChallengeValidateRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());

        request->rolodexInfo(mRolodexInfo);
        request->namespaceGrantChallengeBundle(bundleEl);

        mRolodexNamespaceGrantChallengeValidateMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<RolodexNamespaceGrantChallengeValidateResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "rolodex", "rolodex-namespace-grant-challenge-validate", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepPrepareKeying()
      {
        if (mGrantQueryAccess) {
          ZS_LOG_TRACE(log("waiting for access grant to complete"))
          setState(SessionState_Pending);
          return false;
        }

        if (mIdentityAccessNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_TRACE(log("waiting for identity access namespace grant challenge validate to complete"))
          setState(SessionState_Pending);
          return false;
        }

        if (mEncryptedUserSpecificPassphrase.isEmpty()) {
          ZS_LOG_ERROR(Detail, log("missing user specific passphrase"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "missing user specific passphrase");
          cancel();
          return false;
        }

        if (mEncryptionKeyUponGrantProof.isEmpty()) {
          ZS_LOG_ERROR(Detail, log("missing encryption key upon grant proof"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "missing encryption key upon grant proof");
          cancel();
          return false;
        }

        if (mLockboxInfo.mKeyName.isEmpty()) {
          ZS_LOG_TRACE(log("keying material is not available for identity"))
          mKeyingPrepared = true;
          return true;
        }

        if (mKeyingPrepared) {
          ZS_LOG_TRACE(log("keying material already prepared for lockbox"))
          return true;
        }

        ZS_LOG_DEBUG(log("preparing keying information for lockbox") + ZS_PARAM("encryption key upon grant proof", mEncryptionKeyUponGrantProof) + ZS_PARAM("identity uri", mIdentityInfo.mURI) + ZS_PARAM("key encrypted", mLockboxInfo.mKeyEncrypted) + ZS_PARAM("encrypted user specific passphrase", mEncryptedUserSpecificPassphrase))

        mKeyingPrepared = true;

        notifyLockboxStateChanged();

        if (mIdentityInfo.mReloginKeyEncrypted) {
          SecureByteBlockPtr reloginKey = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mEncryptionKeyUponGrantProof), "identity:" + mIdentityInfo.mURI + ":identity-relogin-key", UseServicesHelper::HashAlgorthm_SHA256);

          mIdentityInfo.mReloginKey = UseServicesHelper::convertToString(*UseStackHelper::splitDecrypt(*reloginKey, mIdentityInfo.mReloginKeyEncrypted));

          if (mIdentityInfo.mReloginKey.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("decrypted relogin key failed") + ZS_PARAM("encrypted relogin key", mIdentityInfo.mReloginKeyEncrypted) + ZS_PARAM("relogin key", mIdentityInfo.mReloginKey))
          } else {
            ZS_LOG_DEBUG(log("decrypted relogin key") + ZS_PARAM("encrypted relogin key", mIdentityInfo.mReloginKeyEncrypted) + ZS_PARAM("relogin key", mIdentityInfo.mReloginKey))
          }
        }

        if (mLockboxInfo.mKeyEncrypted.isEmpty()) {
          ZS_LOG_DEBUG(log("lockbox key name was provided but no encrypted key is available"))
          mLockboxInfo.mKeyName.clear();
          return true;
        }

        SecureByteBlockPtr outerKey = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mEncryptionKeyUponGrantProof), "identity:" + mIdentityInfo.mURI + ":lockbox-key", UseServicesHelper::HashAlgorthm_SHA256);

        String encryptedLockboxPassphrase = UseServicesHelper::convertToString(*stack::IHelper::splitDecrypt(*outerKey, mLockboxInfo.mKeyEncrypted));
        if (encryptedLockboxPassphrase.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("failed to decrypt encryption key thus clearing keying information from lockbox"))
          mLockboxInfo.mKeyName.clear();
          mLockboxInfo.mKeyEncrypted.clear();
          return true;
        }

        ZS_LOG_DEBUG(log("calculated encrypted lockbox passphrase and will now decrypt user specific passphrase") + ZS_PARAM("encrypted lockbox passphrase", encryptedLockboxPassphrase) + ZS_PARAM("encrypted user specific passphrase", mEncryptedUserSpecificPassphrase))

        // <key> = hmac(<encryption-passphrase-upon-grant-proof>, "identity:" + <identity-uri> + ":user-specific-key")

        SecureByteBlockPtr userKey = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mEncryptionKeyUponGrantProof), "identity:" + mIdentityInfo.mURI + ":user-specific-key", UseServicesHelper::HashAlgorthm_SHA256);
        mUserSpecificPassphrase = UseServicesHelper::convertToString(*stack::IHelper::splitDecrypt(*userKey, mEncryptedUserSpecificPassphrase));
        if (mUserSpecificPassphrase.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("failed to decrypt user specific passphrase thus cannot login to this identity"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "failed to decrypt user specific passphrase thus cannot login to this identity");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("calculated user specific passphrase and will now decrypt lockbox passphrase") + ZS_PARAM("user specific passphrase", mUserSpecificPassphrase)  + ZS_PARAM("encrypted lockbox passphrase", encryptedLockboxPassphrase))

        SecureByteBlockPtr innerKey = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mUserSpecificPassphrase), "identity:" + mIdentityInfo.mURI + ":lockbox-key", UseServicesHelper::HashAlgorthm_SHA256);
        mLockboxInfo.mKey = UseStackHelper::splitDecrypt(*innerKey, encryptedLockboxPassphrase);

        if (mLockboxInfo.mKey) {
          if (mLockboxInfo.mKey->SizeInBytes() < 1) {
            mLockboxInfo.mKey.reset();
          }
        }

        if (!mLockboxInfo.mKey) {
          mLockboxInfo.mKeyName.clear();
          ZS_LOG_WARNING(Detail, log("failed to decrypt lockbox key thus clearing keying information from lockbox"))
          return true;
        }

        mLockboxInfo.mKeyEncrypted.clear();

        ZS_LOG_DEBUG(log("successfully decrypted lockbox keying material") + ZS_PARAM("key", UseServicesHelper::convertToString(*mLockboxInfo.mKey)))
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxAssociation()
      {
        if (mAssociatedLockbox.lock()) {
          ZS_LOG_TRACE(log("lockbox associated"))
          return true;
        }

        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need an association to the lockbox if association is being killed"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting for association to lockbox"))

        setState(SessionState_WaitingForAssociationToLockbox);
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxAccessToken()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need lockbox to be ready if association is being killed"))
          return true;
        }

        UseServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        WORD errorCode = 0;
        String reason;
        IServiceLockboxSession::SessionStates state = lockbox->getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration: {

            LockboxInfo lockboxInfo = lockbox->getLockboxInfo();
            if (lockboxInfo.mToken.hasData()) {
              ZS_LOG_TRACE(log("lockbox is still pending but safe to proceed because lockbox has been granted access"))
              return true;
            }

            ZS_LOG_TRACE(log("waiting for lockbox to have access token"))
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("lockbox is ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown: {
            ZS_LOG_ERROR(Detail, log("lockbox shutdown") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_ERROR(Detail, debug("unknown lockbox state"))

        ZS_THROW_BAD_STATE("unknown lockbox state")
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLockboxReady()
      {
        if (mKillAssociation) {
          ZS_LOG_TRACE(log("do not need lockbox to be ready if association is being killed"))
          return true;
        }

        UseServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        WORD errorCode = 0;
        String reason;
        IServiceLockboxSession::SessionStates state = lockbox->getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration: {

            ZS_LOG_TRACE(log("must wait for lockbox to be ready (pending with access token is not enough)"))
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("lockbox is fully ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown: {
            ZS_LOG_ERROR(Detail, log("lockbox shutdown") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_ERROR(Detail, debug("unknown lockbox state"))

        ZS_THROW_BAD_STATE("unknown lockbox state")
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepLookupUpdate()
      {
        if (mIdentityLookupUpdated) {
          ZS_LOG_TRACE(log("lookup already updated"))
          return true;
        }

        if (mIdentityLookupUpdateMonitor) {
          ZS_LOG_TRACE(log("lookup update already in progress (but does not prevent other events from completing)"))
          return false;
        }

        if (mKillAssociation) {
          ZS_LOG_DEBUG(log("clearing identity lookup information (but not preventing other requests from continuing)"))

          IdentityInfo killInfo;

          killInfo.mToken = mIdentityInfo.mToken;
          killInfo.mURI = mIdentityInfo.mURI;
          killInfo.mProvider = mIdentityInfo.mProvider;

          IdentityLookupUpdateRequestPtr request = IdentityLookupUpdateRequest::create();
          request->domain(mActiveBootstrappedNetwork->getDomain());
          request->identityInfo(killInfo);
          request->encryptionKeyUponGrantProof(mEncryptionKeyUponGrantProof);

          mIdentityLookupUpdateMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityLookupUpdateResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "identity", "identity-lookup-update", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
          return false;
        }

        if (mPreviousLookupInfo.mURI.isEmpty()) {
          ZS_LOG_TRACE(log("waiting for identity lookup to complete"))
          return false;
        }

        UseServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) {
          return stepLockboxAssociation();
        }

        setState(SessionState_Pending);

        LockboxInfo lockboxInfo = lockbox->getLockboxInfo();

        bool keyChanged = false;
        if (lockboxInfo.mKeyName != mLockboxInfo.mKeyName) {
          keyChanged = true;
        }

        ZS_THROW_INVALID_ASSUMPTION_IF(!lockboxInfo.mKey)

        if (!mLockboxInfo.mKey) {
          keyChanged = true;
        }

        if (mLockboxInfo.mKey) {
          if (0 != UseServicesHelper::compare(*mLockboxInfo.mKey, *lockboxInfo.mKey)) {
            keyChanged = true;
          }
        }

        IPeerFilesPtr peerFiles;
        IdentityInfo identityInfo = lockbox->getIdentityInfoForIdentity(mThisWeak.lock(), &peerFiles);
        mIdentityInfo.mergeFrom(identityInfo, true);

        if ((!keyChanged) &&
            (identityInfo.mStableID == mPreviousLookupInfo.mStableID) &&
            (isSame(identityInfo.mPeerFilePublic, mPreviousLookupInfo.mPeerFilePublic)) &&
            (identityInfo.mPriority == mPreviousLookupInfo.mPriority) &&
            (identityInfo.mWeight == mPreviousLookupInfo.mWeight) &&
            (mPreviousLookupInfo.mIdentityProofBundle)) {
          ZS_LOG_DEBUG(log("identity information already up-to-date"))
          mIdentityLookupUpdated = true;
          return true;
        }

        ZS_LOG_DEBUG(log("updating identity lookup information (but not preventing other requests from continuing)") + ZS_PARAM("lockbox", mLockboxInfo.toDebug()) + ZS_PARAM("identity info", mIdentityInfo.toDebug()))

        mLockboxInfo.mKeyName.clear();
        mLockboxInfo.mKey.reset();
        mLockboxInfo.mDomain.clear();

        mLockboxInfo.mergeFrom(lockboxInfo, true);

        ZS_THROW_INVALID_ASSUMPTION_IF(mLockboxInfo.mKeyName.isEmpty())
        ZS_THROW_INVALID_ASSUMPTION_IF(!mLockboxInfo.mKey)
        ZS_THROW_INVALID_ASSUMPTION_IF(mUserSpecificPassphrase.isEmpty())

        // <key> = hmac(<user-specific-passphrase>, "identity:" + <identity-uri> + ":lockbox-key")
        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(mUserSpecificPassphrase), "identity:" + mIdentityInfo.mURI + ":lockbox-key", UseServicesHelper::HashAlgorthm_SHA256);

        mLockboxInfo.mKeyEncrypted = UseStackHelper::splitEncrypt(*key, *mLockboxInfo.mKey);

        IdentityLookupUpdateRequestPtr request = IdentityLookupUpdateRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());
        request->peerFiles(peerFiles);
        request->lockboxInfo(mLockboxInfo);
        request->identityInfo(mIdentityInfo);
        request->encryptionKeyUponGrantProof(mEncryptionKeyUponGrantProof);

        mIdentityLookupUpdateMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<IdentityLookupUpdateResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "identity", "identity-lookup-update", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceIdentitySession::stepDownloadContacts()
      {
        if (mFrozenVersion == mRolodexInfo.mVersion) {
          ZS_LOG_TRACE(log("rolodex download has not been initiated yet"))
          return true;
        }

        if (mRolodexNotSupportedForIdentity) {
          ZS_LOG_TRACE(log("rolodex not supported for this identity"))

          mRolodexInfo.mVersion = mFrozenVersion;

          try {
            mDelegate->onServiceIdentitySessionRolodexContactsDownloaded(mThisWeak.lock());
          } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }

          return true;
        }

        if (mRolodexContactsGetMonitor) {
          ZS_LOG_TRACE(log("rolodex contact download already active"))
          return true;
        }

        bool forceRefresh = (Time() != mForceRefresh);
        mForceRefresh = Time();

        if (forceRefresh) {
          ZS_LOG_DEBUG(log("will force refresh of contact list immediately"))
          mRolodexInfo.mUpdateNext = Time();  // download immediately
          mFreshDownload = zsLib::now();
          mIdentities.clear();
          mRolodexInfo.mVersion.clear();
        }

        Time tick = zsLib::now();
        if ((Time() != mRolodexInfo.mUpdateNext) &&
            (tick < mRolodexInfo.mUpdateNext)) {
          // not ready to issue the request yet, must wait, calculate how long to wait
          auto waitTime = mRolodexInfo.mUpdateNext - tick;
          if (waitTime < Seconds(1)) {
            waitTime = Seconds(1);
          }
          mTimer = Timer::create(mThisWeak.lock(), waitTime, false);

          ZS_LOG_TRACE(log("delaying downloading contacts") + ZS_PARAM("wait time (s)", waitTime))
          return true;
        }

        ZS_LOG_DEBUG(log("attempting to download contacts from rolodex"))

        RolodexContactsGetRequestPtr request = RolodexContactsGetRequest::create();
        request->domain(mActiveBootstrappedNetwork->getDomain());

        RolodexInfo rolodexInfo(mRolodexInfo);
        rolodexInfo.mRefreshFlag = forceRefresh;

        request->rolodexInfo(rolodexInfo);

        mRolodexContactsGetMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<RolodexContactsGetResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mActiveBootstrappedNetwork), "rolodex", "rolodex-contacts-get", request, Seconds(OPENPEER_STACK_SERVICE_IDENTITY_TIMEOUT_IN_SECONDS));
        return true;
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))
        mCurrentState = state;

        notifyLockboxStateChanged();

        if (mLastReportedState != mCurrentState) {
          ServiceIdentitySessionPtr pThis = mThisWeak.lock();
          if ((pThis) &&
              (mDelegate)) {
            try {
              ZS_LOG_DEBUG(debug("attempting to report state to delegate"))
              mDelegate->onServiceIdentitySessionStateChanged(pThis, mCurrentState);
              mLastReportedState = mCurrentState;
            } catch (IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_WARNING(Detail, log("delegate gone"))
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        mLastError = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, debug("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::notifyLockboxStateChanged()
      {
        UseServiceLockboxSessionPtr lockbox = mAssociatedLockbox.lock();
        if (!lockbox) return;

        ZS_LOG_DEBUG(log("notifying lockbox of state change"))
        lockbox->notifyStateChanged();
      }

      //-----------------------------------------------------------------------
      void ServiceIdentitySession::sendInnerWindowMessage(MessagePtr message)
      {
        DocumentPtr doc = message->encode();
        mPendingMessagesToDeliver.push_back(DocumentMessagePair(doc, message));

        if (1 != mPendingMessagesToDeliver.size()) {
          ZS_LOG_DEBUG(log("already had previous messages to deliver, no need to send another notification"))
          return;
        }

        ServiceIdentitySessionPtr pThis = mThisWeak.lock();

        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(log("attempting to notify of message to browser window needing to be delivered"))
            mDelegate->onServiceIdentitySessionPendingMessageForInnerBrowserWindowFrame(pThis);
          } catch(IServiceIdentitySessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityProofBundleQuery
      #pragma mark

      ZS_DECLARE_CLASS_PTR(IdentityProofBundleQuery)

      class IdentityProofBundleQuery : public MessageQueueAssociator,
                                       public IServiceIdentityProofBundleQuery,
                                       public IBootstrappedNetworkDelegate
      {
      protected:
        //---------------------------------------------------------------------
        IdentityProofBundleQuery(
                                 IMessageQueuePtr queue,
                                 ElementPtr identityProofBundleEl,
                                 IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                 String identityURI
                                 ) :
          MessageQueueAssociator(queue),
          mID(zsLib::createPUID()),
          mIdentityProofBundleEl(identityProofBundleEl->clone()->toElement()),
          mDelegate(IServiceIdentityProofBundleQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
          mIdentityURI(identityURI),
          mErrorCode(0)
        {
        }

        //---------------------------------------------------------------------
        void init()
        {
          if (0 != mErrorReason) {
            notifyComplete();
          }

          IServiceIdentityPtr serviceIdentity = IServiceIdentity::createServiceIdentityFromIdentityProofBundle(mIdentityProofBundleEl);

          mBootstrappedNetwork = serviceIdentity->getBootstrappedNetwork();

          if (mBootstrappedNetwork->isPreparationComplete()) {
            ZS_LOG_DEBUG(log("bootstrapped network is already prepared, short-circuit answer now..."))
            onBootstrappedNetworkPreparationCompleted(mBootstrappedNetwork);
            return;
          }

          ZS_LOG_DEBUG(log("bootstrapped network is not ready yet, check for validity when ready"))
          IBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
        }

      public:
        //---------------------------------------------------------------------
        ~IdentityProofBundleQuery()
        {
          mThisWeak.reset();
        }

        //---------------------------------------------------------------------
        static IdentityProofBundleQueryPtr create(
                                                  ElementPtr identityProofBundleEl,
                                                  IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                                  String identityURI,
                                                  WORD failedErrorCode,
                                                  const String &failedReason
                                                  )
        {
          ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)
          ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

          IdentityProofBundleQueryPtr pThis = IdentityProofBundleQueryPtr(new IdentityProofBundleQuery(UseStack::queueStack(), identityProofBundleEl, delegate, identityURI));
          pThis->mThisWeak = pThis;
          pThis->mErrorCode = failedErrorCode;
          pThis->mErrorReason = failedReason;
          pThis->init();
          return pThis;
        }

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => IServiceIdentityProofBundleQuery
        #pragma mark

        //---------------------------------------------------------------------
        virtual bool isComplete() const
        {
          AutoRecursiveLock lock(mLock);
          if (0 != mErrorReason) return true;
          return !mDelegate;
        }

        //---------------------------------------------------------------------
        virtual bool wasSuccessful(
                                   WORD *outErrorCode = NULL,
                                   String *outErrorReason = NULL
                                   ) const
        {
          AutoRecursiveLock lock(mLock);
          if (outErrorCode) {
            *outErrorCode = mErrorCode;
          }
          if (outErrorReason) {
            *outErrorReason = mErrorReason;
          }
          if (0 != mErrorReason) {
            return false;
          }
          return !mDelegate;
        }

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => IBootstrappedNetworkDelegate
        #pragma mark

        //---------------------------------------------------------------------
        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
        {
          AutoRecursiveLock lock(mLock);
          if (isComplete()) return;

          WORD errorCode = 0;
          String reason;
          if (!bootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
            if (0 == errorCode) {
              errorCode = IHTTP::HTTPStatusCode_NoResponse;
            }
            setError(errorCode, reason);
          }

          IServiceCertificatesPtr serviceCertificates = IServiceCertificates::createServiceCertificatesFrom(mBootstrappedNetwork);
          ZS_THROW_BAD_STATE_IF(!serviceCertificates)

          ElementPtr identityProofEl = mIdentityProofBundleEl->findFirstChildElement("identityProof");
          ZS_THROW_BAD_STATE_IF(!identityProofEl)

          if (!serviceCertificates->isValidSignature(mIdentityProofBundleEl)) {
            setError(IHTTP::HTTPStatusCode_CertError, (String("identity failed to validate") + ", identity uri=" + mIdentityURI).c_str());
          }

          notifyComplete();
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => (internal)
        #pragma mark

        //---------------------------------------------------------------------
        Log::Params log(const char *message) const
        {
          ElementPtr objectEl = Element::create("IdentityProofBundleQuery");
          UseServicesHelper::debugAppend(objectEl, "id", mID);
          return Log::Params(message, objectEl);
        }

        //---------------------------------------------------------------------
        void setError(WORD errorCode, const char *reason = NULL)
        {
          if (!reason) {
            reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
          }

          if (0 != mErrorCode) {
            ZS_LOG_WARNING(Debug, log("attempting to set an error when error already set") + ZS_PARAM("new error code", mErrorCode) + ZS_PARAM("new reason", reason) + ZS_PARAM("existing error code", mErrorCode) + ZS_PARAM("existing reason", mErrorReason))
            return;
          }

          mErrorCode = errorCode;
          mErrorReason = reason;

          ZS_LOG_WARNING(Debug, log("setting error code") + ZS_PARAM("identity uri", mIdentityURI) + ZS_PARAM("error code", mErrorCode) + ZS_PARAM("reason", reason))
        }

        //---------------------------------------------------------------------
        void notifyComplete()
        {
          if (!mDelegate) return;

          IdentityProofBundleQueryPtr pThis = mThisWeak.lock();
          if (!pThis) return;

          try {
            mDelegate->onServiceIdentityProofBundleQueryCompleted(pThis);
          } catch (IServiceIdentityProofBundleQueryDelegateProxy::Exceptions::DelegateGone &) {
          }

          mDelegate.reset();
        }

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IdentityProofBundleQuery => (data)
        #pragma mark

        mutable RecursiveLock mLock;
        PUID mID;
        IdentityProofBundleQueryWeakPtr mThisWeak;
        String mIdentityURI;

        IServiceIdentityProofBundleQueryDelegatePtr mDelegate;
        IBootstrappedNetworkPtr mBootstrappedNetwork;

        ElementPtr mIdentityProofBundleEl;

        WORD mErrorCode;
        String mErrorReason;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceIdentitySessionFactory &IServiceIdentitySessionFactory::singleton()
      {
        return ServiceIdentitySessionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::loginWithIdentity(
                                                                                  IServiceIdentitySessionDelegatePtr delegate,
                                                                                  IServiceIdentityPtr provider,
                                                                                  IServiceNamespaceGrantSessionPtr grantSession,
                                                                                  IServiceLockboxSessionPtr existingLockbox,
                                                                                  const char *outerFrameURLUponReload,
                                                                                  const char *identityURI_or_identityBaseURI
                                                                                  )
      {
        if (this) {}
        return ServiceIdentitySession::loginWithIdentity(delegate, provider, grantSession, existingLockbox, outerFrameURLUponReload, identityURI_or_identityBaseURI);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::loginWithIdentityPreauthorized(
                                                                                               IServiceIdentitySessionDelegatePtr delegate,
                                                                                               IServiceIdentityPtr provider,
                                                                                               IServiceNamespaceGrantSessionPtr grantSession,
                                                                                               IServiceLockboxSessionPtr existingLockbox,  // pass NULL IServiceLockboxSessionPtr() if none exists
                                                                                               const char *identityURI,
                                                                                               const Token &identityToken
                                                                                               )
      {
        if (this) {}
        return ServiceIdentitySession::loginWithIdentityPreauthorized(delegate, provider, grantSession, existingLockbox, identityURI, identityToken);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::reload(
                                                                       BootstrappedNetworkPtr provider,
                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                       IServiceLockboxSessionPtr existingLockbox,
                                                                       const char *identityURI,
                                                                       const char *reloginKey
                                                                       )
      {
        if (this) {}
        return ServiceIdentitySession::reload(provider, grantSession, existingLockbox, identityURI, reloginKey);
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
    #pragma mark IServiceIdentity
    #pragma mark

    //-------------------------------------------------------------------------
    static Log::Params slog(const char *message)
    {
      return internal::slog(message);
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValid(const char *identityURI)
    {
      if (!identityURI) {
        ZS_LOG_WARNING(Detail, String("identity URI is not valid as it is NULL, uri=(null)"))
        return false;
      }

      String domainOrType;
      String identifier;

      return splitURI(identityURI, domainOrType, identifier);
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValidBase(const char *identityBase)
    {
      if (!identityBase) {
        ZS_LOG_WARNING(Detail, String("identity base is not valid as it is NULL, uri=(null)"))
        return false;
      }

      String domainOrType;
      String identifier;

      bool result = splitURI(identityBase, domainOrType, identifier);
      if (!result) {
        ZS_LOG_WARNING(Detail, slog("identity base URI is not valid") + ZS_PARAM("uri", identityBase))
        return false;
      }

      if (identifier.hasData()) {
        ZS_LOG_WARNING(Detail, slog("identity base URI is not valid") + ZS_PARAM("uri", identityBase) + ZS_PARAM("identifier", identifier))
        return false;
      }

      return true;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isLegacy(const char *identityURI)
    {
      if (!identityURI) {
        ZS_LOG_WARNING(Detail, slog("identity URI is not valid as it is NULL, uri=(null)"))
        return false;
      }

      std::regex e("^identity:[a-zA-Z0-9\\-_]{0,61}:.*$");
      if (!std::regex_match(identityURI, e)) {
        return false;
      }
      return true;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::splitURI(
                                    const char *inIdentityURI,
                                    String &outDomainOrLegacyType,
                                    String &outIdentifier,
                                    bool *outIsLegacy
                                    )
    {
      String identityURI(inIdentityURI ? inIdentityURI : "");

      identityURI.trim();
      if (outIsLegacy) *outIsLegacy = false;

      // scope: check legacy identity
      {
        std::regex e("^identity:[a-zA-Z0-9\\-_]{0,61}:.*$");
        if (std::regex_match(identityURI, e)) {

          // find second colon
          size_t startPos = strlen("identity:");
          size_t colonPos = identityURI.find(':', identityURI.find(':')+1);

          ZS_THROW_BAD_STATE_IF(colonPos == String::npos)

          outDomainOrLegacyType = identityURI.substr(startPos, colonPos - startPos);
          outDomainOrLegacyType.toLower();
          outIdentifier = identityURI.substr(colonPos + 1);

          if (outIsLegacy) *outIsLegacy = true;
          return true;
        }
      }

      std::regex e("^identity:\\/\\/..*\\/.*$");
      if (!std::regex_match(identityURI, e)) {
        ZS_LOG_WARNING(Detail, slog("identity URI is not valid") + ZS_PARAM("uri", identityURI))
        return false;
      }

      size_t startPos = strlen("identity://");
      size_t slashPos = identityURI.find('/', startPos);

      ZS_THROW_BAD_STATE_IF(slashPos == String::npos)

      outDomainOrLegacyType = identityURI.substr(startPos, slashPos - startPos);
      outDomainOrLegacyType.toLower();
      outIdentifier = identityURI.substr(slashPos + 1);

      if (!internal::UseServicesHelper::isValidDomain(outDomainOrLegacyType)) {
        ZS_LOG_WARNING(Detail, slog("identity URI domain is not valid") + ZS_PARAM("uri", identityURI) + ZS_PARAM("domain", outDomainOrLegacyType))

        outDomainOrLegacyType.clear();
        outIdentifier.clear();

        return false;
      }

      outDomainOrLegacyType.toLower();
      return true;
    }

    //-------------------------------------------------------------------------
    String IServiceIdentity::joinURI(
                                     const char *inDomainOrType,
                                     const char *inIdentifier
                                     )
    {
      String domainOrType(inDomainOrType);
      String identifier(inIdentifier);

      domainOrType.trim();
      identifier.trim();

      if (String::npos == domainOrType.find('.')) {
        // this is legacy

        String result = "identity:" + domainOrType + ":" + identifier;
        if (identifier.hasData()) {
          if (!isValid(result)) {
            ZS_LOG_WARNING(Detail, slog("invalid identity URI created after join") + ZS_PARAM("uri", result))
            return String();
          }
        } else {
          if (!isValidBase(result)) {
            ZS_LOG_WARNING(Detail, slog("invalid identity URI created after join") + ZS_PARAM("uri", result))
            return String();
          }
        }
        return result;
      }

      domainOrType.toLower();

      String result = "identity://" + domainOrType + "/" + identifier;
      if (identifier.hasData()) {
        if (!isValid(result)) {
          ZS_LOG_WARNING(Detail, slog("invalid identity URI created after join") + ZS_PARAM("uri", result))
          return String();
        }
      } else {
        if (!isValidBase(result)) {
          ZS_LOG_WARNING(Detail, slog("invalid identity URI created after join") + ZS_PARAM("uri", result))
          return String();
        }
      }
      return result;
    }

    //-------------------------------------------------------------------------
    bool IServiceIdentity::isValidIdentityProofBundle(
                                                      ElementPtr identityProofBundleEl,
                                                      IPeerFilePublicPtr peerFilePublic,
                                                      String *outPeerURI,
                                                      String *outIdentityURI,
                                                      String *outStableID,
                                                      Time *outCreated,
                                                      Time *outExpires
                                                      )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)

      IServiceIdentityPtr serviceIdentity = createServiceIdentityFromIdentityProofBundle(identityProofBundleEl);
      if (!serviceIdentity) {
        ZS_LOG_WARNING(Detail, slog("failed to obtain bootstrapped network from identity proof bundle"))
        return false;
      }

      IBootstrappedNetworkPtr bootstrapper = serviceIdentity->getBootstrappedNetwork();
      if (!bootstrapper->isPreparationComplete()) {
        ZS_LOG_WARNING(Detail, slog("bootstapped network isn't prepared yet"))
        return false;
      }

      WORD errorCode = 0;
      String reason;
      if (!bootstrapper->wasSuccessful(&errorCode, &reason)) {
        ZS_LOG_WARNING(Detail, slog("bootstapped network was not successful") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))
        return false;
      }

      String identityURI;

      bool result = internal::extractAndVerifyProof(
                                                    identityProofBundleEl,
                                                    peerFilePublic,
                                                    outPeerURI,
                                                    &identityURI,
                                                    outStableID,
                                                    outCreated,
                                                    outExpires
                                                    );

      if (outIdentityURI) {
        *outIdentityURI = identityURI;
      }

      if (!result) {
        ZS_LOG_WARNING(Detail, slog("signature validation failed on identity bundle") + ZS_PARAM("identity", identityURI))
        return false;
      }

      IServiceCertificatesPtr serviceCertificate = IServiceCertificates::createServiceCertificatesFrom(bootstrapper);
      ZS_THROW_BAD_STATE_IF(!serviceCertificate)

      ElementPtr identityProofEl = identityProofBundleEl->findFirstChildElement("identityProof");
      ZS_THROW_BAD_STATE_IF(!identityProofEl)

      if (!serviceCertificate->isValidSignature(identityProofEl)) {
        ZS_LOG_WARNING(Detail, slog("signature failed to validate on identity bundle") + ZS_PARAM("identity", identityURI))
        return false;
      }

      ZS_LOG_TRACE(slog("signature verified for identity") + ZS_PARAM("identity", identityURI))
      return true;
    }

    //-------------------------------------------------------------------------
    IServiceIdentityProofBundleQueryPtr IServiceIdentity::isValidIdentityProofBundle(
                                                                                     ElementPtr identityProofBundleEl,
                                                                                     IServiceIdentityProofBundleQueryDelegatePtr delegate,
                                                                                     IPeerFilePublicPtr peerFilePublic, // optional recommended check of associated peer file, can pass in IPeerFilePublicPtr() if not known yet
                                                                                     String *outPeerURI,
                                                                                     String *outIdentityURI,
                                                                                     String *outStableID,
                                                                                     Time *outCreated,
                                                                                     Time *outExpires
                                                                                     )
    {
      ZS_THROW_INVALID_ARGUMENT_IF(!identityProofBundleEl)
      ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

      String identityURI;

      bool result = internal::extractAndVerifyProof(
                                                    identityProofBundleEl,
                                                    peerFilePublic,
                                                    outPeerURI,
                                                    &identityURI,
                                                    outStableID,
                                                    outCreated,
                                                    outExpires
                                                    );

      if (outIdentityURI) {
        *outIdentityURI = identityURI;
      }

      WORD errorCode = 0;
      String reason;
      if (!result) {
        errorCode = IHTTP::HTTPStatusCode_CertError;
        reason = "identity failed to validate, identity=" + identityURI;
      }

      return internal::IdentityProofBundleQuery::create(identityProofBundleEl, delegate, identityURI, errorCode, reason);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceIdentitySession
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IServiceIdentitySession::toDebug(IServiceIdentitySessionPtr session)
    {
      return internal::ServiceIdentitySession::toDebug(session);
    }

    //-------------------------------------------------------------------------
    const char *IServiceIdentitySession::toString(SessionStates state)
    {
      switch (state)
      {
        case SessionState_Pending:                                  return "Pending";
        case SessionState_WaitingAttachmentOfDelegate:              return "Waiting Attachment of Delegate";
        case SessionState_WaitingForBrowserWindowToBeLoaded:        return "Waiting for Browser Window to be Loaded";
        case SessionState_WaitingForBrowserWindowToBeMadeVisible:   return "Waiting for Browser Window to be Made Visible";
        case SessionState_WaitingForBrowserWindowToBeRedirected:    return "Waiting for Browser Window to be Redirected";
        case SessionState_WaitingForBrowserWindowToClose:           return "Waiting for Browser Window to Close";
        case SessionState_WaitingForAssociationToLockbox:           return "Waiting for Association to Lockbox";
        case SessionState_Ready:                                    return "Ready";
        case SessionState_Shutdown:                                 return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IServiceIdentitySessionPtr IServiceIdentitySession::loginWithIdentity(
                                                                          IServiceIdentitySessionDelegatePtr delegate,
                                                                          IServiceIdentityPtr provider,
                                                                          IServiceNamespaceGrantSessionPtr grantSession,
                                                                          IServiceLockboxSessionPtr existingLockbox,
                                                                          const char *outerFrameURLUponReload,
                                                                          const char *identityURI
                                                                          )
    {
      return internal::IServiceIdentitySessionFactory::singleton().loginWithIdentity(delegate, provider, grantSession, existingLockbox, outerFrameURLUponReload, identityURI);
    }

    //-------------------------------------------------------------------------
    IServiceIdentitySessionPtr IServiceIdentitySession::loginWithIdentityPreauthorized(
                                                                                       IServiceIdentitySessionDelegatePtr delegate,
                                                                                       IServiceIdentityPtr provider,
                                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                                       IServiceLockboxSessionPtr existingLockbox,  // pass NULL IServiceLockboxSessionPtr() if none exists
                                                                                       const char *identityURI,
                                                                                       const Token &identityToken
                                                                                       )
    {
      return internal::IServiceIdentitySessionFactory::singleton().loginWithIdentityPreauthorized(delegate, provider, grantSession, existingLockbox, identityURI, identityToken);
    }
  }
}
