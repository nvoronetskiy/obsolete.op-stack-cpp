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

#include <openpeer/stack/message/p2p-database/SubscribeNotify.h>
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
        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;
        using zsLib::Numeric;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "SubscribeNotify");
        }

        //---------------------------------------------------------------------
        SubscribeNotifyPtr SubscribeNotify::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(SubscribeNotify, message);
        }

        //---------------------------------------------------------------------
        SubscribeNotify::SubscribeNotify()
        {
          mAppID.clear();
        }

        //---------------------------------------------------------------------
        SubscribeNotifyPtr SubscribeNotify::create()
        {
          SubscribeNotifyPtr ret(new SubscribeNotify);
          return ret;
        }

        //---------------------------------------------------------------------
        SubscribeNotifyPtr SubscribeNotify::create(
                                                   ElementPtr rootEl,
                                                   IMessageSourcePtr messageSource
                                                   )
        {
          SubscribeNotifyPtr ret(new SubscribeNotify);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr databaseEl = rootEl->findFirstChildElement("database");

          if (databaseEl) {
            ret->mBefore = databaseEl->getAttributeValue("before");
            ret->mVersion = databaseEl->getAttributeValue("version");

            ElementPtr entriesEl = databaseEl->findFirstChildElement("entries");
            if (entriesEl) {
              ret->mEntries = EntryInfoListPtr(new EntryInfoList);

              ElementPtr entryEl = entriesEl->findFirstChildElement("entry");
              while (entryEl) {
                EntryInfo info = EntryInfo::create(entryEl);
                if (info.hasData()) {
                  ret->mEntries->push_back(info);
                }
                entryEl = entryEl->findNextSiblingElement("entry");
              }

              if (ret->mEntries->size() < 1) {
                ret->mEntries = EntryInfoListPtr();
              }
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr SubscribeNotify::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr databaseEl = Element::create("database");

          if (hasAttribute(AttributeType_DatabaseBefore)) {
            databaseEl->setAttribute("before", mBefore);
          }

          if (hasAttribute(AttributeType_DatabaseVersion)) {
            databaseEl->setAttribute("version", mVersion);
          }

          if (hasAttribute(AttributeType_DatabaseEntries)) {
            ElementPtr entriesEl = Element::create("entries");

            for (auto iter = mEntries->begin(); iter != mEntries->end(); ++iter)
            {
              const EntryInfo &info = (*iter);
              if (info.hasData()) {
                entriesEl->adoptAsLastChild(info.createElement());
              }
            }

            if (entriesEl->hasChildren()) {
              databaseEl->adoptAsLastChild(entriesEl);
            }
          }

          if ((databaseEl->hasChildren()) ||
              (databaseEl->hasAttributes())) {
            rootEl->adoptAsLastChild(databaseEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool SubscribeNotify::hasAttribute(AttributeTypes type) const
        {
          switch (type) {
            case AttributeType_DatabaseBefore:      return mBefore.hasData();
            case AttributeType_DatabaseVersion:     return mVersion.hasData();
            case AttributeType_DatabaseEntries:     return (bool)mEntries;
          }
          return false;
        }
        
      }
    }
  }
}
