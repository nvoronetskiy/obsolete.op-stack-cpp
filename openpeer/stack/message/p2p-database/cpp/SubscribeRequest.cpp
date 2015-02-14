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

#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Numeric.h>

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace p2p_database
      {
        ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

        using zsLib::Numeric;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "SubscribeRequest");
        }

        //---------------------------------------------------------------------
        SubscribeRequestPtr SubscribeRequest::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(SubscribeRequest, message);
        }

        //---------------------------------------------------------------------
        SubscribeRequest::SubscribeRequest()
        {
          mAppID.clear();
        }

        //---------------------------------------------------------------------
        SubscribeRequestPtr SubscribeRequest::create()
        {
          SubscribeRequestPtr ret(new SubscribeRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        SubscribeRequestPtr SubscribeRequest::create(
                                                     ElementPtr rootEl,
                                                     IMessageSourcePtr messageSource
                                                     )
        {
          SubscribeRequestPtr ret(new SubscribeRequest);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr databaseEl = rootEl->findFirstChildElement("database");
          if (databaseEl) {
            ret->mDatabaseID = IMessageHelper::getAttribute(databaseEl, "id");
            ret->mVersion = IMessageHelper::getAttribute(databaseEl, "version");
            ret->mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(databaseEl->findFirstChildElement("expires")));

            String dataStr = IMessageHelper::getElementText(databaseEl->findFirstChildElement("data"));

            try {
              ret->mData = Numeric<decltype(ret->mData)>(dataStr);
            } catch(Numeric<decltype(ret->mData)>::ValueOutOfRange &) {
              ZS_LOG_WARNING(Detail, slog("failed to convert") + ZS_PARAMIZE(dataStr))
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr SubscribeRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr databaseEl = Element::create("database");

          if (hasAttribute(AttributeType_DatabaseID)) {
            databaseEl->setAttribute("id", mDatabaseID);
          }
          if (hasAttribute(AttributeType_DatabaseVersion)) {
            databaseEl->setAttribute("version", mVersion);
          }
          if (hasAttribute(AttributeType_SubscriptionExpires)) {
            databaseEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
          }

          if (hasAttribute(AttributeType_DatabaseData)) {
            databaseEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("data", "true"));
          }

          if ((databaseEl->hasChildren()) ||
              (databaseEl->hasAttributes())) {
            rootEl->adoptAsLastChild(databaseEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool SubscribeRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type) {
            case AttributeType_DatabaseID:          return mDatabaseID.hasData();
            case AttributeType_DatabaseVersion:     return mVersion.hasData();
            case AttributeType_SubscriptionExpires: return Time() != mExpires;
            case AttributeType_DatabaseData:        return mData;
          }
          return false;
        }

      }
    }
  }
}
