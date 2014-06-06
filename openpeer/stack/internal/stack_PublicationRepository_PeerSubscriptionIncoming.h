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

#ifndef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_INCOMING
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
        #pragma mark PublicationRepository::PeerSubscriptionIncoming
        #pragma mark

        class PeerSubscriptionIncoming : public MessageQueueAssociator,
                                         public SharedRecursiveLock
        {
        public:
          typedef IPublicationMetaData::SubscribeToRelationshipsMap SubscribeToRelationshipsMap;

          friend class PublicationRepository;

        protected:
          PeerSubscriptionIncoming(
                                   IMessageQueuePtr queue,
                                   PublicationRepositoryPtr outer,
                                   PeerSourcePtr peerSource,
                                   UsePublicationMetaDataPtr subscriptionInfo
                                   );

          void init();

        public:
          ~PeerSubscriptionIncoming();

          static ElementPtr toDebug(PeerSubscriptionIncomingPtr subscription);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => friend PublicationRepository
          #pragma mark

          static PeerSubscriptionIncomingPtr create(
                                                    IMessageQueuePtr queue,
                                                    PublicationRepositoryPtr outer,
                                                    PeerSourcePtr peerSource,
                                                    UsePublicationMetaDataPtr subscriptionInfo
                                                    );

          // (duplicate) PUID getID() const;

          void notifyUpdated(UsePublicationPtr publication);
          void notifyGone(UsePublicationPtr publication);

          void notifyUpdated(const CachedPublicationMap &cachedPublications);
          void notifyGone(const CachedPublicationMap &publication);

          IPublicationMetaDataPtr getSource() const;

          void cancel();

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => (data)
          #pragma mark

          AutoPUID mID;
          PublicationRepositoryWeakPtr mOuter;

          PeerSubscriptionIncomingWeakPtr mThisWeak;

          PeerSourcePtr mPeerSource;
          UsePublicationMetaDataPtr mSubscriptionInfo;
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //ndef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_INCOMING
