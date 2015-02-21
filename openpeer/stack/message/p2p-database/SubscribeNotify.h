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

#pragma once

#include <openpeer/stack/message/MessageNotify.h>
#include <openpeer/stack/message/p2p-database/MessageFactoryP2PDatabase.h>

#include <openpeer/stack/ILocationDatabase.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace p2p_database
      {
        class SubscribeNotify : public MessageNotify
        {
        public:
          typedef std::list<IPublicationMetaDataPtr> PublicationList;

          enum AttributeTypes
          {
            AttributeType_DatabaseID,
            AttributeType_DatabaseBefore,
            AttributeType_DatabaseVersion,
            AttributeType_DatabaseEntries,
          };

          ZS_DECLARE_TYPEDEF_PTR(ILocationDatabase::EntryInfo, EntryInfo)
          ZS_DECLARE_TYPEDEF_PTR(ILocationDatabase::EntryInfoList, EntryInfoList)

        public:
          static SubscribeNotifyPtr convert(MessagePtr message);

          static SubscribeNotifyPtr create();

          static SubscribeNotifyPtr create(
                                           ElementPtr root,
                                           IMessageSourcePtr messageSource
                                           );

          virtual DocumentPtr encode();

          virtual Methods method() const                            {return (Message::Methods)MessageFactoryP2PDatabase::Method_Subscribe;}

          virtual IMessageFactoryPtr factory() const                {return MessageFactoryP2PDatabase::singleton();}

          bool hasAttribute(AttributeTypes type) const;

          const String &databaseID() const                          {return mDatabaseID;}
          void databaseID(const String &value)                      {mDatabaseID = value;}

          const String &before() const                              {return mBefore;}
          void before(const String &value)                          {mBefore = value;}

          const String &version() const                             {return mVersion;}
          void version(const String &value)                         {mVersion = value;}

          EntryInfoListPtr entries() const                          {return mEntries;}
          void entries(EntryInfoListPtr value)                      {mEntries = value;}

        protected:
          SubscribeNotify();

          String mDatabaseID;
          String mBefore;
          String mVersion;
          bool mCompleted {false};

          EntryInfoListPtr mEntries;
        };
      }
    }
  }
}
