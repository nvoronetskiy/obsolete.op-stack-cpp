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

#include <openpeer/stack/IKeyGenerator.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

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
      #pragma mark
      #pragma mark

      class KeyGenerator : public Noop,
                           public MessageQueueAssociator,
                           public IKeyGenerator,
                           public IWakeDelegate,
                           public IKeyGeneratorDelegate
      {
      public:
        friend interaction IKeyGeneratorFactory;
        friend interaction IKeyGenerator;

      protected:
        KeyGenerator(
                     IMessageQueuePtr queue,
                     IKeyGeneratorDelegatePtr delegate
                     );
        
        KeyGenerator(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

        void init();

      public:
        ~KeyGenerator();

        static KeyGeneratorPtr convert(IKeyGeneratorPtr monitor);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark KeyGenerator => IKeyGenerator
        #pragma mark

        static ElementPtr toDebug(IKeyGeneratorPtr monitor);

        static KeyGeneratorPtr generatePeerFiles(
                                                 IKeyGeneratorDelegatePtr delegate,
                                                 const char *password,
                                                 ElementPtr signedSaltEl,
                                                 IKeyGeneratorPtr rsaKeyGenerator = IKeyGeneratorPtr()
                                                 );
        static KeyGeneratorPtr generateRSA(
                                           IKeyGeneratorDelegatePtr delegate,
                                           size_t keySizeInBits = OPENPEER_SERVICES_RSA_PRIVATE_KEY_GENERATION_SIZE)
        ;
        static KeyGeneratorPtr generateDHKeyDomain(
                                                   IKeyGeneratorDelegatePtr delegate,
                                                   size_t keySizeInBits = OPENPEER_SERVICES_DH_KEY_DOMAIN_GENERATION_SIZE
                                                   );

        virtual PUID getID() const {return mID;}

        virtual IKeyGeneratorSubscriptionPtr subscribe(IKeyGeneratorDelegatePtr delegate);

        virtual void cancel();
        virtual bool isComplete() const;

        virtual IPeerFilesPtr getPeerFiles() const;

        virtual void getRSAKey(
                               IRSAPrivateKeyPtr &outRSAPrivateKey,
                               IRSAPublicKeyPtr &outRSAPublicKey
                               ) const;

        virtual IDHKeyDomainPtr getKeyDomain() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark KeyGenerator => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark KeyGenerator => IKeyGeneratorDelegate
        #pragma mark

        virtual void onKeyGenerated(IKeyGeneratorPtr generator);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark KeyGenerator => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        AutoPUID mID;
        mutable RecursiveLock mLock;
        KeyGeneratorWeakPtr mThisWeak;

        IKeyGeneratorDelegateSubscriptions mSubscriptions;
        IKeyGeneratorSubscriptionPtr mDefaultSubscription;

        IKeyGeneratorSubscriptionPtr mDependencySubscription;
        IKeyGeneratorPtr mDependencyKeyGenerator;

        AutoBool mCompleted;

        ElementPtr mSignedSaltEl;
        String mPassword;
        IPeerFilesPtr mPeerFiles;

        size_t mRSAKeyLength;
        IRSAPrivateKeyPtr mRSAPrivateKey;
        IRSAPublicKeyPtr mRSAPublicKey;

        size_t mDHKeyDomainKeyLength;
        IDHKeyDomainPtr mDHKeyDomain;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IKeyGeneratorFactory
      #pragma mark

      interaction IKeyGeneratorFactory
      {
        static IKeyGeneratorFactory &singleton();

        virtual KeyGeneratorPtr generatePeerFiles(
                                                  IKeyGeneratorDelegatePtr delegate,
                                                  const char *password,
                                                  ElementPtr signedSaltEl,
                                                  IKeyGeneratorPtr rsaKeyGenerator = IKeyGeneratorPtr()
                                                  );
        virtual KeyGeneratorPtr generateRSA(
                                            IKeyGeneratorDelegatePtr delegate,
                                            size_t keySizeInBits
                                            );
        virtual KeyGeneratorPtr generateDHKeyDomain(
                                                    IKeyGeneratorDelegatePtr delegate,
                                                    size_t keySizeInBits
                                                    );
      };

      class KeyGeneratorFactory : public IFactory<IKeyGeneratorFactory> {};
      
    }
  }
}
