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

#include <openpeer/stack/message/p2p-database/ListSubscribeRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace p2p_database
      {
        ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

        //---------------------------------------------------------------------
        ListSubscribeRequestPtr ListSubscribeRequest::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(ListSubscribeRequest, message);
        }

        //---------------------------------------------------------------------
        ListSubscribeRequest::ListSubscribeRequest()
        {
          mAppID.clear();
        }

        //---------------------------------------------------------------------
        ListSubscribeRequestPtr ListSubscribeRequest::create()
        {
          ListSubscribeRequestPtr ret(new ListSubscribeRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        ListSubscribeRequestPtr ListSubscribeRequest::create(
                                                             ElementPtr rootEl,
                                                             IMessageSourcePtr messageSource
                                                             )
        {
          ListSubscribeRequestPtr ret(new ListSubscribeRequest);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr databasesEl = rootEl->findFirstChildElement("databases");
          if (databasesEl) {
            ret->mVersion = IMessageHelper::getAttribute(databasesEl, "version");
            ret->mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(databasesEl->findFirstChildElement("expires")));
          }

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr ListSubscribeRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr databasesEl = Element::create("databases");

          if (hasAttribute(AttributeType_DatabasesVersion)) {
            databasesEl->setAttribute("version", mVersion);
          }

          if (hasAttribute(AttributeType_SubscriptionExpires)) {
            databasesEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
          }

          if ((databasesEl->hasChildren()) ||
              (databasesEl->hasAttributes())) {
            rootEl->adoptAsLastChild(databasesEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool ListSubscribeRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type) {
            case AttributeType_DatabasesVersion:    return mVersion.hasData();
            case AttributeType_SubscriptionExpires: return Time() != mExpires;
          }
          return false;
        }

      }
    }
  }
}
