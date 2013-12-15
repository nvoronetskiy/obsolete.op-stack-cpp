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
        typedef message::peer_common::MessageFactoryPeerCommon MessageFactoryPeerCommon;
        typedef message::peer_common::PeerPublishRequest PeerPublishRequest;
        typedef message::peer_common::PeerPublishRequestPtr PeerPublishRequestPtr;
        typedef message::peer_common::PeerPublishResult PeerPublishResult;
        typedef message::peer_common::PeerPublishResultPtr PeerPublishResultPtr;
        typedef message::peer_common::PeerGetRequest PeerGetRequest;
        typedef message::peer_common::PeerGetRequestPtr PeerGetRequestPtr;
        typedef message::peer_common::PeerGetResult PeerGetResult;
        typedef message::peer_common::PeerGetResultPtr PeerGetResultPtr;
        typedef message::peer_common::PeerDeleteRequest PeerDeleteRequest;
        typedef message::peer_common::PeerDeleteRequestPtr PeerDeleteRequestPtr;
        typedef message::peer_common::PeerDeleteResult PeerDeleteResult;
        typedef message::peer_common::PeerDeleteResultPtr PeerDeleteResultPtr;
        typedef message::peer_common::PeerSubscribeRequest PeerSubscribeRequest;
        typedef message::peer_common::PeerSubscribeRequestPtr PeerSubscribeRequestPtr;
        typedef message::peer_common::PeerSubscribeResult PeerSubscribeResult;
        typedef message::peer_common::PeerSubscribeResultPtr PeerSubscribeResultPtr;
        typedef message::peer_common::PeerPublishNotifyRequest PeerPublishNotifyRequest;
        typedef message::peer_common::PeerPublishNotifyRequestPtr PeerPublishNotifyRequestPtr;
        typedef message::peer_common::PeerPublishNotifyResult PeerPublishNotifyResult;
        typedef message::peer_common::PeerPublishNotifyResultPtr PeerPublishNotifyResultPtr;

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
        
        PublicationRepository(Noop) : Noop(true), MessageQueueAssociator(IMessageQueuePtr()) {};

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

        // (duplicate) RecursiveLock &getLock() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Publisher
        #pragma mark

        void notifyPublished(PublisherPtr publisher);
        void notifyPublisherCancelled(PublisherPtr publisher);

        // (duplicate) RecursiveLock &getLock() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Fetcher
        #pragma mark

        void notifyFetched(FetcherPtr fetcher);
        void notifyFetcherCancelled(FetcherPtr fetcher);

        // (duplicate) RecursiveLock &getLock() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend Remover
        #pragma mark

        void notifyRemoved(RemoverPtr remover);

        // (duplicate) RecursiveLock &getLock() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend PeerSubscriptionOutgoing
        #pragma mark

        void notifySubscribed(PeerSubscriptionOutgoingPtr subscriber);
        void notifyPeerOutgoingSubscriptionShutdown(PeerSubscriptionOutgoingPtr subscription);

        // (duplicate) RecursiveLock &getLock() const;
        // (duplicate) IAccountForPublicationRepositoryPtr getOuter() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => friend SubscriptionLocal
        #pragma mark

        void notifyLocalSubscriptionShutdown(SubscriptionLocalPtr subscription);

        UseAccountPtr getAccount() {return mAccount.lock();}

        // (duplicate) RecursiveLock &getLock() const;
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

        // (duplicate) RecursiveLock &getLock() const;
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

        RecursiveLock &getLock() const {return mLock;}

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
                               PeerPublishNotifyRequestPtr request
                               );

        void cancel();

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::PeerCache
        #pragma mark

        class PeerCache : public IPublicationRepositoryPeerCache
        {
        public:
          friend class PublicationRepository;

        protected:
          PeerCache(
                    PeerSourcePtr peerSource,
                    PublicationRepositoryPtr repository
                    );

          void init();

          static PeerCachePtr create(
                                     PeerSourcePtr peerSource,
                                     PublicationRepositoryPtr repository
                                     );

        public:
          ~PeerCache();

          static PeerCachePtr convert(IPublicationRepositoryPeerCachePtr cache);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerCache => IPublicationRepositoryPeerCache
          #pragma mark

          static ElementPtr toDebug(IPublicationRepositoryPeerCachePtr cache);

          virtual bool getNextVersionToNotifyAboutAndMarkNotified(
                                                                  IPublicationPtr publication,
                                                                  size_t &ioMaxSizeAvailableInBytes,
                                                                  ULONG &outNotifyFromVersion,
                                                                  ULONG &outNotifyToVersion
                                                                  );

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerCache => friend PublicationRepository
          #pragma mark

          static PeerCachePtr find(
                                   PeerSourcePtr peerSource,
                                   PublicationRepositoryPtr repository
                                   );

          void notifyFetched(UsePublicationPtr publication);

          Time getExpires() const       {return mExpires;}
          void setExpires(Time expires) {mExpires = expires;}

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerCache => (internal)
          #pragma mark

          Log::Params log(const char *message) const;
          virtual ElementPtr toDebug() const;

          RecursiveLock &getLock() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerCache => (data)
          #pragma mark

          mutable RecursiveLock mBogusLock;
          PUID mID;
          PeerCacheWeakPtr mThisWeak;
          PublicationRepositoryWeakPtr mOuter;

          PeerSourcePtr mPeerSource;

          Time mExpires;

          CachedPeerPublicationMap mCachedPublications;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::Publisher
        #pragma mark

        class Publisher : public MessageQueueAssociator,
                          public IPublicationPublisher,
                          public IMessageMonitorDelegate
        {
        public:
          friend class PublicationRepository;

        protected:
          Publisher(
                    IMessageQueuePtr queue,
                    PublicationRepositoryPtr outer,
                    IPublicationPublisherDelegatePtr delegate,
                    UsePublicationPtr publication
                    );

          void init();

        public:
          ~Publisher();

          static PublisherPtr convert(IPublicationPublisherPtr);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Publisher => friend PublicationRepository
          #pragma mark

          static ElementPtr toDebug(IPublicationPublisherPtr publisher);

          static PublisherPtr create(
                                     IMessageQueuePtr queue,
                                     PublicationRepositoryPtr outer,
                                     IPublicationPublisherDelegatePtr delegate,
                                     UsePublicationPtr publication
                                     );

          // PUID getID() const;

          void setMonitor(IMessageMonitorPtr monitor);
          void notifyCompleted();

          // (duplicate) virtual IPublicationPtr getPublication() const;
          // (duplicate) virtual void cancel();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Publisher => IPublicationPublisher
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
          #pragma mark PublicationRepository::Publisher => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Publisher => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          IMessageMonitorPtr getMonitor() const;

          RecursiveLock &getLock() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Publisher => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PublicationRepositoryWeakPtr mOuter;

          PublisherWeakPtr mThisWeak;

          IPublicationPublisherDelegatePtr mDelegate;

          UsePublicationPtr mPublication;

          IMessageMonitorPtr mMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::Fetcher
        #pragma mark

        class Fetcher : public MessageQueueAssociator,
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

          RecursiveLock &getLock() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Fetcher => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
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

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::Remover
        #pragma mark

        class Remover : public MessageQueueAssociator,
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

          RecursiveLock &getLock() const;
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::Remover => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PublicationRepositoryWeakPtr mOuter;

          RemoverWeakPtr mThisWeak;
          IPublicationRemoverDelegatePtr mDelegate;

          UsePublicationPtr mPublication;

          IMessageMonitorPtr mMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::SubscriptionLocal
        #pragma mark

        class SubscriptionLocal : public MessageQueueAssociator,
                                  public IPublicationSubscription
        {
        public:
          typedef IPublicationMetaData::SubscribeToRelationshipsMap SubscribeToRelationshipsMap;

          friend class PublicationRepository;

        protected:
          SubscriptionLocal(
                            IMessageQueuePtr queue,
                            PublicationRepositoryPtr outer,
                            IPublicationSubscriptionDelegatePtr delegate,
                            const char *publicationPath,
                            const SubscribeToRelationshipsMap &relationships
                            );

          void init();

        public:
          ~SubscriptionLocal();

          static SubscriptionLocalPtr convert(IPublicationSubscriptionPtr subscription);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::SubscriptionLocal => friend SubscriptionLocal
          #pragma mark

          static ElementPtr toDebug(SubscriptionLocalPtr subscription);

          static SubscriptionLocalPtr create(
                                             IMessageQueuePtr queue,
                                             PublicationRepositoryPtr outer,
                                             IPublicationSubscriptionDelegatePtr delegate,
                                             const char *publicationPath,
                                             const SubscribeToRelationshipsMap &relationships
                                             );

          void notifyUpdated(UsePublicationPtr publication);
          void notifyGone(UsePublicationPtr publication);

          // (duplicate) virtual void cancel();

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::SubscriptionLocal => IPublicationSubscription
          #pragma mark

          virtual void cancel();

          virtual PublicationSubscriptionStates getState() const;
          virtual IPublicationMetaDataPtr getSource() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::SubscriptionLocal => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          RecursiveLock &getLock() const;
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          void setState(PublicationSubscriptionStates state);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::SubscriptionLocal => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PublicationRepositoryWeakPtr mOuter;

          SubscriptionLocalWeakPtr mThisWeak;

          IPublicationSubscriptionDelegatePtr mDelegate;

          UsePublicationMetaDataPtr mSubscriptionInfo;
          PublicationSubscriptionStates mCurrentState;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::PeerSubscriptionIncoming
        #pragma mark

        class PeerSubscriptionIncoming : public MessageQueueAssociator,
                                         public IMessageMonitorDelegate
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

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          RecursiveLock &getLock() const;
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionIncoming => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PublicationRepositoryWeakPtr mOuter;

          PeerSubscriptionIncomingWeakPtr mThisWeak;

          PeerSourcePtr mPeerSource;
          UsePublicationMetaDataPtr mSubscriptionInfo;

          typedef std::list<IMessageMonitorPtr> NotificationMonitorList;
          NotificationMonitorList mNotificationMonitors;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::PeerSubscriptionOutgoing
        #pragma mark

        class PeerSubscriptionOutgoing : public MessageQueueAssociator,
                                         public IPublicationSubscription,
                                         public IMessageMonitorDelegate
        {
        public:
          typedef IPublicationMetaData::SubscribeToRelationshipsMap SubscribeToRelationshipsMap;

          friend class PublicationRepository;

        protected:
          PeerSubscriptionOutgoing(
                                   IMessageQueuePtr queue,
                                   PublicationRepositoryPtr outer,
                                   IPublicationSubscriptionDelegatePtr delegate,
                                   UsePublicationMetaDataPtr subscriptionInfo
                                   );

          void init();

        public:
          ~PeerSubscriptionOutgoing();

          static PeerSubscriptionOutgoingPtr convert(IPublicationSubscriptionPtr subscription);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => friend PublicationRepository
          #pragma mark

          static ElementPtr toDebug(PeerSubscriptionOutgoingPtr subscription);

          static PeerSubscriptionOutgoingPtr create(
                                                    IMessageQueuePtr queue,
                                                    PublicationRepositoryPtr outer,
                                                    IPublicationSubscriptionDelegatePtr delegate,
                                                    UsePublicationMetaDataPtr subscriptionInfo
                                                    );

          // (duplicate) PUID getID() const;
          // (duplicate) virtual void cancel();

          // (duplicate) virtual IPublicationMetaDataPtr getSource() const;

          void setMonitor(IMessageMonitorPtr monitor);
          void notifyUpdated(UsePublicationMetaDataPtr metaData);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IPublicationSubscription
          #pragma mark

          // (duplicate) virtual void cancel();

          virtual PublicationSubscriptionStates getState() const;
          virtual IPublicationMetaDataPtr getSource() const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          RecursiveLock &getLock() const;
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          bool isPending() const {return PublicationSubscriptionState_Pending == mCurrentState;}
          bool isEstablished() const {return PublicationSubscriptionState_Established == mCurrentState;}
          bool isShuttingDown() const {return PublicationSubscriptionState_ShuttingDown == mCurrentState;}
          bool isShutdown() const {return PublicationSubscriptionState_Shutdown == mCurrentState;}

          void setState(PublicationSubscriptionStates state);

          void cancel();

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => (data)
          #pragma mark

          PUID mID;
          mutable RecursiveLock mBogusLock;
          PublicationRepositoryWeakPtr mOuter;

          PeerSubscriptionOutgoingWeakPtr mThisWeak;
          PeerSubscriptionOutgoingPtr mGracefulShutdownReference;

          IPublicationSubscriptionDelegatePtr mDelegate;

          UsePublicationMetaDataPtr mSubscriptionInfo;

          IMessageMonitorPtr mMonitor;
          IMessageMonitorPtr mCancelMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;

          PublicationSubscriptionStates mCurrentState;
        };

      private:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
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
