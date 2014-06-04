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

#pragma once

#include <openpeer/stack/internal/types.h>
//#include <openpeer/stack/internal/stack_ServiceNamespaceGrantSession.h>
//
//#include <openpeer/stack/message/identity-lockbox/LockboxAccessResult.h>
//#include <openpeer/stack/message/identity-lockbox/LockboxNamespaceGrantChallengeValidateResult.h>
//#include <openpeer/stack/message/identity-lockbox/LockboxIdentitiesUpdateResult.h>
//#include <openpeer/stack/message/identity-lockbox/LockboxContentGetResult.h>
//#include <openpeer/stack/message/identity-lockbox/LockboxContentSetResult.h>
//#include <openpeer/stack/message/peer/PeerServicesGetResult.h>
//
//#include <openpeer/stack/IBootstrappedNetwork.h>
//#include <openpeer/stack/IKeyGenerator.h>
//#include <openpeer/stack/IMessageMonitor.h>
//#include <openpeer/stack/IMessageSource.h>
//#include <openpeer/stack/IServiceLockbox.h>
//#include <openpeer/stack/IServiceSalt.h>
//
//#include <openpeer/services/IWakeDelegate.h>
//
//#include <zsLib/MessageQueueAssociator.h>
//#include <zsLib/ProxySubscriptions.h>
//
//#include <list>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionForInternalDelegate
      #pragma mark

      interaction IServiceLockboxSessionForInternalDelegate
      {
        virtual void onServiceLockboxSessionStateChanged(ServiceLockboxSessionPtr session) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionForInternalSubscription
      #pragma mark

      interaction IServiceLockboxSessionForInternalSubscription
      {
        virtual PUID getID() const = 0;

        virtual void cancel() = 0;

        virtual void background() = 0;
      };
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::internal::IServiceLockboxSessionForInternalDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::internal::ServiceLockboxSessionPtr, ServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_METHOD_1(onServiceLockboxSessionStateChanged, ServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::internal::IServiceLockboxSessionForInternalDelegate, openpeer::stack::internal::IServiceLockboxSessionForInternalSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::internal::ServiceLockboxSessionPtr, ServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onServiceLockboxSessionStateChanged, ServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()
