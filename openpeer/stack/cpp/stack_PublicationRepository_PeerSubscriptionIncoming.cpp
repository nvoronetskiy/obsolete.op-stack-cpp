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

#include <openpeer/stack/internal/stack_PublicationRepository_PeerSubscriptionIncoming.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Location.h>

#include <openpeer/stack/message/peer-common/PeerPublishNotify.h>
#include <openpeer/services/IHelper.h>

#include <zsLib/RegEx.h>
#include <zsLib/XML.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionIncoming
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionIncoming::PeerSubscriptionIncoming(
                                                                                IMessageQueuePtr queue,
                                                                                PublicationRepositoryPtr outer,
                                                                                PeerSourcePtr peerSource,
                                                                                UsePublicationMetaDataPtr subscriptionInfo
                                                                                ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(*outer),
        mOuter(outer),
        mPeerSource(peerSource),
        mSubscriptionInfo(subscriptionInfo)
      {
        ZS_LOG_DEBUG(log("created") + subscriptionInfo->toDebug())
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionIncoming::~PeerSubscriptionIncoming()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionIncoming => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerSubscriptionIncoming::toDebug(PeerSubscriptionIncomingPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return subscription->toDebug();
      }

      //-----------------------------------------------------------------------
      PublicationRepository::PeerSubscriptionIncomingPtr PublicationRepository::PeerSubscriptionIncoming::create(
                                                                                                                 IMessageQueuePtr queue,
                                                                                                                 PublicationRepositoryPtr outer,
                                                                                                                 PeerSourcePtr peerSource,
                                                                                                                 UsePublicationMetaDataPtr subscriptionInfo
                                                                                                                 )
      {
        PeerSubscriptionIncomingPtr pThis(new PeerSubscriptionIncoming(queue, outer, peerSource, subscriptionInfo));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::notifyUpdated(UsePublicationPtr publication)
      {
        CachedPublicationMap tempCache;
        tempCache[publication] = publication;

        notifyUpdated(tempCache);
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::notifyGone(UsePublicationPtr publication)
      {
        CachedPublicationMap tempCache;
        tempCache[publication] = publication;

        notifyGone(tempCache);
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::notifyUpdated(const CachedPublicationMap &cachedPublications)
      {
        typedef PeerPublishNotify::PublicationList PublicationList;

        AutoRecursiveLock lock(*this);

        PublicationList list;

        PublicationRepositoryPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication update notification as publication respository is gone"))
          return;
        }

        UseAccountPtr account = outer->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication update notification as account is gone"))
          return;
        }

        for (CachedPublicationMap::const_iterator iter = cachedPublications.begin(); iter != cachedPublications.end(); ++iter)
        {
          const UsePublicationPtr &publication = (*iter).second;
          ZS_LOG_TRACE(log("notified of updated publication") + publication->toDebug())

          String name = publication->getName();
          String regex = mSubscriptionInfo->getName();

          zsLib::RegEx e(regex);
          if (!e.hasMatch(name)) {
            ZS_LOG_TRACE(log("name does not match subscription regex") + ZS_PARAM("name", name) + ZS_PARAM("regex", regex))
            continue;
          }

          LocationPtr subscriberLocation = mSubscriptionInfo->getCreatorLocation(); // the subscriber is the person who created this subscription

          if (!outer->canSubscribeToPublisher(
                                              publication->getCreatorLocation(),
                                              publication->getRelationships(),
                                              subscriberLocation,
                                              mSubscriptionInfo->getRelationships())) {
            ZS_LOG_TRACE(log("publication/subscriber do not publish/subscribe to each other (thus ignoring notification)"))
            continue;
          }

          ZS_LOG_DEBUG(log("notifying about publication updated to subscriber") + publication->toDebug())

          list.push_back(publication->toPublicationMetaData());
        }

        if (list.size() < 1) {
          ZS_LOG_TRACE(log("no publications updates are needed to be sent to this subscriber") + mSubscriptionInfo->toDebug())
          return;
        }

        ZS_LOG_TRACE(log("publications will be notified to this subscriber") + ZS_PARAM("total  publications", list.size()) + mSubscriptionInfo->toDebug())

        PeerPublishNotifyPtr request = PeerPublishNotify::create();
        request->domain(account->getDomain());
        request->publicationList(list);
        request->peerCache(PeerCache::find(mPeerSource, outer));

        UseLocationPtr sendToLocation = mSubscriptionInfo->getCreatorLocation();

        sendToLocation->sendMessage(request);
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::notifyGone(const CachedPublicationMap &cachedPublications)
      {
        typedef PeerPublishNotify::PublicationList PublicationList;

        AutoRecursiveLock lock(*this);

        PublicationList list;

        PublicationRepositoryPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication gone notification as publication respository is gone"))
          return;
        }

        UseAccountPtr account = outer->getAccount();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("cannot hanlde publication gone notification as account is gone"))
          return;
        }

        for (CachedPublicationMap::const_iterator iter = cachedPublications.begin(); iter != cachedPublications.end(); ++iter)
        {
          const UsePublicationPtr &publication = (*iter).second;

          String name = publication->getName();
          String regex = mSubscriptionInfo->getName();

          zsLib::RegEx e(regex);
          if (!e.hasMatch(name)) {
            ZS_LOG_TRACE(log("name does not match subscription regex") + ZS_PARAM("name", name) + ZS_PARAM("regex", regex))
            continue;
          }

          LocationPtr subscriptionPublishedLocation = mSubscriptionInfo->getPublishedLocation();

          if (!outer->canSubscribeToPublisher(
                                              publication->getCreatorLocation(),
                                              publication->getRelationships(),
                                              subscriptionPublishedLocation,
                                              mSubscriptionInfo->getRelationships())) {
            ZS_LOG_TRACE(log("publication/subscriber do not publish/subscribe to each other (thus ignoring notification)"))
            continue;
          }

          ZS_LOG_DEBUG(log("notifying about publication gone to subscriber") + publication->toDebug())

          UsePublicationMetaDataPtr metaData = IPublicationMetaDataForPublicationRepository::createFrom(publication->toPublicationMetaData());
          metaData->setVersion(0);
          metaData->setBaseVersion(0);

          list.push_back(metaData->toPublicationMetaData());
        }

        if (list.size() < 1) {
          ZS_LOG_TRACE(log("no 'publications are gone' notification will be send to this subscriber") + mSubscriptionInfo->toDebug())
          return;
        }

        ZS_LOG_TRACE(log("'publications are gone' notification will be notified to this subscriber") + ZS_PARAM("total  publications", list.size()) + mSubscriptionInfo->toDebug())

        PeerPublishNotifyPtr request = PeerPublishNotify::create();
        request->domain(account->getDomain());
        request->publicationList(list);

        UseLocationPtr sendToLocation = mSubscriptionInfo->getCreatorLocation();

        sendToLocation->sendMessage(request);
      }

      //-----------------------------------------------------------------------
      IPublicationMetaDataPtr PublicationRepository::PeerSubscriptionIncoming::getSource() const
      {
        return mSubscriptionInfo->toPublicationMetaData();
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerSubscriptionIncoming::cancel()
      {
        ZS_LOG_DEBUG(log("cancel"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerSubscriptionIncoming => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::PeerSubscriptionIncoming::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::PeerSubscriptionIncoming");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerSubscriptionIncoming::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::PeerSubscriptionIncoming");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "source", UsePublicationMetaData::toDebug(mPeerSource));
        IHelper::debugAppend(resultEl, "subscription info", UsePublicationMetaData::toDebug(mSubscriptionInfo));

        return resultEl;
      }

    }
  }
}
