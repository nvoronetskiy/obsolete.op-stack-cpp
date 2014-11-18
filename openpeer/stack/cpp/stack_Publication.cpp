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

#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Diff.h>
#include <openpeer/stack/internal/stack_Location.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/ICache.h>
#include <openpeer/stack/IPeer.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>
#include <zsLib/Log.h>
#include <zsLib/helpers.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    using message::IMessageHelper;

    typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

    namespace internal
    {
      using services::IHelper;
      typedef IPublicationForMessages::ForMessagesPtr ForMessagesPtr;

      ZS_DECLARE_USING_PTR(services, ISettings)

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static DocumentPtr createDocumentFromRelationships(const IPublication::RelationshipList &relationships)
      {
        typedef IPublication::RelationshipList RelationshipList;

        DocumentPtr doc = Document::create();
        ElementPtr contactsEl = Element::create();
        contactsEl->setValue("contacts");

        for (RelationshipList::const_iterator iter = relationships.begin(); iter != relationships.end(); ++iter)
        {
          const String contact = (*iter);

          ElementPtr contactEl = IMessageHelper::createElementWithTextAndJSONEncode("contact", contact);
          contactsEl->adoptAsLastChild(contactEl);
        }

        doc->adoptAsLastChild(contactsEl);

        return doc;
      }

      //-----------------------------------------------------------------------
      static String toString(NodePtr node)
      {
        if (!node) return "(null)";

        GeneratorPtr generator = Generator::createJSONGenerator();

        std::unique_ptr<char[]> output;
        size_t length = 0;
        output = generator->write(node, &length);

        return (CSTR)output.get();
      }

      //-----------------------------------------------------------------------
      class Publication_UniqueLineage
      {
      public:
        Publication_UniqueLineage() :
          mUnique(0)
        {
        }

        ~Publication_UniqueLineage() {}

        static ULONG getUniqueLineage()
        {
          return singleton().actualGetUniqueLineage();
        }

      protected:
        static Publication_UniqueLineage &singleton()
        {
          static Singleton<Publication_UniqueLineage, false> singleton;  // non-destructable object
          return singleton.singleton();
        }

        ULONG actualGetUniqueLineage()
        {
          AutoRecursiveLock lock(mLock);

          Seconds duration = zsLib::timeSinceEpoch<Seconds>(zsLib::now());

          ULONG proposed = (ULONG)(duration.count());
          if (proposed <= mUnique) {
            proposed = mUnique + 1;
          }
          mUnique = proposed;
          return mUnique;
        }

      private:
        mutable RecursiveLock mLock;
        ULONG mUnique;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPublicationForPublicationRepository::toDebug(ForPublicationRepositoryPtr publication)
      {
        return Publication::toDebug(Publication::convert(publication));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      ForMessagesPtr IPublicationForMessages::create(
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
                                                     )
      {
        return IPublicationFactory::singleton().create(
                                                       version,
                                                       baseVersion,
                                                       lineage,
                                                       creatorLocation,
                                                       name,
                                                       mimeType,
                                                       dataEl,
                                                       encoding,
                                                       publishToRelationships,
                                                       publishedLocation,
                                                       expires
                                                       );
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication
      #pragma mark

      //-----------------------------------------------------------------------
      Publication::Publication(
                               LocationPtr creatorLocation,
                               const char *name,
                               const char *mimeType,
                               const PublishToRelationshipsMap &publishToRelationships,
                               LocationPtr publishedLocation,
                               Time expires
                               ) :
        PublicationMetaData(
                            1,
                            0,
                            Publication_UniqueLineage::getUniqueLineage(),
                            creatorLocation,
                            name,
                            mimeType,
                            Encoding_JSON,  // this will have to be ignored
                            publishToRelationships,
                            publishedLocation,
                            expires
                            )
      {
        ZS_LOG_DEBUG(debug("created"))
      }

      //-----------------------------------------------------------------------
      void Publication::init()
      {
        ZS_LOG_DEBUG(debug("init"))
        logDocument();
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::convert(IPublicationPtr publication)
      {
        return ZS_DYNAMIC_PTR_CAST(Publication, publication);
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::convert(ForPublicationMetaDataPtr publication)
      {
        return ZS_DYNAMIC_PTR_CAST(Publication, publication);
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::convert(ForPublicationRepositoryPtr publication)
      {
        return ZS_DYNAMIC_PTR_CAST(Publication, publication);
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::convert(ForMessagesPtr publication)
      {
        return ZS_DYNAMIC_PTR_CAST(Publication, publication);
      }

      //-----------------------------------------------------------------------
      Publication::~Publication()
      {
        if (isNoop()) return;

        mThisWeakPublication.reset();
        ZS_LOG_DEBUG(debug("destroyed"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication => IPublicationMetaData
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationPtr Publication::toPublication() const
      {
        return mThisWeakPublication.lock();
      }

      //-----------------------------------------------------------------------
      ILocationPtr Publication::getCreatorLocation() const
      {
        AutoRecursiveLock lock(*this);
        return mCreatorLocation;
      }

      //-----------------------------------------------------------------------
      String Publication::getName() const
      {
        AutoRecursiveLock lock(*this);
        return mName;
      }

      //-----------------------------------------------------------------------
      String Publication::getMimeType() const
      {
        AutoRecursiveLock lock(*this);
        return mMimeType;
      }

      //-----------------------------------------------------------------------
      ULONG Publication::getVersion() const
      {
        AutoRecursiveLock lock(*this);
        return mVersion;
      }

      //-----------------------------------------------------------------------
      ULONG Publication::getBaseVersion() const
      {
        AutoRecursiveLock lock(*this);
        return mBaseVersion;
      }

      //-----------------------------------------------------------------------
      ULONG Publication::getLineage() const
      {
        AutoRecursiveLock lock(*this);
        return mLineage;
      }

      //-----------------------------------------------------------------------
      IPublication::Encodings Publication::getEncoding() const
      {
        AutoRecursiveLock lock(*this);
        if (mDocument) {
          return Encoding_JSON;
        }
        return Encoding_Binary;
      }

      //-----------------------------------------------------------------------
      Time Publication::getExpires() const
      {
        AutoRecursiveLock lock(*this);
        return mExpires;
      }

      //-----------------------------------------------------------------------
      ILocationPtr Publication::getPublishedLocation() const
      {
        AutoRecursiveLock lock(*this);
        return mPublishedLocation;
      }

      //-----------------------------------------------------------------------
      const IPublicationMetaData::PublishToRelationshipsMap &Publication::getRelationships() const
      {
        AutoRecursiveLock lock(*this);
        return mPublishedRelationships;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication => IPublication
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Publication::toDebug(IPublicationPtr publication)
      {
        if (!publication) return ElementPtr();
        return Publication::convert(publication)->toDebug();
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::create(
                                         LocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         const SecureByteBlock &data,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         LocationPtr publishedLocation,
                                         Time expires
                                         )
      {
        PublicationPtr pThis(new Publication(creatorLocation, name, mimeType, publishToRelationships, publishedLocation, expires));
        pThis->mThisWeak = pThis;
        pThis->mThisWeakPublication = pThis;
        pThis->mPublication = pThis;
        pThis->mData = SecureByteBlockPtr(new SecureByteBlock(data.SizeInBytes()));
        memcpy(*(pThis->mData), data, data.SizeInBytes());
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::create(
                                         LocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         DocumentPtr documentToBeAdopted,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         LocationPtr publishedLocation,
                                         Time expires
                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!documentToBeAdopted)

        PublicationPtr pThis(new Publication(creatorLocation, name, mimeType, publishToRelationships, publishedLocation, expires));
        pThis->mThisWeak = pThis;
        pThis->mThisWeakPublication = pThis;
        pThis->mPublication = pThis;
        pThis->mDocument = CacheableDocument::create(documentToBeAdopted);
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PublicationPtr Publication::create(
                                         LocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         const RelationshipList &relationshipsDocument,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         LocationPtr publishedLocation,
                                         Time expires
                                         )
      {
        DocumentPtr doc = createDocumentFromRelationships(relationshipsDocument);
        return IPublicationFactory::singleton().create(creatorLocation, name, mimeType, doc, publishToRelationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      void Publication::update(const SecureByteBlock &data)
      {
        AutoRecursiveLock lock(*this);

        mData = SecureByteBlockPtr(new SecureByteBlock(data.SizeInBytes()));
        memcpy(*mData, data, data.SizeInBytes());

        mDiffDocuments.clear();
        mDocument.reset();

        ++mVersion;
      }

      //-----------------------------------------------------------------------
      void Publication::update(DocumentPtr updatedDocumentToBeAdopted)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!updatedDocumentToBeAdopted)

        AutoRecursiveLock lock(*this);
        ZS_LOG_DETAIL(debug("updating document"))
        if (ZS_IS_LOGGING(Trace)) {
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("Updating from JSON") + ZS_PARAM("json", internal::toString(updatedDocumentToBeAdopted)))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
        }

        ++mVersion;

        mData.reset();

        ElementPtr diffElem = updatedDocumentToBeAdopted->findFirstChildElement(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME); // root for diffs elements
        if (!diffElem) {
          mDiffDocuments.clear();

          mDocument = CacheableDocument::create(updatedDocumentToBeAdopted);
          updatedDocumentToBeAdopted.reset(); // document has been adopted
          return;
        }

        if (mDiffDocuments.end() == mDiffDocuments.find(mVersion-1)) {
          if (mVersion > 2) {
            ZS_LOG_WARNING(Detail, log("diff document does not contain version in a row"))
          }
          // if diffs are not in a row then erase the diffs...
          mDiffDocuments.clear();
        }

        DocumentPtr doc;

        {
          AutoRecursiveLockPtr docLock;
          doc = mDocument->getDocument(docLock)->clone()->toDocument();
        }

        // this is a difference document
        IDiff::process(doc, updatedDocumentToBeAdopted);

        ZS_LOG_DEBUG(debug("updating document complete"))

        if (ZS_IS_LOGGING(Trace)) {
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv"))
          ZS_LOG_DEBUG(log("FINAL JSON") + ZS_PARAM("json", internal::toString(doc)))
          ZS_LOG_DEBUG(log("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
        }

        mDocument = CacheableDocument::create(doc);
        doc.reset();  // document has been adopted

        mDiffDocuments[mVersion] = CacheableDocument::create(updatedDocumentToBeAdopted);
        updatedDocumentToBeAdopted.reset(); // document has been adopted
      }

      //-----------------------------------------------------------------------
      void Publication::update(const RelationshipList &relationships)
      {
        DocumentPtr document = createDocumentFromRelationships(relationships);
        update(document);
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Publication::getRawData(IPublicationLockerPtr &outPublicationLock) const
      {
        outPublicationLock = PublicationLock::create(
                                                     mThisWeakPublication.lock(),
                                                     AutoRecursiveLockPtr(new AutoRecursiveLock(*this)),
                                                     CacheableDocumentPtr()
                                                     );
        return mData;
      }

      //-----------------------------------------------------------------------
      DocumentPtr Publication::getJSON(IPublicationLockerPtr &outPublicationLock) const
      {
        AutoRecursiveLockPtr docLock;
        DocumentPtr doc = mDocument->getDocument(docLock);
        outPublicationLock = PublicationLock::create(
                                                     mThisWeakPublication.lock(),
                                                     docLock,
                                                     mDocument
                                                     );
        return doc;
      }

      //-----------------------------------------------------------------------
      IPublication::RelationshipListPtr Publication::getAsContactList() const
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(debug("getting publication as contact list"))

        RelationshipListPtr result = RelationshipListPtr(new RelationshipList);

        RelationshipList &outList = (*result);

        if (!mDocument) {
          ZS_LOG_WARNING(Detail, debug("publication document is empty"))
          return result;
        }

        AutoRecursiveLockPtr docLock;
        DocumentPtr doc = mDocument->getDocument(docLock);

        ElementPtr contactsEl = doc->findFirstChildElement("contacts");
        if (!contactsEl) {
          ZS_LOG_WARNING(Debug, debug("unable to find contact root element"))
          return result;
        }

        ElementPtr contactEl = contactsEl->findFirstChildElement("contact");
        while (contactEl) {
          String contact = contactEl->getTextDecoded();
          if (!contact.isEmpty()) {
            ZS_LOG_TRACE(log("found contact") + ZS_PARAM("contact URI", contact))
            outList.push_back(contact);
          }
          contactEl = contactEl->findNextSiblingElement("contact");
        }

        ZS_LOG_TRACE(log("end of getting as contact list") + ZS_PARAM("total", outList.size()))
        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication => IPublicationForPublicationRepository
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr Publication::toPublicationMetaData() const
      {
        return mThisWeak.lock();
      }

      //-----------------------------------------------------------------------
      LocationPtr Publication::getCreatorLocation(bool) const
      {
        AutoRecursiveLock lock(*this);
        return mCreatorLocation;
      }

      //-----------------------------------------------------------------------
      LocationPtr Publication::getPublishedLocation(bool) const
      {
        AutoRecursiveLock lock(*this);
        return mPublishedLocation;
      }

      //-----------------------------------------------------------------------
      bool Publication::isMatching(
                                   const IPublicationMetaDataPtr &metaData,
                                   bool ignoreLineage
                                   ) const
      {
        AutoRecursiveLock lock(*this);
        return PublicationMetaData::isMatching(metaData, ignoreLineage);
      }

      //-----------------------------------------------------------------------
      bool Publication::isLessThan(
                                   const IPublicationMetaDataPtr &metaData,
                                   bool ignoreLineage
                                   ) const
      {
        AutoRecursiveLock lock(*this);
        return PublicationMetaData::isLessThan(metaData, ignoreLineage);
      }

      //-----------------------------------------------------------------------
      void Publication::setVersion(ULONG version)
      {
        AutoRecursiveLock lock(*this);
        mVersion = version;
      }

      //-----------------------------------------------------------------------
      void Publication::setBaseVersion(ULONG version)
      {
        AutoRecursiveLock lock(*this);
        mBaseVersion = version;
      }

      //-----------------------------------------------------------------------
      void Publication::setCreatorLocation(LocationPtr location)
      {
        AutoRecursiveLock lock(*this);
        mCreatorLocation = location;

        ZS_LOG_TRACE(debug("updated internal publication creator information"))
      }

      //-----------------------------------------------------------------------
      void Publication::setPublishedLocation(LocationPtr location)
      {
        AutoRecursiveLock lock(*this);
        mPublishedLocation = location;

        ZS_LOG_TRACE(debug("updated internal publication published to contact information"))
      }

      //-----------------------------------------------------------------------
      void Publication::setExpires(Time expires)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("updating expires time") + ZS_PARAM("was", mExpires) + ZS_PARAM("now", expires))

        mExpires = expires;
      }

      //-----------------------------------------------------------------------
      Time Publication::getCacheExpires() const
      {
        AutoRecursiveLock lock(*this);
        return mCacheExpires;
      }

      //-----------------------------------------------------------------------
      void Publication::setCacheExpires(Time expires)
      {
        AutoRecursiveLock lock(*this);
        mCacheExpires = expires;
      }

      //-----------------------------------------------------------------------
      void Publication::updateFromFetchedPublication(
                                                     PublicationPtr fetchedPublication,
                                                     bool *noThrowVersionMismatched
                                                     ) throw (Exceptions::VersionMismatch)
      {
        if (NULL != noThrowVersionMismatched)
          *noThrowVersionMismatched = false;

        AutoRecursiveLock lock(*this);

        ZS_LOG_DETAIL(debug("updating from fetched publication"))

        PublicationPtr publication = fetchedPublication;

        if (publication->mData) {
          mData = publication->mData;
          mDocument.reset();
        } else {
          AutoRecursiveLockPtr pubDocLock;
          DocumentPtr pubDoc = publication->mDocument->getDocument(pubDocLock);

          ElementPtr diffEl = pubDoc->findFirstChildElement(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME);
          if (diffEl) {
            if (publication->mBaseVersion != mVersion + 1) {
              if (NULL != noThrowVersionMismatched) {
                *noThrowVersionMismatched = true;
                return;
              }
              ZS_THROW_CUSTOM(Exceptions::VersionMismatch, debug("remote party sent diff based on wrong document"))
              return;
            }

            DocumentPtr doc;

            {
              AutoRecursiveLockPtr docLock;
              doc = mDocument->getDocument(docLock)->clone()->toDocument();
            }

            IDiff::process(doc, pubDoc);

            mDocument = CacheableDocument::create(doc);
            doc.reset();  // document is adopted
          } else {
            mDocument = publication->mDocument;
          }
        }

        mCreatorLocation = publication->mCreatorLocation;

        mMimeType = publication->mMimeType;
        mVersion = publication->mVersion;
        mBaseVersion = 0;
        mLineage = publication->mLineage;

        mExpires = publication->mExpires;

        mPublishedLocation = publication->mPublishedLocation;

        mPublishedRelationships = publication->mPublishedRelationships;

        mDiffDocuments.clear();

        logDocument();

        ZS_LOG_DEBUG(debug("updating from fetched publication complete"))
      }

      //-----------------------------------------------------------------------
      void Publication::getDiffVersionsOutputSize(
                                                  ULONG fromVersionNumber,
                                                  ULONG toVersionNumber,
                                                  size_t &outOutputSizeInBytes,
                                                  bool rawSizeOkay
                                                  ) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(fromVersionNumber > toVersionNumber)

        AutoRecursiveLock lock(*this);
        outOutputSizeInBytes = 0;

        if (!mDocument) {
          if (!mData) {
            ZS_LOG_WARNING(Detail, debug("no data available to return"))
            return;
          }

          if (rawSizeOkay) {
            ZS_LOG_TRACE(debug("document is binary data thus returning non-encoded raw size (no base64 calculating required)"))
            outOutputSizeInBytes = mData->SizeInBytes();
            return;
          }

          ZS_LOG_TRACE(debug("document is binary data thus returning base64 bit encoded size (which required calculating)"))

          ULONG fromVersion = 0;
          NodePtr node = getDiffs(fromVersion, mVersion);

          GeneratorPtr generator =  Generator::createJSONGenerator();
          outOutputSizeInBytes = generator->getOutputSize(node);
          return;
        }

        for (ULONG from = fromVersionNumber; from <= toVersionNumber; ++from) {
          DiffDocumentMap::const_iterator found = mDiffDocuments.find(from);
          if (found == mDiffDocuments.end()) {
            ZS_LOG_TRACE(debug("diff is not available (thus returning entire document size)") + ZS_PARAM("from", fromVersionNumber) + ZS_PARAM("current", from))
            getEntirePublicationOutputSize(outOutputSizeInBytes);
            return;
          }

          ZS_LOG_TRACE(debug("returning size of latest version diff's document") + ZS_PARAM("from", fromVersionNumber) + ZS_PARAM("current", from))
          const DiffDocumentPtr &doc = (*found).second;
          outOutputSizeInBytes += doc->getOutputSize();
        }
      }

      //-----------------------------------------------------------------------
      void Publication::getEntirePublicationOutputSize(
                                                       size_t &outOutputSizeInBytes,
                                                       bool rawSizeOkay
                                                       ) const
      {
        outOutputSizeInBytes = 0;

        AutoRecursiveLock lock(*this);

        if (!mDocument) {
          getDiffVersionsOutputSize(0, 0, outOutputSizeInBytes);
          return;
        }

        outOutputSizeInBytes = mDocument->getOutputSize();
      }

      //-----------------------------------------------------------------------
      ElementPtr Publication::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("Publication");

        UseLocationPtr creatorLocation = Location::convert(getCreatorLocation());
        UseLocationPtr publishedLocation = Location::convert(getPublishedLocation());

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "version", mVersion);
        IHelper::debugAppend(resultEl, "base version", mBaseVersion);
        IHelper::debugAppend(resultEl, "lineage", mLineage);
        IHelper::debugAppend(resultEl, "creator", creatorLocation ? creatorLocation->toDebug() : ElementPtr());
        IHelper::debugAppend(resultEl, "published", publishedLocation ? publishedLocation->toDebug() : ElementPtr());
        IHelper::debugAppend(resultEl, "mime type", mMimeType);
        IHelper::debugAppend(resultEl, "expires",  mExpires);
        IHelper::debugAppend(resultEl, "data length", mData ? mData->SizeInBytes() : 0);
        IHelper::debugAppend(resultEl, "diffs total", mDiffDocuments.size());
        IHelper::debugAppend(resultEl, "total relationships", mPublishedRelationships.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication => IPublicationForMessages
      #pragma mark

      //-----------------------------------------------------------------------
      PublicationPtr Publication::create(
                                         ULONG version,
                                         ULONG baseVersion,
                                         ULONG lineage,
                                         LocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         ElementPtr dataEl,
                                         IPublicationMetaData::Encodings encoding,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         LocationPtr publishedLocation,
                                         Time expires
                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!dataEl)

        PublicationPtr pThis(new Publication(creatorLocation, name, mimeType, publishToRelationships, publishedLocation, expires));
        pThis->mThisWeak = pThis;
        pThis->mThisWeakPublication = pThis;
        pThis->mPublication = pThis;
        pThis->mVersion = version;
        pThis->mBaseVersion = baseVersion;
        pThis->mLineage = lineage;
        switch (encoding) {
          case IPublicationMetaData::Encoding_Binary: {
            String base64String = dataEl->getTextDecoded();
            pThis->mData = services::IHelper::convertFromBase64(base64String);
            break;
          }
          case IPublicationMetaData::Encoding_JSON: {
            DocumentPtr doc = Document::create();
            NodePtr node = dataEl->getFirstChild();
            while (node) {
              NodePtr next = node->getNextSibling();
              node->orphan();
              doc->adoptAsLastChild(node);
              node = next;
            }
            pThis->mDocument = CacheableDocument::create(doc);
            doc.reset();  // document has been adopted
            break;
          }
        }
        pThis->init();
        return pThis;
      }
      
      //-----------------------------------------------------------------------
      NodePtr Publication::getDiffs(
                                    ULONG &ioFromVersion,
                                    ULONG toVersion
                                    ) const
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("requested JSON diffs") + ZS_PARAM("from version", ioFromVersion) + ZS_PARAM("to version", toVersion))

        if ((0 == toVersion) ||
            (toVersion >= mVersion)) {
          toVersion = mVersion;
        }

        if (!mDocument) {
          if (!mData) {
            ZS_LOG_WARNING(Detail, debug("JSON for publishing has no data available to return"))
            return NodePtr();
          }

          ZS_LOG_WARNING(Detail, debug("returning data as base 64 encoded"))

          String data = services::IHelper::convertToBase64(*mData);
          TextPtr text = Text::create();
          text->setValue(data);
          return text;
        }

        if (0 == ioFromVersion) {
          ZS_LOG_DEBUG(debug("first time publishing or fetching document thus returning entire document"))

          AutoRecursiveLockPtr docLock;
          ElementPtr firstEl = mDocument->getDocument(docLock)->getFirstChildElement();
          if (!firstEl) return firstEl;
          return firstEl->clone();
        }

        ZS_LOG_DEBUG(debug("publishing or fetching differences from last version published") + ZS_PARAM("from base version", toVersion) + ZS_PARAM("to version", toVersion))

        // see if we have diffs from the last published point
        DiffDocumentMap::const_iterator foundFrom = mDiffDocuments.find(ioFromVersion);
        DiffDocumentMap::const_iterator foundTo = mDiffDocuments.find(toVersion);

        if ((foundFrom == mDiffDocuments.end()) ||
            (foundTo == mDiffDocuments.end())) {

          ZS_LOG_WARNING(Detail, log("unable to find JSON differences for requested publication (thus returning whole document)"))
          ioFromVersion = 0;
          return getDiffs(ioFromVersion, toVersion);
        }

        DocumentPtr cloned;

        ++foundTo;

        VersionNumber currentVersion = ioFromVersion;

        ElementPtr diffOutputEl;

        // we have the diffs, process them into one document
        for (DiffDocumentMap::const_iterator iter = foundFrom; iter != foundTo; ++iter, ++currentVersion) {
          const VersionNumber &versionNumber = (*iter).first;

          ZS_LOG_TRACE(log("processing diff") + ZS_PARAM("version", versionNumber))

          if (currentVersion != versionNumber) {
            ZS_LOG_ERROR(Detail, log("JSON differences has a version number hole") + ZS_PARAM("expecting", currentVersion) + ZS_PARAM("found", versionNumber))
            ioFromVersion = 0;
            return getDiffs(ioFromVersion, toVersion);
          }

          DiffDocumentPtr doc = (*iter).second;

          try {
            if (!cloned) {
              AutoRecursiveLockPtr docLock;
              cloned = doc->getDocument(docLock)->clone()->toDocument();
              diffOutputEl = cloned->findFirstChildElementChecked(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME);
            } else {
              diffOutputEl = cloned->findFirstChildElementChecked(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME);

              // we need to process all elements and insert them into the other...
              AutoRecursiveLockPtr docLock;
              ElementPtr diffEl = doc->getDocument(docLock)->findFirstChildElementChecked(OPENPEER_STACK_DIFF_DOCUMENT_ROOT_ELEMENT_NAME);

              ElementPtr itemEl = diffEl->findFirstChildElement(OPENPEER_STACK_DIFF_DOCUMENT_ITEM_ELEMENT_NAME);
              while (itemEl) {
                diffOutputEl->adoptAsLastChild(itemEl->clone());
                itemEl = itemEl->findNextSiblingElement(OPENPEER_STACK_DIFF_DOCUMENT_ITEM_ELEMENT_NAME);
              }
            }
          } catch (CheckFailed &) {
            ZS_LOG_ERROR(Detail, log("JSON diff document is corrupted (recovering by returning entire document)") + ZS_PARAM("version", versionNumber))
            ioFromVersion = 0;
            return getDiffs(ioFromVersion, toVersion);
          }
        }

        ZS_THROW_INVALID_ASSUMPTION_IF(!cloned)
        ZS_THROW_INVALID_ASSUMPTION_IF(!diffOutputEl)

        ZS_LOG_DEBUG(log("returning orphaned clone of differences document"))
        diffOutputEl->orphan();
        return diffOutputEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Publication::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("Publication");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Publication::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      void Publication::logDocument() const
      {
        if (ZS_IS_LOGGING(Trace)) {
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          if (mDocument) {
            AutoRecursiveLockPtr docLock;
            ZS_LOG_BASIC(log("publication contains JSON") + ZS_PARAM("json", internal::toString(mDocument->getDocument(docLock))))
          } else if (mData) {
            ZS_LOG_BASIC(log("publication contains binary data") + ZS_PARAM("length", mData ? mData->SizeInBytes() : 0))
          } else {
            ZS_LOG_BASIC(log("publication is NULL"))
          }
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
          ZS_LOG_DEBUG(log("..............................................................................."))
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication::CacheableDocument
      #pragma mark

      //-----------------------------------------------------------------------
      Publication::CacheableDocument::CacheableDocument(
                                                        const SharedRecursiveLock &lock,
                                                        DocumentPtr document
                                                        ) :
        MessageQueueAssociator(UseStack::queueStack()),
        SharedRecursiveLock(lock),
        mOutputSize(0),
        mDocument(document),
        mMoveToCacheDuration(Seconds(ISettings::getUInt(OPENPEER_STACK_SETTING_PUBLICATION_MOVE_DOCUMENT_TO_CACHE_TIME)))
      {
        ZS_LOG_DEBUG(log("created") + ZS_PARAM("move to cache time (s)", mMoveToCacheDuration))
      }

      //-----------------------------------------------------------------------
      void Publication::CacheableDocument::init()
      {
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      Publication::CacheableDocument::~CacheableDocument()
      {
        mThisWeak.reset();

        ZS_LOG_DEBUG(log("destroyed"))

        if (mMoveToCacheTimer) {
          mMoveToCacheTimer->cancel();
          mMoveToCacheTimer.reset();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication::CacheableDocument => friend Publication
      #pragma mark

      //-----------------------------------------------------------------------
      Publication::CacheableDocumentPtr Publication::CacheableDocument::create(DocumentPtr document)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!document)

        CacheableDocumentPtr pThis(new CacheableDocument(SharedRecursiveLock::create(), document));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      size_t Publication::CacheableDocument::getOutputSize() const
      {
        AutoRecursiveLock lock(*this);
        if (0 != mOutputSize) return mOutputSize;

        ZS_THROW_BAD_STATE_IF(!mDocument)

        GeneratorPtr generator = Generator::createJSONGenerator();
        mOutputSize = generator->getOutputSize(mDocument);
        return mOutputSize;
      }

      //-----------------------------------------------------------------------
      DocumentPtr Publication::CacheableDocument::getDocument(AutoRecursiveLockPtr &outDocumentLock) const
      {
        outDocumentLock = AutoRecursiveLockPtr(new AutoRecursiveLock(*this));

        CacheableDocument *pThis = const_cast<CacheableDocument *>(this);

        if (mDocument) {
          if (mMoveToCacheTimer) {
            ZS_LOG_TRACE(log("document fetched thus do not yet move to cache just yet"))
            mMoveToCacheTimer->cancel();
            mMoveToCacheTimer.reset();
          }
          pThis->step();
          return mDocument;
        }

        ZS_LOG_DEBUG(log("restoring from cache"))

        ZS_THROW_BAD_STATE_IF(!mPreviouslyStored)

        String output = ICache::fetch(getCookieName());

        mDocument = Document::createFromParsedJSON(output);
        ZS_THROW_INVALID_ASSUMPTION_IF(!mDocument)

        pThis->step();

        return mDocument;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication::CacheableDocument => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Publication::CacheableDocument::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(*this);

        if (timer != mMoveToCacheTimer) {
          ZS_LOG_WARNING(Trace, log("ignoring obsolete move to cache timer (probably okay)") + ZS_PARAM("timer", timer->getID()))
          return;
        }
        moveToCacheNow();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Publication::CacheableDocument => friend Publication
      #pragma mark

      //-----------------------------------------------------------------------
      void Publication::CacheableDocument::step()
      {
        if (!mDocument) {
          if (mMoveToCacheTimer) {
            ZS_LOG_TRACE(log("no timer needed as document must already be in cache"))
            mMoveToCacheTimer->cancel();
            mMoveToCacheTimer.reset();
          }
          return;
        }

        if (!mMoveToCacheTimer) {
          ZS_LOG_TRACE(log("creating timer to move to cache"))
          mMoveToCacheTimer = Timer::create(mThisWeak.lock(), mMoveToCacheDuration, false);
        }
      }

      //-----------------------------------------------------------------------
      String Publication::CacheableDocument::getCookieName() const
      {
        return String("/diff/publication-") + string(mID);
      }

      //-----------------------------------------------------------------------
      void Publication::CacheableDocument::moveToCacheNow()
      {
        if (!mPreviouslyStored) {
          // time to move to cache
          GeneratorPtr generator = Generator::createJSONGenerator();

          std::unique_ptr<char[]> output;
          size_t length = 0;
          output = generator->write(mDocument, &length);

          mOutputSize = length;

          ZS_LOG_DEBUG(log("moving document to cache"))
          ICache::store(getCookieName(), Time(), output.get());
          mPreviouslyStored = true;
        } else {
          ZS_LOG_TRACE(log("document is already in cache (thus forgetting about document)"))
        }

        mDocument.reset();

        step();
      }

      //-----------------------------------------------------------------------
      Log::Params Publication::CacheableDocument::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("Publication::CacheableDocument");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationFactory &IPublicationFactory::singleton()
      {
        return PublicationFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
                                                 LocationPtr creatorLocation,
                                                 const char *name,
                                                 const char *mimeType,
                                                 const SecureByteBlock &data,
                                                 const PublishToRelationshipsMap &publishToRelationships,
                                                 LocationPtr publishedLocation,
                                                 Time expires
                                                 )
      {
        if (this) {}
        return Publication::create(creatorLocation, name, mimeType, data, publishToRelationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
                                                 LocationPtr creatorLocation,
                                                 const char *name,
                                                 const char *mimeType,
                                                 DocumentPtr documentToBeAdopted,
                                                 const PublishToRelationshipsMap &publishToRelationships,
                                                 LocationPtr publishedLocation,
                                                 Time expires
                                                 )
      {
        if (this) {}
        return Publication::create(creatorLocation, name, mimeType, documentToBeAdopted, publishToRelationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
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
                                                 )
      {
        if (this) {}
        return Publication::create(version, baseVersion, lineage, creatorLocation, name, mimeType, dataEl, encoding, publishToRelationships, publishedLocation, expires);
      }
      
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPublication
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPublication::toDebug(IPublicationPtr publication)
    {
      return internal::Publication::toDebug(publication);
    }

    //-------------------------------------------------------------------------
    IPublicationPtr IPublication::create(
                                         ILocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         const SecureByteBlock &data,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         ILocationPtr publishedLocation,
                                         Time expires
                                         )
    {
      return internal::IPublicationFactory::singleton().create(internal::Location::convert(creatorLocation), name, mimeType, data, publishToRelationships, internal::Location::convert(publishedLocation), expires);
    }

    //-------------------------------------------------------------------------
    IPublicationPtr IPublication::create(
                                         ILocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         DocumentPtr documentToBeAdopted,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         ILocationPtr publishedLocation,
                                         Time expires
                                         )
    {
      return internal::IPublicationFactory::singleton().create(internal::Location::convert(creatorLocation), name, mimeType, documentToBeAdopted, publishToRelationships, internal::Location::convert(publishedLocation), expires);
    }

    //-------------------------------------------------------------------------
    IPublicationPtr IPublication::create(
                                         ILocationPtr creatorLocation,
                                         const char *name,
                                         const char *mimeType,
                                         const RelationshipList &relationshipsDocument,
                                         const PublishToRelationshipsMap &publishToRelationships,
                                         ILocationPtr publishedLocation,
                                         Time expires
                                         )
    {
      return internal::Publication::create(internal::Location::convert(creatorLocation), name, mimeType, relationshipsDocument, publishToRelationships, internal::Location::convert(publishedLocation), expires);
    }
  }
}
