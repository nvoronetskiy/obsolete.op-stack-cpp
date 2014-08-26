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
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/IPublication.h>

#include <zsLib/Exception.h>
#include <zsLib/Timer.h>

#include <boost/shared_array.hpp>

#define OPENPEER_STACK_SETTING_PUBLICATION_MOVE_DOCUMENT_TO_CACHE_TIME "openpeer/stack/move-publication-to-cache-time-in-seconds"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction ILocationForPublication;
      interaction IPublicationMetaDataForPublication;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationForPublicationMetaData
      #pragma mark

      interaction IPublicationForPublicationMetaData
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForPublicationMetaData, ForPublicationMetaData)

        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;
        typedef IPublicationMetaData::Encodings Encodings;

        virtual LocationPtr getCreatorLocation(bool internal = true) const = 0;

        virtual String getName() const = 0;
        virtual String getMimeType() const = 0;

        virtual ULONG getVersion() const = 0;
        virtual ULONG getBaseVersion() const = 0;
        virtual ULONG getLineage() const = 0;

        virtual Encodings getEncoding() const = 0;

        virtual Time getExpires() const = 0;

        virtual LocationPtr getPublishedLocation(bool internal = true) const = 0;

        virtual const PublishToRelationshipsMap &getRelationships() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationForPublicationRepository
      #pragma mark

      interaction IPublicationForPublicationRepository : public IPublicationMetaDataForPublicationRepository
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForPublicationRepository, ForPublicationRepository)

        struct Exceptions
        {
          ZS_DECLARE_CUSTOM_EXCEPTION(VersionMismatch)
        };

        typedef IPublication::RelationshipListPtr RelationshipListPtr;

        static ElementPtr toDebug(ForPublicationRepositoryPtr publication);

        virtual PUID getID() const = 0;

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

        virtual DocumentPtr getJSON(IPublicationLockerPtr &outPublicationLock) const = 0;

        virtual RelationshipListPtr getAsContactList() const = 0;

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

        virtual Time getCacheExpires() const = 0;
        virtual void setCacheExpires(Time expires) = 0;

        virtual void updateFromFetchedPublication(
                                                  PublicationPtr fetchedPublication,
                                                  bool *noThrowVersionMismatched = NULL
                                                  ) throw (Exceptions::VersionMismatch) = 0;

        virtual void getDiffVersionsOutputSize(
                                               ULONG fromVersionNumber,
                                               ULONG toVersionNumber,
                                               size_t &outOutputSizeInBytes,
                                               bool rawSizeOkay = true
                                               ) const = 0;

        virtual void getEntirePublicationOutputSize(
                                                    size_t &outOutputSizeInBytes,
                                                    bool rawSizeOkay = true
                                                    ) const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationForMessages
      #pragma mark

      interaction IPublicationForMessages : public IPublicationMetaDataForMessages
      {
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForMessages, ForMessages)

        typedef IPublicationMetaData::Encodings Encodings;
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

        static ForMessagesPtr create(
                                     ULONG version,
                                     ULONG baseVersion,
                                     ULONG lineage,
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     ElementPtr dataEl,
                                     Encodings encoding,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     LocationPtr publishedLocation,
                                     Time expires = Time()
                                     );

        virtual ULONG getVersion() const = 0;
        virtual ULONG getBaseVersion() const = 0;

        virtual PublicationMetaDataPtr toPublicationMetaData() const = 0;

        virtual NodePtr getDiffs(
                                 ULONG &ioFromVersion,
                                 ULONG toVersion
                                 ) const = 0;

      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication
      #pragma mark

      class Publication : public PublicationMetaData,
                          public IPublication,
                          public IPublicationForPublicationMetaData,
                          public IPublicationForPublicationRepository,
                          public IPublicationForMessages
      {
      public:
        friend interaction IPublicationFactory;
        friend interaction IPublication;
        friend interaction IPublicationForPublicationRepository;

        ZS_DECLARE_TYPEDEF_PTR(IPublicationForPublicationRepository::ForPublicationRepository, ForPublicationRepository)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationForMessages::ForMessages, ForMessages)

        ZS_DECLARE_TYPEDEF_PTR(ILocationForPublication, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(IPublicationMetaDataForPublication, UsePublicationMetaData)

        typedef IPublication::Encodings Encodings;
        typedef IPublication::RelationshipListPtr RelationshipListPtr;
        typedef IPublication::PublishToRelationshipsMap PublishToRelationshipsMap;

        ZS_DECLARE_CLASS_PTR(CacheableDocument)
        ZS_DECLARE_TYPEDEF_PTR(CacheableDocument, DiffDocument)

        ZS_DECLARE_CLASS_PTR(PublicationLock)

        typedef ULONG VersionNumber;

        typedef std::map<VersionNumber, DiffDocumentPtr> DiffDocumentMap;

      protected:
        Publication(
                    LocationPtr creatorLocation,
                    const char *name,
                    const char *mimeType,
                    const PublishToRelationshipsMap &publishToRelationships,
                    LocationPtr publishedLocation,
                    Time expires
                    );

        Publication(Noop) : PublicationMetaData(Noop(true)) {}

        void init();

      public:
        ~Publication();

        static PublicationPtr convert(IPublicationPtr publication);
        static PublicationPtr convert(ForPublicationMetaDataPtr publication);
        static PublicationPtr convert(ForPublicationRepositoryPtr publication);
        static PublicationPtr convert(ForMessagesPtr publication);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => IPublicationMetaData
        #pragma mark

        static ElementPtr toDebug(IPublicationPtr publication);

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
        #pragma mark Publication => IPublication
        #pragma mark

        static PublicationPtr create(
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     const SecureByteBlock &data,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     LocationPtr publishedLocation,
                                     Time expires = Time()
                                     );

        static PublicationPtr create(
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     DocumentPtr documentToBeAdopted,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     LocationPtr publishedLocation,
                                     Time expires = Time()
                                     );

        static PublicationPtr create(
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     const RelationshipList &relationshipsDocument,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     LocationPtr publishedLocation,
                                     Time expires = Time()
                                     );

        virtual void update(const SecureByteBlock &data);

        virtual void update(DocumentPtr updatedDocumentToBeAdopted);

        virtual void update(const RelationshipList &relationships);

        virtual SecureByteBlockPtr getRawData(IPublicationLockerPtr &outPublicationLock) const;

        virtual DocumentPtr getJSON(IPublicationLockerPtr &outPublicationLock) const;

        virtual RelationshipListPtr getAsContactList() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => IPublicationForPublicationMetaData
        #pragma mark

        // (duplicate) virtual LocationPtr getCreatorLocation(bool internal = true) const;

        // (duplicate) virtual String getName() const;
        // (duplicate) virtual String getMimeType() const;

        // (duplicate) virtual ULONG getVersion() const;
        // (duplicate) virtual ULONG getBaseVersion() const;
        // (duplicate) virtual ULONG getLineage() const;

        // (duplicate) virtual Encodings getEncoding() const;
        // (duplicate) virtual Time getExpires() const;

        // (duplicate) virtual LocationPtr getPublishedLocation(bool internal = true) const;

        // (duplicate) virtual const PublishToRelationshipsMap &getRelationships() const;

        // (duplicate) virtual RelationshipListPtr getAsContactList() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => IPublicationForPublicationRepository
        #pragma mark

      protected:
        // (duplicate) virtual PUID getID() const {return mID;}

        virtual PublicationMetaDataPtr toPublicationMetaData() const;
        // (duplicate) virtual IPublicationPtr toPublication() const;

        virtual LocationPtr getCreatorLocation(bool) const;

        // (duplicate) virtual String getName() const;

        // (duplicate) virtual ULONG getVersion() const;
        // (duplicate) virtual ULONG getBaseVersion() const;
        // (duplicate) virtual ULONG getLineage() const;

        // (duplicate) virtual Time getExpires() const;

        virtual LocationPtr getPublishedLocation(bool) const;

        // (duplicate) virtual DocumentPtr getJSON(IPublicationLockerPtr &outPublicationLock) const;

        // (duplicate) virtual RelationshipListPtr getAsContactList() const;

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

        virtual Time getCacheExpires() const;
        virtual void setCacheExpires(Time expires);

        virtual void updateFromFetchedPublication(
                                                  PublicationPtr fetchedPublication,
                                                  bool *noThrowVersionMismatched = NULL
                                                  ) throw (Exceptions::VersionMismatch);

        virtual void getDiffVersionsOutputSize(
                                               ULONG fromVersionNumber,
                                               ULONG toVersionNumber,
                                               size_t &outOutputSizeInBytes,
                                               bool rawSizeOkay = true
                                               ) const;

        virtual void getEntirePublicationOutputSize(
                                                    size_t &outOutputSizeInBytes,
                                                    bool rawSizeOkay = true
                                                    ) const;

        virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => IPublicationForMessages
        #pragma mark

      protected:
        static PublicationPtr create(
                                     ULONG version,
                                     ULONG baseVersion,
                                     ULONG lineage,
                                     LocationPtr creatorLocation,
                                     const char *name,
                                     const char *mimeType,
                                     ElementPtr dataEl,
                                     Encodings encoding,
                                     const PublishToRelationshipsMap &publishToRelationships,
                                     LocationPtr publishedLocation,
                                     Time expires
                                     );


        // (duplicate) virtual ULONG getVersion() const;
        // (duplicate) virtual ULONG getBaseVersion() const;

        // (duplicate) virtual PublicationMetaDataPtr toPublicationMetaData() const;

        virtual NodePtr getDiffs(
                                 ULONG &ioFromVersion,
                                 ULONG toVersion
                                 ) const;

      public:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication::CacheableDocument
        #pragma mark

        class CacheableDocument : public MessageQueueAssociator,
                                  public SharedRecursiveLock,
                                  public ITimerDelegate
        {
        protected:
          CacheableDocument(
                            const SharedRecursiveLock &lock,
                            DocumentPtr document
                            );

          void init();

        public:
          ~CacheableDocument();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Publication::CacheableDocument => friend Publication
          #pragma mark

          static CacheableDocumentPtr create(DocumentPtr document);

          size_t getOutputSize() const;
          DocumentPtr getDocument(AutoRecursiveLockPtr &outDocumentLock) const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Publication::CacheableDocument => ITimerDelegate
          #pragma mark

          void onTimer(TimerPtr timer);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Publication::CacheableDocument => (internal)
          #pragma mark

          void step();

          String getCookieName() const;
          void moveToCacheNow();

          Log::Params log(const char *message) const;

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark Publication::CacheableDocument => (data)
          #pragma mark

          AutoPUID mID;
          CacheableDocumentWeakPtr mThisWeak;

          Duration mMoveToCacheDuration;

          mutable DocumentPtr mDocument;
          mutable size_t mOutputSize;

          mutable bool mPreviouslyStored {};
          mutable TimerPtr mMoveToCacheTimer;
        };

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication::PublicationLock
        #pragma mark

        class PublicationLock : public IPublicationLocker
        {
        public:
          static PublicationLockPtr create(
                                           IPublicationPtr publication,
                                           AutoRecursiveLockPtr autoRecursiveLock,
                                           CacheableDocumentPtr cacheableDocument
                                           )
          {
            PublicationLockPtr pThis(new PublicationLock);
            pThis->mPublication = publication;
            pThis->mAutoLock = autoRecursiveLock;
            pThis->mDocument = cacheableDocument;
            return pThis;
          }

        private:
          IPublicationPtr mPublication;
          AutoRecursiveLockPtr mAutoLock;
          CacheableDocumentPtr mDocument;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        void logDocument() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Publication => (data)
        #pragma mark

        PublicationWeakPtr mThisWeakPublication;

        SecureByteBlockPtr mData;

        CacheableDocumentPtr mDocument;

        Time mCacheExpires;

        DiffDocumentMap mDiffDocuments;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationFactory
      #pragma mark

      interaction IPublicationFactory
      {
        typedef IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;
        typedef IPublicationMetaData::Encodings Encodings;

        static IPublicationFactory &singleton();

        virtual PublicationPtr create(
                                      LocationPtr creatorLocation,
                                      const char *name,
                                      const char *mimeType,
                                      const SecureByteBlock &data,
                                      const PublishToRelationshipsMap &publishToRelationships,
                                      LocationPtr publishedLocation,
                                      Time expires = Time()
                                      );

        virtual PublicationPtr create(
                                      LocationPtr creatorLocation,
                                      const char *name,
                                      const char *mimeType,
                                      DocumentPtr documentToBeAdopted,
                                      const PublishToRelationshipsMap &publishToRelationships,
                                      LocationPtr publishedLocation,
                                      Time expires = Time()
                                      );

        virtual PublicationPtr create(
                                      ULONG version,
                                      ULONG baseVersion,
                                      ULONG lineage,
                                      LocationPtr creatorLocation,
                                      const char *name,
                                      const char *mimeType,
                                      ElementPtr dataEl,
                                      Encodings encoding,
                                      const PublishToRelationshipsMap &publishToRelationships,
                                      LocationPtr publishedLocation,
                                      Time expires
                                      );
      };

      class PublicationFactory : public IFactory<IPublicationFactory> {};
      
    }
  }
}
