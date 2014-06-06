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

#include <openpeer/stack/internal/stack_KeyGenerator.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/IPeerFiles.h>

#include <openpeer/services/IRSAPrivateKey.h>
#include <openpeer/services/IRSAPublicKey.h>
#include <openpeer/services/IDHKeyDomain.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark KeyGenerator
      #pragma mark

      //-----------------------------------------------------------------------
      KeyGenerator::KeyGenerator(
                                 IMessageQueuePtr queue,
                                 IKeyGeneratorDelegatePtr delegate
                                 ) :
        MessageQueueAssociator(queue),
        mRSAKeyLength(0),
        mDHKeyDomainKeyLength(0)
      {
        ZS_LOG_DEBUG(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate, queue);
      }

      //-----------------------------------------------------------------------
      void KeyGenerator::init()
      {
        if (mDependencyKeyGenerator) {
          mDependencySubscription = mDependencyKeyGenerator->subscribe(mThisWeak.lock());
        }
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      KeyGenerator::~KeyGenerator()
      {
        if(isNoop()) return;

        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr KeyGenerator::convert(IKeyGeneratorPtr monitor)
      {
        return dynamic_pointer_cast<KeyGenerator>(monitor);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark KeyGenerator => IKeyGenerator
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr KeyGenerator::toDebug(IKeyGeneratorPtr object)
      {
        if (!object) return ElementPtr();
        return KeyGenerator::convert(object)->toDebug();
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr KeyGenerator::generatePeerFiles(
                                                      IKeyGeneratorDelegatePtr delegate,
                                                      const char *password,
                                                      ElementPtr signedSaltEl,
                                                      IKeyGeneratorPtr rsaKeyGenerator
                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!password)
        ZS_THROW_INVALID_ARGUMENT_IF(!signedSaltEl)

        KeyGeneratorPtr pThis(new KeyGenerator(UseStack::queueKeyGeneration(), delegate));
        pThis->mThisWeak = pThis;
        pThis->mPassword = String(password);
        pThis->mSignedSaltEl = signedSaltEl;
        pThis->mDependencyKeyGenerator = rsaKeyGenerator;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr KeyGenerator::generateRSA(
                                                IKeyGeneratorDelegatePtr delegate,
                                                size_t keySizeInBits
                                                )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(keySizeInBits < 1)

        KeyGeneratorPtr pThis(new KeyGenerator(UseStack::queueKeyGeneration(), delegate));
        pThis->mThisWeak = pThis;
        pThis->mRSAKeyLength = keySizeInBits;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr KeyGenerator::generateDHKeyDomain(
                                                        IKeyGeneratorDelegatePtr delegate,
                                                        size_t keySizeInBits
                                                        )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(keySizeInBits < 1)

        KeyGeneratorPtr pThis(new KeyGenerator(UseStack::queueKeyGeneration(), delegate));
        pThis->mThisWeak = pThis;
        pThis->mDHKeyDomainKeyLength = keySizeInBits;
        pThis->init();

        return pThis;
      }

      //-----------------------------------------------------------------------
      IKeyGeneratorSubscriptionPtr KeyGenerator::subscribe(IKeyGeneratorDelegatePtr originalDelegate)
      {
        ZS_LOG_DETAIL(log("subscribing to key generator"))

        AutoRecursiveLock lock(mLock);
        if (!originalDelegate) return mDefaultSubscription;

        IKeyGeneratorSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate);

        IKeyGeneratorDelegatePtr delegate = mSubscriptions.delegate(subscription);

        if (delegate) {
          KeyGeneratorPtr pThis = mThisWeak.lock();

          if (mCompleted) {
            delegate->onKeyGenerated(pThis);
          }
        }

        if (mCompleted) {
          mSubscriptions.clear();
        }

        return subscription;
      }
      
      //-----------------------------------------------------------------------
      void KeyGenerator::cancel()
      {
        AutoRecursiveLock lock(mLock);

        get(mCompleted) = true;

        mSubscriptions.clear();

        if (mDefaultSubscription) {
          mDefaultSubscription->cancel();
          mDefaultSubscription.reset();
        }

        if (mDependencySubscription) {
          mDependencySubscription->cancel();
          mDependencySubscription.reset();
        }

        mSignedSaltEl.reset();
        mPeerFiles.reset();

        mRSAKeyLength = 0;
        mRSAPrivateKey.reset();
        mRSAPublicKey.reset();

        mDHKeyDomainKeyLength = 0;
        mDHKeyDomain.reset();
      }

      //-----------------------------------------------------------------------
      bool KeyGenerator::isComplete() const
      {
        AutoRecursiveLock lock(mLock);
        return mCompleted;
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr KeyGenerator::getPeerFiles() const
      {
        AutoRecursiveLock lock(mLock);
        return mPeerFiles;
      }

      //-----------------------------------------------------------------------
      void KeyGenerator::getRSAKey(
                                   IRSAPrivateKeyPtr &outRSAPrivateKey,
                                   IRSAPublicKeyPtr &outRSAPublicKey
                                   ) const
      {
        AutoRecursiveLock lock(mLock);
        outRSAPrivateKey = mRSAPrivateKey;
        outRSAPublicKey = mRSAPublicKey;
      }

      //-----------------------------------------------------------------------
      IDHKeyDomainPtr KeyGenerator::getKeyDomain() const
      {
        AutoRecursiveLock lock(mLock);
        return mDHKeyDomain;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark KeyGenerator => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void KeyGenerator::onWake()
      {
        ZS_LOG_DEBUG(debug("on wake"))

        ElementPtr signedSaltEl;
        String password;
        size_t generateRSAKeyLength = 0;
        size_t generateDHKeyDomainKeyLength = 0;

        IPeerFilesPtr peerFiles;
        IRSAPrivateKeyPtr rsaPrivateKey;
        IRSAPublicKeyPtr rsaPublicKey;

        IDHKeyDomainPtr dhKeyDomain;

        Time startTime = zsLib::now();

        // scope: figure out what to generate
        {
          AutoRecursiveLock lock(mLock);

          if (mDependencyKeyGenerator) {
            if (!mDependencyKeyGenerator->isComplete()) {
              ZS_LOG_DEBUG(log("waiting on dependency key generator to complete") + IKeyGenerator::toDebug(mDependencyKeyGenerator))
              return;
            }

            mDependencyKeyGenerator->getRSAKey(rsaPrivateKey, rsaPublicKey);
          }

          ZS_LOG_DEBUG(debug("key generator started") + ZS_PARAM("started at", startTime))

          signedSaltEl = mSignedSaltEl;
          password = mPassword;
          generateRSAKeyLength = mRSAKeyLength;
          generateDHKeyDomainKeyLength = mDHKeyDomainKeyLength;
        }

        // do generation (not within the context of a lock to not block access to key generator
        if (generateRSAKeyLength > 0) {
          rsaPrivateKey = IRSAPrivateKey::generate(rsaPublicKey);

          if (!rsaPrivateKey) {
            ZS_LOG_ERROR(Detail, log("failed to generate rsa key pair"))
          }
        }

        if (signedSaltEl) {
          if ((rsaPrivateKey) &&
              (rsaPublicKey)) {
            ZS_LOG_DEBUG(log("generating peer files with existing RSA public / private key pair"))
            peerFiles = IPeerFiles::generate(rsaPrivateKey, rsaPublicKey, IHelper::randomString((32*8)/5+1), signedSaltEl);
          } else {
            peerFiles = IPeerFiles::generate(IHelper::randomString((32*8)/5+1), signedSaltEl);
          }
          if (!peerFiles) {
            ZS_LOG_ERROR(Detail, log("failed to generate peer files"))
          }
        }

        if (generateDHKeyDomainKeyLength > 0) {
          dhKeyDomain = IDHKeyDomain::generate();

          if (!dhKeyDomain) {
            ZS_LOG_ERROR(Detail, log("failed to generate key domain"))
          }
        }

        // scope: finalize result
        {
          AutoRecursiveLock lock(mLock);
          get(mCompleted) = true;

          // NOTE: during the LONG time it takes to generate these keys the
          // caller may have decided to cancel thus we must re-assert that
          // the generation was not already cancelled since it's not possible
          // to interrupt the generation process itself.

          if (mSignedSaltEl) {
            mPeerFiles = peerFiles;
            mPassword.clear();
            mSignedSaltEl.reset();
          }

          if (mRSAKeyLength > 0) {
            mRSAPrivateKey = rsaPrivateKey;
            mRSAPublicKey = rsaPublicKey;
            mRSAKeyLength = 0;
          }

          if (mDHKeyDomainKeyLength > 0) {
            mDHKeyDomain = dhKeyDomain;
            mDHKeyDomainKeyLength = 0;
          }

          mSubscriptions.delegate()->onKeyGenerated(mThisWeak.lock());
          mSubscriptions.clear();
        }

        Duration calculationDuration = zsLib::now() - startTime;

        ZS_LOG_DEBUG(debug("key generator complete") + ZS_PARAM("duration (s)", calculationDuration))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark KeyGenerator => IKeyGeneratorDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void KeyGenerator::onKeyGenerated(IKeyGeneratorPtr generator)
      {
        ZS_LOG_DEBUG(log("on key generator") + ZS_PARAM("generator id", generator->getID()))

        AutoRecursiveLock lock(mLock);
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark KeyGenerator => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params KeyGenerator::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::KeyGenerator");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params KeyGenerator::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr KeyGenerator::toDebug() const
      {
        AutoRecursiveLock lock(mLock);
        ElementPtr resultEl = Element::create("stack::KeyGenerator");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "subscribers", mSubscriptions.size());
        IHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        IHelper::debugAppend(resultEl, "dependency subscription", (bool)mDependencySubscription);
        IHelper::debugAppend(resultEl, "dependency key generator id", mDependencyKeyGenerator ? mDependencyKeyGenerator->getID() : 0);

        IHelper::debugAppend(resultEl, "completed", mCompleted);

        IHelper::debugAppend(resultEl, "signed salt", (bool)mSignedSaltEl);
        IHelper::debugAppend(resultEl, "password", mPassword.hasData());
        IHelper::debugAppend(resultEl, "peer files", (bool)mPeerFiles);

        IHelper::debugAppend(resultEl, "rsa key length", mRSAKeyLength);
        IHelper::debugAppend(resultEl, "rsa private key", (bool)mRSAPrivateKey);
        IHelper::debugAppend(resultEl, "rsa public key", (bool)mRSAPublicKey);

        IHelper::debugAppend(resultEl, "dh key domain key length", mDHKeyDomainKeyLength);
        IHelper::debugAppend(resultEl, "dh key domain", (bool)mDHKeyDomain);

        return resultEl;
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IKeyGenerator
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IKeyGenerator::toDebug(IKeyGeneratorPtr monitor)
    {
      return internal::KeyGenerator::toDebug(monitor);
    }

    //-------------------------------------------------------------------------
    IKeyGeneratorPtr IKeyGenerator::generatePeerFiles(
                                                      IKeyGeneratorDelegatePtr delegate,
                                                      const char *password,
                                                      ElementPtr signedSaltEl,
                                                      IKeyGeneratorPtr rsaKeyGenerator
                                                      )
    {
      return internal::IKeyGeneratorFactory::singleton().generatePeerFiles(delegate, password, signedSaltEl, rsaKeyGenerator);
    }

    //-------------------------------------------------------------------------
    IKeyGeneratorPtr IKeyGenerator::generateRSA(
                                                IKeyGeneratorDelegatePtr delegate,
                                                size_t keySizeInBits
                                                )
    {
      return internal::IKeyGeneratorFactory::singleton().generateRSA(delegate, keySizeInBits);
    }

    //-------------------------------------------------------------------------
    IKeyGeneratorPtr IKeyGenerator::generateDHKeyDomain(
                                                        IKeyGeneratorDelegatePtr delegate,
                                                        size_t keySizeInBits
                                                        )
    {
      return internal::IKeyGeneratorFactory::singleton().generateDHKeyDomain(delegate, keySizeInBits);
    }

  }
}
