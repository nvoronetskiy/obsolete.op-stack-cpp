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

#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/ILocation.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/internal/stack_Location.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using zsLib::Stringize;
      using services::IHelper;

      typedef IPublicationMetaDataForPublicationRepository::ForPublicationRepositoryPtr ForPublicationRepositoryPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPublicationMetaDataForPublicationRepository::toDebug(ForPublicationRepositoryPtr metaData)
      {
        return PublicationMetaData::toDebug(PublicationMetaData::convert(metaData));
      }

      //-----------------------------------------------------------------------
      ForPublicationRepositoryPtr IPublicationMetaDataForPublicationRepository::create(
                                                                                       ULONG version,
                                                                                       ULONG baseVersion,
                                                                                       ULONG lineage,
                                                                                       LocationPtr creatorLocation,
                                                                                       const char *name,
                                                                                       const char *mimeType,
                                                                                       Encodings encoding,
                                                                                       const PublishToRelationshipsMap &relationships,
                                                                                       LocationPtr publishedLocation,
                                                                                       Time expires
                                                                                       )
      {
        return IPublicationMetaDataFactory::singleton().creatPublicationMetaData(
                                                                                 version,
                                                                                 baseVersion,
                                                                                 lineage,
                                                                                 creatorLocation,
                                                                                 name,
                                                                                 mimeType,
                                                                                 encoding,
                                                                                 relationships,
                                                                                 publishedLocation,
                                                                                 expires
                                                                                 );
      }

      //-----------------------------------------------------------------------
      ForPublicationRepositoryPtr IPublicationMetaDataForPublicationRepository::createFrom(IPublicationMetaDataPtr metaData)
      {
        return IPublicationMetaDataFactory::singleton().creatPublicationMetaData(
                                                                                 metaData->getVersion(),
                                                                                 metaData->getBaseVersion(),
                                                                                 metaData->getLineage(),
                                                                                 Location::convert(metaData->getCreatorLocation()),
                                                                                 metaData->getName(),
                                                                                 metaData->getMimeType(),
                                                                                 metaData->getEncoding(),
                                                                                 metaData->getRelationships(),
                                                                                 Location::convert(metaData->getPublishedLocation()),
                                                                                 metaData->getExpires()
                                                                                 );
      }

      //-----------------------------------------------------------------------
      ForPublicationRepositoryPtr IPublicationMetaDataForPublicationRepository::createForSource(LocationPtr location)
      {
        PublishToRelationshipsMap empty;
        return IPublicationMetaDataFactory::singleton().creatPublicationMetaData(
                                                                                 0,
                                                                                 0,
                                                                                 0,
                                                                                 location,
                                                                                 "",
                                                                                 "",
                                                                                 IPublicationMetaData::Encoding_JSON,
                                                                                 empty,
                                                                                 location,
                                                                                 Time()
                                                                                 );
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationMetaDataForMessages::ForMessagesPtr IPublicationMetaDataForMessages::create(
                                                                                              ULONG version,
                                                                                              ULONG baseVersion,
                                                                                              ULONG lineage,
                                                                                              LocationPtr creatorLocation,
                                                                                              const char *name,
                                                                                              const char *mimeType,
                                                                                              Encodings encoding,
                                                                                              const PublishToRelationshipsMap &relationships,
                                                                                              LocationPtr publishedLocation,
                                                                                              Time expires
                                                                                              )
      {
        return IPublicationMetaDataFactory::singleton().creatPublicationMetaData(
                                                                                 version,
                                                                                 baseVersion,
                                                                                 lineage,
                                                                                 creatorLocation,
                                                                                 name,
                                                                                 mimeType,
                                                                                 encoding,
                                                                                 relationships,
                                                                                 publishedLocation,
                                                                                 expires
                                                                                 );
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationMetaData::PublicationMetaData(
                                               ULONG version,
                                               ULONG baseVersion,
                                               ULONG lineage,
                                               LocationPtr creatorLocation,
                                               const char *name,
                                               const char *mimeType,
                                               Encodings encoding,
                                               const PublishToRelationshipsMap &relationships,
                                               LocationPtr publishedLocation,
                                               Time expires
                                               ) :
        mID(zsLib::createPUID()),
        mVersion(version),
        mBaseVersion(baseVersion),
        mLineage(lineage),
        mCreatorLocation(creatorLocation),
        mName(name ? name : ""),
        mMimeType(mimeType ? mimeType : ""),
        mEncoding(encoding),
        mPublishedRelationships(relationships),
        mPublishedLocation(publishedLocation),
        mExpires(expires)
      {
        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::init()
      {
      }

      //-----------------------------------------------------------------------
      PublicationMetaData::~PublicationMetaData()
      {
        if (isNoop()) return;

        mThisWeak.reset();
        ZS_LOG_DEBUG(debug("destroyed"))
      }

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr PublicationMetaData::convert(IPublicationMetaDataPtr publication)
      {
        return dynamic_pointer_cast<PublicationMetaData>(publication);
      }

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr PublicationMetaData::convert(ForPublicationRepositoryPtr publication)
      {
        return dynamic_pointer_cast<PublicationMetaData>(publication);
      }

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr PublicationMetaData::convert(ForMessagesPtr publication)
      {
        return dynamic_pointer_cast<PublicationMetaData>(publication);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData => IPublicationMetaData
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationPtr PublicationMetaData::toPublication() const
      {
        AutoRecursiveLock lock(mLock);
        return Publication::convert(mPublication);
      }

      //-----------------------------------------------------------------------
      ILocationPtr PublicationMetaData::getCreatorLocation() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getCreatorLocation();
        return mCreatorLocation;
      }

      //-----------------------------------------------------------------------
      String PublicationMetaData::getName() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getName();
        return mName;
      }

      //-----------------------------------------------------------------------
      String PublicationMetaData::getMimeType() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getMimeType();
        return mMimeType;
      }

      //-----------------------------------------------------------------------
      ULONG PublicationMetaData::getVersion() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getVersion();
        return mVersion;
      }

      //-----------------------------------------------------------------------
      ULONG PublicationMetaData::getBaseVersion() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getBaseVersion();
        return mBaseVersion;
      }

      //-----------------------------------------------------------------------
      ULONG PublicationMetaData::getLineage() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getLineage();
        return mLineage;
      }

      //-----------------------------------------------------------------------
      IPublicationMetaData::Encodings PublicationMetaData::getEncoding() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getEncoding();
        return mEncoding;
      }

      //-----------------------------------------------------------------------
      Time PublicationMetaData::getExpires() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getExpires();
        return mExpires;
      }

      //-----------------------------------------------------------------------
      ILocationPtr PublicationMetaData::getPublishedLocation() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getPublishedLocation();
        return mPublishedLocation;
      }

      //-----------------------------------------------------------------------
      const IPublicationMetaData::PublishToRelationshipsMap &PublicationMetaData::getRelationships() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->getRelationships();
        return mPublishedRelationships;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData => IPublicationMetaDataForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr PublicationMetaData::create(
                                                         ULONG version,
                                                         ULONG baseVersion,
                                                         ULONG lineage,
                                                         LocationPtr creatorLocation,
                                                         const char *name,
                                                         const char *mimeType,
                                                         Encodings encoding,
                                                         const PublishToRelationshipsMap &relationships,
                                                         LocationPtr publishedLocation,
                                                         Time expires
                                                         )
      {
        PublicationMetaDataPtr pThis(new PublicationMetaData(
                                                             version,
                                                             baseVersion,
                                                             lineage,
                                                             creatorLocation,
                                                             name,
                                                             mimeType,
                                                             encoding,
                                                             relationships,
                                                             publishedLocation,
                                                             expires
                                                             )
                                     );
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData => IPublicationMetaDataForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PublicationMetaData::toDebug(IPublicationMetaDataPtr metaData)
      {
        if (!metaData) return ElementPtr();
        return PublicationMetaData::convert(metaData)->toDebug();
      }

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr PublicationMetaData::toPublicationMetaData() const
      {
        return mThisWeak.lock();
      }

      //-----------------------------------------------------------------------
      LocationPtr PublicationMetaData::getCreatorLocation(bool) const
      {
        AutoRecursiveLock lock(mLock);
        return mCreatorLocation;
      }

      //-----------------------------------------------------------------------
      LocationPtr PublicationMetaData::getPublishedLocation(bool) const
      {
        AutoRecursiveLock lock(mLock);
        return mPublishedLocation;
      }

      //-----------------------------------------------------------------------
      bool PublicationMetaData::isMatching(
                                           const IPublicationMetaDataPtr &metaData,
                                           bool ignoreLineage
                                           ) const
      {
        AutoRecursiveLock lock(mLock);

        const char *reason = NULL;
        if (0 != UseLocation::locationCompare(mCreatorLocation, metaData->getCreatorLocation(), reason)) {
          return false;
        }

        if (!ignoreLineage) {
          if (metaData->getLineage() != getLineage()) return false;
        }
        if (metaData->getName() != getName()) return false;
        if (0 != UseLocation::locationCompare(mPublishedLocation, metaData->getPublishedLocation(), reason)) {
          return false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool PublicationMetaData::isLessThan(
                                           const IPublicationMetaDataPtr &metaData,
                                           bool ignoreLineage
                                           ) const
      {
        AutoRecursiveLock lock(mLock);

        const char *reason = "match";

        {
          int compare = UseLocation::locationCompare(mCreatorLocation, metaData->getCreatorLocation(), reason);

          if (compare < 0) {goto result_true;}
          if (compare > 0) {goto result_false;}

          ignoreLineage = ignoreLineage || (0 == getLineage()) || (0 == metaData->getLineage());

          if (!ignoreLineage) {
            if (getLineage() < metaData->getLineage()) {reason = "lineage"; goto result_true;}
            if (getLineage() > metaData->getLineage()) {reason = "lineage"; goto result_false;}
          }
          if (getName() < metaData->getName()) {reason = "name"; goto result_true;}
          if (getName() > metaData->getName()) {reason = "name"; goto result_false;}

          compare = UseLocation::locationCompare(mPublishedLocation, metaData->getPublishedLocation(), reason);
          if (compare < 0) {goto result_true;}
          if (compare > 0) {goto result_false;}
          goto result_false;
        }

      result_true:
        {
          UsePublicationPtr publication = Publication::convert(metaData->toPublication());
          ZS_LOG_INSANE(log("less than is TRUE") + ZS_PARAM("reason", reason))
          ZS_LOG_INSANE(log("less than X (TRUE)") + toDebug())
          ZS_LOG_INSANE(log("less than Y (TRUE)") + (publication ? publication->toDebug() : PublicationMetaData::convert(metaData)->toDebug()))
          return true;
        }
      result_false:
        {
          UsePublicationPtr publication = Publication::convert(metaData->toPublication());
          ZS_LOG_INSANE(log("less than is FALSE") + ZS_PARAM("reason", reason))
          ZS_LOG_INSANE(log("less than X (FALSE):") + toDebug())
          ZS_LOG_INSANE(log("less than Y (FALSE):") + (publication ? publication->toDebug() : PublicationMetaData::convert(metaData)->toDebug()))
        }
        return false;
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::setVersion(ULONG version)
      {
        AutoRecursiveLock lock(mLock);
        mVersion = version;
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::setBaseVersion(ULONG version)
      {
        AutoRecursiveLock lock(mLock);
        mBaseVersion = version;
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::setCreatorLocation(LocationPtr location)
      {
        AutoRecursiveLock lock(mLock);
        mCreatorLocation = location;
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::setPublishedLocation(LocationPtr location)
      {
        AutoRecursiveLock lock(mLock);
        mPublishedLocation = location;
      }

      //-----------------------------------------------------------------------
      void PublicationMetaData::setExpires(Time expires)
      {
        AutoRecursiveLock lock(mLock);
        mExpires = expires;
      }

      //-----------------------------------------------------------------------
      ElementPtr PublicationMetaData::toDebug() const
      {
        AutoRecursiveLock lock(mLock);
        if (mPublication) return mPublication->toDebug();

        UseLocationPtr creatorLocation = Location::convert(getCreatorLocation());
        UseLocationPtr publishedLocation = Location::convert(getPublishedLocation());

        ElementPtr resultEl = Element::create("PublicationMetaData");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "mapped to publication", (bool)mPublication);
        IHelper::debugAppend(resultEl, "name", getName());
        IHelper::debugAppend(resultEl, "version", mVersion);
        IHelper::debugAppend(resultEl, "base version", mBaseVersion);
        IHelper::debugAppend(resultEl, "lineage", mLineage);
        IHelper::debugAppend(resultEl, "creator", creatorLocation ? creatorLocation->toDebug() : ElementPtr());
        IHelper::debugAppend(resultEl, "published", publishedLocation ? publishedLocation->toDebug() : ElementPtr());
        IHelper::debugAppend(resultEl, "mime type", getMimeType());
        IHelper::debugAppend(resultEl, "expires", mExpires);
        IHelper::debugAppend(resultEl, "total relationships", mPublishedRelationships.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PublicationMetaData::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PublicationMetaData");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params PublicationMetaData::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPublicationMetaData
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPublicationMetaData::toDebug(IPublicationMetaDataPtr metaData)
    {
      return internal::PublicationMetaData::toDebug(metaData);
    }

    //-------------------------------------------------------------------------
    const char *IPublicationMetaData::toString(Encodings encoding)
    {
      switch (encoding) {
        case Encoding_Binary: return "Binary";
        case Encoding_JSON:   return "json";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IPublicationMetaData::toString(Permissions permission)
    {
      switch (permission) {
        case Permission_All:      return "All";
        case Permission_None:     return "None";
        case Permission_Some:     return "Some";
        case Permission_Add:      return "Add";
        case Permission_Remove:   return "Remove";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IPublicationMetaDataPtr IPublicationMetaData::create(
                                                         ULONG version,
                                                         ULONG baseVersion,
                                                         ULONG lineage,
                                                         ILocationPtr creatorLocation,
                                                         const char *name,
                                                         const char *mimeType,
                                                         Encodings encoding,
                                                         const PublishToRelationshipsMap &relationships,
                                                         ILocationPtr publishedLocation,
                                                         Time expires
                                                         )
    {
      return internal::IPublicationMetaDataFactory::singleton().creatPublicationMetaData(
                                                                                         version,
                                                                                         baseVersion,
                                                                                         lineage,
                                                                                         internal::Location::convert(creatorLocation),
                                                                                         name,
                                                                                         mimeType,
                                                                                         encoding,
                                                                                         relationships,
                                                                                         internal::Location::convert(publishedLocation),
                                                                                         expires
                                                                                         );
    }

  }
}
