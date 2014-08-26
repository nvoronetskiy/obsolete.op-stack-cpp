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

#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
//#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
//
//#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
//#include <openpeer/stack/internal/stack_Helper.h>
//#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>
//
//#include <openpeer/stack/message/bootstrapped-servers/ServersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
//#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateRequest.h>
//#include <openpeer/stack/message/push-mailbox/PeerValidateRequest.h>
//
#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>
//#include <openpeer/services/ITCPMessaging.h>
//
//#include <zsLib/Log.h>
#include <zsLib/XML.h>
//#include <zsLib/helpers.h>
//
//#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;
//
      using services::IHelper;
//
//      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;
//
//      ZS_DECLARE_USING_PTR(openpeer::services, IBackgrounding)
//
//      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, AccessRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, NamespaceGrantChallengeValidateRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, PeerValidateRequest)

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
      #pragma mark ServicePushMailboxSession::RegisterQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::RegisterQuery::RegisterQuery(
                                                              IMessageQueuePtr queue,
                                                              const SharedRecursiveLock &lock,
                                                              ServicePushMailboxSessionPtr outer,
                                                              IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                              const PushSubscriptionInfo &subscriptionInfo
                                                              ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(lock),
        mOuter(outer),
        mDelegate(IServicePushMailboxRegisterQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mSubscriptionInfo(subscriptionInfo)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::RegisterQuery::init()
      {
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::RegisterQuery::~RegisterQuery()
      {
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::RegisterQuery => friend ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::RegisterQueryPtr ServicePushMailboxSession::RegisterQuery::create(
                                                                                                   IMessageQueuePtr queue,
                                                                                                   const SharedRecursiveLock &lock,
                                                                                                   ServicePushMailboxSessionPtr outer,
                                                                                                   IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                                                                   const PushSubscriptionInfo &subscriptionInfo
                                                                                                   )
      {
        RegisterQueryPtr pThis(new RegisterQuery(queue, lock, outer, delegate, subscriptionInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::RegisterQuery::needsRequest() const
      {
        AutoRecursiveLock lock(*this);

        if (mComplete) return false;
        return !((bool)mRegisterMonitor);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::RegisterQuery::monitor(RegisterPushRequestPtr requestMessage)
      {
        AutoRecursiveLock lock(*this);

        IMessageMonitorPtr monitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<RegisterPushResult>::convert(mThisWeak.lock()), requestMessage, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
        if (monitor) {
          ZS_LOG_DEBUG(log("monitoring register request") + IMessageMonitor::toDebug(monitor))
          return;
        }

        ZS_LOG_WARNING(Detail, log("failed to create monitor"))

        setError(IHTTP::HTTPStatusCode_InternalServerError, "unable to monitor message request");
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::RegisterQuery => IServicePushMailboxRegisterQuery
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::RegisterQuery::isComplete(
                                                                WORD *outErrorCode,
                                                                String *outErrorReason
                                                                ) const
      {
        AutoRecursiveLock lock(*this);

        if (outErrorCode) {
          *outErrorCode = mLastError;
        }
        if (outErrorReason) {
          *outErrorReason = mLastErrorReason;
        }

        return mComplete;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<RegisterPushResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::RegisterQuery::handleMessageMonitorResultReceived(
                                                                                        IMessageMonitorPtr monitor,
                                                                                        RegisterPushResultPtr result
                                                                                        )
      {
        ZS_LOG_DEBUG(log("register push result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mRegisterMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::RegisterQuery::handleMessageMonitorErrorResultReceived(
                                                                                             IMessageMonitorPtr monitor,
                                                                                             RegisterPushResultPtr ignore,
                                                                                             message::MessageResultPtr result
                                                                                             )
      {
        ZS_LOG_ERROR(Debug, log("register push error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mRegisterMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        setError(result->errorCode(), result->errorReason());
        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::RegisterQuery => (interal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::RegisterQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession::RegisterQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::RegisterQuery::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::RegisterQuery::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePushMailboxSession::RegisterQuery");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "complete", mComplete);
        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, mSubscriptionInfo.toDebug());

        IHelper::debugAppend(resultEl, "monitor", mRegisterMonitor ? mRegisterMonitor->getID() : 0);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::RegisterQuery::cancel()
      {
        mComplete = true;

        RegisterQueryPtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            mDelegate->onPushMailboxRegisterQueryCompleted(pThis);
          } catch(IServicePushMailboxRegisterQueryDelegateProxy::Exceptions::DelegateGone &) {
          }
        }

        ServicePushMailboxSessionPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyComplete(*this);
          mOuter.reset();
        }

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::RegisterQuery::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (mComplete) {
          ZS_LOG_WARNING(Detail, log("cannot set error after already complete") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

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
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }


  }
}
