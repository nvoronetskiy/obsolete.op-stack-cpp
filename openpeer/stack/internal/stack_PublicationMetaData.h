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
#include <openpeer/stack/IPublicationMetaData.h>

#include <boost/shared_array.hpp>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction ILocationForPublication;
      interaction IPublicationForPublicationMetaData;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataForPublication
      #pragma mark

      interaction IPublicationMetaDataForPublication
      {
        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataForPublicationRepository
      #pragma mark

      interaction IPublicationMetaDataForPublicationRepository
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationMetaDataForPublicationRepository, ForPublicationRepository)

        typedef IPublicationMetaData::Encodings Encodings;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

        static ElementPtr toDebug(ForPublicationRepositoryPtr metaData);

        static ForPublicationRepositoryPtr create(
                                                  ULONG version,
                                                  ULONG baseVersion,
                                                  ULONG lineage,
                                                  LocationPtr creatorLocation,
                                                  const char *name,
                                                  const char *mimeType,
                                                  Encodings encoding,
                                                  const PublishToRelationshipsMap &relationships,
                                                  LocationPtr publishedLocation,
                                                  Time expires = Time()
                                                  );

        static ForPublicationRepositoryPtr createFrom(IPublicationMetaDataPtr metaData);

        static ForPublicationRepositoryPtr createForSource(LocationPtr location);

        virtual PublicationMetaDataPtr toPublicationMetaData() const = 0;
        virtual IPublicationPtr toPublication() const = 0;

        virtual LocationPtr getCreatorLocation(bool internal = true) const = 0;

        virtual String getName() const = 0;

        virtual ULONG getVersion() const = 0;
        virtual ULONG getBaseVersion() const = 0;
        virtual ULONG getLineage() const = 0;

        virtual Time getExpires() const = 0;

        virtual LocationPtr getPublishedLocation(bool internal = true) const = 0;

        virtual const PublishToRelationshipsMap &getRelationships() const = 0;

        virtual bool isMatching(
                                const IPublicationMetaDataPtr &metaData,
                                bool ignoreLineage = false
                                ) const = 0;

        virtual bool isLessThan(
                                const IPublicationMetaDataPtr &metaData,
                                bool ignoreLineage = false
                                ) const = 0;

        virtual void setVersion(ULONG version) = 0;
        virtual void setBaseVersion(ULONG version) = 0;

        virtual void setCreatorLocation(LocationPtr location) = 0;
        virtual void setPublishedLocation(LocationPtr location) = 0;

        virtual void setExpires(Time expires) = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataForMessages
      #pragma mark

      interaction IPublicationMetaDataForMessages
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationMetaDataForMessages, ForMessages)

        typedef IPublicationMetaData::Encodings Encodings;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

        static ForMessagesPtr create(
                                     ULONG version,
                                     ULONG baseVersion,
                                     ULONG lineage,
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     Encodings encoding,
                                     const PublishToRelationshipsMap &relationships,
                                     LocationPtr publishedLocation,
                                     Time expires = Time()
                                     );

        virtual ~IPublicationMetaDataForMessages() {} // NOTE: to make polymophic - remove if virtual method is needed
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PublicationMetaData
      #pragma mark

      class PublicationMetaData : public Noop,
                                  public IPublicationMetaData,
                                  public IPublicationMetaDataForPublication,
                                  public IPublicationMetaDataForPublicationRepository,
                                  public IPublicationMetaDataForMessages
      {
      public:
        friend interaction IPublicationMetaDataFactory;
        friend interaction IPublicationMetaData;
        friend interaction IPublicationMetaDataForPublicationRepository;

        ZS_DECLARE_TYPEDEF_PTR(ILocationForPublication, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForPublicationMetaData, UsePublication)

        typedef IPublicationMetaData::Encodings Encodings;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

      protected:
        PublicationMetaData(
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
                            );
        PublicationMetaData(Noop) : Noop(true) {}

        void init();

      public:
        ~PublicationMetaData();

        static PublicationMetaDataPtr convert(IPublicationMetaDataPtr publication);
        static PublicationMetaDataPtr convert(ForPublicationRepositoryPtr publication);
        static PublicationMetaDataPtr convert(ForMessagesPtr publication);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => IPublicationMetaData
        #pragma mark

        static ElementPtr toDebug(IPublicationMetaDataPtr metaData);

        virtual PUID getID() const {return mID;}

        virtual IPublicationPtr toPublication() const;

        virtual ILocationPtr getCreatorLocation() const;

        virtual String getName() const;
        virtual String getMimeType() const;

        virtual ULONG getVersion() const;
        virtual ULONG getBaseVersion() const;
        virtual ULONG getLineage() const;

        virtual Encodings getEncoding() const;

        virtual Time getExpires() const;

        virtual ILocationPtr getPublishedLocation() const;

        virtual const PublishToRelationshipsMap &getRelationships() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => IPublicationMetaDataForPublication
        #pragma mark

        // (duplicate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => IPublicationMetaDataForPublicationRepository
        #pragma mark

        static PublicationMetaDataPtr create(
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
                                             );

        virtual PublicationMetaDataPtr toPublicationMetaData() const;
        // (duplicate) virtual IPublicationPtr toPublication() const;

        virtual LocationPtr getCreatorLocation(bool) const;

        // (duplicate) virtual String getName() const;

        // (duplicate) virtual ULONG getVersion() const;
        // (duplicate) virtual ULONG getBaseVersion() const;
        // (duplicate) virtual ULONG getLineage() const;

        // (duplicate) virtual Time getExpires() const;

        virtual LocationPtr getPublishedLocation(bool) const;

        // (duplicate) virtual const PublishToRelationshipsMap &getRelationships() const;

        virtual bool isMatching(
                                const IPublicationMetaDataPtr &metaData,
                                bool ignoreLineage = false
                                ) const;

        virtual bool isLessThan(
                                const IPublicationMetaDataPtr &metaData,
                                bool ignoreLineage = false
                                ) const;

        virtual void setVersion(ULONG version);
        virtual void setBaseVersion(ULONG version);

        virtual void setCreatorLocation(LocationPtr location);
        virtual void setPublishedLocation(LocationPtr location);

        virtual void setExpires(Time expires);

        virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => IPublicationMetaDataForMessages
        #pragma mark

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationMetaData => (data)
        #pragma mark

        PUID mID;
        mutable RecursiveLock mLock;
        PublicationMetaDataWeakPtr mThisWeak;

        UsePublicationPtr mPublication;

        LocationPtr mCreatorLocation;

        String mName;
        String mMimeType;

        ULONG mVersion;
        ULONG mBaseVersion;
        ULONG mLineage;

        Encodings mEncoding;

        Time mExpires;

        LocationPtr mPublishedLocation;

        PublishToRelationshipsMap mPublishedRelationships;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataFactory
      #pragma mark

      interaction IPublicationMetaDataFactory
      {
        typedef IPublicationMetaData::Encodings Encodings;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

        static IPublicationMetaDataFactory &singleton();

        virtual PublicationMetaDataPtr creatPublicationMetaData(
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
                                                                );
      };
      
    }
  }
}
