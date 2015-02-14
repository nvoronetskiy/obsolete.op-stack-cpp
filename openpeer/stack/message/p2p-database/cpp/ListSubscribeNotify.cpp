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

#include <openpeer/stack/message/p2p-database/ListSubscribeNotify.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

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
        using zsLib::Numeric;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "ListSubscribeNotify");
        }

        //---------------------------------------------------------------------
        ListSubscribeNotifyPtr ListSubscribeNotify::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(ListSubscribeNotify, message);
        }

        //---------------------------------------------------------------------
        ListSubscribeNotify::ListSubscribeNotify()
        {
          mAppID.clear();
        }

        //---------------------------------------------------------------------
        ListSubscribeNotifyPtr ListSubscribeNotify::create()
        {
          ListSubscribeNotifyPtr ret(new ListSubscribeNotify);
          return ret;
        }

        //---------------------------------------------------------------------
        ListSubscribeNotifyPtr ListSubscribeNotify::create(
                                                           ElementPtr rootEl,
                                                           IMessageSourcePtr messageSource
                                                           )
        {
          ListSubscribeNotifyPtr ret(new ListSubscribeNotify);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr databasesEl = rootEl->findFirstChildElement("databases");

          if (databasesEl) {
            ret->mBefore = databasesEl->getAttributeValue("before");
            ret->mVersion = databasesEl->getAttributeValue("version");

            ret->mDatabases = DatabaseInfoListPtr(new DatabaseInfoList);

            ElementPtr databaseEl = databasesEl->findFirstChildElement("database");
            while (databaseEl) {
              DatabaseInfo info = DatabaseInfo::create(databaseEl);
              if (info.hasData()) {
                ret->mDatabases->push_back(info);
              }
              databaseEl = databaseEl->findNextSiblingElement("database");
            }

            if (ret->mDatabases->size() < 1) {
              ret->mDatabases = DatabaseInfoListPtr();
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr ListSubscribeNotify::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr databasesEl = Element::create("databases");

          if (hasAttribute(AttributeType_DatabasesBefore)) {
            databasesEl->setAttribute("before", mBefore);
          }

          if (hasAttribute(AttributeType_DatabasesVersion)) {
            databasesEl->setAttribute("version", mVersion);
          }

          if (hasAttribute(AttributeType_Databases)) {
            for (auto iter = mDatabases->begin(); iter != mDatabases->end(); ++iter)
            {
              const DatabaseInfo &info = (*iter);
              if (info.hasData()) {
                databasesEl->adoptAsLastChild(info.createElement());
              }
            }
          }

          if ((databasesEl->hasChildren()) ||
              (databasesEl->hasAttributes())) {
            rootEl->adoptAsLastChild(databasesEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool ListSubscribeNotify::hasAttribute(AttributeTypes type) const
        {
          switch (type) {
            case AttributeType_DatabasesBefore:       return mBefore.hasData();
            case AttributeType_DatabasesVersion:      return mVersion.hasData();
            case AttributeType_Databases:             return (bool)mDatabases;
          }
          return false;
        }
        
      }
    }
  }
}
