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
//#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
//#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateRequest.h>
//#include <openpeer/stack/message/push-mailbox/PeerValidateRequest.h>
//
#include <openpeer/services/IHelper.h>
//#include <openpeer/services/ISettings.h>
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
      #pragma mark ServicePushMailboxSession::SendQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQuery::SendQuery(
                                                      IMessageQueuePtr queue,
                                                      const SharedRecursiveLock &lock,
                                                      ServicePushMailboxSessionPtr outer,
                                                      IServicePushMailboxSendQueryDelegatePtr delegate,
                                                      const String &messageID
                                                      ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(lock),
        mOuter(outer),
        mDelegate(IServicePushMailboxSendQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate))
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::init()
      {
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQuery::~SendQuery()
      {
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => friend ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQueryPtr ServicePushMailboxSession::SendQuery::create(
                                                                                           IMessageQueuePtr queue,
                                                                                           const SharedRecursiveLock &lock,
                                                                                           ServicePushMailboxSessionPtr outer,
                                                                                           IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                                           const String &messageID
                                                                                           )
      {
        SendQueryPtr pThis(new SendQuery(queue, lock, outer, delegate, messageID));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => IServicePushMailboxSendQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::PushMessagePtr ServicePushMailboxSession::SendQuery::getPushMessage()
      {
        AutoRecursiveLock lock(*this);
        return PushMessagePtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => (interal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::SendQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession::SendQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::SendQuery::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::SendQuery::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePushMailboxSession::SendQuery");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "complete", (bool)mComplete);

        IHelper::debugAppend(resultEl, "message id", mMessageID);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::cancel()
      {
        get(mComplete) = true;

        //SendQueryPtr pThis = mThisWeak.lock();
        //if ((pThis) &&
        //    (mDelegate)) {
        //}

        ServicePushMailboxSessionPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyComplete(*this);
          mOuter.reset();
        }

        mDelegate.reset();
      }

      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }


  }
}
