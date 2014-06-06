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

#include <openpeer/stack/internal/stack_PublicationRepository_PeerCache.h>
#include <openpeer/stack/internal/stack_Publication.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using services::IHelper;

      ZS_DECLARE_TYPEDEF_PTR(PublicationRepository::PeerCache, PeerCache)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerCache
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationRepository::PeerCache::PeerCache(
                                                  PeerSourcePtr peerSource,
                                                  PublicationRepositoryPtr repository
                                                  ) :
        SharedRecursiveLock(*repository),
        mOuter(repository),
        mPeerSource(peerSource)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerCache::init()
      {
      }

      //-----------------------------------------------------------------------
      PeerCachePtr PublicationRepository::PeerCache::create(
                                                            PeerSourcePtr peerSource,
                                                            PublicationRepositoryPtr repository
                                                            )
      {
        PeerCachePtr pThis(new PeerCache(peerSource, repository));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PublicationRepository::PeerCache::~PeerCache()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      PeerCachePtr PublicationRepository::PeerCache::convert(IPublicationRepositoryPeerCachePtr cache)
      {
        return dynamic_pointer_cast<PeerCache>(cache);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerCache => IPublicationRepositoryPeerCache
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerCache::toDebug(IPublicationRepositoryPeerCachePtr cache)
      {
        if (!cache) return ElementPtr();
        return PeerCache::convert(cache)->toDebug();
      }

      //-----------------------------------------------------------------------
      bool PublicationRepository::PeerCache::getNextVersionToNotifyAboutAndMarkNotified(
                                                                                        IPublicationPtr inPublication,
                                                                                        size_t &ioMaxSizeAvailableInBytes,
                                                                                        ULONG &outNotifyFromVersion,
                                                                                        ULONG &outNotifyToVersion
                                                                                        )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inPublication)

        AutoRecursiveLock lock(*this);

        UsePublicationPtr publication = Publication::convert(inPublication);

        CachedPeerPublicationMap::iterator found = mCachedPublications.find(publication);
        if (found != mCachedPublications.end()) {

          // this document was fetched before...
          UsePublicationMetaDataPtr &metaData = (*found).second;
          metaData->setExpires(publication->getExpires());  // not used yet but could be used to remember when this document will expire

          ULONG nextVersionToNotify = metaData->getVersion() + 1;
          if (nextVersionToNotify > publication->getVersion()) {
            ZS_LOG_WARNING(Detail, log("already fetched/notified of this version so why is it being notified? (probably okay and likely because of peer disconnection)") + publication->toDebug())
            return false;
          }

          size_t outputSize = 0;
          publication->getDiffVersionsOutputSize(nextVersionToNotify, publication->getVersion(), outputSize);

          if (outputSize > ioMaxSizeAvailableInBytes) {
            ZS_LOG_WARNING(Detail, log("diff document is too large for notify") + ZS_PARAM("output size", outputSize) + ZS_PARAM("max size", ioMaxSizeAvailableInBytes))
            return false;
          }

          outNotifyFromVersion = nextVersionToNotify;
          outNotifyToVersion = publication->getVersion();

          ioMaxSizeAvailableInBytes -= outputSize;

          metaData->setVersion(outNotifyToVersion);

          ZS_LOG_DETAIL(log("recommend notify about diff version") +
                        ZS_PARAM("from", outNotifyFromVersion) +
                        ZS_PARAM("to", outNotifyToVersion)  +
                        ZS_PARAM("size", outputSize) +
                        ZS_PARAM("remaining", ioMaxSizeAvailableInBytes))
          return true;
        }

        size_t outputSize = 0;
        publication->getEntirePublicationOutputSize(outputSize);

        if (outputSize > ioMaxSizeAvailableInBytes) {
          ZS_LOG_WARNING(Detail, log("diff document is too large for notify") + ZS_PARAM("output size", outputSize) + ZS_PARAM("max size", ioMaxSizeAvailableInBytes))
          return false;
        }

        UsePublicationMetaDataPtr metaData = IPublicationMetaDataForPublicationRepository::createFrom(publication->toPublicationMetaData());
        metaData->setExpires(publication->getExpires());  // not used yet but could be used to remember when this document will expire
        metaData->setBaseVersion(0);
        metaData->setVersion(publication->getVersion());

        mCachedPublications[metaData] = metaData;

        outNotifyFromVersion = 0;
        outNotifyToVersion = publication->getVersion();
        ioMaxSizeAvailableInBytes -= outputSize;

        ZS_LOG_DETAIL(log("recommend notify about entire document") +
                      ZS_PARAM("from", outNotifyFromVersion) +
                      ZS_PARAM("to", outNotifyToVersion)  +
                      ZS_PARAM("size", outputSize) +
                      ZS_PARAM("remaining", ioMaxSizeAvailableInBytes))
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerCache => friend PublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PeerCachePtr PublicationRepository::PeerCache::find(
                                                          PeerSourcePtr peerSource,
                                                          PublicationRepositoryPtr repository
                                                          )
      {
        AutoRecursiveLock lock(*repository);

        CachedPeerSourceMap &cache = repository->getCachedPeerSources();
        CachedPeerSourceMap::iterator found = cache.find(peerSource);
        if (found != cache.end()) {
          PeerCachePtr &peerCache = (*found).second;
          return peerCache;
        }
        PeerCachePtr pThis = PeerCache::create(peerSource, repository);
        cache[peerSource] = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      void PublicationRepository::PeerCache::notifyFetched(UsePublicationPtr publication)
      {
        AutoRecursiveLock lock(*this);

        CachedPeerPublicationMap::iterator found = mCachedPublications.find(publication);
        if (found != mCachedPublications.end()) {
          UsePublicationMetaDataPtr &metaData = (*found).second;

          // remember up to which version was last fetched
          metaData->setVersion(publication->getVersion());
          return;
        }

        UsePublicationMetaDataPtr metaData = IPublicationMetaDataForPublicationRepository::createFrom(publication->toPublicationMetaData());
        metaData->setBaseVersion(0);
        metaData->setVersion(publication->getVersion());

        mCachedPublications[metaData] = metaData;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationRepository::PeerCache => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationRepository::PeerCache::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationRepository::PeerCache");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationRepository::PeerCache::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("PublicationRepository::PeerCache");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePublicationMetaData::toDebug(mPeerSource));
        IHelper::debugAppend(resultEl, "expires", mExpires);
        IHelper::debugAppend(resultEl, "cached remote", mCachedPublications.size());

        return resultEl;
      }


    }
  }
}
