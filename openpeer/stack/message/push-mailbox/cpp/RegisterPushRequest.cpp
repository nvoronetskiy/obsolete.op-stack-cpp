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

#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <zsLib/XML.h>

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace push_mailbox
      {
        //---------------------------------------------------------------------
        RegisterPushRequestPtr RegisterPushRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<RegisterPushRequest>(message);
        }

        //---------------------------------------------------------------------
        RegisterPushRequest::RegisterPushRequest() :
          mPushServiceType(PushServiceType_NA)
        {
        }

        //---------------------------------------------------------------------
        RegisterPushRequestPtr RegisterPushRequest::create()
        {
          RegisterPushRequestPtr ret(new RegisterPushRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool RegisterPushRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_PushServiceType:   return (PushServiceType_NA != mPushServiceType);
            case AttributeType_Token:             return mToken.hasData();
            case AttributeType_Subscriptions:     return mSubscriptions.size() > 0;
            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr RegisterPushRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          if (hasAttribute(AttributeType_PushServiceType)) {
            const char *type = (mPushServiceType == PushServiceType_APNS ? "apns" : "gcm");
            rootEl->adoptAsLastChild(IMessageHelper::createElementWithText("type", type));
          }

          if (hasAttribute(AttributeType_Token)) {
            rootEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("token", mToken));
          }

          if (hasAttribute(AttributeType_Subscriptions)) {
            ElementPtr subscriptionsEl = Element::create("subscriptions");
            for (PushSubscriptionInfoList::const_iterator iter = mSubscriptions.begin(); iter != mSubscriptions.end(); ++iter) {
              const PushSubscriptionInfo &info = (*iter);
              if (info.hasData()) {
                ElementPtr subscriptionEl = info.createElement();

                if ((subscriptionEl->hasChildren()) ||
                    (subscriptionEl->hasAttributes())) {
                  subscriptionsEl->adoptAsLastChild(subscriptionEl);
                }
              }
            }
            if (subscriptionsEl->hasChildren()) {
              rootEl->adoptAsLastChild(subscriptionsEl);
            }
          }

          return ret;
        }


      }
    }
  }
}
