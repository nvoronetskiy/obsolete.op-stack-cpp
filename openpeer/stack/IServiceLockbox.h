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

#include <openpeer/stack/types.h>


namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockbox
    #pragma mark

    interaction IServiceLockbox
    {
      static IServiceLockboxPtr createServiceLockboxFrom(IBootstrappedNetworkPtr bootstrappedNetwork);

      virtual PUID getID() const = 0;

      virtual IBootstrappedNetworkPtr getBootstrappedNetwork() const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSession
    #pragma mark

    interaction IServiceLockboxSession
    {
      enum SessionStates
      {
        SessionState_Pending,
        SessionState_PendingPeerFilesGeneration,
        SessionState_Ready,
        SessionState_Shutdown,
      };
      static const char *toString(SessionStates state);

      static ElementPtr toDebug(IServiceLockboxSessionPtr session);

      static IServiceLockboxSessionPtr login(
                                             IServiceLockboxSessionDelegatePtr delegate,
                                             IServiceLockboxPtr serviceLockbox,
                                             IServiceNamespaceGrantSessionPtr grantSession,
                                             IServiceIdentitySessionPtr identitySession,
                                             bool forceNewAccount = false
                                             );

      static IServiceLockboxSessionPtr relogin(
                                               IServiceLockboxSessionDelegatePtr delegate,
                                               IServiceLockboxPtr serviceLockbox,
                                               IServiceNamespaceGrantSessionPtr grantSession,
                                               const char *lockboxAccountID,
                                               const SecureByteBlock &lockboxKey
                                               );

      virtual PUID getID() const = 0;

      virtual IServiceLockboxPtr getService() const = 0;

      virtual SessionStates getState(
                                     WORD *lastErrorCode,
                                     String *lastErrorReason
                                     ) const = 0;

      virtual IPeerFilesPtr getPeerFiles() const = 0;

      virtual String getAccountID() const = 0;
      virtual String getDomain() const = 0;
      virtual String getStableID() const = 0;

      virtual SecureByteBlockPtr getLockboxKey() const = 0;

      virtual ServiceIdentitySessionListPtr getAssociatedIdentities() const = 0;
      virtual void associateIdentities(
                                       const ServiceIdentitySessionList &identitiesToAssociate,
                                       const ServiceIdentitySessionList &identitiesToRemove
                                       ) = 0;

      virtual void cancel() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSessionDelegate
    #pragma mark

    interaction IServiceLockboxSessionDelegate
    {
      typedef IServiceLockboxSession::SessionStates SessionStates;

      virtual void onServiceLockboxSessionStateChanged(
                                                       IServiceLockboxSessionPtr session,
                                                       SessionStates state
                                                       ) = 0;
      virtual void onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceLockboxSessionSubscription
    #pragma mark

    interaction IServiceLockboxSessionSubscription
    {
      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual void background() = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IServiceLockboxSessionDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServiceLockboxSessionPtr, IServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IServiceLockboxSessionDelegate::SessionStates, SessionStates)
ZS_DECLARE_PROXY_METHOD_2(onServiceLockboxSessionStateChanged, IServiceLockboxSessionPtr, SessionStates)
ZS_DECLARE_PROXY_METHOD_1(onServiceLockboxSessionAssociatedIdentitiesChanged, IServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::IServiceLockboxSessionDelegate, openpeer::stack::IServiceLockboxSessionSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::IServiceLockboxSessionPtr, IServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::IServiceLockboxSessionDelegate::SessionStates, SessionStates)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_2(onServiceLockboxSessionStateChanged, IServiceLockboxSessionPtr, SessionStates)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onServiceLockboxSessionAssociatedIdentitiesChanged, IServiceLockboxSessionPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()
