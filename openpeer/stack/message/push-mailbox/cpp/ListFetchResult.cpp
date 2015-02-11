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

#include <openpeer/stack/message/push-mailbox/ListFetchResult.h>
#include <openpeer/stack/message/push-mailbox/ListFetchRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace push_mailbox
      {
        using message::internal::MessageHelper;

        //---------------------------------------------------------------------
        ListFetchResultPtr ListFetchResult::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(ListFetchResult, message);
        }

        //---------------------------------------------------------------------
        ListFetchResult::ListFetchResult()
        {
        }

        //---------------------------------------------------------------------
        ListFetchResultPtr ListFetchResult::create(ListFetchRequestPtr request)
        {
          ListFetchResultPtr ret(new ListFetchResult);
          ret->fillFrom(request);
          return ret;
        }

        //---------------------------------------------------------------------
        ListFetchResultPtr ListFetchResult::create(
                                                   ElementPtr rootEl,
                                                   IMessageSourcePtr messageSource
                                                   )
        {
          ListFetchResultPtr ret(new ListFetchResult);

          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr listsEl = rootEl->findFirstChildElement("lists");

          if (listsEl) {
            ElementPtr listEl = listsEl->findFirstChildElement("list");

            while (listEl) {
              ListID listID = IMessageHelper::getAttributeID(listEl);

              URIList uris;

              ElementPtr peersEl = listEl->findFirstChildElement("peers");

              if (peersEl) {
                ElementPtr peerEl = peersEl->findFirstChildElement("peer");
                while (peerEl) {
                  URI uri = IMessageHelper::getElementTextAndDecode(peerEl);

                  uris.push_back(uri);

                  peerEl = peerEl->findNextSiblingElement("peer");
                }
              }

              if ((listID.hasData()) &&
                  (uris.size() > 0)) {
                ret->mListToURIs[listID] = uris;
              }

              listEl = listEl->findNextSiblingElement("list");
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr ListFetchResult::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr listsEl = Element::create("lists");

          if (hasAttribute(AttributeType_ListToURIs)) {
            for (URIListMap::const_iterator iter = mListToURIs.begin(); iter != mListToURIs.end(); ++iter) {
              const String &listID = (*iter).first;
              const URIList &uriList = (*iter).second;

              if (listID.hasData()) {
                ElementPtr listEl = IMessageHelper::createElementWithTextID("list", listID);

                ElementPtr peersEl = IMessageHelper::createElement("peers");

                for (URIList::const_iterator peerIter = uriList.begin(); peerIter != uriList.end(); ++peerIter)
                {
                  const URI &uri = (*peerIter);
                  if (uri.hasData()) {
                    ElementPtr peerEl = IMessageHelper::createElementWithTextAndJSONEncode("peer", uri);
                    peersEl->adoptAsLastChild(peerEl);
                  }
                }

                if (peersEl->hasChildren()) {
                  listsEl->adoptAsLastChild(peersEl);
                }
              }
            }
          }

          if (listsEl->hasChildren()) {
            rootEl->adoptAsFirstChild(listsEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool ListFetchResult::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_ListToURIs:        return mListToURIs.size() > 0;
            default:                              break;
          }
          return MessageResult::hasAttribute((MessageResult::AttributeTypes)type);
        }

      }
    }
  }
}
