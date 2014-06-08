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

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/message/IMessageHelper.h>
#include <openpeer/stack/message/IMessageFactory.h>
#include <openpeer/stack/message/MessageResult.h>

#include <openpeer/stack/message/peer-common/MessageFactoryPeerCommon.h>
#include <openpeer/stack/message/peer-common/PeerPublishRequest.h>
#include <openpeer/stack/message/peer-common/PeerGetResult.h>

#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>

#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Numeric.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using zsLib::DWORD;
      using zsLib::QWORD;
      using zsLib::Numeric;

      using namespace stack::internal;
      using services::IHelper;

      using peer_common::MessageFactoryPeerCommon;
      using peer_common::PeerPublishRequest;
      using peer_common::PeerGetResult;

      typedef stack::IPublicationMetaData::PublishToRelationshipsMap PublishToRelationshipsMap;

      ZS_DECLARE_TYPEDEF_PTR(stack::internal::IPublicationForMessages, UsePublication)
      ZS_DECLARE_TYPEDEF_PTR(stack::internal::IPublicationMetaDataForMessages, UsePublicationMetaData)

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageHelper
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "IMessageHelper");
      }

      //-----------------------------------------------------------------------
      DocumentPtr IMessageHelper::createDocumentWithRoot(const Message &message)
      {
        const char *tagName = Message::toString(message.messageType());

        IMessageFactoryPtr factory = message.factory();

        if (!factory) return DocumentPtr();

        DocumentPtr ret = Document::create();
        ret->setElementNameIsCaseSensative(true);
        ret->setAttributeNameIsCaseSensative(true);

        ElementPtr rootEl = Element::create(tagName);
        ret->adoptAsFirstChild(rootEl);

        String domain = message.domain();

        if (domain.hasData()) {
          IMessageHelper::setAttribute(rootEl, "domain", domain);
        }

        String appID = message.appID();
        if (appID.hasData()) {
          IMessageHelper::setAttribute(rootEl, "appid", appID);
        }

        Time time = message.time();
        if (Time() != time) {
          IMessageHelper::setAttributeTimestamp(rootEl, time);
        }

        IMessageHelper::setAttribute(rootEl, "handler", factory->getHandler());
        IMessageHelper::setAttributeID(rootEl, message.messageID());
        IMessageHelper::setAttribute(rootEl, "method", factory->toString(message.method()));

        if (message.isResult()) {
          const message::MessageResult *msgResult = (dynamic_cast<const message::MessageResult *>(&message));
          if ((msgResult->hasAttribute(MessageResult::AttributeType_ErrorCode)) ||
              (msgResult->hasAttribute(MessageResult::AttributeType_ErrorReason))) {

            ElementPtr errorEl;
            if (msgResult->hasAttribute(MessageResult::AttributeType_ErrorReason)) {
              errorEl = IMessageHelper::createElementWithTextAndJSONEncode("error", msgResult->errorReason());
            } else {
              errorEl = IMessageHelper::createElement("error");
            }
            if (msgResult->hasAttribute(MessageResult::AttributeType_ErrorCode)) {
              IMessageHelper:setAttributeID(errorEl, string(msgResult->errorCode()));
            }

            rootEl->adoptAsLastChild(errorEl);
          }
        }

        return ret;
      }

      //-----------------------------------------------------------------------
      Message::MessageTypes IMessageHelper::getMessageType(ElementPtr root)
      {
        if (!root) return Message::MessageType_Invalid;
        return Message::toMessageType(root->getValue());
      }

      //---------------------------------------------------------------------
      String IMessageHelper::getAttributeID(ElementPtr node)
      {
        return IMessageHelper::getAttribute(node, "id");
      }

      //---------------------------------------------------------------------
      void IMessageHelper::setAttributeID(ElementPtr elem, const String &value)
      {
        if (value.isEmpty()) return;
        IMessageHelper::setAttribute(elem, "id", value);
      }

      //---------------------------------------------------------------------
      Time IMessageHelper::getAttributeEpoch(ElementPtr node)
      {
        return IHelper::stringToTime(IMessageHelper::getAttribute(node, "timestamp"));
      }

      //---------------------------------------------------------------------
      void IMessageHelper::setAttributeTimestamp(ElementPtr elem, const Time &value)
      {
        if (!elem) return;
        if (Time() == value) return;
        elem->setAttribute("timestamp", IHelper::timeToString(value), false);
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getAttribute(
                                          ElementPtr node,
                                          const String &attributeName
                                          )
      {
        if (!node) return String();

        AttributePtr attribute = node->findAttribute(attributeName);
        if (!attribute) return String();

        return attribute->getValue();
      }

      //-----------------------------------------------------------------------
      void IMessageHelper::setAttribute(
                                        ElementPtr elem,
                                        const String &attrName,
                                        const String &value
                                        )
      {
        if (!elem) return;
        if (value.isEmpty()) return;

        AttributePtr attr = Attribute::create();
        attr->setName(attrName);
        attr->setValue(value);

        elem->setAttribute(attr);
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElement(const String &elName)
      {
        ElementPtr tmp = Element::create();
        tmp->setValue(elName);
        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithText(
                                                       const String &elName,
                                                       const String &textVal
                                                       )
      {
        ElementPtr tmp = Element::create(elName);

        if (textVal.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(textVal, Text::Format_JSONStringEncoded);

        tmp->adoptAsFirstChild(tmpTxt);

        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithNumber(
                                                         const String &elName,
                                                         const String &numberAsStringValue
                                                         )
      {
        ElementPtr tmp = Element::create(elName);

        if (numberAsStringValue.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(numberAsStringValue, Text::Format_JSONNumberEncoded);
        tmp->adoptAsFirstChild(tmpTxt);

        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithTime(
                                                       const String &elName,
                                                       Time time
                                                       )
      {
        return createElementWithNumber(elName, IHelper::timeToString(time));
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithTextAndJSONEncode(
                                                                    const String &elName,
                                                                    const String &textVal
                                                                    )
      {
        ElementPtr tmp = Element::create(elName);
        if (textVal.isEmpty()) return tmp;

        TextPtr tmpTxt = Text::create();
        tmpTxt->setValueAndJSONEncode(textVal);
        tmp->adoptAsFirstChild(tmpTxt);
        return tmp;
      }

      //-----------------------------------------------------------------------
      ElementPtr IMessageHelper::createElementWithID(
                                                     const String &elName,
                                                     const String &idValue
                                                     )
      {
        ElementPtr tmp = createElement(elName);

        if (idValue.isEmpty()) return tmp;

        setAttributeID(tmp, idValue);
        return tmp;
      }

      //-----------------------------------------------------------------------
      TextPtr IMessageHelper::createText(const String &textVal)
      {
        TextPtr tmpTxt = Text::create();
        tmpTxt->setValue(textVal);

        return tmpTxt;
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getElementText(ElementPtr node)
      {
        if (!node) return String();
        return node->getText();
      }

      //-----------------------------------------------------------------------
      String IMessageHelper::getElementTextAndDecode(ElementPtr node)
      {
        if (!node) return String();
        return node->getTextDecoded();
      }

      //-----------------------------------------------------------------------
      void IMessageHelper::fill(
                                Message &message,
                                ElementPtr rootEl,
                                IMessageSourcePtr source
                                )
      {
        String id = IMessageHelper::getAttribute(rootEl, "id");
        String domain = IMessageHelper::getAttribute(rootEl, "domain");
        String appID = IMessageHelper::getAttribute(rootEl, "appid");

        if (id.hasData()) {
          message.messageID(id);
        }
        if (domain.hasData()) {
          message.domain(domain);
        }
        message.appID(appID);

        Time time = IMessageHelper::getAttributeEpoch(rootEl);
        if (Time() != time) {
          message.time(time);
        }
      }

      namespace internal
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageHelper
        #pragma mark

        //---------------------------------------------------------------------
        ElementPtr MessageHelper::createElement(
                                                const PublishToRelationshipsMap &relationships,
                                                const char *elementName
                                                )
        {
          ElementPtr rootEl = IMessageHelper::createElement(elementName);

          for (PublishToRelationshipsMap::const_iterator iter = relationships.begin(); iter != relationships.end(); ++iter)
          {
            String name = (*iter).first;
            const stack::IPublicationMetaData::PermissionAndPeerURIListPair &permission = (*iter).second;

            const char *permissionStr = "all";
            switch (permission.first) {
              case stack::IPublicationMetaData::Permission_All:     permissionStr = "all"; break;
              case stack::IPublicationMetaData::Permission_None:    permissionStr = "none"; break;
              case stack::IPublicationMetaData::Permission_Some:    permissionStr = "some"; break;
              case stack::IPublicationMetaData::Permission_Add:     permissionStr = "add"; break;
              case stack::IPublicationMetaData::Permission_Remove:  permissionStr = "remove"; break;
            }

            ElementPtr relationshipsEl = IMessageHelper::createElement("relationships");
            relationshipsEl->setAttribute("name", name);
            relationshipsEl->setAttribute("allow", permissionStr);

            for (stack::IPublicationMetaData::PeerURIList::const_iterator contactIter = permission.second.begin(); contactIter != permission.second.end(); ++contactIter)
            {
              ElementPtr contactEl = IMessageHelper::createElementWithTextAndJSONEncode("contact", (*contactIter));
              relationshipsEl->adoptAsLastChild(contactEl);
            }

            rootEl->adoptAsLastChild(relationshipsEl);
          }

          return rootEl;
        }

        //---------------------------------------------------------------------
        DocumentPtr MessageHelper::createDocument(
                                                  Message &msg,
                                                  IPublicationMetaDataPtr publicationMetaData,
                                                  size_t *notifyPeerPublishMaxDocumentSizeInBytes,
                                                  IPublicationRepositoryPeerCachePtr peerCache
                                                  )
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(msg);
          ElementPtr root = ret->getFirstChildElement();

          UsePublicationPtr publication = Publication::convert(publicationMetaData->toPublication());

          ULONG fromVersion = 0;
          ULONG toVersion = 0;

          if (publication) {
            fromVersion = publication->getBaseVersion();
            toVersion = publication->getVersion();
          } else {
            fromVersion = publicationMetaData->getBaseVersion();
            toVersion = publicationMetaData->getVersion();
          }

          // do not use the publication for these message types...
          switch ((MessageFactoryPeerCommon::Methods)msg.method()) {
            case MessageFactoryPeerCommon::Method_PeerPublish:
            {
              if (Message::MessageType_Request == msg.messageType()) {
                PeerPublishRequest &request = *(dynamic_cast<PeerPublishRequest *>(&msg));
                fromVersion = request.publishedFromVersion();
                toVersion = request.publishedToVersion();
              }
              if (Message::MessageType_Request != msg.messageType()) {
                // only the request can include a publication...
                publication.reset();
              }
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerGet:
            {
              if (Message::MessageType_Result == msg.messageType()) {
                message::PeerGetResult &result = *(dynamic_cast<message::PeerGetResult *>(&msg));
                IPublicationMetaDataPtr originalMetaData = result.originalRequestPublicationMetaData();
                fromVersion = originalMetaData->getVersion();
                if (0 != fromVersion) {
                  // remote party already has this version so just return from that version onward...
                  ++fromVersion;
                }
                toVersion = 0;
              }

              if (Message::MessageType_Result != msg.messageType()) {
                // only the result can include a publication
                publication.reset();
              }
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerDelete:
            case MessageFactoryPeerCommon::Method_PeerSubscribe:
            {
              // these messages requests/results will never include a publication (at least at this time)
              publication.reset();
              break;
            }
            case MessageFactoryPeerCommon::Method_PeerPublishNotify:
            {
              if (publication) {
                if (peerCache) {
                  size_t bogusFillSize = 0;
                  size_t &maxFillSize = (notifyPeerPublishMaxDocumentSizeInBytes ? *notifyPeerPublishMaxDocumentSizeInBytes : bogusFillSize);
                  if (peerCache->getNextVersionToNotifyAboutAndMarkNotified(Publication::convert(publication), maxFillSize, fromVersion, toVersion)) {
                    break;
                  }
                }
              }
              publication.reset();
              fromVersion = 0;
              break;
            }
            default:                                  break;
          }

          // make a copy of the relationships
          PublishToRelationshipsMap relationships = publicationMetaData->getRelationships();

          ElementPtr docEl = IMessageHelper::createElement("document");
          ElementPtr detailsEl = IMessageHelper::createElement("details");
          ElementPtr publishToRelationshipsEl = MessageHelper::createElement(relationships, MessageFactoryPeerCommon::Method_PeerSubscribe == (MessageFactoryPeerCommon::Methods)msg.method() ? "subscribeToRelationships" : "publishToRelationships");
          ElementPtr dataEl = IMessageHelper::createElement("data");

          String creatorPeerURI;
          String creatorLocationID;

          UseLocationPtr creatorLocation = Location::convert(publicationMetaData->getCreatorLocation());
          if (creatorLocation ) {
            creatorLocationID = creatorLocation->getLocationID();
            creatorPeerURI = creatorLocation->getPeerURI();
          }

          ElementPtr contactEl = IMessageHelper::createElementWithTextAndJSONEncode("contact", creatorPeerURI);
          ElementPtr locationEl = IMessageHelper::createElementWithTextAndJSONEncode("location", creatorLocationID);

          NodePtr publishedDocEl;
          if (publication) {
            publishedDocEl = publication->getDiffs(
                                                   fromVersion,
                                                   toVersion
                                                   );

            if (0 == toVersion) {
              // put the version back to something more sensible
              toVersion = publicationMetaData->getVersion();
            }
          }

          ElementPtr nameEl = IMessageHelper::createElementWithText("name", publicationMetaData->getName());
          ElementPtr versionEl = IMessageHelper::createElementWithNumber("version", string(toVersion));
          ElementPtr baseVersionEl = IMessageHelper::createElementWithNumber("baseVersion", string(fromVersion));
          ElementPtr lineageEl = IMessageHelper::createElementWithNumber("lineage", string(publicationMetaData->getLineage()));
          ElementPtr chunkEl = IMessageHelper::createElementWithText("chunk", "1/1");

          ElementPtr expiresEl;
          if (publicationMetaData->getExpires() != Time()) {
            expiresEl = IMessageHelper::createElementWithNumber("expires", IHelper::timeToString(publicationMetaData->getExpires()));
          }

          ElementPtr mimeTypeEl = IMessageHelper::createElementWithText("mime", publicationMetaData->getMimeType());

          stack::IPublication::Encodings encoding = publicationMetaData->getEncoding();
          const char *encodingStr = "binary";
          switch (encoding) {
            case stack::IPublication::Encoding_Binary:  encodingStr = "binary"; break;
            case stack::IPublication::Encoding_JSON:    encodingStr = "json"; break;
          }

          ElementPtr encodingEl = IMessageHelper::createElementWithText("encoding", encodingStr);

          root->adoptAsLastChild(docEl);
          docEl->adoptAsLastChild(detailsEl);
          docEl->adoptAsLastChild(publishToRelationshipsEl);

          detailsEl->adoptAsLastChild(nameEl);
          detailsEl->adoptAsLastChild(versionEl);
          if (0 != fromVersion)
            detailsEl->adoptAsLastChild(baseVersionEl);
          detailsEl->adoptAsLastChild(lineageEl);
          detailsEl->adoptAsLastChild(chunkEl);
          detailsEl->adoptAsLastChild(contactEl);
          detailsEl->adoptAsLastChild(locationEl);
          if (expiresEl)
            detailsEl->adoptAsLastChild(expiresEl);
          detailsEl->adoptAsLastChild(mimeTypeEl);
          detailsEl->adoptAsLastChild(encodingEl);

          if (publishedDocEl) {
            dataEl->adoptAsLastChild(publishedDocEl);
            docEl->adoptAsLastChild(dataEl);
          }

          switch (msg.messageType()) {
            case Message::MessageType_Request:
            case Message::MessageType_Notify:   {
              switch ((MessageFactoryPeerCommon::Methods)msg.method()) {

                case MessageFactoryPeerCommon::Method_PeerPublish:         break;

                case MessageFactoryPeerCommon::Method_PeerGet:             {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  publishToRelationshipsEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerDelete:          {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  publishToRelationshipsEl->orphan();
                }
                case MessageFactoryPeerCommon::Method_PeerSubscribe:       {
                  versionEl->orphan();
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  lineageEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerPublishNotify:
                {
                  if (!publication) {
                    if (baseVersionEl)
                      baseVersionEl->orphan();
                  }
                  chunkEl->orphan();
                  publishToRelationshipsEl->orphan();
                  break;
                }
                default:                                  break;
              }
              break;
            }
            case Message::MessageType_Result: {
              switch ((MessageFactoryPeerCommon::Methods)msg.method()) {
                case MessageFactoryPeerCommon::Method_PeerPublish:       {
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  locationEl->orphan();
                  contactEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerGet:           {
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerDelete:        {
                  ZS_THROW_INVALID_USAGE("this method should not be used for delete result")
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerSubscribe:     {
                  versionEl->orphan();
                  if (baseVersionEl)
                    baseVersionEl->orphan();
                  lineageEl->orphan();
                  chunkEl->orphan();
                  contactEl->orphan();
                  locationEl->orphan();
                  if (expiresEl)
                    expiresEl->orphan();
                  mimeTypeEl->orphan();
                  encodingEl->orphan();
                  dataEl->orphan();
                  break;
                }
                case MessageFactoryPeerCommon::Method_PeerPublishNotify: {
                  ZS_THROW_INVALID_USAGE("this method should not be used for publication notify result")
                  break;
                }
                default:                                break;
              }
              break;
            }
            case Message::MessageType_Invalid:  break;
          }

          return ret;
        }


/*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <!-- <baseVersion>10</baseVersion> -->
   <lineage>5849943</lineage>
   <chunk>1/12</chunk>
   <scope>location</scope>
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
  <data>
   ...
  </data>
 </document>

</request>
*/

/*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish” epoch=”13494934”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>5849943</lineage>
   <chunk>1/12</chunk>
   <scope>location</scope>
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
 </document>

</result>
*/

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-get”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>39239392</lineage>
   <scope>location</scope>
   <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5<contact/>
   <location id=”524e609f337663bdbf54f7ef47d23ca9” />
   <chunk>1/1</chunk>
  </details>
 </document>

</request>
         */

/*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-get” epoch=”13494934”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <!-- <baseVersion>10</baseVersion> -->
   <lineage>39239392</lineage>
   <chunk>1/10</chunk>
   <scope>location</scope>
   <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5<contact/>
   <location id=”524e609f337663bdbf54f7ef47d23ca9” />
   <lifetime>session</lifetime>
   <expires>2002-01-20 23:59:59.000</expires>
   <mime>text/xml</mime>
   <encoding>xml</encoding>
  </details>
  <publishToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” allow=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” allow=”all” />
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” allow=”all” />
  </publishToRelationships>
  <data>
   ...
  </data>
 </document>

</result>
 */

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-delete”>

 <document>
  <details>
   <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
   <version>12</version>
   <lineage>39239392</lineage>
   <scope>location</scope>
  </details>
 </document>

</request>
         */

        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-subscribe”>

 <document>
  <name>/openpeer.com/presence/1.0/</name>
  <subscribeToRelationships>
   <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” subscribe=”all” />
   <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” subscribe =”add”>
    <contact>peer://domain.com/bd520f1dbaa13c0cc9b7ff528e83470e</contact>
   </relationships>
   <relationships name=”/openpeer.com/shared-groups/1.0/foobar” subscribe =”all” />
  </subscribeToRelationships>
 </document>

</request>
         */

        /*
<result xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-subscribe” epoch=”13494934”>

 <document>
   <name>/openpeer.com/presence/1.0/</name>
   <subscribeToRelationships>
    <relationships name=”/openpeer.com/authorization-list/1.0/whitelist” subscribe =”all” />
    <relationships name=”/openpeer.com/authorization-list/1.0/adhoc” subscribe =”some”>
     <contact>peer://domain.com/bd520f1dbaa13c0cc9b7ff528e83470e</contact>
     <contact>peer://domain.com/8d17a88e8d42ffbd138f3895ec45375c</contact>
    </relationships>
    <relationships name=”/openpeer.com/shared-groups/1.0/foobar” subscribe =”all” />
   </subscribeToRelationships>
 </document>

</result>
         */
        /*
<request xmlns="http://www.openpeer.com/openpeer/1.0/message" id=”abc123” method=”peer-publish-notify”>

 <documents>
  <document>
   <details>
    <name>/openpeer.com/presence/1.0/bd520f1dbaa13c0cc9b7ff528e83470e/883fa7...9533609131</name>
    <version>12</version>
    <lineage>43493943</lineage>
    <scope>location</scope>
    <contact>peer://domain.com/ea00ede4405c99be9ae45739ebfe57d5</contact>
    <location id=”524e609f337663bdbf54f7ef47d23ca9” />
    <lifetime>session</lifetime>
    <expires>49494393</expires>
    <mime>text/xml</mime>
    <encoding>xml</encoding>
   </details>
  </document>
  <!-- <data> ... </data> -->
 </documents>

</request>
         */

        //---------------------------------------------------------------------
        void MessageHelper::fillFrom(
                                     IMessageSourcePtr messageSource,
                                     MessagePtr msg,
                                     ElementPtr rootEl,
                                     IPublicationPtr &outPublication,
                                     IPublicationMetaDataPtr &outPublicationMetaData
                                     )
        {
          try {
            ElementPtr docEl = rootEl->findFirstChildElementChecked("document");
            ElementPtr detailsEl = docEl->findFirstChildElementChecked("details");

            ElementPtr nameEl = detailsEl->findFirstChildElementChecked("name");
            ElementPtr versionEl = detailsEl->findFirstChildElement("version");
            ElementPtr baseVersionEl = detailsEl->findFirstChildElement("baseVersion");
            ElementPtr lineageEl = detailsEl->findFirstChildElement("lineage");
            ElementPtr scopeEl = detailsEl->findFirstChildElement("scope");
            ElementPtr lifetimeEl = detailsEl->findFirstChildElement("lifetime");
            ElementPtr expiresEl = detailsEl->findFirstChildElement("expires");
            ElementPtr mimeTypeEl = detailsEl->findFirstChildElement("mime");
            ElementPtr encodingEl = detailsEl->findFirstChildElement("encoding");

            String contact;
            ElementPtr contactEl = detailsEl->findFirstChildElement("contact");
            if (contactEl) {
              contact = contactEl->getTextDecoded();
            }

            String locationID;
            ElementPtr locationEl = detailsEl->findFirstChildElement("location");
            if (locationEl) {
              locationID = locationEl->getTextDecoded();
            }

            UseLocationPtr location = UseLocation::create(messageSource, contact, locationID);

            ElementPtr dataEl = docEl->findFirstChildElement("data");

            ULONG version = 0;
            if (versionEl) {
              String versionStr = versionEl->getText();
              try {
                version = Numeric<ULONG>(versionStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            ULONG baseVersion = 0;
            if (baseVersionEl) {
              String baseVersionStr = baseVersionEl->getText();
              try {
                baseVersion = Numeric<ULONG>(baseVersionStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            ULONG lineage = 0;
            if (lineageEl) {
              String lineageStr = lineageEl->getText();
              try {
                lineage = Numeric<ULONG>(lineageStr);
              } catch(Numeric<ULONG>::ValueOutOfRange &) {
              }
            }

            IPublicationMetaData::Encodings encoding = IPublicationMetaData::Encoding_Binary;
            if (encodingEl) {
              String encodingStr = encodingEl->getText();
              if (encodingStr == "json") encoding = IPublicationMetaData::Encoding_JSON;
              else if (encodingStr == "binary") encoding = IPublicationMetaData::Encoding_Binary;
            }

            Time expires;
            if (expiresEl) {
              String expiresStr = expiresEl->getText();
              expires = IHelper::stringToTime(expiresStr);
            }

            String mimeType;
            if (mimeTypeEl) {
              mimeType = mimeTypeEl->getText();
            }

            IPublicationMetaData::PublishToRelationshipsMap relationships;

            ElementPtr publishToRelationshipsEl = docEl->findFirstChildElement(MessageFactoryPeerCommon::Method_PeerSubscribe == (MessageFactoryPeerCommon::Methods)msg->method() ? "subscribeToRelationships" : "publishToRelationships");
            if (publishToRelationshipsEl) {
              ElementPtr relationshipsEl = publishToRelationshipsEl->findFirstChildElement("relationships");
              while (relationshipsEl)
              {
                String name = relationshipsEl->getAttributeValue("name");
                String allowStr = relationshipsEl->getAttributeValue("allow");

                IPublicationMetaData::PeerURIList contacts;
                ElementPtr contactEl = relationshipsEl->findFirstChildElement("contact");
                while (contactEl)
                {
                  String contact = contactEl->getTextDecoded();
                  if (contact.size() > 0) {
                    contacts.push_back(contact);
                  }
                  contactEl = contactEl->findNextSiblingElement("contact");
                }

                IPublicationMetaData::Permissions permission = IPublicationMetaData::Permission_All;
                if (allowStr == "all") permission = IPublicationMetaData::Permission_All;
                else if (allowStr == "none") permission = IPublicationMetaData::Permission_None;
                else if (allowStr == "some") permission = IPublicationMetaData::Permission_Some;
                else if (allowStr == "add") permission = IPublicationMetaData::Permission_Add;
                else if (allowStr == "remove") permission = IPublicationMetaData::Permission_Remove;

                if (name.size() > 0) {
                  relationships[name] = IPublicationMetaData::PermissionAndPeerURIListPair(permission, contacts);
                }

                relationshipsEl = relationshipsEl->findNextSiblingElement("relationships");
              }
            }

            bool hasPublication = false;

            switch (msg->messageType()) {
              case Message::MessageType_Request:
              case Message::MessageType_Notify:   {
                switch ((MessageFactoryPeerCommon::Methods)msg->method()) {
                  case MessageFactoryPeerCommon::Method_PeerPublish:
                  case MessageFactoryPeerCommon::Method_PeerPublishNotify: {
                    hasPublication = true;
                    break;
                  }
                  case MessageFactoryPeerCommon::Method_PeerGet:
                  case MessageFactoryPeerCommon::Method_PeerDelete:
                  case MessageFactoryPeerCommon::Method_PeerSubscribe: {
                    hasPublication = false;
                  }
                  default: break;
                }
                break;
              }
              case Message::MessageType_Result:   {
                switch ((MessageFactoryPeerCommon::Methods)msg->method()) {
                  case MessageFactoryPeerCommon::Method_PeerPublish:
                  case MessageFactoryPeerCommon::Method_PeerSubscribe: {
                    hasPublication = false;
                  }
                  case MessageFactoryPeerCommon::Method_PeerGet:
                  {
                    hasPublication = true;
                    break;
                  }
                  default: break;
                }
                break;
              }
              case Message::MessageType_Invalid:  break;
            }

            if (!dataEl) {
              hasPublication = false;
            }

            if (hasPublication) {
              UsePublicationPtr publication = IPublicationForMessages::create(
                                                                              version,
                                                                              baseVersion,
                                                                              lineage,
                                                                              Location::convert(location),
                                                                              nameEl->getText(),
                                                                              mimeType,
                                                                              dataEl,
                                                                              encoding,
                                                                              relationships,
                                                                              Location::convert(location),
                                                                              expires
                                                                              );
              outPublicationMetaData = publication->toPublicationMetaData();
              outPublication = Publication::convert(publication);
            } else {
              UsePublicationMetaDataPtr metaData = IPublicationMetaDataForMessages::create(
                                                                                           version,
                                                                                           baseVersion,
                                                                                           lineage,
                                                                                           Location::convert(location),
                                                                                           nameEl->getText(),
                                                                                           mimeType,
                                                                                           encoding,
                                                                                           relationships,
                                                                                           Location::convert(location),
                                                                                           expires
                                                                                           );
              outPublicationMetaData = PublicationMetaData::convert(metaData);
            }
          } catch (CheckFailed &) {
          }
        }

        //---------------------------------------------------------------------
        int MessageHelper::stringToInt(const String &s)
        {
          if (s.isEmpty()) return 0;

          try {
            return Numeric<int>(s);
          } catch (Numeric<int>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, slog("unable to convert value to int") + ZS_PARAM("value", s))
          }
          return 0;
        }


        //---------------------------------------------------------------------
        UINT MessageHelper::stringToUint(const String &s)
        {
          if (s.isEmpty()) return 0;

          try {
            return Numeric<UINT>(s);
          } catch (Numeric<UINT>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, slog("unable to convert value to unsigned int") + ZS_PARAM("value", s))
          }
          return 0;
        }

        //---------------------------------------------------------------------
        WORD MessageHelper::getErrorCode(ElementPtr root)
        {
          if (!root) return 0;

          ElementPtr errorEl = root->findFirstChildElement("error");
          if (!errorEl) return 0;

          String ec = IMessageHelper::getAttributeID(errorEl);
          if (ec.isEmpty()) return 0;

          try {
            return (Numeric<WORD>(ec));
          } catch(Numeric<WORD>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, slog("unable to convert value to error code") + ZS_PARAM("value", ec))
          }
          return 0;
        }

        //---------------------------------------------------------------------
        String MessageHelper::getErrorReason(ElementPtr root)
        {
          if (!root) return String();

          ElementPtr errorEl = root->findFirstChildElement("error");
          if (!errorEl) return String();

          return IMessageHelper::getElementText(errorEl);
        }

      }
    }
  }
}

