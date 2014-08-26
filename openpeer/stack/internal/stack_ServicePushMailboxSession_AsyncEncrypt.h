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

#ifdef OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_ASYNC_ENCRYPT

#define OPENPEER_STACK_SETTING_PUSH_MAILBOX_SESSION_ENCRYPT_BLOCK_SIZE "openpeer/stack/push-mailbox-encrypt-block-size"

#if 0
namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      class ServicePushMailboxSession : public ...
      {

#endif //0

        class AsyncEncrypt : public MessageQueueAssociator,
                             public SharedRecursiveLock,
                             public IWakeDelegate
        {
        public:
          ZS_DECLARE_TYPEDEF_PTR(services::IEncryptor, UseEncryptor)

        protected:
          AsyncEncrypt(
                       IMessageQueuePtr queue,
                       IWakeDelegatePtr wakeWhenComplete,
                       UseEncryptorPtr encryptor,
                       const char *sourceFileName,
                       const char *destFileName
                       );

          void init();

        public:
          ~AsyncEncrypt();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePushMailboxSession::AsyncEncrypt => friend ServicePushMailboxSession
          #pragma mark

          static AsyncEncryptPtr create(
                                        IMessageQueuePtr queue,
                                        IWakeDelegatePtr wakeWhenCompleteDelegate,
                                        UseEncryptorPtr encryptor,
                                        const char *sourceFileName,
                                        const char *destFileName
                                        );

          bool isComplete() const;
          bool wasSuccessful() const;
          String getHash() const;
          size_t getOutputSize() const;

          void cancel();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePushMailboxSession::AsyncEncrypt => IWakeDelegate
          #pragma mark

          virtual void onWake();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePushMailboxSession::AsyncEncrypt => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

          virtual void cancel(int error);
          void write(SecureByteBlockPtr buffer);

        private:
          AsyncEncryptWeakPtr mThisWeak;

          AutoPUID mID;

          IWakeDelegatePtr mOuterWakeWhenDone;

          UseEncryptorPtr mEncryptor;

          bool mSuccessful {};

          FILE *mSourceFile;
          FILE *mDestFile;

          String mDestFileName;

          size_t mBlockSize;
          SecureByteBlockPtr mReadBuffer;

          String mFinalHex;
          size_t mOutputSize;

          void *mAsyncData;
        };

#if 0
      }
    }
  }
}
#endif //0

#else
#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#endif //OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_ASYNC_ENCRYPT
