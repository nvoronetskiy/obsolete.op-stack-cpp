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

#ifdef OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_ASYNC_NOTIFIER

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

        class AsyncNotifier : public MessageQueueAssociator,
                              public SharedRecursiveLock,
                              public IServicePushMailboxDatabaseAbstractionNotifier
        {
        protected:
          AsyncNotifier(
                        IMessageQueuePtr queue,
                        IServicePushMailboxSessionAsyncDelegatePtr delegate,
                        const char *messageID
                        );

          void init();

        public:
          ~AsyncNotifier();

          static AsyncNotifierPtr create(
                                         IMessageQueuePtr queue,
                                         IServicePushMailboxSessionAsyncDelegatePtr delegate,
                                         const char *messageID
                                         );

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePushMailboxSession::AsyncNotifier => IServicePushMailboxDatabaseAbstractionNotifier
          #pragma mark

          virtual void notifyComplete(bool wasSuccessful);

        private:
          AsyncNotifierWeakPtr mThisWeak;

          IServicePushMailboxSessionAsyncDelegatePtr mDelegate;
          String mMessageID;
        };
        

#if 0
      }
    }
  }
}
#endif //0

#else
#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#endif //OPENPEER_STACK_SERVICE_PUSH_MAILBOX_SESSION_ASYNC_NOTIFIER
