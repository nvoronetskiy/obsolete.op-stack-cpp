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

#include <openpeer/stack/message/push-mailbox/MessagesDataGetRequest.h>
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
        MessagesDataGetRequestPtr MessagesDataGetRequest::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(MessagesDataGetRequest, message);
        }

        //---------------------------------------------------------------------
        MessagesDataGetRequest::MessagesDataGetRequest()
        {
        }

        //---------------------------------------------------------------------
        MessagesDataGetRequestPtr MessagesDataGetRequest::create()
        {
          MessagesDataGetRequestPtr ret(new MessagesDataGetRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool MessagesDataGetRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_MessageIDs:        return mMessageIDs.size() > 0;
            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr MessagesDataGetRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr messagesEl = Element::create("messages");

          if (hasAttribute(AttributeType_MessageIDs)) {
            for (MessageIDList::const_iterator iter = mMessageIDs.begin(); iter != mMessageIDs.end(); ++iter) {
              const MessageID &messageID = (*iter);
              if (messageID.hasData()) {
                ElementPtr messageEl = IMessageHelper::createElementWithTextAndJSONEncode("message", messageID);
                messagesEl->adoptAsLastChild(messageEl);
              }
            }
          }

          if (messagesEl->hasChildren()) {
            rootEl->adoptAsFirstChild(messagesEl);
          }

          return ret;
        }


      }
    }
  }
}
