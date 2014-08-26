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

#include <openpeer/stack/internal/stack_PublicationRepository.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Diff.h>
#include <openpeer/stack/IMessageIncoming.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerPublishRequest.h>
#include <openpeer/stack/message/peer-common/PeerPublishResult.h>
#include <openpeer/stack/message/peer-common/PeerGetRequest.h>
#include <openpeer/stack/message/peer-common/PeerGetResult.h>
#include <openpeer/stack/message/peer-common/PeerDeleteRequest.h>
#include <openpeer/stack/message/peer-common/PeerDeleteResult.h>
#include <openpeer/stack/message/peer-common/PeerSubscribeRequest.h>
#include <openpeer/stack/message/peer-common/PeerSubscribeResult.h>
#include <openpeer/stack/message/peer-common/PeerPublishNotify.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

#define OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS (60)
#define OPENPEER_STACK_PUBLICATIONREPOSITORY_EXPIRES_TIMER_IN_SECONDS (60)

//*****************************************************************************
//*****************************************************************************
//*****************************************************************************
//*****************************************************************************
// HERE - DO NOT LEAVE THIS SET TO THE TEMP FOR PRODUCTION!
//#define OPENPEER_STACK_PUBLICATIONREPOSITORY_EXPIRE_DISCONNECTED_REMOTE_PUBLICATIONS_IN_SECONDS (60)  // temp expire in 1 minute
#define OPENPEER_STACK_PUBLICATIONREPOSITORY_EXPIRE_DISCONNECTED_REMOTE_PUBLICATIONS_IN_SECONDS (2*(60*60))  // expire in 2 hrs


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      ZS_DECLARE_USING_PTR(message::peer_common, MessageFactoryPeerCommon)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerPublishResult)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerGetResult)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerDeleteResult)
      ZS_DECLARE_USING_PTR(message::peer_common, PeerSubscribeResult)

      ZS_DECLARE_TYPEDEF_PTR(IPublicationRepositoryForAccount::ForAccount, ForAccount)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (heleprs)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationRepositoryForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ForAccountPtr IPublicationRepositoryForAccount::create(AccountPtr account)
      {
        return IPublicationRepositoryFactory::singleton().createPublicationRepository(account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::CacheCompare
      #pragma mark

      //-----------------------------------------------------------------------
      bool PublicationRepository::CacheCompare::operator()(const UsePublicationMetaDataPtr &x, const UsePublicationMetaDataPtr &y) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!x)
        ZS_THROW_INVALID_ARGUMENT_IF(!y)
        return x->isLessThan(y->toPublicationMetaData());
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PublicationRepository(
                                                   IMessageQueuePtr queue,
                                                   UseAccountPtr account
                                                   ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create()),
        mAccount(account)
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::init()
      {
        AutoRecursiveLock lock(*this);

        UseAccountPtr account = mAccount.lock();
        ZS_THROW_BAD_STATE_IF(!account)

        mPeerSubscription = IPeerSubscription::subscribeAll(Account::convert(account), mThisWeak.lock());
        mExpiresTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_EXPIRES_TIMER_IN_SECONDS));

        ZS_LOG_BASIC(log("init"))
      }

      //-----------------------------------------------------------------------
      PublicationRepository::~PublicationRepository()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr PublicationRepository::convert(IPublicationRepositoryPtr repository)
      {
        return ZS_DYNAMIC_PTR_CAST(PublicationRepository, repository);
      }

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr PublicationRepository::convert(ForAccountPtr repository)
      {
        return ZS_DYNAMIC_PTR_CAST(PublicationRepository, repository);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => IPublicationRepositoryForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr PublicationRepository::create(AccountPtr account)
      {
        PublicationRepositoryPtr pThis(new PublicationRepository(UseStack::queueStack(), account));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => IPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::toDebug(IPublicationRepositoryPtr repository)
      {
        if (!repository) return ElementPtr();
        return PublicationRepository::convert(repository)->toDebug();
      }

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr PublicationRepository::getFromAccount(IAccountPtr inAccount)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inAccount)
        UseAccountPtr account = Account::convert(inAccount);

        return account->getRepository();
      }

      //-----------------------------------------------------------------------
      IPublicationPublisherPtr PublicationRepository::publish(
                                                              IPublicationPublisherDelegatePtr delegate,
                                                              IPublicationPtr inPublication
                                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!inPublication)

        AutoRecursiveLock lock(*this);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot publish document as account object is gone"))
          return IPublicationPublisherPtr();
        }

        UsePublicationPtr publication = Publication::convert(inPublication);

        PublisherPtr publisher = Publisher::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, publication);

        UseLocationPtr publishedLocation = publication->getPublishedLocation();

        if (ILocation::LocationType_Local == publishedLocation->getLocationType()) {
          ZS_LOG_DEBUG(log("publicaton is local thus can publish immediately") + publication->toDebug())

          publication->setBaseVersion(inPublication->getVersion());

          // scope: remove old cached publication
          {
            CachedPublicationMap::iterator found = mCachedLocalPublications.find(publication);
            if (found != mCachedLocalPublications.end()) {
              ZS_LOG_DEBUG(log("previous publication found thus removing old map entry"))
              mCachedLocalPublications.erase(found);
            }
          }
          // scope: remove old permission publication
          {
            CachedPublicationPermissionMap::iterator found = mCachedPermissionDocuments.find(publication->getName());
            if (found != mCachedPermissionDocuments.end()) {
              ZS_LOG_DEBUG(log("previous permission publication found thus removing old map entry"))
              mCachedPermissionDocuments.erase(found);
            }
          }

          mCachedLocalPublications[publication] = publication;
          mCachedPermissionDocuments[publication->getName()] = publication;

          ZS_LOG_DEBUG(log("publication inserted into local cache") + ZS_PARAM("local cache total", mCachedLocalPublications.size()) + ZS_PARAM("local permissions total", mCachedPermissionDocuments.size()))

          publisher->notifyCompleted();

          ZS_LOG_DEBUG(log("notifying local subscribers of publish") + ZS_PARAM("total", mSubscriptionsLocal.size()))

          // notify subscriptions about updated publication
          for (SubscriptionLocalMap::iterator iter = mSubscriptionsLocal.begin(); iter != mSubscriptionsLocal.end(); ++iter) {
            ZS_LOG_TRACE(log("notifying local subscription of publish") + ZS_PARAM("subscriber ID", (*iter).first))
            (*iter).second->notifyUpdated(publication);
          }

          ZS_LOG_DEBUG(log("notifying incoming subscribers of publish") + ZS_PARAM("total", mPeerSubscriptionsIncoming.size()))

          for (PeerSubscriptionIncomingMap::iterator iter = mPeerSubscriptionsIncoming.begin(); iter != mPeerSubscriptionsIncoming.end(); ++iter) {
            ZS_LOG_TRACE(log("notifying peer subscription of publish") + ZS_PARAM("subscriber ID", (*iter).first))
            (*iter).second->notifyUpdated(publication);
          }

          return publisher;
        }

        ZS_LOG_DEBUG(log("publication requires publishing to an external source") + publication->toDebug())

        mPendingPublishers.push_back(publisher);
        activatePublisher(publication);

        return publisher;
      }

      //-----------------------------------------------------------------------
      IPublicationFetcherPtr PublicationRepository::fetch(
                                                          IPublicationFetcherDelegatePtr delegate,
                                                          IPublicationMetaDataPtr inMetaData
                                                          )


      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!inMetaData)

        AutoRecursiveLock lock(*this);

        UsePublicationMetaDataPtr metaData = IPublicationMetaDataForPublicationRepository::createFrom(inMetaData);

        ZS_LOG_DEBUG(log("requesting to fetch publication") + metaData->toDebug())

        FetcherPtr fetcher = Fetcher::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, metaData);

        UseLocationPtr publishedLocation = metaData->getPublishedLocation();

        if (ILocation::LocationType_Local == publishedLocation->getLocationType()) {
          CachedPublicationMap::iterator found = mCachedLocalPublications.find(metaData);
          if (found != mCachedLocalPublications.end()) {
            UsePublicationPtr existingPublication = (*found).second;
            ZS_LOG_WARNING(Detail, log("local publication was found"))

            fetcher->setPublication(existingPublication);
            fetcher->notifyCompleted();
          } else {
            ZS_LOG_DEBUG(log("local publication was not found"))

            fetcher->cancel();
          }
          return fetcher;
        }

        ZS_LOG_DEBUG(log("will push fetch request to back of fetching list"))

        mPendingFetchers.push_back(fetcher);

        activateFetcher(metaData);
        return fetcher;
      }

      //-----------------------------------------------------------------------
      IPublicationRemoverPtr PublicationRepository::remove(
                                                           IPublicationRemoverDelegatePtr delegate,
                                                           IPublicationPtr inPublication
                                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!inPublication)

        AutoRecursiveLock lock(*this);

        UsePublicationPtr publication = Publication::convert(inPublication);

        ZS_LOG_DEBUG(log("requesting to remove publication") + publication->toDebug())

        RemoverPtr remover = Remover::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, publication);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          remover->cancel();  // sorry, this could not be completed...
          return remover;
        }

        UseLocationPtr publishedLocation = publication->getPublishedLocation();

        switch (publishedLocation->getLocationType()) {
          case ILocation::LocationType_Local: {

            bool wasErased = false;

            // erase from local cache
            {
              CachedPublicationMap::iterator found = mCachedLocalPublications.find(publication);
              if (found != mCachedLocalPublications.end()) {

                ZS_LOG_DEBUG(log("found publication to erase and removing now"))

                // found the document to be erased
                mCachedLocalPublications.erase(found);
                wasErased = true;
              } else {
                ZS_LOG_WARNING(Detail, log("publication was not found in local cache"))
              }
            }

            // find in permission cache
            {
              CachedPublicationPermissionMap::iterator found = mCachedPermissionDocuments.find(publication->getName());
              if (found != mCachedPermissionDocuments.end()) {
                UsePublicationPtr &existingPublication = (*found).second;

                if (existingPublication->getLineage() == publication->getLineage()) {
                  ZS_LOG_DEBUG(log("found permission publication to erase and removing now"))
                  mCachedPermissionDocuments.erase(found);
                  wasErased = true;
                } else {
                  ZS_LOG_DEBUG(log("found permission publication but it doesn't have the same lineage thus will not erase") + existingPublication->toDebug())
                }
              } else {
                ZS_LOG_DEBUG(log("did not find permisison document to remove"))
              }
            }

            //*****************************************************************
            //*****************************************************************
            //*****************************************************************
            //*****************************************************************
            // HERE - notify subscribers the document is gone?

            if (wasErased) {
              remover->notifyCompleted();
            }
            remover->cancel();
          }
          case ILocation::LocationType_Finder:
          case ILocation::LocationType_Peer:
          {
            PeerDeleteRequestPtr request = PeerDeleteRequest::create();
            request->publicationMetaData(publication->toPublicationMetaData());
            request->domain(account->getDomain());

            ZS_LOG_DEBUG(log("requesting to remove remote finder publication"))

            remover->setMonitor(IMessageMonitor::monitorAndSendToLocation(remover, Location::convert(publishedLocation), request, Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS)));
            break;
          }
        }

        return remover;
      }

      //-----------------------------------------------------------------------
      IPublicationSubscriptionPtr PublicationRepository::subscribe(
                                                                   IPublicationSubscriptionDelegatePtr delegate,
                                                                   ILocationPtr inSubscribeToLocation,
                                                                   const char *publicationPath,
                                                                   const SubscribeToRelationshipsMap &relationships
                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!inSubscribeToLocation)
        ZS_THROW_INVALID_ARGUMENT_IF(!publicationPath)

        AutoRecursiveLock lock(*this);

        UseLocationPtr subscribeToLocation = Location::convert(inSubscribeToLocation);

        ZS_LOG_DEBUG(log("creating subcription") + ZS_PARAM("publication path", publicationPath) + subscribeToLocation->toDebug())

        UseAccountPtr account = mAccount.lock();

        switch (subscribeToLocation->getLocationType()) {
          case ILocation::LocationType_Local: {
            SubscriptionLocalPtr subscriber = SubscriptionLocal::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, publicationPath, relationships);
            if (!account) {
              ZS_LOG_WARNING(Detail, log("subscription must be cancelled as account is gone"))
              subscriber->cancel();  // sorry, this could not be completed...
              return subscriber;
            }
            for (CachedPublicationMap::iterator iter = mCachedLocalPublications.begin(); iter != mCachedLocalPublications.end(); ++iter)
            {
              UsePublicationPtr publication = (*iter).second;
              ZS_LOG_TRACE(log("notifying location subcription about document") + publication->toDebug())
              subscriber->notifyUpdated(publication);
            }
            return subscriber;
          }
          case ILocation::LocationType_Finder:
          case ILocation::LocationType_Peer:  {
            if (!account) {
              SubscriptionLocalPtr subscriber = SubscriptionLocal::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, publicationPath, relationships);
              ZS_LOG_WARNING(Detail, log("subscription must be cancelled as account is gone"))
              return subscriber;
            }

            UseLocationPtr localLocation = UseLocation::getForLocal(Account::convert(account));

            UsePublicationMetaDataPtr metaData = IPublicationMetaDataForPublicationRepository::create(0, 0, 0, Location::convert(localLocation), publicationPath, "", IPublicationMetaData::Encoding_JSON, relationships, Location::convert(subscribeToLocation));

            PeerSubscriptionOutgoingPtr subscriber = PeerSubscriptionOutgoing::create(getAssociatedMessageQueue(), mThisWeak.lock(), delegate, metaData);

            PeerSubscribeRequestPtr request = PeerSubscribeRequest::create();
            request->domain(account->getDomain());

            request->publicationMetaData(metaData->toPublicationMetaData());

            subscriber->setMonitor(IMessageMonitor::monitorAndSendToLocation(subscriber, Location::convert(subscribeToLocation), request, Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS)));

            mPeerSubscriptionsOutgoing[subscriber->getID()] = subscriber;
            ZS_LOG_TRACE(log("outgoing subscription is created"))
            return subscriber;
          }
        }

        ZS_LOG_WARNING(Detail, log("subscribing to unknown location type") + subscribeToLocation->toDebug())
        return IPublicationSubscriptionPtr();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(*this);

        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("ignoring connection subscription shutdown on obsolete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("connection subscription shutdown"))
        mPeerSubscription.reset();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onPeerSubscriptionFindStateChanged(
                                                                     IPeerSubscriptionPtr subscription,
                                                                     IPeerPtr peer,
                                                                     PeerFindStates state
                                                                     )
      {
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                   IPeerSubscriptionPtr subscription,
                                                                                   ILocationPtr inLocation,
                                                                                   LocationConnectionStates state
                                                                                   )
      {
        AutoRecursiveLock lock(*this);
        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("ignoring peer subscription location connection state change on obsolete subscription"))
          return;
        }

        Time recommendedExpires = zsLib::now() + Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_EXPIRE_DISCONNECTED_REMOTE_PUBLICATIONS_IN_SECONDS);

        UseLocationPtr location = Location::convert(inLocation);

        ZS_LOG_DEBUG(log("peer location state changed") + ZS_PARAM("state", ILocation::toString(state)) + location->toDebug())
        switch (state)
        {
          case ILocation::LocationConnectionState_Pending:        break;
          case ILocation::LocationConnectionState_Connected:      {

            // remove every remote document expiry for the peer...
            for (CachedPublicationMap::iterator iter = mCachedRemotePublications.begin(); iter != mCachedRemotePublications.end(); ++iter)
            {
              UsePublicationPtr &publication = (*iter).second;

              const char *ignoredReaon = NULL;
              if (0 == UseLocation::locationCompare(Location::convert(location), publication->getPublishedLocation(), ignoredReaon)) {
                ZS_LOG_TRACE(log("removing expiry time on document") + ZS_PARAM("recommend", recommendedExpires) + publication->toDebug())
                publication->setExpires(Time());
              }
            }

            // remove the peer source expiry
            {
              PeerSourcePtr peerSource = IPublicationMetaDataForPublicationRepository::createForSource(Location::convert(location));
              CachedPeerSourceMap::iterator found = mCachedPeerSources.find(peerSource);
              if (found != mCachedPeerSources.end()) {
                PeerCachePtr &peerCache = (*found).second;

                ZS_LOG_DEBUG(log("removing the peer source cache expiry") + ZS_PARAM("recommended", recommendedExpires))
                peerCache->setExpires(Time());
              }
            }
            break;
          }
          case ILocation::LocationConnectionState_Disconnecting:
          case ILocation::LocationConnectionState_Disconnected:   {

            // everything published from a remote peer into the local cache must be removed
            for (CachedPublicationMap::iterator pubIter = mCachedLocalPublications.begin(); pubIter != mCachedLocalPublications.end(); )
            {
              CachedPublicationMap::iterator current = pubIter;
              ++pubIter;

              UsePublicationPtr &publication = (*current).second;

              const char *ignoredReaon = NULL;

              if (0 == ILocationForPublicationRepository::locationCompare(publication->getCreatorLocation(), Location::convert(location), ignoredReaon)) {
                ZS_LOG_DEBUG(log("removing publication published by peer") + publication->toDebug())

                // remove this document from the cache
                mCachedLocalPublications.erase(current);
              }
            }

            // set every remote document downloaded from peer to expire after a period of time for the peer...
            for (CachedPublicationMap::iterator iter = mCachedRemotePublications.begin(); iter != mCachedRemotePublications.end(); ++iter)
            {
              UsePublicationPtr &publication = (*iter).second;

              const char *ignoredReaon = NULL;
              if (0 == ILocationForPublicationRepository::locationCompare(publication->getPublishedLocation(), Location::convert(location), ignoredReaon)) {
                ZS_LOG_TRACE(log("setting expiry time on document") + ZS_PARAM("recommend", recommendedExpires) + publication->toDebug())
                publication->setExpires(recommendedExpires);
              }
            }

            // set the peer source expiry (peer source represents what this side believes the remote side has downloaded from this peer and cached already)
            {
              PeerSourcePtr peerSource = IPublicationMetaDataForPublicationRepository::createForSource(Location::convert(location));
              CachedPeerSourceMap::iterator found = mCachedPeerSources.find(peerSource);
              if (found != mCachedPeerSources.end()) {

                if (ILocation::LocationType_Finder == location->getLocationType()) {
                  // the finder would immediately forget all downloaded publications from this local location upon disconnect so remove the entire cache representation...
                  mCachedPeerSources.erase(found);
                } else {
                  PeerCachePtr &peerCache = (*found).second;

                  ZS_LOG_DEBUG(log("setting the peer source cache to expire at the recommended time") + ZS_PARAM("recommended", recommendedExpires))
                  peerCache->setExpires(recommendedExpires);
                }
              }
            }

            // clean all outgoing subscriptions going to the peer...
            for (PeerSubscriptionOutgoingMap::iterator subIter = mPeerSubscriptionsOutgoing.begin(); subIter != mPeerSubscriptionsOutgoing.end(); )
            {
              PeerSubscriptionOutgoingMap::iterator current = subIter;
              ++subIter;

              PeerSubscriptionOutgoingPtr &outgoing = (*current).second;
              IPublicationMetaDataPtr source = outgoing->getSource();

              const char *ignoredReaon = NULL;
              if (0 == ILocationForPublicationRepository::locationCompare(source->getPublishedLocation(), Location::convert(location), ignoredReaon)) {
                // cancel this subscription since its no longer valid
                ZS_LOG_DEBUG(log("shutting down outgoing peer subscription") + ZS_PARAM("id", outgoing->getID()))

                outgoing->cancel();
                mPeerSubscriptionsOutgoing.erase(current);
              }
            }

            // clean all incoming subscriptions coming from the peer...
            for (PeerSubscriptionIncomingMap::iterator subIter = mPeerSubscriptionsIncoming.begin(); subIter != mPeerSubscriptionsIncoming.end(); )
            {
              PeerSubscriptionIncomingMap::iterator current = subIter;
              ++subIter;

              PeerSubscriptionIncomingPtr &incoming = (*current).second;
              IPublicationMetaDataPtr source = incoming->getSource();
              const char *ignoredReaon = NULL;
              if (0 == ILocationForPublicationRepository::locationCompare(source->getCreatorLocation(), Location::convert(location), ignoredReaon)) {
                // cancel this subscription since its no longer valid
                ZS_LOG_DEBUG(log("shutting down incoming subscriptions coming from the peer") + ZS_PARAM("id", incoming->getID()))
                incoming->cancel();
                mPeerSubscriptionsIncoming.erase(current);
              }
            }
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onPeerSubscriptionMessageIncoming(
                                                                    IPeerSubscriptionPtr subscription,
                                                                    IMessageIncomingPtr messageIncoming
                                                                    )
      {
        UseLocationPtr location = Location::convert(messageIncoming->getLocation());
        message::MessagePtr message = messageIncoming->getMessage();
        ZS_LOG_TRACE(log("received notification of incoming message") + ZS_PARAM("message ID", message->messageID()) + ZS_PARAM("type", message::Message::toString(message->messageType())) + ZS_PARAM("method", message->methodAsString()) + location->toDebug())

        if (message->factory() != MessageFactoryPeerCommon::singleton()) {
          ZS_LOG_DEBUG(log("reposity does not handle messages from non \"peer-common\" factories"))
          return;
        }

        AutoRecursiveLock lock(*this);
        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("ignoring peer subscription incoming message on obsolete subscription"))
          return;
        }

        switch (message->messageType()) {
          case message::Message::MessageType_Request:
          case message::Message::MessageType_Notify:  break;
          default:                                    {
            ZS_LOG_WARNING(Detail, log("ignoring message that is neither a request nor a notification"))
            return;
          }
        }

        switch ((MessageFactoryPeerCommon::Methods)message->method()) {
          case MessageFactoryPeerCommon::Method_PeerPublish:        onMessageIncoming(messageIncoming, PeerPublishRequest::convert(message)); break;
          case MessageFactoryPeerCommon::Method_PeerGet:            onMessageIncoming(messageIncoming, PeerGetRequest::convert(message)); break;
          case MessageFactoryPeerCommon::Method_PeerDelete:         onMessageIncoming(messageIncoming, PeerDeleteRequest::convert(message)); break;
          case MessageFactoryPeerCommon::Method_PeerSubscribe:      onMessageIncoming(messageIncoming, PeerSubscribeRequest::convert(message)); break;
          case MessageFactoryPeerCommon::Method_PeerPublishNotify:  onMessageIncoming(messageIncoming, PeerPublishNotify::convert(message)); break;
          default:                                          {
            ZS_LOG_TRACE(log("method was not understood (thus ignoring)"))
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::onTimer(TimerPtr timer)
      {
        ZS_LOG_TRACE(log("tick"))

        AutoRecursiveLock lock(*this);
        if (timer != mExpiresTimer) {
          ZS_LOG_WARNING(Detail, log("received notification of an obsolete timer"))
          return;
        }

        Time tick = zsLib::now();

        // go through the remote cache and expire the documents now...
        for (CachedPublicationMap::iterator cacheIter = mCachedRemotePublications.begin(); cacheIter != mCachedRemotePublications.end(); )
        {
          CachedPublicationMap::iterator current = cacheIter;
          ++cacheIter;

          UsePublicationPtr &publication = (*current).second;

          Time expires = publication->getExpires();
          Time cacheExpires = publication->getCacheExpires();

          if (Time() == expires) {
            expires = cacheExpires;
          }
          if (Time() == cacheExpires) {
            cacheExpires = expires;
          }

          if (Time() == expires) {
            ZS_LOG_TRACE(log("publication does not have an expiry") + publication->toDebug())
            continue;
          }

          if (cacheExpires < expires)
            expires = cacheExpires;

          if (expires < tick) {
            ZS_LOG_DEBUG(log("document is now expiring") + publication->toDebug())
            mCachedRemotePublications.erase(current);
            continue;
          }

          ZS_LOG_TRACE(log("publication is not expirying yet") + publication->toDebug())
        }

        // go through the peer sources and see if any should expire...
        for (CachedPeerSourceMap::iterator cacheIter = mCachedPeerSources.begin(); cacheIter != mCachedPeerSources.end(); )
        {
          CachedPeerSourceMap::iterator current = cacheIter;
          ++cacheIter;

          const PeerSourcePtr &peerSource = (*current).first;
          PeerCachePtr peerCache = (*current).second;

          Time expires = peerCache->getExpires();
          if (Time() == expires) {
            ZS_LOG_TRACE(log("peer source does not have an expiry") + peerSource->toDebug())
            continue;
          }

          if (expires < tick) {
            ZS_LOG_DEBUG(log("peer source is now expiring") + peerSource->toDebug())
            mCachedPeerSources.erase(current);
            continue;
          }

          ZS_LOG_TRACE(log("peer source is not expirying yet") + peerSource->toDebug())
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => friend Publisher
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyPublished(PublisherPtr publisher)
      {
        AutoRecursiveLock lock(*this);
        // nothing special to do (although may decide to cache the document published in the future to prevent re-fetching)
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyPublisherCancelled(PublisherPtr publisher)
      {
        AutoRecursiveLock lock(*this);

        UsePublicationPtr publication;

        // find and remove the fetcher from the pending list...
        for (PendingPublisherList::iterator iter = mPendingPublishers.begin(); iter != mPendingPublishers.end(); ++iter)
        {
          PublisherPtr &foundPublisher = (*iter);
          if (foundPublisher->getID() == publisher->getID()) {
            publication = Publication::convert(foundPublisher->getPublication());
            ZS_LOG_DEBUG(log("removing remote publisher") + ZS_PARAM("publisher ID", publisher->getID()) + publication->toDebug())
            mPendingPublishers.erase(iter);
            break;
          }
        }

        if (publication) {
          // ensure that only one fectcher for the same publication is activated at a time...
          activatePublisher(publication);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => friend Fetcher
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyFetched(FetcherPtr fetcher)
      {
        AutoRecursiveLock lock(*this);

        UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(fetcher->getPublicationMetaData());

        ZS_LOG_DEBUG(log("publication was fetched") + ZS_PARAM("fetcher ID", fetcher->getID()) + metaData->toDebug())

        UsePublicationPtr publication = Publication::convert(fetcher->getFetchedPublication());

        // force the creator/published to to be the correct locations
        publication->setCreatorLocation(metaData->getCreatorLocation());
        publication->setPublishedLocation(metaData->getPublishedLocation());

        CachedPublicationMap::iterator found = mCachedRemotePublications.find(metaData);
        if (found != mCachedRemotePublications.end()) {
          UsePublicationPtr &existingPublication = (*found).second;

          ZS_LOG_DEBUG(log("existing internal publication found thus updating") + existingPublication->toDebug())
          try {
            existingPublication->updateFromFetchedPublication(Publication::convert(publication));

            // override what the fetcher thinks is returned and replace with the existing document
            fetcher->setPublication(existingPublication);
          } catch(IPublicationForPublicationRepository::Exceptions::VersionMismatch &) {
            ZS_LOG_ERROR(Detail, log("version fetched is not compatible with the version already known"))
          }
        } else {
          ZS_LOG_DEBUG(log("new entry for cache will be created since existing publication in cache was not found") + publication->toDebug())
          mCachedRemotePublications[publication] = publication;

          ZS_LOG_DEBUG(log("publication inserted into remote cache") + ZS_PARAM("remote cache total", mCachedRemotePublications.size()))
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyFetcherCancelled(FetcherPtr fetcher)
      {
        AutoRecursiveLock lock(*this);

        UsePublicationMetaDataPtr metaData;

        // find and remove the fetcher from the pending list...
        for (PendingFetcherList::iterator iter = mPendingFetchers.begin(); iter != mPendingFetchers.end(); ++iter)
        {
          FetcherPtr &foundFetcher = (*iter);
          if (foundFetcher->getID() == fetcher->getID()) {
            ZS_LOG_DEBUG(log("fetcher is being removed from pending fetchers list") + ZS_PARAM("fetcher ID", fetcher->getID()))
            metaData = PublicationMetaData::convert(fetcher->getPublicationMetaData());
            mPendingFetchers.erase(iter);
            break;
          }
        }

        if (metaData) {
          ZS_LOG_DEBUG(log("will attempt to activate next fetcher based on previous fetch's publication meta data") + metaData->toDebug())
          // ensure that only one fectcher for the same publication is activated at a time...
          activateFetcher(metaData);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => friend Remover
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyRemoved(RemoverPtr remover)
      {
        UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(remover->getPublication());
        CachedPublicationMap::iterator found = mCachedRemotePublications.find(metaData);
        if (found == mCachedRemotePublications.end()) {
          ZS_LOG_DEBUG(log("unable to locate publication in 'remote' cache") + metaData->toDebug())
          return;
        }

        UsePublicationPtr &existingPublication = (*found).second;

        ZS_LOG_DEBUG(log("removing remotely cached publication") + existingPublication->toDebug())
        mCachedRemotePublications.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => friend PeerSubscriptionOutgoing
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::notifySubscribed(PeerSubscriptionOutgoingPtr subscriber)
      {
        ZS_LOG_DEBUG(log("notify outoing subscription is subscribed") + ZS_PARAM("subscriber ID", subscriber->getID()))
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyPeerOutgoingSubscriptionShutdown(PeerSubscriptionOutgoingPtr subscription)
      {
        PeerSubscriptionOutgoingMap::iterator found = mPeerSubscriptionsOutgoing.find(subscription->getID());
        if (found == mPeerSubscriptionsOutgoing.end()) {
          ZS_LOG_WARNING(Detail, log("outoing subscription was shutdown but it was not found in local subscription map") + ZS_PARAM("subscription ID", subscription->getID()))
          return;
        }

        ZS_LOG_DEBUG(log("outgoing subscription was shutdown") + ZS_PARAM("subscription id", subscription->getID()))
        mPeerSubscriptionsOutgoing.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => friend SubscriptionLocal
      #pragma mark

      //-----------------------------------------------------------------------
      void PublicationRepository::notifyLocalSubscriptionShutdown(SubscriptionLocalPtr subscription)
      {
        SubscriptionLocalMap::iterator found = mSubscriptionsLocal.find(subscription->getID());
        if (found == mSubscriptionsLocal.end()) {
          ZS_LOG_WARNING(Detail, log("local subscription was shutdown but it was not found in local subscription map") + ZS_PARAM("subscription ID", subscription->getID()))
          return;
        }

        ZS_LOG_DEBUG(log("local subscription was shutdown") + ZS_PARAM("subscription id", subscription->getID()))
        mSubscriptionsLocal.erase(found);
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::resolveRelationships(
                                                       const PublishToRelationshipsMap &publishToRelationships,
                                                       RelationshipList &outContacts
                                                       ) const
      {
        typedef String PeerURI;
        typedef std::map<PeerURI, PeerURI> PeerURIMap;

        typedef IPublicationMetaData::DocumentName DocumentName;
        typedef IPublicationMetaData::PermissionAndPeerURIListPair PermissionAndPeerURIListPair;
        typedef IPublicationMetaData::PeerURIList PeerURIList;

        PeerURIMap contacts;

        for (PublishToRelationshipsMap::const_iterator iter = publishToRelationships.begin(); iter != publishToRelationships.end(); ++iter)
        {
          DocumentName name = (*iter).first;

          ZS_LOG_TRACE(log("resolving relationships for document") + ZS_PARAM("name", name))

          const PermissionAndPeerURIListPair &permissionPair = (*iter).second;
          const PeerURIList &diffContacts = permissionPair.second;

          UsePublicationPtr relationshipsPublication;

          // scope: find the permission document
          {
            CachedPublicationPermissionMap::const_iterator found = mCachedPermissionDocuments.find(name);
            if (found != mCachedPermissionDocuments.end()) {
              relationshipsPublication = (*found).second;
            }
          }

          if (!relationshipsPublication) {
            ZS_LOG_WARNING(Detail, log("failed to find relationships document for resolving") + ZS_PARAM("name", name))
            continue;
          }

          RelationshipListPtr docContacts = relationshipsPublication->getAsContactList();

          switch (permissionPair.first) {
            case IPublicationMetaData::Permission_All:    {
              for (RelationshipList::iterator relIter = (*docContacts).begin(); relIter != (*docContacts).end(); ++relIter) {
                ZS_LOG_TRACE(log("adding all contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*relIter)))
                contacts[(*relIter)] = (*relIter);
              }
              break;
            }
            case IPublicationMetaData::Permission_None:   {
              for (RelationshipList::iterator relIter = (*docContacts).begin(); relIter != (*docContacts).end(); ++relIter) {
                PeerURIMap::iterator found = contacts.find(*relIter);
                if (found == contacts.end()) {
                  ZS_LOG_TRACE(log("failed to remove all contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*relIter)))
                  continue;
                }
                ZS_LOG_TRACE(log("removing all contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*relIter)))
                contacts.erase(found);
              }
              break;
            }
            case IPublicationMetaData::Permission_Add:
            case IPublicationMetaData::Permission_Some:   {
              for (RelationshipList::const_iterator diffIter = diffContacts.begin(); diffIter != diffContacts.end(); ++diffIter) {
                PeerURIList::const_iterator found = find((*docContacts).begin(), (*docContacts).end(), (*diffIter));
                if (found == (*docContacts).end()) {
                  ZS_LOG_TRACE(log("cannot add some of the contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*diffIter)))
                  continue; // cannot add anyone that isn't part of the relationship list
                }
                ZS_LOG_TRACE(log("adding some of the contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*diffIter)))
                contacts[(*diffIter)] = (*diffIter);
              }
              break;
            }
            case IPublicationMetaData::Permission_Remove: {
              for (RelationshipList::const_iterator diffIter = diffContacts.begin(); diffIter != diffContacts.end(); ++diffIter) {
                PeerURIList::const_iterator found = find((*docContacts).begin(), (*docContacts).end(), (*diffIter));
                if (found == (*docContacts).end()) {
                  ZS_LOG_TRACE(log("cannot remove some of the contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*diffIter)))
                  continue; // cannot remove anyone that isn't part of the relationship list
                }

                PeerURIMap::iterator foundExisting = contacts.find(*diffIter);
                if (foundExisting == contacts.end()) {
                  ZS_LOG_TRACE(log("cannot removing some of the contacts found as the contact was never added to relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*diffIter)))
                  continue;
                }
                ZS_LOG_TRACE(log("removing some of the contacts found in relationships document") + ZS_PARAM("name", name) + ZS_PARAM("peer URI", (*diffIter)))
                contacts.erase(foundExisting);
              }
              break;
            }
          }
        }

        for (PeerURIMap::const_iterator iter = contacts.begin(); iter != contacts.end(); ++iter) {
          ZS_LOG_TRACE(log("final list of the resolved relationships contains this contact") + ZS_PARAM("peer URI", (*iter).first))
          outContacts.push_back((*iter).first);
        }
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::canFetchPublication(
                                                      const PublishToRelationshipsMap &publishToRelationships,
                                                      UseLocationPtr location
                                                      ) const
      {
        switch (location->getLocationType()) {
          case ILocation::LocationType_Local:   return true;    // local is always allowed to fetch the publication
          case ILocation::LocationType_Finder:  return false;   // finder cannot fetch publications
          case ILocation::LocationType_Peer:    break;
        }

        UsePeerPtr peer = location->getPeer();
        if (!peer) {
          ZS_LOG_ERROR(Detail, log("peer contact on incoming message was empty") + location->toDebug())
          return false;
        }

        String peerURI = peer->getPeerURI();

        RelationshipList publishToContacts;  // all these contacts are being published to
        resolveRelationships(publishToRelationships, publishToContacts);

        bool found = false;
        // the document must publish to this contact or its ignored...
        for (RelationshipList::iterator iter = publishToContacts.begin(); iter != publishToContacts.end(); ++iter)
        {
          if ((*iter) == peerURI) {
            found = true;
            break;
          }
        }

        if (!found) {
          ZS_LOG_WARNING(Detail, log("publication is not published to this fetcher contact") + ZS_PARAM("fetcher peer URI", peerURI))
          return false; // does not publish to this contact...
        }

        ZS_LOG_TRACE(log("publication is publishing to this fetcher contact (thus safe to fetch)") + ZS_PARAM("fetcher peer URI", peerURI))
        return true;
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::canSubscribeToPublisher(
                                                          UseLocationPtr publicationCreatorLocation,
                                                          const PublishToRelationshipsMap &publishToRelationships,
                                                          UseLocationPtr subscriberLocation,
                                                          const SubscribeToRelationshipsMap &subscribeToRelationships
                                                          ) const
      {
        RelationshipList publishToContacts;  // all these contacts are being published to
        resolveRelationships(publishToRelationships, publishToContacts);

        UsePeerPtr publicationPeer = publicationCreatorLocation->getPeer();
        UsePeerPtr subscriberPeer = subscriberLocation->getPeer();

        if ((!publicationPeer) ||
            (!subscriberPeer)) {
          ZS_LOG_TRACE(log("publisher is not publishing to this subscriber contact") + ZS_PARAM("publication", publicationCreatorLocation->toDebug()) + ZS_PARAM("subscriber", subscriberLocation->toDebug()))
          return false;
        }

        bool found = false;
        // the document must publish to this contact or its ignored...
        for (RelationshipList::iterator iter = publishToContacts.begin(); iter != publishToContacts.end(); ++iter)
        {
          if ((*iter) == subscriberPeer->getPeerURI()) {
            found = true;
            break;
          }
        }

        if (!found) {
          ZS_LOG_TRACE(log("publisher is not publishing to this subscriber contact") + ZS_PARAM("publication", publicationCreatorLocation->toDebug()) + ZS_PARAM("subscriber", subscriberLocation->toDebug()))
          return false; // does not publish to this contact...
        }

        RelationshipList subscribeToContacts; // all these cotnacts are being subscribed to
        resolveRelationships(subscribeToRelationships, subscribeToContacts);

        found = false;
        // the document must publish to this contact or its ignored...
        for (RelationshipList::iterator iter = subscribeToContacts.begin(); iter != subscribeToContacts.end(); ++iter)
        {
          if ((*iter) == publicationPeer->getPeerURI()) {
            found = true;
            break;
          }
        }

        if (!found) {
          ZS_LOG_TRACE(log("subscriber is not subscribing to this publisher") + ZS_PARAM("publication", publicationCreatorLocation->toDebug()) + ZS_PARAM("subscriber", subscriberLocation->toDebug()))
          return false; // does not publish to this contact...
        }

        ZS_LOG_TRACE(log("subscriber is subscribing to this publication's creator and creator is publishing this publication to the subscriber") + ZS_PARAM("publication", publicationCreatorLocation->toDebug()) + ZS_PARAM("subscriber", subscriberLocation->toDebug()))
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "cached local", mCachedLocalPublications.size());
        IHelper::debugAppend(resultEl, "cached remote", mCachedRemotePublications.size());
        IHelper::debugAppend(resultEl, IPeerSubscription::toDebug(mPeerSubscription));
        IHelper::debugAppend(resultEl, "cached permissions", mCachedPermissionDocuments.size());
        IHelper::debugAppend(resultEl, "subscriptions local", mSubscriptionsLocal.size());
        IHelper::debugAppend(resultEl, "subscriptions incoming", mPeerSubscriptionsIncoming.size());
        IHelper::debugAppend(resultEl, "subscriptions outgoing", mPeerSubscriptionsOutgoing.size());
        IHelper::debugAppend(resultEl, "pending fetchers", mPendingFetchers.size());
        IHelper::debugAppend(resultEl, "pending publishers", mPendingPublishers.size());
        IHelper::debugAppend(resultEl, "cached peer sources", mCachedPeerSources.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::activateFetcher(UsePublicationMetaDataPtr metaData)
      {
        AutoRecursiveLock lock(*this);

        if (mPendingFetchers.size() < 1) {
          ZS_LOG_DEBUG(log("no pending fetchers to activate..."))
          return;
        }

        ZS_LOG_DEBUG(log("activating next fetcher found with this meta data") + metaData->toDebug())

        for (PendingFetcherList::iterator iter = mPendingFetchers.begin(); iter != mPendingFetchers.end(); ++iter)
        {
          FetcherPtr &fetcher = (*iter);

          UsePublicationMetaDataPtr fetcherMetaData = PublicationMetaData::convert(fetcher->getPublicationMetaData());

          ZS_LOG_TRACE(log("comparing against fetcher's meta data") + fetcherMetaData->toDebug())

          if (!metaData->isMatching(fetcherMetaData->toPublicationMetaData())) {
            ZS_LOG_TRACE(log("activation meta data does not match fetcher"))
            continue;
          }

          // this is an exact match...
          ZS_LOG_DEBUG(log("an exact match of the fetcher's meta data was found thus will attempt a fetch now"))

          // if already has a monitor then already activated
          if (fetcher->getMonitor()) {
            ZS_LOG_DEBUG(log("cannot activate next fetcher as fetcher is already activated"))
            return;
          }

          CachedPublicationMap::iterator found = mCachedRemotePublications.find(metaData);

          if (found != mCachedRemotePublications.end()) {
            UsePublicationPtr &existingPublication = (*found).second;

            ZS_LOG_TRACE(log("existing publication found in 'remote' cache for meta data") + existingPublication->toDebug())
            ULONG fetchingVersion = metaData->getVersion();
            if (existingPublication->getVersion() >= fetchingVersion) {
              ZS_LOG_DETAIL(log("short circuit the fetch since the document is already in our cache") + existingPublication->toDebug())

              fetcher->setPublication(existingPublication);
              fetcher->notifyCompleted();
              return;
            }

            metaData->setVersion(existingPublication->getVersion());
          } else {
            ZS_LOG_TRACE(log("existing publication was not found in 'remote' cache"))
            metaData->setVersion(0);
          }

          // find the contacts to publish to...
          UseAccountPtr account = mAccount.lock();
          if (!account) {
            ZS_LOG_WARNING(Detail, log("cannot fetch publication as account object is gone"))
            fetcher->cancel();  // sorry, this could not be completed...
            mPendingFetchers.erase(iter);
            return;
          }
          PeerGetRequestPtr request = PeerGetRequest::create();
          request->domain(account->getDomain());
          request->publicationMetaData(metaData->toPublicationMetaData());

          fetcher->setMonitor(IMessageMonitor::monitorAndSendToLocation(fetcher, metaData->getPublishedLocation(), request, Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS)));
          return;
        }

        ZS_LOG_DEBUG(log("no pending fetchers of this type were found"))
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::activatePublisher(UsePublicationPtr publication)
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("attempting to activate next publisher for publication") + publication->toDebug())

        for (PendingPublisherList::iterator iter = mPendingPublishers.begin(); iter != mPendingPublishers.end(); ++iter)
        {
          PublisherPtr &publisher = (*iter);
          UsePublicationPtr publisherPublication = Publication::convert(publisher->getPublication());

          if (publication->getID() != publisherPublication->getID()) {
            ZS_LOG_TRACE(log("pending publication is not the correct one to activate") + ZS_PARAM("publisher publication", publisherPublication->getID()))
            continue;
          }

          ZS_LOG_DEBUG(log("found the correct publisher to activate and will attempt to activate it now"))

          UseAccountPtr account = mAccount.lock();
          if (!account) {
            ZS_LOG_WARNING(Detail, log("cannot activate next publisher as account object is gone"))
            mPendingPublishers.erase(iter);
            publisher->cancel();  // sorry, this could not be completed...
            return;
          }

          if (publisher->getMonitor()) {
            ZS_LOG_DEBUG(log("cannot active the next publisher as it is already activated"))
            return;
          }

          PeerPublishRequestPtr request = PeerPublishRequest::create();
          request->domain(account->getDomain());
          request->publication(Publication::convert(publication));
          request->publishedFromVersion(publication->getBaseVersion());
          request->publishedToVersion(publication->getVersion());

          publisher->setMonitor(IMessageMonitor::monitorAndSendToLocation(publisher, publication->getPublishedLocation(), request, Seconds(OPENPEER_STACK_PUBLICATIONREPOSITORY_REQUEST_TIMEOUT_IN_SECONDS)));
          return;
        }

        ZS_LOG_DEBUG(log("no pending publishers of this type were found"))
      }

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionIncomingPtr PublicationRepository::findIncomingSubscription(UsePublicationMetaDataPtr inMetaData) const
      {
        UseLocationPtr location = inMetaData->getCreatorLocation();
        ZS_LOG_TRACE(log("finding incoming peer subscription with publication path") + ZS_PARAM("publication name", inMetaData->getName()) + location->toDebug())

        for (PeerSubscriptionIncomingMap::const_iterator iter = mPeerSubscriptionsIncoming.begin(); iter != mPeerSubscriptionsIncoming.end(); ++iter)
        {
          const PeerSubscriptionIncomingPtr &subscription = (*iter).second;
          IPublicationMetaDataPtr metaData = subscription->getSource();

          if (inMetaData->getName() != metaData->getName()) continue;

          const char *ignoreReason = NULL;
          if (0 != ILocationForPublicationRepository::locationCompare(inMetaData->getCreatorLocation(), metaData->getCreatorLocation(), ignoreReason)) continue;

          ZS_LOG_TRACE(log("incoming peer subscription was found"))
          return subscription;
        }
        return PeerSubscriptionIncomingPtr();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onMessageIncoming(
                                                    IMessageIncomingPtr messageIncoming,
                                                    PeerPublishRequestPtr request
                                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!request)

        ZS_LOG_DEBUG(log("incoming peer publish request"))

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_TRACE(log("cannot respond to incoming peer publish request as account object is gone"))
          return;
        }

        UsePublicationPtr publication = Publication::convert(request->publication());
        publication->setCreatorLocation(Location::convert(messageIncoming->getLocation()));
        publication->setPublishedLocation(ILocationForPublicationRepository::getForLocal(Account::convert(account)));

        ZS_LOG_TRACE(log("incoming request to publish document") + publication->toDebug())

        CachedPublicationMap::iterator found = mCachedLocalPublications.find(publication);

        UsePublicationMetaDataPtr responceMetaData;
        if (found != mCachedLocalPublications.end()) {
          UsePublicationPtr &existingPublication = (*found).second;

          ZS_LOG_DEBUG(log("updating existing publication in 'local' cache with remote publication") + existingPublication->toDebug())
          try {
            existingPublication->updateFromFetchedPublication(Publication::convert(publication));
          } catch(IPublicationForPublicationRepository::Exceptions::VersionMismatch &) {
            ZS_LOG_WARNING(Detail, log("cannot update with the publication published since the versions are incompatible"))
            message::MessageResultPtr errorResult = message::MessageResult::create(request, 409, "Conflict");
            messageIncoming->sendResponse(errorResult);
            return;
          }

          responceMetaData = existingPublication;
        } else {
          ZS_LOG_DEBUG(log("creating new entry in 'local' cache for remote publication") + publication->toDebug())
          mCachedLocalPublications[publication] = publication;
          responceMetaData = publication;

          ZS_LOG_DEBUG(log("publication inserted into local cache") + ZS_PARAM("local cache total", mCachedLocalPublications.size()))
        }

        PeerPublishResultPtr reply = PeerPublishResult::create(request);
        reply->publicationMetaData(responceMetaData->toPublicationMetaData());
        messageIncoming->sendResponse(reply);

        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        //*********************************************************************
        // HERE - notify other subscriptions?
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onMessageIncoming(
                                                    IMessageIncomingPtr messageIncoming,
                                                    PeerGetRequestPtr request
                                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!request)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_TRACE(log("cannot respond to incoming peer get request as account object is gone"))
          return;
        }

        LocationPtr location = Location::convert(messageIncoming->getLocation());

        UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(request->publicationMetaData());

        PeerSourcePtr sourceMetaData = IPublicationMetaDataForPublicationRepository::createForSource(location);

        // the publication must have be published to "this" repository...
        metaData->setPublishedLocation(ILocationForPublicationRepository::getForLocal(Account::convert(account)));

        ZS_LOG_DEBUG(log("incoming request to get a document published to the local cache") + metaData->toDebug())

        CachedPublicationMap::iterator found = mCachedLocalPublications.find(metaData);
        if (found == mCachedLocalPublications.end()) {
          ZS_LOG_WARNING(Detail, log("failed to find publicatin thus ignoring get request"))
          return;
        }

        UsePublicationPtr &existingPublication = (*found).second;

        if (!canFetchPublication(
                                 existingPublication->getRelationships(),
                                 location)) {
          ZS_LOG_WARNING(Detail, log("publication is not published to the peer requesting the document (thus unable to reply to fetch request)"))
          return;
        }

        ZS_LOG_TRACE(log("requesting peer has authorization to get the requested document"))

        ZS_LOG_TRACE(log("incoming get request to will return this document") + existingPublication->toDebug())

        PeerCachePtr peerCache = PeerCache::find(sourceMetaData, mThisWeak.lock());
        peerCache->notifyFetched(existingPublication);

        PeerGetResultPtr reply = PeerGetResult::create(request);
        reply->publication(Publication::convert(existingPublication));
        messageIncoming->sendResponse(reply);
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onMessageIncoming(
                                                    IMessageIncomingPtr messageIncoming,
                                                    PeerDeleteRequestPtr request
                                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!request)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_TRACE(log("cannot respond to incoming peer delete request as account object is gone"))
          return;
        }

        IPublicationMetaDataPtr requestMetaData = request->publicationMetaData();
        UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(requestMetaData);

        metaData->setCreatorLocation(Location::convert(messageIncoming->getLocation()));
        metaData->setPublishedLocation(ILocationForPublicationRepository::getForLocal(Account::convert(account)));

        ZS_LOG_DEBUG(log("incoming request to delete document") + metaData->toDebug())

        CachedPublicationMap::iterator found = mCachedLocalPublications.find(metaData);
        if (found == mCachedLocalPublications.end()) {
          ZS_LOG_WARNING(Detail, log("failed to find publicatin thus ignoring delete request"))
          return;
        }

        UsePublicationPtr &existingPublication = (*found).second;

        ZS_LOG_DEBUG(log("delete request will delete this publication") + existingPublication->toDebug())

        mCachedLocalPublications.erase(found);

        PeerDeleteResultPtr reply = PeerDeleteResult::create(request);
        messageIncoming->sendResponse(reply);

        // notify all the subscribers that the document is gone
        for (SubscriptionLocalMap::iterator iter = mSubscriptionsLocal.begin(); iter != mSubscriptionsLocal.end(); ++iter)
        {
          SubscriptionLocalPtr &subscriber = (*iter).second;
          subscriber->notifyGone(existingPublication);
        }

        for (PeerSubscriptionIncomingMap::iterator iter = mPeerSubscriptionsIncoming.begin(); iter != mPeerSubscriptionsIncoming.end(); ++iter)
        {
          PeerSubscriptionIncomingPtr &subscriber = (*iter).second;
          subscriber->notifyGone(existingPublication);
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onMessageIncoming(
                                                    IMessageIncomingPtr messageIncoming,
                                                    PeerSubscribeRequestPtr request
                                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!request)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_TRACE(log("cannot respond to incoming peer subscribe request as account object is gone"))
          return;
        }

        UseLocationPtr location = Location::convert(messageIncoming->getLocation());

        UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(request->publicationMetaData());

        if (!location->getPeer()) {
          ZS_LOG_WARNING(Detail, log("incoming request from a location that does not have a peer (thus ignoring)") + ZS_PARAM("location", location->toDebug()) + ZS_PARAM("publication", metaData->toDebug()))
          return;
        }

        metaData->setCreatorLocation(Location::convert(location));
        metaData->setPublishedLocation(ILocationForPublicationRepository::getForLocal(Account::convert(account)));

        ZS_LOG_DEBUG(log("incoming request to subscribe to document path") + metaData->toDebug())

        PeerSubscriptionIncomingPtr existingSubscription = findIncomingSubscription(metaData);
        if (existingSubscription) {
          ZS_LOG_TRACE(log("removing existing subscription of the same source"))

          PeerSubscriptionIncomingMap::iterator found = mPeerSubscriptionsIncoming.find(existingSubscription->getID());
          ZS_THROW_BAD_STATE_IF(found == mPeerSubscriptionsIncoming.end())

          mPeerSubscriptionsIncoming.erase(found);
        }

        // send the reply now...
        PeerSubscribeResultPtr reply = PeerSubscribeResult::create(request);
        reply->publicationMetaData(metaData->toPublicationMetaData());
        messageIncoming->sendResponse(reply);

        const IPublicationMetaData::PublishToRelationshipsMap &relationships = metaData->getRelationships();
        if (relationships.size() > 0) {
          ZS_LOG_TRACE(log("incoming subscription is being created"))

          PeerSourcePtr sourceMetaData = IPublicationMetaDataForPublicationRepository::createForSource(Location::convert(location));

          // if there are relationships then this is an addition not a removal
          PeerSubscriptionIncomingPtr incoming = PeerSubscriptionIncoming::create(getAssociatedMessageQueue(), mThisWeak.lock(), sourceMetaData, metaData);

          mPeerSubscriptionsIncoming[incoming->getID()] = incoming;

          ZS_LOG_TRACE(log("notifying of all cached publications published to the new incoming party (so that all notifications can arrive in one message)"))
          incoming->notifyUpdated(mCachedLocalPublications);
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::onMessageIncoming(
                                                    IMessageIncomingPtr messageIncoming,
                                                    PeerPublishNotifyPtr request
                                                    )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!request)

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_TRACE(log("cannot respond to incoming peer notify request as account object is gone"))
          return;
        }

        typedef PeerPublishNotify::PublicationList PublicationList;

        const PublicationList &publicationList = request->publicationList();

        UseLocationPtr location = Location::convert(messageIncoming->getLocation());

        ZS_LOG_DEBUG(log("received publish notification") + ZS_PARAM("total publications", publicationList.size()) + location->toDebug())

        for (PublicationList::const_iterator iter = publicationList.begin(); iter != publicationList.end(); ++iter) {
          const IPublicationMetaDataPtr &requestMetaData = (*iter);
          UsePublicationMetaDataPtr metaData = PublicationMetaData::convert(requestMetaData);

          ZS_LOG_DEBUG(log("received notification of document change") + metaData->toDebug())

          metaData->setPublishedLocation(Location::convert(location));

          UsePublicationPtr publication = Publication::convert(metaData->toPublication());
          if (publication) {
            ZS_LOG_DEBUG(log("publication was included with notification") + publication->toDebug())

            // not only is meta data available an update to the publication is available, search the cache for the document
            CachedPublicationMap::iterator found = mCachedRemotePublications.find(metaData);;
            if (found != mCachedRemotePublications.end()) {
              UsePublicationPtr &existingPublication = (*found).second;
              ZS_LOG_DEBUG(log("existing internal publication found thus updating (if possible)") + existingPublication->toDebug())
              try {
                existingPublication->updateFromFetchedPublication(Publication::convert(publication));
              } catch(IPublicationForPublicationRepository::Exceptions::VersionMismatch &) {
                ZS_LOG_WARNING(Detail, log("version from the notify does not match our last version (thus ignoring change)"))
              }
            } else {
              bool okayToCreate = true;
              IPublicationLockerPtr docLock;
              DocumentPtr doc = publication->getJSON(docLock);
              if (doc) {
                ElementPtr diffEl = doc->findFirstChildElement(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME);
                okayToCreate = !diffEl;
                if (!okayToCreate) {
                  ZS_LOG_WARNING(Detail, log("new entry for remote cache cannot be created for publication which only contains diff updates") + publication->toDebug())
                }
              }
              if (okayToCreate) {
                ZS_LOG_DEBUG(log("new entry for remote cache will be created since existing publication in cache was not found") + publication->toDebug())
                mCachedRemotePublications[publication] = publication;

                ZS_LOG_DEBUG(log("publication inserted into remote cache") + ZS_PARAM("remote cache total", mCachedRemotePublications.size()))
              }
            }
          }

          for (PeerSubscriptionOutgoingMap::iterator subIter = mPeerSubscriptionsOutgoing.begin(); subIter != mPeerSubscriptionsOutgoing.end(); ++subIter)
          {
            PeerSubscriptionOutgoingPtr &subscriber = (*subIter).second;
            ZS_LOG_TRACE(log("notifying outgoing subscription of change") + ZS_PARAM("susbcriber ID", subscriber->getID()))
            subscriber->notifyUpdated(metaData);
          }
        }
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::cancel()
      {
        ZS_LOG_DETAIL(log("publication repository cancel is called"))

        AutoRecursiveLock lock(*this);

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        if (mExpiresTimer) {
          mExpiresTimer->cancel();
          mExpiresTimer.reset();
        }

        // clear out all pending fetchers
        {
          // need to copy to a temporary list and clean out the original pending list to prevent cancelling of one activating another...
          PendingFetcherList pending = mPendingFetchers;
          mPendingFetchers.clear();

          for (PendingFetcherList::iterator iter = pending.begin(); iter != pending.end(); ++iter)
          {
            FetcherPtr &fetcher = (*iter);
            ZS_LOG_DEBUG(log("cancelling pending fetcher") + ZS_PARAM("fetcher id", fetcher->getID()))
            fetcher->cancel();
          }
        }

        // clear out all pending publishers
        {
          // need to copy to a temporary list and clean out the original pending list to prevent cancelling of one activating another...
          PendingPublisherList pending = mPendingPublishers;
          mPendingPublishers.clear();

          for (PendingPublisherList::iterator iter = pending.begin(); iter != pending.end(); ++iter)
          {
            PublisherPtr &publisher = (*iter);
            ZS_LOG_DEBUG(log("cancelling pending publisher") + ZS_PARAM("publisher id", publisher->getID()))
            publisher->cancel();
          }
        }

        // clear out all the cached publications...
        {
          for (CachedPublicationMap::iterator iter = mCachedLocalPublications.begin(); iter != mCachedLocalPublications.end(); ++iter)
          {
            UsePublicationPtr &publication = (*iter).second;

            // notify all the local subscribers that the documents are now gone...
            for (SubscriptionLocalMap::iterator subscriberIter = mSubscriptionsLocal.begin(); subscriberIter != mSubscriptionsLocal.end(); ++subscriberIter)
            {
              SubscriptionLocalPtr &subscriber = (*subscriberIter).second;
              ZS_LOG_DEBUG(log("notifying local subscriber that publication is gone during cancel") + ZS_PARAM("subscriber id", subscriber->getID()))
              subscriber->notifyGone(publication);
            }

            // notify all the incoming subscriptions that the documents are now gone...
            for (PeerSubscriptionIncomingMap::iterator subscriberIter = mPeerSubscriptionsIncoming.begin(); subscriberIter != mPeerSubscriptionsIncoming.end(); ++subscriberIter)
            {
              PeerSubscriptionIncomingPtr &subscriber = (*subscriberIter).second;
              ZS_LOG_DEBUG(log("notifying incoming subscriber that publication is gone during cancel") + ZS_PARAM("subscriber id", subscriber->getID()))
              subscriber->notifyGone(publication);
            }
          }

          mCachedLocalPublications.clear();
          mCachedRemotePublications.clear();
        }

        // clear out all the local subscriptions...
        for (SubscriptionLocalMap::iterator subIter = mSubscriptionsLocal.begin(); subIter != mSubscriptionsLocal.end(); )
        {
          SubscriptionLocalMap::iterator current = subIter;
          ++subIter;

          SubscriptionLocalPtr &subscriber = (*current).second;
          ZS_LOG_DEBUG(log("cancelling local subscription") + ZS_PARAM("subscriber id", subscriber->getID()))
          subscriber->cancel();
        }
        mSubscriptionsLocal.clear();

        // clear out all the incoming subscriptions
        for (PeerSubscriptionIncomingMap::iterator iter = mPeerSubscriptionsIncoming.begin(); iter != mPeerSubscriptionsIncoming.end(); ++iter)
        {
          PeerSubscriptionIncomingPtr &subscriber = (*iter).second;
          ZS_LOG_DEBUG(log("cancelling incoming subscription") + ZS_PARAM("subscriber id", subscriber->getID()))
          subscriber->cancel();
        }
        mPeerSubscriptionsIncoming.clear();

        // clear out all the outgoing subscriptions
        for (PeerSubscriptionOutgoingMap::iterator subIter = mPeerSubscriptionsOutgoing.begin(); subIter != mPeerSubscriptionsOutgoing.end(); )
        {
          PeerSubscriptionOutgoingMap::iterator current = subIter;
          ++subIter;

          PeerSubscriptionOutgoingPtr &subscriber = (*current).second;
          ZS_LOG_DEBUG(log("cancelling outgoing subscription") + ZS_PARAM("subscriber id", subscriber->getID()))
          subscriber->cancel();
        }
        mPeerSubscriptionsOutgoing.clear();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationRepositoryFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationRepositoryFactory &IPublicationRepositoryFactory::singleton()
      {
        return PublicationRepositoryFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr IPublicationRepositoryFactory::createPublicationRepository(AccountPtr account)
      {
        if (this) {}
        return PublicationRepository::create(account);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPublicationSubscription
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IPublicationSubscription::toString(PublicationSubscriptionStates state)
    {
      switch (state) {
        case PublicationSubscriptionState_Pending:        return "Pending";
        case PublicationSubscriptionState_Established:    return "Established";
        case PublicationSubscriptionState_ShuttingDown:   return "Shutting down";
        case PublicationSubscriptionState_Shutdown:       return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationRepository::toDebug(IPublicationRepositoryPtr repository)
    {
      return internal::PublicationRepository::toDebug(repository);
    }

    //-------------------------------------------------------------------------
    IPublicationRepositoryPtr IPublicationRepository::getFromAccount(IAccountPtr account)
    {
      return internal::PublicationRepository::getFromAccount(account);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationPublisher::toDebug(IPublicationPublisherPtr publisher)
    {
      return internal::PublicationRepository::Publisher::toDebug(publisher);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationFetcher::toDebug(IPublicationFetcherPtr fetcher)
    {
      return internal::PublicationRepository::Fetcher::toDebug(fetcher);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationRemover::toDebug(IPublicationRemoverPtr remover)
    {
      return internal::PublicationRepository::Remover::toDebug(remover);
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationSubscription::toDebug(IPublicationSubscriptionPtr subscription)
    {
      internal::PublicationRepository::SubscriptionLocalPtr localSubscription = internal::PublicationRepository::SubscriptionLocal::convert(subscription);
      if (localSubscription) {
        return internal::PublicationRepository::SubscriptionLocal::toDebug(localSubscription);
      }
      return internal::PublicationRepository::PeerSubscriptionOutgoing::toDebug(internal::PublicationRepository::PeerSubscriptionOutgoing::convert(subscription));
    }

    //-------------------------------------------------------------------------
    ElementPtr IPublicationRepositoryPeerCache::toDebug(IPublicationRepositoryPeerCachePtr cache)
    {
      return internal::PublicationRepository::PeerCache::toDebug(cache);
    }
  }
}
