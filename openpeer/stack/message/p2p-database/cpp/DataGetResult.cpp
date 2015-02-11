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
#include <openpeer/stack/message/p2p-database/DataGetRequest.h>
#include <openpeer/stack/message/p2p-database/DataGetResult.h>

#include <zsLib/XML.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace p2p_database
      {
        //---------------------------------------------------------------------
        DataGetResultPtr DataGetResult::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(DataGetResult, message);
        }

        //---------------------------------------------------------------------
        DataGetResult::DataGetResult()
        {
          mAppID.clear();
        }

        //---------------------------------------------------------------------
        DataGetResultPtr DataGetResult::create(DataGetRequestPtr request)
        {
          DataGetResultPtr ret(new DataGetResult);
          ret->fillFrom(request);
          return ret;
        }

        //---------------------------------------------------------------------
        DataGetResultPtr DataGetResult::create(
                                               ElementPtr rootEl,
                                               IMessageSourcePtr messageSource
                                               )
        {
          DataGetResultPtr ret(new DataGetResult);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr databaseEl = rootEl->findFirstChildElement("database");

          if (databaseEl) {
            ElementPtr entriesEl = databaseEl->findFirstChildElement("entries");
            if (entriesEl) {
              ret->mEntries = DatabaseEntryInfoListPtr(new DatabaseEntryInfoList);

              ElementPtr entryEl = entriesEl->findFirstChildElement("entry");
              while (entryEl) {
                DatabaseEntryInfo info = DatabaseEntryInfo::create(entryEl);
                if (info.hasData()) {
                  ret->mEntries->push_back(info);
                }
                entryEl = entryEl->findNextSiblingElement("entry");
              }

              if (ret->mEntries->size() < 1) {
                ret->mEntries = DatabaseEntryInfoListPtr();
              }
            }
          }
          
          return ret;
        }

        //---------------------------------------------------------------------
        DocumentPtr DataGetResult::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr databaseEl = Element::create("database");

          if (hasAttribute(AttributeType_DatabaseEntries)) {
            ElementPtr entriesEl = Element::create("entries");

            for (auto iter = mEntries->begin(); iter != mEntries->end(); ++iter)
            {
              const DatabaseEntryInfo &info = (*iter);
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
        bool DataGetResult::hasAttribute(DataGetResult::AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_DatabaseEntries:   return (bool)mEntries;
          }
          return MessageResult::hasAttribute((MessageResult::AttributeTypes)type);
        }

      }
    }
  }
}
