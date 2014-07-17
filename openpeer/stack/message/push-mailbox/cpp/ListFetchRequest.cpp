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

#include <openpeer/stack/message/push-mailbox/ListFetchRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <zsLib/XML.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace push_mailbox
      {
        //---------------------------------------------------------------------
        ListFetchRequestPtr ListFetchRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<ListFetchRequest>(message);
        }

        //---------------------------------------------------------------------
        ListFetchRequest::ListFetchRequest()
        {
        }

        //---------------------------------------------------------------------
        ListFetchRequestPtr ListFetchRequest::create()
        {
          ListFetchRequestPtr ret(new ListFetchRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool ListFetchRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_ListIDs:           return mListIDs.size() > 0;
            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        ListFetchRequestPtr ListFetchRequest::create(
                                                     ElementPtr rootEl,
                                                     IMessageSourcePtr messageSource
                                                     )
        {
          ListFetchRequestPtr ret(new ListFetchRequest);

          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr listsEl = rootEl->findFirstChildElement("lists");

          if (listsEl) {
            ElementPtr listEl = listsEl->findFirstChildElement("list");
            while (listEl) {
              String uri = IMessageHelper::getElementTextAndDecode(listEl);
              if (uri.hasData()) {
                ret->mListIDs.push_back(uri);
              }
              listEl = listEl->findNextSiblingElement("list");
            }
          }

          return ret;
        }
        
        //---------------------------------------------------------------------
        DocumentPtr ListFetchRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr listsEl = Element::create("lists");

          if (hasAttribute(AttributeType_ListIDs)) {
            for (ListIDList::const_iterator iter = mListIDs.begin(); iter != mListIDs.end(); ++iter) {
              const ListID &listID = (*iter);
              if (listID.hasData()) {
                ElementPtr listEl = IMessageHelper::createElementWithTextAndJSONEncode("list", listID);
                listsEl->adoptAsLastChild(listEl);
              }
            }
          }

          if (listsEl->hasChildren()) {
            rootEl->adoptAsFirstChild(listsEl);
          }

          return ret;
        }


      }
    }
  }
}
