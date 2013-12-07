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

#include <openpeer/stack/types.h>
#include <openpeer/services/IRSAPrivateKey.h>
#include <openpeer/services/IDHKeyDomain.h>

namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IKeyGenerator
    #pragma mark

    interaction IKeyGenerator
    {
      static ElementPtr toDebug(IKeyGeneratorPtr generator);

      static IKeyGeneratorPtr generatePeerFiles(
                                                IKeyGeneratorDelegatePtr delegate,
                                                const char *password,
                                                ElementPtr signedSaltEl,
                                                IKeyGeneratorPtr rsaKeyGenerator = IKeyGeneratorPtr()
                                                );

      static IKeyGeneratorPtr generateRSA(
                                          IKeyGeneratorDelegatePtr delegate,
                                          size_t keySizeInBits = OPENPEER_SERVICES_RSA_PRIVATE_KEY_GENERATION_SIZE
                                          );
      static IKeyGeneratorPtr generateDHKeyDomain(
                                                  IKeyGeneratorDelegatePtr delegate,
                                                  size_t keySizeInBits = OPENPEER_SERVICES_DH_KEY_DOMAIN_GENERATION_SIZE
                                                  );

      virtual PUID getID() const = 0;

      virtual IKeyGeneratorSubscriptionPtr subscribe(IKeyGeneratorDelegatePtr delegate) = 0;

      virtual void cancel() = 0;

      virtual bool isComplete() const = 0;

      virtual IPeerFilesPtr getPeerFiles() const = 0;

      virtual void getRSAKey(
                             IRSAPrivateKeyPtr &outRSAPrivateKey,
                             IRSAPublicKeyPtr &outRSAPublicKey
                             ) const = 0;

      virtual IDHKeyDomainPtr getKeyDomain() const = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IKeyGeneratorDelegate
    #pragma mark

    interaction IKeyGeneratorDelegate
    {
      virtual void onKeyGenerated(IKeyGeneratorPtr generator) = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IKeyGeneratorSubscription
    #pragma mark

    interaction IKeyGeneratorSubscription
    {
      virtual PUID getID() const = 0;

      virtual void cancel() = 0;

      virtual void background() = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::IKeyGeneratorDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IKeyGeneratorPtr, IKeyGeneratorPtr)
ZS_DECLARE_PROXY_METHOD_1(onKeyGenerated, IKeyGeneratorPtr)
ZS_DECLARE_PROXY_END()

ZS_DECLARE_PROXY_SUBSCRIPTIONS_BEGIN(openpeer::stack::IKeyGeneratorDelegate, openpeer::stack::IKeyGeneratorSubscription)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_TYPEDEF(openpeer::stack::IKeyGeneratorPtr, IKeyGeneratorPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_METHOD_1(onKeyGenerated, IKeyGeneratorPtr)
ZS_DECLARE_PROXY_SUBSCRIPTIONS_END()
