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

#ifndef OPENPEER_STACK_PUBLICATION_REPOSITORY_FETCHER
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
        #pragma mark PublicationRepository::Fetcher
        #pragma mark

        class Fetcher : public MessageQueueAssociator,
                        public SharedRecursiveLock,
                        public IPublicationFetcher,
                        public IMessageMonitorDelegate
        {
        public:
          friend class PublicationRepository;

        protected:
          Fetcher(
                  IMessageQueuePtr queue,
                  PublicationRepositoryPtr outer,
                  IPublicationFetcherDelegatePtr delegate,
                  UsePublicationMetaDataPtr metaData
                  );

          void init();

        public:
          ~Fetcher();

          static FetcherPtr convert(IPublicationFetcherPtr fetcher);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => friend PublicationRepository
          #pragma mark

          static ElementPtr toDebug(IPublicationFetcherPtr fetcher);

          static FetcherPtr create(
                                   IMessageQueuePtr queue,
                                   PublicationRepositoryPtr outer,
                                   IPublicationFetcherDelegatePtr delegate,
                                   UsePublicationMetaDataPtr metaData
                                   );

          void setPublication(UsePublicationPtr publication);
          void setMonitor(IMessageMonitorPtr monitor);
          void notifyCompleted();

          // (duplicate) virtual void cancel();
          // (duplicate) virtual IPublicationMetaDataPtr getPublicationMetaData() const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => IPublicationFetcher
          #pragma mark

          virtual void cancel();
          virtual bool isComplete() const;

          virtual bool wasSuccessful(
                                     WORD *outErrorResult = NULL,
                                     String *outReason = NULL
                                     ) const;

          virtual IPublicationPtr getFetchedPublication() const;

          virtual IPublicationMetaDataPtr getPublicationMetaData() const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          IMessageMonitorPtr getMonitor() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => (data)
          #pragma mark

          AutoPUID mID;
          PublicationRepositoryWeakPtr mOuter;

          FetcherWeakPtr mThisWeak;

          IPublicationFetcherDelegatePtr mDelegate;

          UsePublicationMetaDataPtr mPublicationMetaData;

          IMessageMonitorPtr mMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;

          UsePublicationPtr mFetchedPublication;
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //ndef OPENPEER_STACK_PUBLICATION_REPOSITORY_FETCHER
