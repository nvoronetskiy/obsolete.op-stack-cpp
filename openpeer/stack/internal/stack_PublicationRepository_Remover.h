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

#ifndef OPENPEER_STACK_PUBLICATION_REPOSITORY_REMOVER
#include <openpeer/stack/internal/stack_PublicationRepository.h>
#else

#if 0
namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      class PublicationRepository : ...
#endif //0

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::Remover
        #pragma mark

        class Remover : public MessageQueueAssociator,
                        public SharedRecursiveLock,
                        public IPublicationRemover,
                        public IMessageMonitorDelegate
        {
        public:
          friend class PublicationRepository;

        protected:
          Remover(
                  IMessageQueuePtr queue,
                  PublicationRepositoryPtr outer,
                  IPublicationRemoverDelegatePtr delegate,
                  UsePublicationPtr publication
                  );

          void init();

        public:
          ~Remover();

          static RemoverPtr convert(IPublicationRemoverPtr remover);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => friend PublicationRepository
          #pragma mark

          static ElementPtr toDebug(IPublicationRemoverPtr remover);

          static RemoverPtr create(
                                   IMessageQueuePtr queue,
                                   PublicationRepositoryPtr outer,
                                   IPublicationRemoverDelegatePtr delegate,
                                   UsePublicationPtr publication
                                   );

          void setMonitor(IMessageMonitorPtr monitor);

          void notifyCompleted();

          // (duplicate) virtual void cancel();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => IPublicationRemover
          #pragma mark

          virtual void cancel();
          virtual bool isComplete() const;

          virtual bool wasSuccessful(
                                     WORD *outErrorResult = NULL,
                                     String *outReason = NULL
                                     ) const;

          virtual IPublicationPtr getPublication() const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => (data)
          #pragma mark

          AutoPUID mID;
          PublicationRepositoryWeakPtr mOuter;

          RemoverWeakPtr mThisWeak;
          IPublicationRemoverDelegatePtr mDelegate;

          UsePublicationPtr mPublication;

          IMessageMonitorPtr mMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //ndef OPENPEER_STACK_PUBLICATION_REPOSITORY_REMOVER
