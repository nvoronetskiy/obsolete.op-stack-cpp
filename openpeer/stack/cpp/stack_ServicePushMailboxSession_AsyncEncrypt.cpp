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

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IEncryptor.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>

#include <cryptopp/sha.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using CryptoPP::SHA1;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      struct EncryptorData
      {
        SHA1 mHasher;

        static EncryptorData *create()
        {
          return new EncryptorData;
        }

        static EncryptorData &get(void *ptr)
        {
          return *((EncryptorData *)ptr);
        }

        static void destroy(void *ptr)
        {
          delete ((EncryptorData *)ptr);
        }
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::AsyncEncrypt
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::AsyncEncrypt::AsyncEncrypt(
                                                            IMessageQueuePtr queue,
                                                            IWakeDelegatePtr wakeWhenCompleteDelegate,
                                                            UseEncryptorPtr encryptor,
                                                            const char *sourceFileName,
                                                            const char *destFileName
                                                            ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mOuterWakeWhenDone(IWakeDelegateProxy::createWeak(queue, wakeWhenCompleteDelegate)),
        mSourceFile(NULL),
        mDestFile(NULL),
        mDestFileName(destFileName),
        mEncryptor(encryptor),
        mBlockSize(0),
        mOutputSize(0),
        mAsyncData(EncryptorData::create())
      {
        mSourceFile = fopen(sourceFileName, "rb");
        if (NULL == mSourceFile) {
          cancel(errno);
          return;
        }

        mDestFile = fopen(sourceFileName, "wb+");
        if (NULL == mDestFile) {
          cancel(errno);
          return;
        }

        size_t targetBlockSize = UseSettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SESSION_ENCRYPT_BLOCK_SIZE);

        mBlockSize = (mBlockSize > targetBlockSize ? mBlockSize : ((targetBlockSize / mBlockSize) * mBlockSize));

        mReadBuffer = SecureByteBlockPtr(new SecureByteBlock(mBlockSize));
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::AsyncEncrypt::init()
      {
        AutoRecursiveLock lock(*this);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::AsyncEncrypt::~AsyncEncrypt()
      {
        mThisWeak.reset();

        EncryptorData::destroy(mAsyncData);
        mAsyncData = NULL;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::AsyncEncrypt => friend ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::AsyncEncryptPtr ServicePushMailboxSession::AsyncEncrypt::create(
                                                                                                 IMessageQueuePtr queue,
                                                                                                 IWakeDelegatePtr wakeWhenCompleteDelegate,
                                                                                                 UseEncryptorPtr encryptor,
                                                                                                 const char *sourceFileName,
                                                                                                 const char *destFileName
                                                                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!queue)
        ZS_THROW_INVALID_ARGUMENT_IF(!wakeWhenCompleteDelegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!sourceFileName)
        ZS_THROW_INVALID_ARGUMENT_IF(!destFileName)
        ZS_THROW_INVALID_ARGUMENT_IF(!encryptor)

        AsyncEncryptPtr pThis(new AsyncEncrypt(queue, wakeWhenCompleteDelegate, encryptor, sourceFileName, destFileName));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::AsyncEncrypt::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !((bool)mEncryptor);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::AsyncEncrypt::wasSuccessful() const
      {
        AutoRecursiveLock lock(*this);
        return mSuccessful;
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::AsyncEncrypt::getHash() const
      {
        AutoRecursiveLock lock(*this);
        return mFinalHex;
      }

      //-----------------------------------------------------------------------
      size_t ServicePushMailboxSession::AsyncEncrypt::getOutputSize() const
      {
        AutoRecursiveLock lock(*this);
        return mOutputSize;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::AsyncEncrypt::cancel()
      {
        ZS_LOG_TRACE(log("cancel called"))

        AutoRecursiveLock lock(*this);
        cancel(0);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::AsyncEncrypt => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::AsyncEncrypt::onWake()
      {
        AutoRecursiveLock lock(*this);

        if (!mEncryptor) return;

        size_t read = fread(*mReadBuffer, sizeof(BYTE), mBlockSize, mSourceFile);

        if (0 != read) {
          EncryptorData::get(mAsyncData).mHasher.Update(*mReadBuffer, read);
          write(mEncryptor->encrypt(*mReadBuffer, read));
        }

        if (read == mBlockSize) {
          // do the next chunk of encryption in next queue event processing
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return;
        }

        if (read != mBlockSize) {
          feof(mSourceFile);

          SecureByteBlock originalDataHash(EncryptorData::get(mAsyncData).mHasher.DigestSize());
          EncryptorData::get(mAsyncData).mHasher.Final(originalDataHash);

          mFinalHex = UseServicesHelper::convertToHex(originalDataHash);

          mSuccessful = true;

          write(mEncryptor->finalize());  // note: final write might fail

          close(0);
          return;
        }

        close(ferror(mSourceFile));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::AsyncEncrypt => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::AsyncEncrypt::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession::AsyncEncrypt");
        UseServicesHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::AsyncEncrypt::cancel(int error)
      {
        if (!mEncryptor) return;

        if (!mSuccessful) {
          ZS_LOG_ERROR(Detail, log("error reported during encryption") + ZS_PARAM("error", error) + ZS_PARAM("error string", strerror(error)))
        } else {
          ZS_LOG_TRACE(log("encryption comleted successfully"))
        }

        mEncryptor.reset();

        if (mSourceFile) {
          fclose(mSourceFile);
          mSourceFile = NULL;
        }

        if (mDestFile) {
          fclose(mDestFile);
          mDestFile = NULL;
        }

        if (!mSuccessful) {
          if (mDestFileName.hasData()) {
            remove(mDestFileName);
            mDestFileName.clear();
          }
        }

        if (mOuterWakeWhenDone) {
          try {
            mOuterWakeWhenDone->onWake();
          } catch(IWakeDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
        mOuterWakeWhenDone.reset();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::AsyncEncrypt::write(SecureByteBlockPtr buffer)
      {
        if (NULL == mDestFile) return;
        if (!buffer) return;
        if (buffer->SizeInBytes() < 1) return;

        size_t written = fwrite(*buffer, sizeof(BYTE), buffer->SizeInBytes(), mDestFile);

        mOutputSize += written;

        if (written != buffer->SizeInBytes()) {
          mSuccessful = false;
          close(ferror(mDestFile));
          return;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }


  }
}
