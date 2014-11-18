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

#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
#include <openpeer/stack/message/identity-lockbox/LockboxAccessRequest.h>
#include <openpeer/stack/message/identity-lockbox/LockboxNamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/identity-lockbox/LockboxIdentitiesUpdateRequest.h>
#include <openpeer/stack/message/identity-lockbox/LockboxContentGetRequest.h>
#include <openpeer/stack/message/identity-lockbox/LockboxContentSetRequest.h>
#include <openpeer/stack/message/peer/PeerFileSetRequest.h>
#include <openpeer/stack/message/peer/PeerServicesGetRequest.h>

#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Stringize.h>

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/md5.h>

#define OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS (60*2)

#define OPENPEER_STACK_SERVICE_LOCKBOX_EXPIRES_TIME_PERCENTAGE_CONSUMED_CAUSES_REGENERATION (80)

#define OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE "https://meta.openpeer.org/permission/private-peer-file"
#define OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE "https://meta.openpeer.org/permission/identity-relogins"

#define OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_SECRET_VALUE_NAME "privatePeerFileSecret"
#define OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_VALUE_NAME "privatePeerFile"

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      using CryptoPP::Weak::MD5;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxAccessRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxNamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxIdentitiesUpdateRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxContentGetRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxContentSetRequest)

      ZS_DECLARE_USING_PTR(message::peer, PeerFileSetRequest)
      ZS_DECLARE_USING_PTR(message::peer, PeerServicesGetRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static void getNamespaces(NamespaceInfoMap &outNamespaces)
      {
        static const char *gPermissions[] = {
          OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE,
          OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE,
          NULL
        };

        for (int index = 0; NULL != gPermissions[index]; ++index)
        {
          NamespaceInfo info;
          info.mURL = gPermissions[index];
          outNamespaces[info.mURL] = info;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceLockboxSession::ServiceLockboxSession(
                                                   IMessageQueuePtr queue,
                                                   BootstrappedNetworkPtr network,
                                                   IServiceLockboxSessionDelegatePtr delegate,
                                                   ServiceNamespaceGrantSessionPtr grantSession
                                                   ) :
        zsLib::MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mDelegate(delegate ? IServiceLockboxSessionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate) : IServiceLockboxSessionDelegatePtr()),
        mBootstrappedNetwork(network),
        mGrantSession(grantSession),
        mCurrentState(SessionState_Pending)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSession::~ServiceLockboxSession()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::init()
      {
        AutoRecursiveLock lock(*this);

        calculateAndNotifyIdentityChanges();  // calculate the identities hash for the firs ttime

        UseBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::convert(IServiceLockboxSessionPtr session)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceLockboxSession, session);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::convert(ForAccountPtr session)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceLockboxSession, session);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::convert(ForServiceIdentityPtr session)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceLockboxSession, session);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::convert(ForServicePushMailboxPtr session)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceLockboxSession, session);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServiceLockboxSession::toDebug(IServiceLockboxSessionPtr session)
      {
        if (!session) return ElementPtr();
        return ServiceLockboxSession::convert(session)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::login(
                                                            IServiceLockboxSessionDelegatePtr delegate,
                                                            IServiceLockboxPtr serviceLockbox,
                                                            IServiceNamespaceGrantSessionPtr grantSession,
                                                            IServiceIdentitySessionPtr identitySession,
                                                            bool forceNewAccount
                                                            )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!serviceLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!identitySession)

        ServiceLockboxSessionPtr pThis(new ServiceLockboxSession(UseStack::queueStack(), BootstrappedNetwork::convert(serviceLockbox), delegate, ServiceNamespaceGrantSession::convert(grantSession)));
        pThis->mThisWeak = pThis;
        pThis->mLoginIdentity = ServiceIdentitySession::convert(identitySession);
        pThis->mForceNewAccount = forceNewAccount;
        if (forceNewAccount) {
          ZS_LOG_WARNING(Detail, pThis->log("forcing creation of a new lockbox account (user must have indicated their account is compromised or corrupted)"))
        }
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr ServiceLockboxSession::relogin(
                                                              IServiceLockboxSessionDelegatePtr delegate,
                                                              IServiceLockboxPtr serviceLockbox,
                                                              IServiceNamespaceGrantSessionPtr grantSession,
                                                              const char *lockboxAccountID,
                                                              const SecureByteBlock &lockboxKey
                                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!serviceLockbox)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantSession)
        ZS_THROW_INVALID_ARGUMENT_IF(!lockboxAccountID)
        ZS_THROW_INVALID_ARGUMENT_IF(lockboxKey.SizeInBytes() < 1)

        ServiceLockboxSessionPtr pThis(new ServiceLockboxSession(UseStack::queueStack(), BootstrappedNetwork::convert(serviceLockbox), delegate, ServiceNamespaceGrantSession::convert(grantSession)));
        pThis->mThisWeak = pThis;
        pThis->mLockboxInfo.mAccountID = String(lockboxAccountID);
        pThis->mLockboxInfo.mKey = IHelper::clone(lockboxKey);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceLockboxPtr ServiceLockboxSession::getService() const
      {
        AutoRecursiveLock lock(*this);
        return BootstrappedNetwork::convert(mBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      IServiceLockboxSession::SessionStates ServiceLockboxSession::getState(
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
      IPeerFilesPtr ServiceLockboxSession::getPeerFiles() const
      {
        AutoRecursiveLock lock(*this);
        return mPeerFiles;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getAccountID() const
      {
        AutoRecursiveLock lock(*this);
        return mLockboxInfo.mAccountID;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getDomain() const
      {
        AutoRecursiveLock lock(*this);
        if (!mBootstrappedNetwork) return String();
        return mBootstrappedNetwork->getDomain();
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getStableID() const
      {
        AutoRecursiveLock lock(*this);

        if (mLockboxInfo.mAccountID.isEmpty()) return String();
        if (!mBootstrappedNetwork) return String();

        return IHelper::convertToHex(*IHelper::hash(String("stable-id:") + mBootstrappedNetwork->getDomain() + ":" + mLockboxInfo.mAccountID));
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr ServiceLockboxSession::getLockboxKey() const
      {
        AutoRecursiveLock lock(*this);

        return IHelper::clone(mLockboxInfo.mKey);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionListPtr ServiceLockboxSession::getAssociatedIdentities() const
      {
        AutoRecursiveLock lock(*this);
        ServiceIdentitySessionListPtr result(new ServiceIdentitySessionList);
        for (ServiceIdentitySessionMap::const_iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          result->push_back(ServiceIdentitySession::convert((*iter).second));
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::associateIdentities(
                                                      const ServiceIdentitySessionList &identitiesToAssociate,
                                                      const ServiceIdentitySessionList &identitiesToRemove
                                                      )
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, debug("unable to associate identity as already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("associate identities called") + ZS_PARAM("associate size", identitiesToAssociate.size()) + ZS_PARAM("remove size", identitiesToRemove.size()))

        for (ServiceIdentitySessionList::const_iterator iter = identitiesToAssociate.begin(); iter != identitiesToAssociate.end(); ++iter)
        {
          UseServiceIdentitySessionPtr session = ServiceIdentitySession::convert(*iter);
          session->associate(mThisWeak.lock());
          mPendingUpdateIdentities[session->getID()] = session;
        }
        for (ServiceIdentitySessionList::const_iterator iter = identitiesToRemove.begin(); iter != identitiesToRemove.end(); ++iter)
        {
          UseServiceIdentitySessionPtr session = ServiceIdentitySession::convert(*iter);
          mPendingRemoveIdentities[session->getID()] = session;
        }

        ZS_LOG_DEBUG(log("waking up to process identities") + ZS_PARAM("pending size", mPendingUpdateIdentities.size()) + ZS_PARAM("pending remove size", mPendingRemoveIdentities.size()))

        // handle the association now (but do it asynchronously)
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        if (mGrantQuery) {
          mGrantQuery->cancel();
          mGrantQuery.reset();
        }

        if (mGrantWait) {
          mGrantWait->cancel();
          mGrantWait.reset();
        }

        if (mLockboxAccessMonitor) {
          mLockboxAccessMonitor->cancel();
          mLockboxAccessMonitor.reset();
        }
        if (mLockboxNamespaceGrantChallengeValidateMonitor) {
          mLockboxNamespaceGrantChallengeValidateMonitor->cancel();
          mLockboxNamespaceGrantChallengeValidateMonitor.reset();
        }
        if (mLockboxIdentitiesUpdateMonitor) {
          mLockboxIdentitiesUpdateMonitor->cancel();
          mLockboxIdentitiesUpdateMonitor.reset();
        }
        if (mLockboxContentGetMonitor) {
          mLockboxContentGetMonitor->cancel();
          mLockboxContentGetMonitor.reset();
        }
        if (mLockboxContentSetMonitor) {
          mLockboxContentSetMonitor->cancel();
          mLockboxContentSetMonitor.reset();
        }
        if (mPeerFileSetMonitor) {
          mPeerFileSetMonitor->cancel();
          mPeerFileSetMonitor.reset();
        }
        if (mPeerServicesGetMonitor) {
          mPeerServicesGetMonitor->cancel();
          mPeerServicesGetMonitor.reset();
        }

        if (mSaltQuery) {
          mSaltQuery->cancel();
          mSaltQuery.reset();
        }

        if (mPeerFileKeyGenerator) {
          mPeerFileKeyGenerator->cancel();
          mPeerFileKeyGenerator.reset();
        }

        setState(SessionState_Shutdown);

        mAccount.reset();

        ServiceLockboxSessionPtr pThis = mThisWeak.lock();
        if (pThis) {
          mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(pThis);
        }

        mLoginIdentity.reset();

        mAssociatedIdentities.clear();
        mPendingUpdateIdentities.clear();
        mPendingRemoveIdentities.clear();

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageSource
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::attach(AccountPtr account)
      {
        AutoRecursiveLock lock(*this);
        mAccount = account;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      Service::MethodListPtr ServiceLockboxSession::findServiceMethods(
                                                                       const char *serviceType,
                                                                       const char *method
                                                                       ) const
      {
        if (NULL == serviceType) return Service::MethodListPtr();
        if (NULL == method) return Service::MethodListPtr();

        ServiceTypeMap::const_iterator found = mServicesByType.find(serviceType);
        if (found == mServicesByType.end()) return Service::MethodListPtr();

        Service::MethodListPtr result;

        const ServiceMap &serviceMap = (*found).second;

        for (ServiceMap::const_iterator iter = serviceMap.begin(); iter != serviceMap.end(); ++iter)
        {
          const Service &service = (*iter).second;

          Service::MethodMap::const_iterator foundMethod = service.mMethods.find(method);
          if (foundMethod == service.mMethods.end()) continue;

          if (!result) {
            result = Service::MethodListPtr(new Service::MethodList);
          }

          Service::MethodPtr method(new Service::Method);
          (*method) = ((*foundMethod).second);

          result->push_back(method);
        }

        return result;
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr ServiceLockboxSession::getBootstrappedNetwork() const
      {
        AutoRecursiveLock lock(*this);
        return BootstrappedNetwork::convert(mBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionForServiceIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      LockboxInfo ServiceLockboxSession::getLockboxInfo() const
      {
        AutoRecursiveLock lock(*this);
        return mLockboxInfo;
      }

      //-----------------------------------------------------------------------
      IdentityInfo ServiceLockboxSession::getIdentityInfoForIdentity(
                                                                     ServiceIdentitySessionPtr inSession,
                                                                     IPeerFilesPtr *outPeerFiles
                                                                     ) const
      {
        UseServiceIdentitySessionPtr session = inSession;

        if (outPeerFiles) {
          *outPeerFiles = IPeerFilesPtr();
        }

        AutoRecursiveLock lock(*this);

        IdentityInfo info;

        info.mStableID = getStableID();

        if (mPeerFiles) {
          info.mPeerFilePublic = mPeerFiles->getPeerFilePublic();

          if (outPeerFiles) {
            *outPeerFiles = mPeerFiles;
          }
        }

        WORD priority = 0;

        for (ServiceIdentitySessionMap::const_iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          const UseServiceIdentitySessionPtr &identity = (*iter).second;

          if (identity->getID() == session->getID()) {
            break;
          }

          ++priority;
        }

        info.mPriority = priority;

        return info;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::notifyStateChanged()
      {
        ZS_LOG_DEBUG(log("notify state changed"))
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceLockboxSessionForServicePushMailbox
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceLockboxSessionForInternalSubscriptionPtr ServiceLockboxSession::subscribe(IServiceLockboxSessionForInternalDelegatePtr originalDelegate)
      {
        ZS_LOG_DETAIL(log("subscribing to lockbox session"))

        AutoRecursiveLock lock(*this);
        if (!originalDelegate) return IServiceLockboxSessionForInternalSubscriptionPtr();

        IServiceLockboxSessionForInternalSubscriptionPtr subscription = mLockboxSubscriptions.subscribe(originalDelegate);

        IServiceLockboxSessionForInternalDelegatePtr delegate = mLockboxSubscriptions.delegate(subscription);

        if (delegate) {
          // ServiceLockboxSessionPtr pThis = mThisWeak.lock();
        }

        if (isShutdown()) {
          mLockboxSubscriptions.clear();
        }

        return subscription;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onWake()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IKeyGeneratorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onKeyGenerated(IKeyGeneratorPtr generator)
      {
        ZS_LOG_DEBUG(log("key generator is complete") + ZS_PARAM("generator id", generator->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceSaltFetchSignedSaltQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onServiceSaltFetchSignedSaltCompleted(IServiceSaltFetchSignedSaltQueryPtr query)
      {
        ZS_LOG_DEBUG(log("salt service reported complete"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceNamespaceGrantSessionWaitDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session)
      {
        ZS_LOG_DEBUG(log("namespace grant session wait is ready to be obtained (if needed)"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IServiceSaltFetchSignedSaltQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                                         IServiceNamespaceGrantSessionQueryPtr query,
                                                                                         ElementPtr namespaceGrantChallengeBundleEl
                                                                                         )
      {
        ZS_LOG_DEBUG(log("namespace grant session state changed"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxAccessResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     LockboxAccessResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxAccessMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_DEBUG(log("handle lockbox access result"))

        mLockboxAccessMonitor->cancel();
        mLockboxAccessMonitor.reset();

        LockboxInfo info = result->lockboxInfo();
        mLockboxInfo.mergeFrom(info, true);

        mServerIdentities = result->identities();

        NamespaceGrantChallengeInfo challengeInfo = result->namespaceGrantChallengeInfo();

        if (challengeInfo.mID.hasData()) {
          // a namespace grant challenge was issue
          mGrantQuery = mGrantSession->query(mThisWeak.lock(), challengeInfo);
        }

        step();

        ServiceLockboxSessionPtr pThis = mThisWeak.lock();
        mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(pThis);

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          LockboxAccessResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxAccessMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("lockbox access failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxNamespaceGrantChallengeValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     LockboxNamespaceGrantChallengeValidateResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mLockboxNamespaceGrantChallengeValidateMonitor->cancel();
        mLockboxNamespaceGrantChallengeValidateMonitor.reset();

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          LockboxNamespaceGrantChallengeValidateResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxAccessMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("lockbox access failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxIdentitiesUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     LockboxIdentitiesUpdateResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxIdentitiesUpdateMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mLockboxIdentitiesUpdateMonitor->cancel();
        mLockboxIdentitiesUpdateMonitor.reset();

        mServerIdentities = result->identities();

        step();

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          LockboxIdentitiesUpdateResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxIdentitiesUpdateMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("identities update failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxContentGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     LockboxContentGetResultPtr result
                                                                     )
      {
        typedef LockboxContentGetResult::NameValueMap NameValueMap;

        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxContentGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mLockboxContentGetMonitor->cancel();
        mLockboxContentGetMonitor.reset();

        ZS_LOG_DEBUG(log("content get completed"))

        mContent = result->namespaceURLNameValues();

        // add some bogus content just to ensure there is some values in the map
        if (mContent.size() < 1) {
          NameValueMap values;
          mContent["bogus"] = values;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          LockboxContentGetResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxContentGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("content get failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<LockboxContentSetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     LockboxContentSetResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxContentSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mLockboxContentSetMonitor->cancel();
        mLockboxContentSetMonitor.reset();

        ZS_LOG_DEBUG(log("content set completed"))

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          LockboxContentSetResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mLockboxContentSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("content set failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerFileSetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     PeerFileSetResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mPeerFileSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mPeerFileSetMonitor->cancel();
        mPeerFileSetMonitor.reset();

        ZS_LOG_DEBUG(log("peer file set completed"))

        mPeerFileSet = true;

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          PeerFileSetResultPtr ignore,
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mPeerFileSetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("peer file set failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => IMessageMonitorResultDelegate<PeerServicesGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     PeerServicesGetResultPtr result
                                                                     )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mPeerServicesGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        mPeerServicesGetMonitor->cancel();
        mPeerServicesGetMonitor.reset();

        ZS_LOG_DEBUG(log("peer services get completed"))

        mServicesByType = result->servicesByType();
        if (mServicesByType.size() < 1) {
          // make sure to add at least one bogus service so we know this request completed
          Service service;
          service.mID = "bogus";
          service.mType = "bogus";

          ServiceMap bogusMap;
          bogusMap[service.mID] = service;

          mServicesByType[service.mType] = bogusMap;
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          PeerServicesGetResultPtr ignore, // will always be NULL
                                                                          message::MessageResultPtr result
                                                                          )
      {
        AutoRecursiveLock lock(*this);
        if (monitor != mPeerServicesGetMonitor) {
          ZS_LOG_DEBUG(log("notified about obsolete monitor"))
          return false;
        }

        ZS_LOG_WARNING(Detail, log("peer services get failed"))

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceLockboxSession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServiceLockboxSession::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServiceLockboxSession");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServiceLockboxSession::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServiceLockboxSession::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServiceLockboxSession");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);
        IHelper::debugAppend(resultEl, "account", (bool)mAccount.lock());

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));
        IHelper::debugAppend(resultEl, "grant session", mGrantSession ? mGrantSession->getID() : 0);
        IHelper::debugAppend(resultEl, "grant query", mGrantQuery ? mGrantQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "grant wait", mGrantWait ? mGrantWait->getID() : 0);

        IHelper::debugAppend(resultEl, "lockbox access monitor", (bool)mLockboxAccessMonitor);
        IHelper::debugAppend(resultEl, "lockbox grant validate monitor", (bool)mLockboxNamespaceGrantChallengeValidateMonitor);
        IHelper::debugAppend(resultEl, "lockbox identities update monitor", (bool)mLockboxIdentitiesUpdateMonitor);
        IHelper::debugAppend(resultEl, "lockbox content get monitor", (bool)mLockboxContentGetMonitor);
        IHelper::debugAppend(resultEl, "lockbox content set monitor", (bool)mLockboxContentSetMonitor);
        IHelper::debugAppend(resultEl, "peer file set monitor", (bool)mPeerFileSetMonitor);
        IHelper::debugAppend(resultEl, "peer services get monitor", (bool)mPeerServicesGetMonitor);

        IHelper::debugAppend(resultEl, mLockboxInfo.toDebug());

        IHelper::debugAppend(resultEl, "login identity id", mLoginIdentity ? mLoginIdentity->getID() : 0);

        IHelper::debugAppend(resultEl, IPeerFiles::toDebug(mPeerFiles));

        IHelper::debugAppend(resultEl, "obtained lock", mObtainedLock);

        IHelper::debugAppend(resultEl, "login identity set to become associated", mLoginIdentitySetToBecomeAssociated);

        IHelper::debugAppend(resultEl, "force new account", mForceNewAccount);

        IHelper::debugAppend(resultEl, "salt query id", mSaltQuery ? mSaltQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "peer file key generator id", mPeerFileKeyGenerator ? mPeerFileKeyGenerator->getID() : 0);
        IHelper::debugAppend(resultEl, "peer file generated", mPeerFilesGenerated);
        IHelper::debugAppend(resultEl, "peer file set", mPeerFileSet);

        IHelper::debugAppend(resultEl, "services by type", mServicesByType.size());

        IHelper::debugAppend(resultEl, "server identities", mServerIdentities.size());

        IHelper::debugAppend(resultEl, "associated identities", mAssociatedIdentities.size());
        IHelper::debugAppend(resultEl, "last notification hash", mLastNotificationHash ? IHelper::convertToHex(*mLastNotificationHash) : String());

        IHelper::debugAppend(resultEl, "pending updated identities", mPendingUpdateIdentities.size());
        IHelper::debugAppend(resultEl, "pending remove identities", mPendingRemoveIdentities.size());

        IHelper::debugAppend(resultEl, "relogin change hash", mReloginChangeHash ? IHelper::convertToHex(*mReloginChangeHash) : String());

        IHelper::debugAppend(resultEl, "content", mContent.size());

        IHelper::debugAppend(resultEl, "updated content", mUpdatedContent.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepBootstrapper()) goto post_step;
        if (!stepGrantLock()) goto post_step;
        if (!stepIdentityLogin()) goto post_step;
        if (!stepLockboxAccess()) goto post_step;
        if (!stepGrantLockClear()) goto post_step;
        if (!stepGrantChallenge()) goto post_step;
        if (!stepContentGet()) goto post_step;
        if (!stepPreparePeerFiles()) goto post_step;
        if (!stepPeerFileSet()) goto post_step;
        if (!stepPeerServicesGet()) goto post_step;
        if (!stepPreReadyCheck()) goto post_step;

        setState(SessionState_Ready);

        if (!stepLoginIdentityBecomeAssociated()) goto post_step;
        if (!stepConvertFromServerToRealIdentities()) goto post_step;
        if (!stepPruneDuplicatePendingIdentities()) goto post_step;
        if (!stepPruneShutdownIdentities()) goto post_step;
        if (!stepPendingAssociationAndRemoval()) goto post_step;
        if (!stepContentUpdate()) goto post_step;

      post_step:
        postStep();

        ZS_LOG_TRACE(debug("step done"))
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepBootstrapper()
      {
        if (!mBootstrappedNetwork->isPreparationComplete()) {
          setState(SessionState_Pending);

          ZS_LOG_TRACE(log("waiting for preparation of lockbox bootstrapper to complete"))
          return false;
        }

        WORD errorCode = 0;
        String reason;

        if (mBootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
          ZS_LOG_TRACE(log("lockbox bootstrapper was successful"))
          return true;
        }

        ZS_LOG_ERROR(Detail, log("bootstrapped network failed for lockbox") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

        setError(errorCode, reason);
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepGrantLock()
      {
        if (mObtainedLock) {
          ZS_LOG_TRACE(log("already informed browser window ready thus no need to make sure grant wait lock is obtained"))
          return true;
        }

        if (mGrantWait) {
          ZS_LOG_TRACE(log("grant wait lock is already obtained"))
          return true;
        }

        // NOTE: While the lockbox service itself doesn't load a browser window
        // and thus does not need to obtain a grant wait directly, it can cause
        // a namespace grant challenge at the same time the rolodex causes a
        // grant challenge, so it's better to cause all namespace grants to
        // happen at once rather than asking the user to issue a grant
        // permission twice.

        mGrantWait = mGrantSession->obtainWaitToProceed(mThisWeak.lock());

        if (!mGrantWait) {
          ZS_LOG_TRACE(log("waiting to obtain grant wait lock"))
          return false;
        }

        ZS_LOG_DEBUG(log("obtained grant lock"))

        mObtainedLock = true;
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepIdentityLogin()
      {
        if (!mLoginIdentity) {
          ZS_LOG_TRACE(log("no identity being logged in"))
          return true;
        }

        if (mLoginIdentity->isShutdown()) {
          WORD errorCode = 0;
          String reason;

          mLoginIdentity->getState(&errorCode, &reason);

          if (0 == errorCode) {
            errorCode = IHTTP::HTTPStatusCode_ClientClosedRequest;
          }

          ZS_LOG_WARNING(Detail, log("shutting down lockbox because identity login is shutdown"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        if (!mLoginIdentity->isAssociated()) {
          // require the association now to ensure the identity changes in state cause lockbox state changes too
          ZS_LOG_DEBUG(log("associated login identity to lockbox"))
          
          mLoginIdentity->associate(mThisWeak.lock());
        }

        if (mLoginIdentity->isLoginComplete()) {
          ZS_LOG_TRACE(log("identity login is complete"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting for login to complete"))

        setState(SessionState_Pending);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepLockboxAccess()
      {
        if (mLockboxAccessMonitor) {
          ZS_LOG_TRACE(log("waiting for lockbox access monitor to complete"))
          return false;
        }

        if (mLockboxInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("already have a lockbox token"))
          return true;
        }

        setState(SessionState_Pending);

        LockboxAccessRequestPtr request = LockboxAccessRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        if (mLoginIdentity) {
          IdentityInfo identityInfo = mLoginIdentity->getIdentityInfo();
          request->identityInfo(identityInfo);

          LockboxInfo lockboxInfo = mLoginIdentity->getLockboxInfo();
          mLockboxInfo.mergeFrom(lockboxInfo);

          if (!IHelper::isValidDomain(mLockboxInfo.mDomain)) {
            ZS_LOG_DEBUG(log("domain from identity is invalid, reseting to default domain") + ZS_PARAM("domain", mLockboxInfo.mDomain))

            mLockboxInfo.mDomain = mBootstrappedNetwork->getDomain();

            // account/keying information must also be incorrect if domain is not valid
            mLockboxInfo.mKey.reset();
          }

          if (mBootstrappedNetwork->getDomain() != mLockboxInfo.mDomain) {
            ZS_LOG_DEBUG(log("default bootstrapper is not to be used for this lockbox as an altenative lockbox must be used thus preparing replacement bootstrapper"))

            mBootstrappedNetwork = BootstrappedNetwork::convert(IBootstrappedNetwork::prepare(mLockboxInfo.mDomain, mThisWeak.lock()));
            return false;
          }

          if (mForceNewAccount) {
            ZS_LOG_DEBUG(log("forcing a new lockbox account to be created for the identity"))
            mForceNewAccount = false;
            mLockboxInfo.mKey.reset();
          }

          if ((!mLockboxInfo.mKey) ||
              (mLockboxInfo.mKeyName.isEmpty())) {
            mLockboxInfo.mAccountID.clear();
            mLockboxInfo.mKeyName.clear();
            mLockboxInfo.mKey.reset();
            mLockboxInfo.mResetFlag = true;

            mLockboxInfo.mKeyName = IHelper::convertToHex(*IHelper::hash(IHelper::randomString(32)));
            mLockboxInfo.mKey = IHelper::convertToBuffer(IHelper::randomString(32*8/5));

            ZS_LOG_DEBUG(log("created new lockbox key") + ZS_PARAM("key", IHelper::convertToBase64(*mLockboxInfo.mKey)))
          }

          ZS_THROW_BAD_STATE_IF(!mLockboxInfo.mKey)
        }

        mLockboxInfo.mDomain = mBootstrappedNetwork->getDomain();

        request->grantID(mGrantSession->getGrantID());
        request->lockboxInfo(mLockboxInfo);

        NamespaceInfoMap namespaces;
        getNamespaces(namespaces);
        request->namespaceURLs(namespaces);

        mLockboxAccessMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxAccessResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-access", request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepGrantLockClear()
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
      bool ServiceLockboxSession::stepGrantChallenge()
      {
        if (mLockboxNamespaceGrantChallengeValidateMonitor) {
          ZS_LOG_TRACE(log("waiting for lockbox namespace grant challenge validate monitor to complete"))
          return false;
        }

        if (!mGrantQuery) {
          ZS_LOG_TRACE(log("no grant challenge query thus continuing..."))
          return true;
        }

        if (!mGrantQuery->isComplete()) {
          ZS_LOG_TRACE(log("waiting for the grant query to complete"))
          return false;
        }

        ElementPtr bundleEl = mGrantQuery->getNamespaceGrantChallengeBundle();
        if (!bundleEl) {
          ZS_LOG_ERROR(Detail, log("namespaces were no granted in challenge"))
          setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access lockbox");
          cancel();
          return false;
        }

        NamespaceInfoMap namespaces;
        getNamespaces(namespaces);

        for (NamespaceInfoMap::iterator iter = namespaces.begin(); iter != namespaces.end(); ++iter)
        {
          NamespaceInfo &namespaceInfo = (*iter).second;

          if (!mGrantSession->isNamespaceURLInNamespaceGrantChallengeBundle(bundleEl, namespaceInfo.mURL)) {
            ZS_LOG_WARNING(Detail, log("lockbox was not granted required namespace") + ZS_PARAM("namespace", namespaceInfo.mURL))
            setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access lockbox");
            cancel();
            return false;
          }
        }

        mGrantQuery->cancel();
        mGrantQuery.reset();

        ZS_LOG_DEBUG(log("all namespaces required were correctly granted, notify the lockbox of the newly created access"))

        LockboxNamespaceGrantChallengeValidateRequestPtr request = LockboxNamespaceGrantChallengeValidateRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->lockboxInfo(mLockboxInfo);
        request->namespaceGrantChallengeBundle(bundleEl);

        mLockboxNamespaceGrantChallengeValidateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxNamespaceGrantChallengeValidateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-namespace-grant-challenge-validate", request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepContentGet()
      {
        if (mLockboxContentGetMonitor) {
          ZS_LOG_TRACE(log("waiting for content get to complete"))
          return false;
        }

        if (mContent.size() > 0) {
          ZS_LOG_TRACE(log("content has been obtained already"))
          return true;
        }

        setState(SessionState_Pending);

        LockboxContentGetRequestPtr request = LockboxContentGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        NamespaceInfoMap namespaces;
        getNamespaces(namespaces);

        request->lockboxInfo(mLockboxInfo);
        request->namespaceInfos(namespaces);

        mLockboxContentGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxContentGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-content-get", request);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPreparePeerFiles()
      {
        if (mPeerFiles) {
          ZS_LOG_TRACE(log("peer files already created/loaded"))
          return true;
        }

        if (mSaltQuery) {
          if (!mSaltQuery->isComplete()) {
            ZS_LOG_TRACE(log("waiting for salt query to complete"))
            return false;
          }

          WORD errorCode = 0;
          String reason;
          if (!mSaltQuery->wasSuccessful(&errorCode, &reason)) {
            ZS_LOG_ERROR(Detail, log("failed to fetch signed salt") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        if (mPeerFileKeyGenerator) {
          if (!mPeerFileKeyGenerator->isComplete()) {
            ZS_LOG_TRACE(log("waiting for key generation to complete"))
            return false;
          }

          mPeerFiles = mPeerFileKeyGenerator->getPeerFiles();
          if (!mPeerFiles) {
            ZS_LOG_ERROR(Detail, log("failed to generate peer files"))
            setError(IHTTP::HTTPStatusCode_InternalServerError, "failed to generate peer files");
            cancel();
            return false;
          }

          ZS_LOG_DEBUG(log("peer files were generated"))
          setState(SessionState_Pending);

          IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();
          ZS_THROW_BAD_STATE_IF(!peerFilePrivate)

          SecureByteBlockPtr peerFileSecret = peerFilePrivate->getPassword();
          ZS_THROW_BAD_STATE_IF(!peerFileSecret)

          ElementPtr peerFileEl = peerFilePrivate->saveToElement();
          ZS_THROW_BAD_STATE_IF(!peerFileEl)

          GeneratorPtr generator = Generator::createJSONGenerator();
          std::unique_ptr<char[]> output = generator->write(peerFileEl);

          String privatePeerFileStr = output.get();

          setContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_SECRET_VALUE_NAME, IHelper::convertToString(*peerFileSecret));
          setContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_VALUE_NAME, privatePeerFileStr);

          mPeerFileKeyGenerator->cancel();
          mPeerFileKeyGenerator.reset();

          mPeerFilesGenerated = true;

          mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(mThisWeak.lock());
          return true;
        }

        setState(SessionState_Pending);

        String privatePeerSecretStr = getContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_SECRET_VALUE_NAME);
        String privatePeerFileStr = getContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_VALUE_NAME);

        if ((privatePeerSecretStr.hasData()) &&
            (privatePeerFileStr.hasData())) {

          ZS_LOG_TRACE(log("loading private peer file"))

          // attempt to load the private peer file data
          DocumentPtr doc = Document::createFromParsedJSON(privatePeerFileStr);
          if (doc) {
            mPeerFiles = IPeerFiles::loadFromElement(privatePeerSecretStr, doc->getFirstChildElement());
            if (!mPeerFiles) {
              ZS_LOG_ERROR(Detail, log("peer files failed to load (will generate new peer files) - recoverable but this should not happen"))
            }
          }
        }

        if (mPeerFiles) {
          ZS_LOG_DEBUG(log("peer files successfully loaded"))

          mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(mThisWeak.lock());

          IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();
          ZS_THROW_BAD_STATE_IF(!peerFilePublic)

          Time created = peerFilePublic->getCreated();
          Time expires = peerFilePublic->getExpires();

          Time now = zsLib::now();

          if (now > expires) {
            ZS_LOG_WARNING(Detail, log("peer file expired") + IPeerFilePublic::toDebug(peerFilePublic) + ZS_PARAM("now", now))
            mPeerFiles.reset();
          }

          auto totalLifetime(expires - created);
          auto lifeConsumed(now - created);

          if (((lifeConsumed.count() * 100) / totalLifetime.count()) > OPENPEER_STACK_SERVICE_LOCKBOX_EXPIRES_TIME_PERCENTAGE_CONSUMED_CAUSES_REGENERATION) {
            ZS_LOG_WARNING(Detail, log("peer file are past acceptable expiry window") + ZS_PARAM("lifetime consumed (s)", lifeConsumed) + ZS_PARAM("total lifetime (s)", totalLifetime) + IPeerFilePublic::toDebug(peerFilePublic) + ZS_PARAM("now", now))
            mPeerFiles.reset();
          }

          if (mPeerFiles) {
            ZS_LOG_DEBUG(log("peer files are still valid"))
            return true;
          }

          ZS_LOG_DEBUG(log("peer files will be regenerated"))

          // erase out the current peer file information if it exists from memory (prevents them from becoming reloaded / retested later)
          clearContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_SECRET_VALUE_NAME);
          clearContent(OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_NAMESPACE, OPENPEER_STACK_SERVICE_LOCKBOX_PRIVATE_PEER_FILE_VALUE_NAME);
        }

        if (!mSaltQuery) {
          IServiceSaltPtr saltService = IServiceSalt::createServiceSaltFrom(BootstrappedNetwork::convert(mBootstrappedNetwork));
          mSaltQuery = IServiceSaltFetchSignedSaltQuery::fetchSignedSalt(mThisWeak.lock(), saltService);

          ZS_LOG_DEBUG(log("waiting for signed salt query to complete"))
          return false;
        }

        setState(SessionState_PendingPeerFilesGeneration);

        ElementPtr signedSaltEl = mSaltQuery->getNextSignedSalt();
        if (!signedSaltEl) {
          ZS_LOG_ERROR(Detail, log("failed to obtain signed salt from salt query"))
          setError(IHTTP::HTTPStatusCode_PreconditionFailed, "signed salt query was successful but failed to obtain signed salt");
          cancel();
          return false;
        }

        IKeyGeneratorPtr rsaKeyGenerator;

        UseAccountPtr account = mAccount.lock();
        if (account) {
          // the account might have attempted to create an RSA key on our behalf in the background while attempting to log in, take it over now...
          rsaKeyGenerator = account->takeOverRSAGeyGeneration();
        }

        mPeerFileKeyGenerator = IKeyGenerator::generatePeerFiles(mThisWeak.lock(), IHelper::randomString((32*8)/5+1), signedSaltEl, rsaKeyGenerator);
        ZS_THROW_BAD_STATE_IF(!mPeerFileKeyGenerator)

        ZS_LOG_DEBUG(log("generating peer files (may take a while)..."))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPeerFileSet()
      {
        if (!mPeerFilesGenerated) {
          ZS_LOG_TRACE(log("no need to set peer files as peer file was not generated"))
          return true;
        }

        if (mPeerFileSet) {
          ZS_LOG_TRACE(log("peer files already set (thus no need to do it again)"))
          return true;
        }

        if (mPeerFileSetMonitor) {
          ZS_LOG_TRACE(log("waiting for services get to complete"))
          return true;
        }

        ZS_THROW_BAD_STATE_IF(!mPeerFiles)  // thus must be now valid

        IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();
        ZS_THROW_BAD_STATE_IF(!peerFilePublic)  // thus must be now valid

        setState(SessionState_Pending);

        ZS_LOG_DEBUG(log("requesting peer file set to be completed on server"))

        PeerFileSetRequestPtr request = PeerFileSetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->lockboxInfo(mLockboxInfo);
        request->peerFilePublic(peerFilePublic);

        mPeerFileSetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerFileSetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("peer", "peer-file-set", request);
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPeerServicesGet()
      {
        if (mServicesByType.size() > 0) {
          ZS_LOG_TRACE(log("already download services"))
          return true;
        }

        if (mPeerServicesGetMonitor) {
          ZS_LOG_TRACE(log("waiting for services get to complete"))
          return true;
        }

        setState(SessionState_Pending);

        ZS_LOG_DEBUG(log("requesting information about the peer services available"))

        PeerServicesGetRequestPtr request = PeerServicesGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->lockboxInfo(mLockboxInfo);

        mPeerServicesGetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<PeerServicesGetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("peer", "peer-services-get", request);
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPreReadyCheck()
      {
        ZS_LOG_TRACE(log("make sure any requests that are parallelized are completed before going to ready"))

        if (mPeerFileSetMonitor) {
          ZS_LOG_TRACE(log("waiting for peer file set to complete"))
          return false;
        }

        if (mPeerServicesGetMonitor) {
          ZS_LOG_TRACE(log("waiting for services get to complete"))
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepLoginIdentityBecomeAssociated()
      {
        if (!mLoginIdentity) {
          ZS_LOG_TRACE(log("did not login with a login identity (thus no need to force it to associate)"))
          return true;
        }

        if (mLoginIdentitySetToBecomeAssociated) {
          ZS_LOG_TRACE(log("login identity is already set to become associated"))
          return true;
        }

        ZS_LOG_DEBUG(log("login identity will become associated identity") + UseServiceIdentitySession::toDebug(mLoginIdentity))

        mLoginIdentitySetToBecomeAssociated = true;

        for (ServiceIdentitySessionMap::iterator associatedIter = mAssociatedIdentities.begin(); associatedIter != mAssociatedIdentities.end(); ++associatedIter)
        {
          UseServiceIdentitySessionPtr &identity = (*associatedIter).second;
          if (identity->getID() == mLoginIdentity->getID()) {
            ZS_LOG_DEBUG(log("login identity is already associated"))
            return true;
          }
        }

        for (ServiceIdentitySessionMap::iterator pendingIter = mPendingUpdateIdentities.begin(); pendingIter != mPendingUpdateIdentities.end(); ++pendingIter)
        {
          UseServiceIdentitySessionPtr &identity = (*pendingIter).second;
          if (identity->getID() == mLoginIdentity->getID()) {
            ZS_LOG_DEBUG(log("login identity is already in pending list"))
            return true;
          }
        }

        mPendingUpdateIdentities[mLoginIdentity->getID()] = mLoginIdentity;
        ZS_LOG_DEBUG(log("adding login identity to the pending list so that it will become associated (if it is not already known by the server)") + ZS_PARAM("pending size", mPendingUpdateIdentities.size()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepConvertFromServerToRealIdentities()
      {
        if (mServerIdentities.size() < 1) {
          ZS_LOG_TRACE(log("no server identities that need to be converted to identities"))
          return true;
        }

        for (IdentityInfoList::iterator iter = mServerIdentities.begin(); iter != mServerIdentities.end();)
        {
          IdentityInfoList::iterator current = iter;
          ++iter;

          IdentityInfo &info = (*current);

          bool foundMatch = false;

          for (ServiceIdentitySessionMap::iterator assocIter = mAssociatedIdentities.begin(); assocIter != mAssociatedIdentities.end(); ++assocIter)
          {
            UseServiceIdentitySessionPtr &identitySession = (*assocIter).second;
            IdentityInfo associatedInfo = identitySession->getIdentityInfo();

            if ((info.mURI == associatedInfo.mURI) &&
                (info.mProvider == associatedInfo.mProvider)) {
              // found an existing match...
              ZS_LOG_DEBUG(log("found a match to a previously associated identity") + ZS_PARAM("uri", info.mURI) + ZS_PARAM("provider", info.mProvider))
              foundMatch = true;
              break;
            }
          }

          if (foundMatch) continue;

          for (ServiceIdentitySessionMap::iterator pendingIter = mPendingUpdateIdentities.begin(); pendingIter != mPendingUpdateIdentities.end(); )
          {
            ServiceIdentitySessionMap::iterator pendingCurrentIter = pendingIter;
            ++pendingIter;

            UseServiceIdentitySessionPtr &identitySession = (*pendingCurrentIter).second;
            IdentityInfo pendingInfo = identitySession->getIdentityInfo();

            if ((info.mURI == pendingInfo.mURI) &&
                (info.mProvider == pendingInfo.mProvider)) {

              if (pendingInfo.mReloginKey.hasData()) {
                String hash = IHelper::convertToHex(*IHelper::hash(String("identity-relogin:") + pendingInfo.mURI + ":" + pendingInfo.mProvider));

                setContent(OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE, hash, pendingInfo.mReloginKey);
              }

              // move the pending identity to the actual identity rather than creating a new identity
              mAssociatedIdentities[identitySession->getID()] = identitySession;

              // found an existing match...
              ZS_LOG_DEBUG(log("found a match to a pending identity (moving pending identity to associated identity)") + ZS_PARAM("uri", info.mURI) + ZS_PARAM("provider", info.mProvider) + ZS_PARAM("associated size", mAssociatedIdentities.size()))

              foundMatch = true;
              break;
            }
          }

          if (foundMatch) continue;

          // no match to an existing identity, attempt a relogin
          String domain;
          String id;
          IServiceIdentity::splitURI(info.mURI, domain, id);

          UseBootstrappedNetworkPtr network = mBootstrappedNetwork;
          if (domain != info.mProvider) {
            // not using the lockbox provider, instead using the provider specified
            network = BootstrappedNetwork::convert(IBootstrappedNetwork::prepare(info.mProvider));
          }

          String hash = IHelper::convertToHex(*IHelper::hash(String("identity-relogin:") + info.mURI + ":" + info.mProvider));

          String reloginKey = getContent(OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE, hash);

          ZS_LOG_DEBUG(log("reloading identity") + ZS_PARAM("identity uri", info.mURI) + ZS_PARAM("provider", info.mProvider) + ZS_PARAM("relogin key", reloginKey))

          UseServiceIdentitySessionPtr identitySession = UseServiceIdentitySession::reload(BootstrappedNetwork::convert(network), ServiceNamespaceGrantSession::convert(mGrantSession), mThisWeak.lock(), info.mURI, reloginKey);
          mAssociatedIdentities[identitySession->getID()] = identitySession;
        }

        // all server identities should now be processed or matched
        mServerIdentities.clear();

        ZS_LOG_DEBUG(log("finished moving server identity to associated identities"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPruneDuplicatePendingIdentities()
      {
        if (mPendingUpdateIdentities.size() < 1) {
          ZS_LOG_TRACE(log("no identities pending update"))
          return true;
        }

        ZS_LOG_DEBUG(log("checking if any pending identities that are already associated that need to be pruned"))

        for (ServiceIdentitySessionMap::iterator assocIter = mAssociatedIdentities.begin(); assocIter != mAssociatedIdentities.end(); ++assocIter)
        {
          UseServiceIdentitySessionPtr &identitySession = (*assocIter).second;
          IdentityInfo associatedInfo = identitySession->getIdentityInfo();

          for (ServiceIdentitySessionMap::iterator pendingUpdateIter = mPendingUpdateIdentities.begin(); pendingUpdateIter != mPendingUpdateIdentities.end();)
          {
            ServiceIdentitySessionMap::iterator pendingUpdateCurrentIter = pendingUpdateIter;
            ++pendingUpdateIter;

            UseServiceIdentitySessionPtr &pendingIdentity = (*pendingUpdateCurrentIter).second;

            IdentityInfo pendingUpdateInfo = pendingIdentity->getIdentityInfo();
            if ((pendingUpdateInfo.mURI == associatedInfo.mURI) &&
                (pendingUpdateInfo.mProvider = associatedInfo.mProvider)) {
              ZS_LOG_DEBUG(log("identity pending update is actually already associated so remove it from the pending list (thus will prune this identity)") + ZS_PARAM("uri", associatedInfo.mURI) + ZS_PARAM("provider", associatedInfo.mProvider))
              mPendingUpdateIdentities.erase(pendingUpdateCurrentIter);
              continue;
            }
          }
        }

        ZS_LOG_DEBUG(log("finished pruning duplicate pending identities"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPruneShutdownIdentities()
      {
        ZS_LOG_TRACE(log("pruning shutdown identities"))

        for (ServiceIdentitySessionMap::iterator pendingUpdateIter = mPendingUpdateIdentities.begin(); pendingUpdateIter != mPendingUpdateIdentities.end();)
        {
          ServiceIdentitySessionMap::iterator pendingUpdateCurrentIter = pendingUpdateIter;
          ++pendingUpdateIter;

          UseServiceIdentitySessionPtr &pendingUpdateIdentity = (*pendingUpdateCurrentIter).second;
          if (!pendingUpdateIdentity->isShutdown()) continue;

          // cannot associate an identity that shutdown
          ZS_LOG_WARNING(Detail, log("pending identity shutdown unexpectedly") + UseServiceIdentitySession::toDebug(pendingUpdateIdentity));
          mPendingUpdateIdentities.erase(pendingUpdateCurrentIter);
        }

        ZS_LOG_TRACE(log("pruning of shutdown identities complete"))

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepPendingAssociationAndRemoval()
      {
        if ((mLockboxIdentitiesUpdateMonitor) ||
            (mLockboxContentSetMonitor)) {
          ZS_LOG_TRACE(log("waiting for identities association or content set to complete before attempting to associate identities"))
          return false;
        }

        if ((mPendingUpdateIdentities.size() < 1) &&
            (mPendingRemoveIdentities.size() < 1)) {
          ZS_LOG_TRACE(log("no identities are pending addition or removal"))
          return true;
        }

        ServiceIdentitySessionMap removedIdentities;
        ServiceIdentitySessionMap completedIdentities;

        for (ServiceIdentitySessionMap::iterator pendingRemovalIter = mPendingRemoveIdentities.begin(); pendingRemovalIter != mPendingRemoveIdentities.end();)
        {
          ServiceIdentitySessionMap::iterator pendingRemovalCurrentIter = pendingRemovalIter;
          ++pendingRemovalIter;

          UseServiceIdentitySessionPtr &pendingRemovalIdentity = (*pendingRemovalCurrentIter).second;
          IdentityInfo pendingRemovalIdentityInfo = pendingRemovalIdentity->getIdentityInfo();

          ZS_LOG_DEBUG(log("checking if identity to be removed is in the udpate list") + UseServiceIdentitySession::toDebug(pendingRemovalIdentity))

          for (ServiceIdentitySessionMap::iterator pendingUpdateIter = mPendingUpdateIdentities.begin(); pendingUpdateIter != mPendingUpdateIdentities.end(); )
          {
            ServiceIdentitySessionMap::iterator pendingUpdateCurrentIter = pendingUpdateIter;
            ++pendingUpdateIter;

            UseServiceIdentitySessionPtr &pendingUpdateIdentity = (*pendingUpdateCurrentIter).second;

            if (pendingUpdateIdentity->getID() == pendingRemovalIdentity->getID()) {
              ZS_LOG_DEBUG(log("identity being removed is in the pending list (thus will remove it from pending list)") + UseServiceIdentitySession::toDebug(pendingRemovalIdentity))

              mPendingUpdateIdentities.erase(pendingUpdateCurrentIter);
              continue;
            }
          }

          bool foundMatch = false;

          for (ServiceIdentitySessionMap::iterator associatedIter = mAssociatedIdentities.begin(); associatedIter != mAssociatedIdentities.end();)
          {
            ServiceIdentitySessionMap::iterator associatedCurrentIter = associatedIter;
            ++associatedIter;

            UseServiceIdentitySessionPtr &associatedIdentity = (*associatedCurrentIter).second;

            if (associatedIdentity->getID() != pendingRemovalIdentity->getID()) continue;

            foundMatch = true;

            ZS_LOG_DEBUG(log("killing association to the associated identity") + UseServiceIdentitySession::toDebug(pendingRemovalIdentity))

            // clear relogin key (if present)
            {
              String hash = IHelper::convertToHex(*IHelper::hash(String("identity-relogin:") + pendingRemovalIdentityInfo.mURI + ":" + pendingRemovalIdentityInfo.mProvider));
              clearContent(OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE, hash);
            }

            removedIdentities[pendingRemovalIdentity->getID()] = pendingRemovalIdentity;

            // force the identity to disassociate from the lockbox
            pendingRemovalIdentity->killAssociation(mThisWeak.lock());

            mAssociatedIdentities.erase(associatedCurrentIter);
            mPendingRemoveIdentities.erase(pendingRemovalCurrentIter);
          }

          if (foundMatch) continue;

          ZS_LOG_DEBUG(log("killing identity that was never associated") + UseServiceIdentitySession::toDebug(pendingRemovalIdentity))

          mPendingRemoveIdentities.erase(pendingRemovalCurrentIter);
        }

        for (ServiceIdentitySessionMap::iterator pendingUpdateIter = mPendingUpdateIdentities.begin(); pendingUpdateIter != mPendingUpdateIdentities.end();)
        {
          ServiceIdentitySessionMap::iterator pendingUpdateCurrentIter = pendingUpdateIter;
          ++pendingUpdateIter;

          UseServiceIdentitySessionPtr &pendingUpdateIdentity = (*pendingUpdateCurrentIter).second;

          if (!pendingUpdateIdentity->isLoginComplete()) continue;

          ZS_LOG_DEBUG(log("pending identity is now logged in (thus can cause the association)") + UseServiceIdentitySession::toDebug(pendingUpdateIdentity))
          completedIdentities[pendingUpdateIdentity->getID()] = pendingUpdateIdentity;

          IdentityInfo info = pendingUpdateIdentity->getIdentityInfo();
          if (info.mReloginKey.hasData()) {
            String hash = IHelper::convertToHex(*IHelper::hash(String("identity-relogin:") + info.mURI + ":" + info.mProvider));

            setContent(OPENPEER_STACK_SERVICE_LOCKBOX_IDENTITY_RELOGINS_NAMESPACE, hash, info.mReloginKey);
          }
        }

        if ((removedIdentities.size() > 0) ||
            (completedIdentities.size() > 0))
        {
          IdentityInfoList removeInfos;
          IdentityInfoList updateInfos;

          for (ServiceIdentitySessionMap::iterator iter = removedIdentities.begin(); iter != removedIdentities.end(); ++iter)
          {
            UseServiceIdentitySessionPtr &identity = (*iter).second;

            IdentityInfo info = identity->getIdentityInfo();

            if ((info.mURI.hasData()) &&
                (info.mProvider.hasData())) {
              ZS_LOG_DEBUG(log("adding identity to request removal list") + info.toDebug())
              removeInfos.push_back(info);
            }
          }

          for (ServiceIdentitySessionMap::iterator iter = completedIdentities.begin(); iter != completedIdentities.end(); ++iter)
          {
            UseServiceIdentitySessionPtr &identity = (*iter).second;

            IdentityInfo info = identity->getIdentityInfo();

            ZS_LOG_DEBUG(log("adding identity to request update list") + info.toDebug())
            updateInfos.push_back(info);
          }

          if ((removeInfos.size() > 0) ||
              (updateInfos.size() > 0)) {

            ZS_LOG_DEBUG(log("sending update identities request"))

            LockboxIdentitiesUpdateRequestPtr request = LockboxIdentitiesUpdateRequest::create();
            request->domain(mBootstrappedNetwork->getDomain());
            request->lockboxInfo(mLockboxInfo);
            request->identitiesToUpdate(updateInfos);
            request->identitiesToRemove(removeInfos);

            mLockboxIdentitiesUpdateMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxIdentitiesUpdateResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
            mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-identities-update", request);

            // NOTE: It's entirely possible the associate request can fail. Unfortunately, there is very little that can be done upon failure. The user will have to take some responsibility to keep their identities associated.
          }
        }

        ZS_LOG_DEBUG(log("associating and removing of identities completed") + ZS_PARAM("updated", completedIdentities.size()) + ZS_PARAM("removed", removedIdentities.size()))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceLockboxSession::stepContentUpdate()
      {
        if (mUpdatedContent.size() < 1) {
          ZS_LOG_TRACE(log("no content to update"))
          return true;
        }

        if (mLockboxContentSetMonitor) {
          ZS_LOG_TRACE(log("waiting for lockbox content set monitor to complete"))
          return true;
        }

        ZS_LOG_DEBUG(log("sending content set request"))

        LockboxContentSetRequestPtr request = LockboxContentSetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());
        request->lockboxInfo(mLockboxInfo);

        request->namespaceURLNameValues(mUpdatedContent);

        mLockboxContentSetMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxContentSetResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-content-set", request);

        mUpdatedContent.clear();  // forget this content ever changed so newly changed content will update

        return true;
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::postStep()
      {
        calculateAndNotifyIdentityChanges();
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))
        mCurrentState = state;

        ServiceLockboxSessionPtr pThis = mThisWeak.lock();
        if (pThis) {
          mLockboxSubscriptions.delegate()->onServiceLockboxSessionStateChanged(pThis);
        }

        UseAccountPtr account = mAccount.lock();
        if (account) {
          account->notifyServiceLockboxSessionStateChanged();
        }

        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(debug("attempting to report state to delegate"))
            mDelegate->onServiceLockboxSessionStateChanged(pThis, mCurrentState);
          } catch (IServiceLockboxSessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::setError(WORD errorCode, const char *inReason)
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

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      void ServiceLockboxSession::calculateAndNotifyIdentityChanges()
      {
        MD5 hasher;
        SecureByteBlockPtr output(new SecureByteBlock(hasher.DigestSize()));

        for (ServiceIdentitySessionMap::iterator iter = mAssociatedIdentities.begin(); iter != mAssociatedIdentities.end(); ++iter)
        {
          PUID id = (*iter).first;
          hasher.Update((const BYTE *)(&id), sizeof(id));
        }
        hasher.Final(*output);

        if (!mLastNotificationHash) {
          ZS_LOG_DEBUG(log("calculated identities for the first time"))
          mLastNotificationHash = output;
          return;
        }

        if (0 == IHelper::compare(*output, *mLastNotificationHash)) {
          // no change
          return;
        }

        mLastNotificationHash = output;

        if (mDelegate) {
          try {
            mDelegate->onServiceLockboxSessionAssociatedIdentitiesChanged(mThisWeak.lock());
          } catch(IServiceLockboxSessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getContent(
                                               const char *namespaceURL,
                                               const char *valueName
                                               ) const
      {
        typedef LockboxContentGetResult::NameValueMap NameValueMap;

        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceURL)
        ZS_THROW_INVALID_ARGUMENT_IF(!valueName)

        if (!mLockboxInfo.mKey) {
          ZS_LOG_DEBUG(debug("lockbox key is not known") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return String();
        }

        NamespaceURLNameValueMap::const_iterator found = mContent.find(namespaceURL);
        if (found == mContent.end()) {
          ZS_LOG_DEBUG(log("content does not contain namespace") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return String();
        }

        const NameValueMap &values = (*found).second;

        NameValueMap::const_iterator foundValue = values.find(valueName);
        if (foundValue == values.end()) {
          ZS_LOG_DEBUG(log("content does not contain namespace value") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return String();
        }

        const String &preSplitValue = (*foundValue).second;

        IHelper::SplitMap split;
        IHelper::split(preSplitValue, split, ':');

        SecureByteBlockPtr key = IHelper::hmac(*mLockboxInfo.mKey, (String("lockbox:") + namespaceURL + ":" + valueName).c_str(), IHelper::HashAlgorthm_SHA256);

        SecureByteBlockPtr result = stack::IHelper::splitDecrypt(*key, preSplitValue);
        if (!result) {
          ZS_LOG_WARNING(Detail, debug("failed to decrypt value") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("value", preSplitValue))
          return String();
        }

        String output = IHelper::convertToString(*result);

        ZS_LOG_TRACE(log("obtained content") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("output", output))
        return output;
      }

      //-----------------------------------------------------------------------
      String ServiceLockboxSession::getRawContent(
                                                  const char *namespaceURL,
                                                  const char *valueName
                                                  ) const
      {
        typedef LockboxContentGetResult::NameValueMap NameValueMap;

        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceURL)
        ZS_THROW_INVALID_ARGUMENT_IF(!valueName)

        NamespaceURLNameValueMap::const_iterator found = mContent.find(namespaceURL);
        if (found == mContent.end()) {
          ZS_LOG_WARNING(Detail, log("content does not contain namespace") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return String();
        }

        const NameValueMap &values = (*found).second;

        NameValueMap::const_iterator foundValue = values.find(valueName);
        if (foundValue == values.end()) {
          ZS_LOG_WARNING(Detail, log("content does not contain namespace value") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return String();
        }

        const String &value = (*foundValue).second;

        ZS_LOG_TRACE(log("found raw content value") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("raw value", value))
        return value;
      }
      
      //-----------------------------------------------------------------------
      void ServiceLockboxSession::setContent(
                                             const char *namespaceURL,
                                             const char *valueName,
                                             const char *value
                                             )
      {
        typedef LockboxContentGetResult::NameValueMap NameValueMap;

        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceURL)
        ZS_THROW_INVALID_ARGUMENT_IF(!valueName)

        String oldValue = getContent(namespaceURL, valueName);
        if (oldValue == value) {
          ZS_LOG_TRACE(log("content has not changed thus no need to update") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("content", value))
          return;
        }

        NamespaceURLNameValueMap::iterator found = mContent.find(namespaceURL);
        if (found == mContent.end()) {
          ZS_LOG_DEBUG(log("content does not contain namespace (thus creating)") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))

          NameValueMap empty;
          mContent[namespaceURL] = empty;
          found = mContent.find(namespaceURL);

          ZS_THROW_BAD_STATE_IF(found == mContent.end())
        }

        NamespaceURLNameValueMap::iterator foundUpdated = mUpdatedContent.find(namespaceURL);
        if (foundUpdated == mUpdatedContent.end()) {
          ZS_LOG_DEBUG(log("updated content does not contain namespace (thus creating)") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))

          NameValueMap empty;
          mUpdatedContent[namespaceURL] = empty;
          foundUpdated = mUpdatedContent.find(namespaceURL);

          ZS_THROW_BAD_STATE_IF(foundUpdated == mUpdatedContent.end())
        }

        NameValueMap &values = (*found).second;
        NameValueMap &valuesUpdated = (*foundUpdated).second;

        if (!mLockboxInfo.mKey) {
          ZS_LOG_WARNING(Detail, debug("failed to create a lockbox key") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return;
        }

        ZS_LOG_TRACE(log("encrpting content using") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("key", IHelper::convertToBase64(*mLockboxInfo.mKey)))

        SecureByteBlockPtr key = IHelper::hmac(*mLockboxInfo.mKey, (String("lockbox:") + namespaceURL + ":" + valueName).c_str(), IHelper::HashAlgorthm_SHA256);

        SecureByteBlockPtr dataToConvert = IHelper::convertToBuffer(value);
        if (!dataToConvert) {
          ZS_LOG_WARNING(Detail, debug("failed to prepare data to convert") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("value", value))
          return;
        }

        String encodedValue = stack::IHelper::splitEncrypt(*key, *dataToConvert);
        if (encodedValue.isEmpty()) {
          ZS_LOG_WARNING(Detail, debug("failed to encode encrypted to base64") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("value", value))
          return;
        }

        ZS_LOG_TRACE(log("content was set") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName) + ZS_PARAM("value", value))
        values[valueName] = encodedValue;
        valuesUpdated[valueName] = encodedValue;
      }
      
      //-----------------------------------------------------------------------
      void ServiceLockboxSession::clearContent(
                                               const char *namespaceURL,
                                               const char *valueName
                                               )
      {
        typedef LockboxContentGetResult::NameValueMap NameValueMap;

        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceURL)
        ZS_THROW_INVALID_ARGUMENT_IF(!valueName)

        NamespaceURLNameValueMap::iterator found = mContent.find(namespaceURL);
        if (found == mContent.end()) {
          ZS_LOG_WARNING(Detail, log("content does not contain namespace") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return;
        }

        NameValueMap &values = (*found).second;

        NameValueMap::iterator foundValue = values.find(valueName);
        if (foundValue == values.end()) {
          ZS_LOG_WARNING(Detail, log("content does not contain namespace value") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
          return;
        }

        NamespaceURLNameValueMap::iterator foundUpdated = mUpdatedContent.find(namespaceURL);
        if (foundUpdated == mUpdatedContent.end()) {
          ZS_LOG_DEBUG(log("updated content does not contain namespace (thus creating)") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))

          NameValueMap empty;
          mUpdatedContent[namespaceURL] = empty;
          foundUpdated = mUpdatedContent.find(namespaceURL);

          ZS_THROW_BAD_STATE_IF(foundUpdated == mUpdatedContent.end())
        }

        NameValueMap &valuesUpdated = (*foundUpdated).second;

        ZS_LOG_TRACE(log("content value cleared") + ZS_PARAM("namespace", namespaceURL) + ZS_PARAM("value name", valueName))
        values.erase(foundValue);

        valuesUpdated[valueName] = "-"; // this is the entry value for "delete" during an update
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceLockboxSessionFactory &IServiceLockboxSessionFactory::singleton()
      {
        return ServiceLockboxSessionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr IServiceLockboxSessionFactory::login(
                                                                    IServiceLockboxSessionDelegatePtr delegate,
                                                                    IServiceLockboxPtr serviceLockbox,
                                                                    IServiceNamespaceGrantSessionPtr grantSession,
                                                                    IServiceIdentitySessionPtr identitySession,
                                                                    bool forceNewAccount
                                                                    )
      {
        if (this) {}
        return ServiceLockboxSession::login(delegate, serviceLockbox, grantSession, identitySession, forceNewAccount);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr IServiceLockboxSessionFactory::relogin(
                                                                      IServiceLockboxSessionDelegatePtr delegate,
                                                                      IServiceLockboxPtr serviceLockbox,
                                                                      IServiceNamespaceGrantSessionPtr grantSession,
                                                                      const char *lockboxAccountID,
                                                                      const SecureByteBlock &lockboxKey
                                                                      )
      {
        if (this) {}
        return ServiceLockboxSession::relogin(delegate, serviceLockbox, grantSession, lockboxAccountID, lockboxKey);
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
    #pragma mark IServiceLockboxSession
    #pragma mark


    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IServiceLockboxSession::toString(SessionStates state)
    {
      switch (state)
      {
        case SessionState_Pending:                      return "Pending";
        case SessionState_PendingPeerFilesGeneration:   return "Pending Peer File Generation";
        case SessionState_Ready:                        return "Ready";
        case SessionState_Shutdown:                     return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    ElementPtr IServiceLockboxSession::toDebug(IServiceLockboxSessionPtr session)
    {
      return internal::ServiceLockboxSession::toDebug(session);
    }

    //-------------------------------------------------------------------------
    IServiceLockboxSessionPtr IServiceLockboxSession::login(
                                                            IServiceLockboxSessionDelegatePtr delegate,
                                                            IServiceLockboxPtr serviceLockbox,
                                                            IServiceNamespaceGrantSessionPtr grantSession,
                                                            IServiceIdentitySessionPtr identitySession,
                                                            bool forceNewAccount
                                                            )
    {
      return internal::IServiceLockboxSessionFactory::singleton().login(delegate, serviceLockbox, grantSession, identitySession, forceNewAccount);
    }

    //-------------------------------------------------------------------------
    IServiceLockboxSessionPtr IServiceLockboxSession::relogin(
                                                              IServiceLockboxSessionDelegatePtr delegate,
                                                              IServiceLockboxPtr serviceLockbox,
                                                              IServiceNamespaceGrantSessionPtr grantSession,
                                                              const char *lockboxAccountID,
                                                              const SecureByteBlock &lockboxKey
                                                              )
    {
      return internal::IServiceLockboxSessionFactory::singleton().relogin(delegate, serviceLockbox, grantSession, lockboxAccountID, lockboxKey);
    }

  }
}
