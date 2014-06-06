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
#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/IPeerSubscription.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForPublicationRepository;
      interaction ILocationForPublicationRepository;
      interaction IPeerForPeerPublicationRepository;
      interaction IPublicationForPublicationRepository;
      interaction IPublicationMetaDataForPublicationRepository;

      ZS_DECLARE_USING_PTR(message::peer_common, PeerPublishRequest)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerGetRequest)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerDeleteRequest)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerSubscribeRequest)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerPublishNotify)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationRepositoryForAccount
      #pragma mark

      interaction IPublicationRepositoryForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationRepositoryForAccount, ForAccount)

        static ForAccountPtr create(AccountPtr account);

        virtual PUID getID() const = 0;

        virtual void cancel() = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository
      #pragma mark

      class PublicationRepository : public Noop,
                                    public MessageQueueAssociator,
                                    public SharedRecursiveLock,
                                    public IPublicationRepository,
                                    public IPublicationRepositoryForAccount,
                                    public IPeerSubscriptionDelegate,
                                    public ITimerDelegate
      {
      public:
        friend interaction IPublicationRepositoryFactory;
        friend interaction IPublicationRepository;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForPublicationRepository, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForPublicationRepository, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(IPeerForPeerPublicationRepository, UsePeer)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationMetaDataForPublicationRepository, UsePublicationMetaData)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForPublicationRepository, UsePublication)

        struct CacheCompare
        {
          bool operator()(const UsePublicationMetaDataPtr &x, const UsePublicationMetaDataPtr &y) const;
        };

      public:
        typedef IPublication::RelationshipList RelationshipList;
        typedef IPublication::RelationshipListPtr RelationshipListPtr;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;
        typedef std::map<UsePublicationMetaDataPtr, UsePublicationPtr, CacheCompare> CachedPublicationMap;
        typedef String PublicationName;
        typedef std::map<PublicationName, UsePublicationPtr> CachedPublicationPermissionMap;
        typedef UsePublicationMetaDataPtr PeerSourcePtr;
        typedef std::map<UsePublicationMetaDataPtr, UsePublicationMetaDataPtr, CacheCompare> CachedPeerPublicationMap;

        ZS_DECLARE_CLASS_PTR(PeerCache)
        ZS_DECLARE_CLASS_PTR(Publisher)
        ZS_DECLARE_CLASS_PTR(Fetcher)
        ZS_DECLARE_CLASS_PTR(Remover)
        ZS_DECLARE_CLASS_PTR(SubscriptionLocal)
        ZS_DECLARE_CLASS_PTR(PeerSubscriptionIncoming)
        ZS_DECLARE_CLASS_PTR(PeerSubscriptionOutgoing)

        friend class PeerCache;
        friend class Publisher;
        friend class Fetcher;
        friend class Remover;
        friend class SubscriptionLocal;
        friend class PeerSubscriptionIncoming;
        friend class PeerSubscriptionOutgoing;

        typedef std::map<PeerSourcePtr, PeerCachePtr, CacheCompare> CachedPeerSourceMap;

        typedef PUID SubscriptionLocationID;
        typedef std::map<SubscriptionLocationID, SubscriptionLocalPtr> SubscriptionLocalMap;

        typedef PUID PeerSubscriptionIncomingID;
        typedef std::map<PeerSubscriptionIncomingID, PeerSubscriptionIncomingPtr> PeerSubscriptionIncomingMap;

        typedef PUID PeerSubscriptionOutgoingID;
        typedef std::map<PeerSubscriptionOutgoingID, PeerSubscriptionOutgoingPtr> PeerSubscriptionOutgoingMap;

        typedef std::list<FetcherPtr> PendingFetcherList;
        typedef std::list<PublisherPtr> PendingPublisherList;

      protected:
        PublicationRepository(
                              IMessageQueuePtr queue,
                              UseAccountPtr account
                              );
        
        PublicationRepository(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~PublicationRepository();

        static PublicationRepositoryPtr convert(IPublicationRepositoryPtr repository);
        static PublicationRepositoryPtr convert(ForAccountPtr repository);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => IPublicationRepositoryForAccount
        #pragma mark

        static PublicationRepositoryPtr create(AccountPtr account);

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => IPublicationRepository
        #pragma mark

        static ElementPtr toDebug(IPublicationRepositoryPtr repository);

        static PublicationRepositoryPtr getFromAccount(IAccountPtr account);

        virtual PUID getID() const {return mID;}

        virtual IPublicationPublisherPtr publish(
                                                 IPublicationPublisherDelegatePtr delegate,
                                                 IPublicationPtr publication
                                                 );

        virtual IPublicationFetcherPtr fetch(
                                             IPublicationFetcherDelegatePtr delegate,
                                             IPublicationMetaDataPtr metaData
                                             );

        virtual IPublicationRemoverPtr remove(
                                              IPublicationRemoverDelegatePtr delegate,
                                              IPublicationPtr publication
                                              );

        virtual IPublicationSubscriptionPtr subscribe(
                                                      IPublicationSubscriptionDelegatePtr delegate,
                                                      ILocationPtr subscribeToLocation,
                                                      const char *publicationPath,
                                                      const SubscribeToRelationshipsMap &relationships
                                                      );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => IPeerSubscriptionDelegate
        #pragma mark

        virtual void onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription);

        virtual void onPeerSubscriptionFindStateChanged(
                                                        IPeerSubscriptionPtr subscription,
                                                        IPeerPtr peer,
                                                        PeerFindStates state
                                                        );

        virtual void onPeerSubscriptionLocationConnectionStateChanged(
                                                                      IPeerSubscriptionPtr subscription,
                                                                      ILocationPtr location,
                                                                      LocationConnectionStates state
                                                                      );

        virtual void onPeerSubscriptionMessageIncoming(
                                                       IPeerSubscriptionPtr subscription,
                                                       IMessageIncomingPtr message
                                                       );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend PeerCache
        #pragma mark

        CachedPeerSourceMap &getCachedPeerSources() {return mCachedPeerSources;}

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Publisher
        #pragma mark

        void notifyPublished(PublisherPtr publisher);
        void notifyPublisherCancelled(PublisherPtr publisher);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Fetcher
        #pragma mark

        void notifyFetched(FetcherPtr fetcher);
        void notifyFetcherCancelled(FetcherPtr fetcher);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Remover
        #pragma mark

        void notifyRemoved(RemoverPtr remover);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend PeerSubscriptionOutgoing
        #pragma mark

        void notifySubscribed(PeerSubscriptionOutgoingPtr subscriber);
        void notifyPeerOutgoingSubscriptionShutdown(PeerSubscriptionOutgoingPtr subscription);

        // (duplicate) IAccountForPublicationRepositoryPtr getOuter() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend SubscriptionLocal
        #pragma mark

        void notifyLocalSubscriptionShutdown(SubscriptionLocalPtr subscription);

        UseAccountPtr getAccount() {return mAccount.lock();}

        // (duplicate) IAccountForPublicationRepositoryPtr getOuter() const;

        void resolveRelationships(
                                  const PublishToRelationshipsMap &publishToRelationships,
                                  RelationshipList &outContacts
                                  ) const;

        bool canFetchPublication(
                                 const PublishToRelationshipsMap &publishToRelationships,
                                 UseLocationPtr location
                                 ) const;

        bool canSubscribeToPublisher(
                                     UseLocationPtr publicationCreatorLocation,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     UseLocationPtr subscriberLocation,
                                     const SubscribeToRelationshipsMap &subscribeToRelationships
                                     ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend PeerSubscriptionIncoming
        #pragma mark

        // (duplicate) IAccountForPublicationRepositoryPtr getOuter() const;

        // (duplicate) void resolveRelationships(
        //                                       const PublishToRelationshipsMap &publishToRelationships,
        //                                       RelationshipList &outContacts
        //                                       ) const;

      private:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

        void activateFetcher(UsePublicationMetaDataPtr metaData);
        void activatePublisher(UsePublicationPtr publication);


        PeerSubscriptionIncomingPtr findIncomingSubscription(UsePublicationMetaDataPtr metaData) const;

        void onMessageIncoming(
                               IMessageIncomingPtr messageIncoming,
                               PeerPublishRequestPtr request
                               );

        void onMessageIncoming(
                               IMessageIncomingPtr messageIncoming,
                               PeerGetRequestPtr request
                               );

        void onMessageIncoming(
                               IMessageIncomingPtr messageIncoming,
                               PeerDeleteRequestPtr request
                               );

        void onMessageIncoming(
                               IMessageIncomingPtr messageIncoming,
                               PeerSubscribeRequestPtr request
                               );

        void onMessageIncoming(
                               IMessageIncomingPtr messageIncoming,
                               PeerPublishNotifyPtr request
                               );

        void cancel();

      public:
#define OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_CACHE
#include <openpeer/stack/internal/stack_PublicationRepository_PeerCache.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_CACHE

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_PUBLISHER
#include <openpeer/stack/internal/stack_PublicationRepository_Publisher.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_PUBLISHER

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_FETCHER
#include <openpeer/stack/internal/stack_PublicationRepository_Fetcher.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_FETCHER

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_REMOVER
#include <openpeer/stack/internal/stack_PublicationRepository_Remover.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_REMOVER

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_SUBSCRIPTION_LOCAL
#include <openpeer/stack/internal/stack_PublicationRepository_SubscriptionLocal.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_SUBSCRIPTION_LOCAL

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_INCOMING
#include <openpeer/stack/internal/stack_PublicationRepository_PeerSubscriptionIncoming.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_INCOMING

#define OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_OUTGOING
#include <openpeer/stack/internal/stack_PublicationRepository_PeerSubscriptionOutgoing.h>
#undef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_OUTGOING

      private:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => (data)
        #pragma mark

        AutoPUID mID;
        PublicationRepositoryWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;

        TimerPtr mExpiresTimer;

        IPeerSubscriptionPtr mPeerSubscription;

        CachedPublicationMap mCachedLocalPublications;        // documents that have been published from a source to the local repository
        CachedPublicationMap mCachedRemotePublications;       // documents that have been fetched from a remote repository

        CachedPublicationPermissionMap mCachedPermissionDocuments;

        SubscriptionLocalMap mSubscriptionsLocal;

        PeerSubscriptionIncomingMap mPeerSubscriptionsIncoming;

        PeerSubscriptionOutgoingMap mPeerSubscriptionsOutgoing;

        PendingFetcherList mPendingFetchers;

        PendingPublisherList mPendingPublishers;

        CachedPeerSourceMap mCachedPeerSources;               // represents the document notification state of each peer subscribing to this location
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationRepositoryFactory
      #pragma mark

      interaction IPublicationRepositoryFactory
      {
        static IPublicationRepositoryFactory &singleton();

        virtual PublicationRepositoryPtr createPublicationRepository(AccountPtr account);
      };

    }
  }
}
