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

#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>

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

      using services::IHelper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxAccessRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxNamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxIdentitiesUpdateRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxContentGetRequest)
      ZS_DECLARE_USING_PTR(message::identity_lockbox, LockboxContentSetRequest)

      ZS_DECLARE_USING_PTR(message::peer, PeerServicesGetRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::ServicePushMailboxSession(
                                                           IMessageQueuePtr queue,
                                                           BootstrappedNetworkPtr network,
                                                           IServicePushMailboxSessionDelegatePtr delegate,
                                                           IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                           IServicePushMailboxPtr servicePushMailbox,
                                                           ServiceNamespaceGrantSessionPtr grantSession,
                                                           IServiceLockboxSessionPtr lockboxSession
                                                           ) :
        zsLib::MessageQueueAssociator(queue),
        mLockPtr(new RecursiveLock),
        mLock(*mLockPtr),
        mDelegate(delegate ? IServicePushMailboxSessionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate) : IServicePushMailboxSessionDelegatePtr()),
        mBootstrappedNetwork(network),
        mGrantSession(grantSession),
        mCurrentState(SessionState_Pending)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::~ServicePushMailboxSession()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::init()
      {
        AutoRecursiveLock lock(getLock());

        UseBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionPtr ServicePushMailboxSession::convert(IServicePushMailboxSessionPtr session)
      {
        return dynamic_pointer_cast<ServicePushMailboxSession>(session);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::toDebug(IServicePushMailboxSessionPtr session)
      {
        if (!session) return ElementPtr();
        return ServicePushMailboxSession::convert(session)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionPtr ServicePushMailboxSession::create(
                                                                     IServicePushMailboxSessionDelegatePtr delegate,
                                                                     IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
      {
        return ServicePushMailboxSessionPtr();
      }

      //-----------------------------------------------------------------------
      PUID ServicePushMailboxSession::getID() const
      {
        return mID;
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionSubscriptionPtr ServicePushMailboxSession::subscribe(IServicePushMailboxSessionDelegatePtr delegate)
      {
        return IServicePushMailboxSessionSubscriptionPtr();
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxPtr ServicePushMailboxSession::getService() const
      {
        return IServicePushMailboxPtr();
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSession::SessionStates ServicePushMailboxSession::getState(
                                                                                    WORD *lastErrorCode,
                                                                                    String *lastErrorReason
                                                                                    ) const
      {
        return SessionState_Pending;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::cancel()
      {
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxRegisterQueryPtr ServicePushMailboxSession::registerDevice(
                                                                                    const char *deviceToken,
                                                                                    const char *mappedType,
                                                                                    bool unreadBadge,
                                                                                    const char *sound,
                                                                                    const char *action,
                                                                                    const char *launchImage,
                                                                                    unsigned int priority
                                                                                    )
      {
        return IServicePushMailboxRegisterQueryPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::monitorFolder(const char *folderName)
      {
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::getFolderMessageUpdates(
                                                              const char *inFolder,
                                                              String inLastVersionDownloaded,
                                                              String &outUpdatedToVersion,
                                                              PushMessageListPtr &outMessagesAdded,
                                                              MessageIDListPtr &outMessagesRemoved
                                                              )
      {
        outUpdatedToVersion = String();
        outMessagesAdded = PushMessageListPtr();
        outMessagesRemoved = MessageIDListPtr();

        return false;
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSendQueryPtr ServicePushMailboxSession::sendMessage(
                                                                             IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                             const PeerOrIdentityList &to,
                                                                             const PeerOrIdentityList &cc,
                                                                             const PeerOrIdentityList &bcc,
                                                                             const PushMessage &message,
                                                                             bool copyToSentFolder
                                                                             )
      {
        return IServicePushMailboxSendQueryPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::markPushMessageRead(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::deletePushMessage(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onWake()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServiceNamespaceGrantSessionWaitDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session)
      {
        ZS_LOG_DEBUG(log("namespace grant session wait is ready to be obtained (if needed)"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ServicePushMailboxSession::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("ServicePushMailboxSession");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));
        IHelper::debugAppend(resultEl, "grant session", mGrantSession ? mGrantSession->getID() : 0);
        IHelper::debugAppend(resultEl, "grant query", mGrantQuery ? mGrantQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "grant wait", mGrantWait ? mGrantWait->getID() : 0);

        IHelper::debugAppend(resultEl, "obtained lock", mObtainedLock);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepBootstrapper()) goto post_step;
        if (!stepGrantLock()) goto post_step;

        if (!stepGrantLockClear()) goto post_step;
        if (!stepGrantChallenge()) goto post_step;

        setState(SessionState_Ready);

      post_step:
        postStep();

        ZS_LOG_TRACE(debug("step done"))
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepBootstrapper()
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
      bool ServicePushMailboxSession::stepGrantLock()
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

        get(mObtainedLock) = true;
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepGrantLockClear()
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
      bool ServicePushMailboxSession::stepGrantChallenge()
      {
        return false;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::postStep()
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))
        mCurrentState = state;

        ServicePushMailboxSessionPtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            ZS_LOG_DEBUG(debug("attempting to report state to delegate"))
            mDelegate->onServicePushMailboxSessionStateChanged(pThis, mCurrentState);
          } catch (IServicePushMailboxSessionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }
        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
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
    #pragma mark IServicePushMailboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IServicePushMailboxSession::toString(SessionStates state)
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
    ElementPtr IServicePushMailboxSession::toDebug(IServicePushMailboxSessionPtr session)
    {
      return internal::ServicePushMailboxSession::toDebug(session);
    }

    //-------------------------------------------------------------------------
    IServicePushMailboxSessionPtr IServicePushMailboxSession::create(
                                                                     IServicePushMailboxSessionDelegatePtr delegate,
                                                                     IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
    {
      return internal::IServicePushMailboxSessionFactory::singleton().create(delegate, databaseDelegate, servicePushMailbox, grantSession, lockboxSession);
    }


  }
}
