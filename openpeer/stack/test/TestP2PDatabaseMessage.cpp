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

#include "TestP2PDatabaseMessage.h"
#include "TestSetup.h"

#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeResult.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeNotify.h>

#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/SubscribeResult.h>
#include <openpeer/stack/message/p2p-database/SubscribeNotify.h>

#include <openpeer/stack/message/p2p-database/DataGetRequest.h>
#include <openpeer/stack/message/p2p-database/DataGetResult.h>

#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IStack.h>
#include <openpeer/stack/ISettings.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Proxy.h>
#include <zsLib/XML.h>

#include "config.h"
#include "testing.h"

#include <list>

using zsLib::ULONG;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack, ISettings)

ZS_DECLARE_USING_PTR(openpeer::stack::test, TestP2PDatabaseMessage)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)

ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, ListSubscribeRequest)
ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, ListSubscribeResult)
ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, ListSubscribeNotify)

ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, SubscribeRequest)
ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, SubscribeResult)
ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, SubscribeNotify)

ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, DataGetRequest)
ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, DataGetResult)

ZS_DECLARE_USING_PTR(openpeer::stack::message::p2p_database, MessageFactoryP2PDatabase)

namespace openpeer { namespace stack { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_stack_test) } } }

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IHelper, UseStackHelper)
      ZS_DECLARE_TYPEDEF_PTR(openpeer::services::IHelper, UseServicesHelper)

      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::Message, Message)
      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::DatabaseInfo, DatabaseInfo)
      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::DatabaseInfoList, DatabaseInfoList)
      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::DatabaseEntryInfo, DatabaseEntryInfo)
      ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::message::DatabaseEntryInfoList, DatabaseEntryInfoList)

      //-----------------------------------------------------------------------
      TestP2PDatabaseMessage::TestP2PDatabaseMessage()
      {
      }

      //-----------------------------------------------------------------------
      TestP2PDatabaseMessage::~TestP2PDatabaseMessage()
      {
        mThisWeak.reset();
      }

      //-----------------------------------------------------------------------
      TestP2PDatabaseMessagePtr TestP2PDatabaseMessage::create()
      {
        TestP2PDatabaseMessagePtr pThis(new TestP2PDatabaseMessage());
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::init()
      {
        ISettings::applyDefaults();

        ISettings::setString(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID, "test-application-id");
        ISettings::setString(OPENPEER_COMMON_SETTING_USER_AGENT, "test-user-agent");
        ISettings::setString(OPENPEER_COMMON_SETTING_DEVICE_ID, "test-device-id");
        ISettings::setString(OPENPEER_COMMON_SETTING_OS, "test-os");
        ISettings::setString(OPENPEER_COMMON_SETTING_SYSTEM, "test-system");
        ISettings::setString(OPENPEER_COMMON_SETTING_INSTANCE_ID, "test-instance-id");
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testMessage()
      {
        testListSubscribeRequest();
        testListSubscribeResult();
        testListSubscribeNotify();
        testSubscribeRequest();
        testSubscribeResult();
        testSubscribeNotify();
        testDataGetRequest();
        testDataGetResult();
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testListSubscribeRequest()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          ListSubscribeRequestPtr request = ListSubscribeRequest::create();
          request->time(now);
          request->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("list-subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(!request->hasAttribute(ListSubscribeRequest::AttributeType_DatabasesVersion))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}")
        }

        {
          ListSubscribeRequestPtr request = ListSubscribeRequest::create();
          request->time(now);
          request->messageID("test-id");
          request->version("test-version");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("list-subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(request->hasAttribute(ListSubscribeRequest::AttributeType_DatabasesVersion))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\"}}}")
        }

        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeRequestPtr request = ListSubscribeRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("list-subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL("", request->version())
          TESTING_CHECK(!request->hasAttribute(ListSubscribeRequest::AttributeType_DatabasesVersion))
        }
        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\"}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeRequestPtr request = ListSubscribeRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("list-subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL("test-version", request->version())
          TESTING_CHECK(request->hasAttribute(ListSubscribeRequest::AttributeType_DatabasesVersion))
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testListSubscribeResult()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          ListSubscribeResultPtr result;

          {
            ListSubscribeRequestPtr request = ListSubscribeRequest::create();
            request->messageID("test-id");

            result = ListSubscribeResult::create(request);
          }

          result->time(now);
          result->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("list-subscribe", result->methodAsString())

          TESTING_EQUAL("", result->appID())

          DocumentPtr doc = result->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}")
        }

        {
          const char *messageStr = "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeResultPtr result = ListSubscribeResult::convert(message);

          TESTING_CHECK(result)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("list-subscribe", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_EQUAL("test-id", result->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(result->time()).count())
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testListSubscribeNotify()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}")
        }

        {
          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\"}}}")
        }
        {
          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");
          notify->completed(true);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\",\"completed\":true}}}")
        }
        {
          ListSubscribeNotifyPtr notify = ListSubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");
          notify->completed(true);

          DatabaseInfoListPtr databases = DatabaseInfoListPtr(new DatabaseInfoList);

          {
            DatabaseInfo info;
            info.mDisposition = DatabaseInfo::Disposition_Add;
            info.mDatabaseID = "foo-a";
            info.mVersion = "ver-a";
            info.mMetaData = UseServicesHelper::toJSON("{\"meta\":\"data-a\"}");
            info.mCreated = now + Seconds(1);
            info.mExpires = now + Seconds(2);

            databases->push_back(info);
          }
          {
            DatabaseInfo info;
            info.mDisposition = DatabaseInfo::Disposition_Update;
            info.mDatabaseID = "foo-b";
            info.mVersion = "ver-b";
            info.mMetaData = UseServicesHelper::toJSON("{\"metaData\":\"data-b\"}");
            info.mCreated = now + Seconds(3);

            databases->push_back(info);
          }
          {
            DatabaseInfo info;
            databases->push_back(info);
          }
          {
            DatabaseInfo info;
            info.mDisposition = DatabaseInfo::Disposition_Remove;
            info.mDatabaseID = "foo-c";
            databases->push_back(info);
          }

          notify->databases(databases);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\",\"completed\":true,\"database\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":\"ver-a\",\"metaData\":{\"meta\":\"data-a\"},\"created\":10001,\"expires\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":\"ver-b\",\"metaData\":\"data-b\",\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}")
        }

        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("", notify->version())
          TESTING_EQUAL(false, notify->completed())
          TESTING_CHECK(!((bool) notify->databases()))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\"}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(false, notify->completed())
          TESTING_CHECK(!((bool) notify->databases()))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\",\"completed\":true}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(true, notify->completed())
          TESTING_CHECK(!((bool) notify->databases()))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(!notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"list-subscribe\",\"databases\":{\"$version\":\"test-version\",\"completed\":true,\"database\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":\"ver-a\",\"metaData\":{\"meta\":\"data-a\"},\"created\":10001,\"expires\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":\"ver-b\",\"metaData\":\"data-b\",\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          ListSubscribeNotifyPtr notify = ListSubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_ListSubscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("list-subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(true, notify->completed())
          TESTING_CHECK((bool) notify->databases())
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesVersion))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_DatabasesCompleted))
          TESTING_CHECK(notify->hasAttribute(ListSubscribeNotify::AttributeType_Databases))

          TESTING_CHECK(notify->databases())

          int loop = 0;
          for (auto iter = notify->databases()->begin(); iter != notify->databases()->end(); ++iter, ++loop) {
            auto info = (*iter);
            switch (loop) {
              case 0:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseInfo::Disposition_Add)
                TESTING_EQUAL(info.mDatabaseID, "foo-a")
                TESTING_EQUAL(info.mVersion, "ver-a")
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":{\"meta\":\"data-a\"}}")
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(1)).count())
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mExpires).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(2)).count())
                break;
              }
              case 1:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseInfo::Disposition_Update)
                TESTING_EQUAL(info.mDatabaseID, "foo-b")
                TESTING_EQUAL(info.mVersion, "ver-b")
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":\"data-b\"}")
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(3)).count())
                TESTING_CHECK(Time() == info.mExpires)
                break;
              }
              case 2:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseInfo::Disposition_Remove)
                TESTING_EQUAL(info.mDatabaseID, "foo-c")
                TESTING_EQUAL(info.mVersion, "")
                TESTING_CHECK(! ((bool)info.mMetaData))
                TESTING_CHECK(Time() == info.mCreated)
                TESTING_CHECK(Time() == info.mExpires)
                break;
              }
              default: {
                TESTING_CHECK(false)
                break;
              }
            }
          }

          TESTING_EQUAL(loop, 3)
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testSubscribeRequest()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          SubscribeRequestPtr request = SubscribeRequest::create();
          request->time(now);
          request->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseVersion))
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseData))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}")
        }

        {
          SubscribeRequestPtr request = SubscribeRequest::create();
          request->time(now);
          request->messageID("test-id");
          request->databaseID("database-a");
          request->version("test-version");
          request->data(true);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseID))
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseVersion))
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseData))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$id\":\"database-a\",\"$version\":\"test-version\",\"data\":true}}}")
        }

        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeRequestPtr request = SubscribeRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL("", request->version())
          TESTING_EQUAL("", request->databaseID())
          TESTING_EQUAL(false, request->data())
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseVersion))
          TESTING_CHECK(!request->hasAttribute(SubscribeRequest::AttributeType_DatabaseData))
        }
        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$id\":\"database-a\",\"$version\":\"test-version\",\"data\":true}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeRequestPtr request = SubscribeRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("subscribe", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL(request->databaseID(), "database-a");
          TESTING_EQUAL(request->version(), "test-version");
          TESTING_EQUAL(request->data(), true);
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseID))
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseVersion))
          TESTING_CHECK(request->hasAttribute(SubscribeRequest::AttributeType_DatabaseData))
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testSubscribeResult()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          SubscribeResultPtr result;

          {
            SubscribeRequestPtr request = SubscribeRequest::create();
            request->messageID("test-id");

            result = SubscribeResult::create(request);
          }

          result->time(now);
          result->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("subscribe", result->methodAsString())

          TESTING_EQUAL("", result->appID())

          DocumentPtr doc = result->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}")
        }

        {
          const char *messageStr = "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeResultPtr result = SubscribeResult::convert(message);

          TESTING_CHECK(result)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("subscribe", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_EQUAL("test-id", result->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(result->time()).count())
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testSubscribeNotify()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          SubscribeNotifyPtr notify = SubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}")
        }

        {
          SubscribeNotifyPtr notify = SubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\"}}}")
        }
        {
          SubscribeNotifyPtr notify = SubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");
          notify->completed(true);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\",\"completed\":true}}}")
        }
        {
          SubscribeNotifyPtr notify = SubscribeNotify::create();
          notify->time(now);
          notify->messageID("test-id");
          notify->version("test-version");
          notify->completed(true);

          DatabaseEntryInfoListPtr entries = DatabaseEntryInfoListPtr(new DatabaseEntryInfoList);

          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Add;
            info.mEntryID = "foo-a";
            info.mVersion = 1;
            info.mMetaData = UseServicesHelper::toJSON("{\"meta\":\"data-a\"}");
            info.mData = UseServicesHelper::toJSON("{\"data\":\"bar-a\"}");
            info.mCreated = now + Seconds(1);
            info.mUpdated = now + Seconds(2);

            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Update;
            info.mEntryID = "foo-b";
            info.mVersion = 2;
            info.mMetaData = UseServicesHelper::toJSON("{\"metaData\":\"data-b\"}");
            info.mData = UseServicesHelper::toJSON("{\"data2\":\"bar-b\"}");
            info.mDataSize = UseServicesHelper::toString(info.mData).length();
            info.mCreated = now + Seconds(3);

            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Remove;
            info.mEntryID = "foo-c";
            entries->push_back(info);
          }

          notify->entries(entries);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))

          DocumentPtr doc = notify->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\",\"completed\":true,\"entries\":{\"entry\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":1,\"metaData\":{\"meta\":\"data-a\"},\"data\":\"bar-a\",\"created\":10001,\"updated\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":2,\"metaData\":\"data-b\",\"data\":{\"data2\":\"bar-b\"},\"size\":17,\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}}")
        }

        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeNotifyPtr notify = SubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("", notify->version())
          TESTING_EQUAL(false, notify->completed())
          TESTING_CHECK(!((bool) notify->entries()))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\"}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeNotifyPtr notify = SubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(false, notify->completed())
          TESTING_CHECK(!((bool) notify->entries()))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\",\"completed\":true}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeNotifyPtr notify = SubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(true, notify->completed())
          TESTING_CHECK(!((bool) notify->entries()))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(!notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"notify\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"subscribe\",\"database\":{\"$version\":\"test-version\",\"completed\":true,\"entries\":{\"entry\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":1,\"metaData\":{\"meta\":\"data-a\"},\"data\":\"bar-a\",\"created\":10001,\"updated\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":2,\"metaData\":\"data-b\",\"data\":{\"data2\":\"bar-b\"},\"size\":17,\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          SubscribeNotifyPtr notify = SubscribeNotify::convert(message);

          TESTING_CHECK(notify)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_Subscribe == ((MessageFactoryP2PDatabase::Methods) notify->method()))
          TESTING_EQUAL("subscribe", notify->methodAsString())

          TESTING_EQUAL("", notify->appID())
          TESTING_EQUAL("test-id", notify->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(notify->time()).count())
          TESTING_EQUAL("test-version", notify->version())
          TESTING_EQUAL(true, notify->completed())
          TESTING_CHECK((bool) notify->entries())
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseVersion))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseCompleted))
          TESTING_CHECK(notify->hasAttribute(SubscribeNotify::AttributeType_DatabaseEntries))

          TESTING_CHECK(notify->entries())

          int loop = 0;
          for (auto iter = notify->entries()->begin(); iter != notify->entries()->end(); ++iter, ++loop) {
            auto info = (*iter);
            switch (loop) {
              case 0:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Add)
                TESTING_EQUAL(info.mEntryID, "foo-a")
                TESTING_EQUAL(info.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":{\"meta\":\"data-a\"}}")
                TESTING_EQUAL(UseServicesHelper::toString(info.mData), "{\"data\":\"bar-a\"}")
                TESTING_EQUAL(info.mDataSize, 0)
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(1)).count())
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mUpdated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(2)).count())
                break;
              }
              case 1:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Update)
                TESTING_EQUAL(info.mEntryID, "foo-b")
                TESTING_EQUAL(info.mVersion, 2)
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":\"data-b\"}")
                TESTING_EQUAL(UseServicesHelper::toString(info.mData), "{\"data\":{\"data2\":\"bar-b\"}}")
                TESTING_EQUAL(info.mDataSize, 17)
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(3)).count())
                TESTING_CHECK(Time() == info.mUpdated)
                break;
              }
              case 2:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Remove)
                TESTING_EQUAL(info.mEntryID, "foo-c")
                TESTING_EQUAL(info.mVersion, 0)
                TESTING_CHECK(! ((bool)info.mMetaData))
                TESTING_CHECK(! ((bool)info.mData))
                TESTING_EQUAL(info.mDataSize, 0)
                TESTING_CHECK(Time() == info.mCreated)
                TESTING_CHECK(Time() == info.mUpdated)
                break;
              }
              default: {
                TESTING_CHECK(false)
                break;
              }
            }
          }
          
          TESTING_EQUAL(loop, 3)
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testDataGetRequest()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          DataGetRequestPtr request = DataGetRequest::create();
          request->time(now);
          request->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\"}}")
        }

        {
          DataGetRequestPtr request = DataGetRequest::create();
          request->time(now);
          request->messageID("test-id");
          request->databaseID("database-a");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"$id\":\"database-a\"}}}")
        }

        {
          DataGetRequestPtr request = DataGetRequest::create();
          request->time(now);
          request->messageID("test-id");
          request->databaseID("database-a");

          {
            DataGetRequest::EntryIDList entries;

            entries.push_back("entry-a");
            entries.push_back("entry-b");
            entries.push_back(String());
            entries.push_back("entry-c");

            request->entries(entries);
          }

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))

          DocumentPtr doc = request->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"$id\":\"database-a\",\"entries\":{\"entry\":[\"entry-a\",\"entry-b\",\"entry-c\"]}}}}")
        }

        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          DataGetRequestPtr request = DataGetRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL("", request->databaseID())
          TESTING_CHECK(request->entries().size() < 1)
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"$id\":\"database-a\"}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          DataGetRequestPtr request = DataGetRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL(request->databaseID(), "database-a");
          TESTING_CHECK(request->entries().size() < 1)
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(!request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"request\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"$id\":\"database-a\",\"entries\":{\"entry\":[\"entry-a\",\"entry-b\",\"entry-c\"]}}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          DataGetRequestPtr request = DataGetRequest::convert(message);

          TESTING_CHECK(request)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) request->method()))
          TESTING_EQUAL("data-get", request->methodAsString())

          TESTING_EQUAL("", request->appID())
          TESTING_EQUAL("test-id", request->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(request->time()).count())
          TESTING_EQUAL(request->databaseID(), "database-a");
          TESTING_EQUAL(request->entries().size(), 3)
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseID))
          TESTING_CHECK(request->hasAttribute(DataGetRequest::AttributeType_DatabaseEntries))

          int loop = 0;
          for (auto iter = request->entries().begin(); iter != request->entries().end(); ++iter, ++loop) {
            auto info = (*iter);
            switch (loop) {
              case 0:
              {
                TESTING_EQUAL(info, "entry-a")
                break;
              }
              case 1:
              {
                TESTING_EQUAL(info, "entry-b")
                break;
              }
              case 2:
              {
                TESTING_EQUAL(info, "entry-c")
                break;
              }
              default: {
                TESTING_CHECK(false)
                break;
              }
            }
          }

          TESTING_EQUAL(loop, 3)
        }
      }

      //-----------------------------------------------------------------------
      void TestP2PDatabaseMessage::testDataGetResult()
      {
        Time now = zsLib::timeSinceEpoch(Seconds(10000));

        {
          DataGetResultPtr result;

          {
            DataGetRequestPtr request = DataGetRequest::create();
            request->messageID("test-id");

            result = DataGetResult::create(request);
          }

          result->time(now);
          result->messageID("test-id");

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("data-get", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_CHECK(!result->hasAttribute(DataGetResult::AttributeType_DatabaseEntries))

          DocumentPtr doc = result->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\"}}")
        }

        {
          DataGetResultPtr result;

          {
            DataGetRequestPtr request = DataGetRequest::create();
            request->messageID("test-id");

            result = DataGetResult::create(request);
          }

          result->time(now);
          result->messageID("test-id");

          DatabaseEntryInfoListPtr entries = DatabaseEntryInfoListPtr(new DatabaseEntryInfoList);

          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Add;
            info.mEntryID = "foo-a";
            info.mVersion = 1;
            info.mMetaData = UseServicesHelper::toJSON("{\"meta\":\"data-a\"}");
            info.mData = UseServicesHelper::toJSON("{\"data\":\"bar-a\"}");
            info.mCreated = now + Seconds(1);
            info.mUpdated = now + Seconds(2);

            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Update;
            info.mEntryID = "foo-b";
            info.mVersion = 2;
            info.mMetaData = UseServicesHelper::toJSON("{\"metaData\":\"data-b\"}");
            info.mData = UseServicesHelper::toJSON("{\"data2\":\"bar-b\"}");
            info.mDataSize = UseServicesHelper::toString(info.mData).length();
            info.mCreated = now + Seconds(3);

            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            entries->push_back(info);
          }
          {
            DatabaseEntryInfo info;
            info.mDisposition = DatabaseEntryInfo::Disposition_Remove;
            info.mEntryID = "foo-c";
            entries->push_back(info);
          }

          result->entries(entries);

          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("data-get", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_CHECK(result->hasAttribute(DataGetResult::AttributeType_DatabaseEntries))

          DocumentPtr doc = result->encode();

          TESTING_CHECK(doc)

          ElementPtr rootEl = doc->getFirstChildElement();
          TESTING_CHECK(rootEl)

          String encodeStr = UseServicesHelper::toString(rootEl);

          TESTING_EQUAL(encodeStr, "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"entries\":{\"entry\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":1,\"metaData\":{\"meta\":\"data-a\"},\"data\":\"bar-a\",\"created\":10001,\"updated\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":2,\"metaData\":\"data-b\",\"data\":{\"data2\":\"bar-b\"},\"size\":17,\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}}")
        }

        {
          const char *messageStr = "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\"}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          DataGetResultPtr result = DataGetResult::convert(message);

          TESTING_CHECK(result)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("data-get", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_EQUAL("test-id", result->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(result->time()).count())
          TESTING_CHECK(!((bool) result->entries()))
          TESTING_CHECK(!result->hasAttribute(DataGetResult::AttributeType_DatabaseEntries))
        }
        {
          const char *messageStr = "{\"result\":{\"$timestamp\":10000,\"$handler\":\"p2p-database\",\"$id\":\"test-id\",\"$method\":\"data-get\",\"database\":{\"entries\":{\"entry\":[{\"$id\":\"foo-a\",\"$disposition\":\"add\",\"$version\":1,\"metaData\":{\"meta\":\"data-a\"},\"data\":\"bar-a\",\"created\":10001,\"updated\":10002},{\"$id\":\"foo-b\",\"$disposition\":\"update\",\"$version\":2,\"metaData\":\"data-b\",\"data\":{\"data2\":\"bar-b\"},\"size\":17,\"created\":10003},{\"$id\":\"foo-c\",\"$disposition\":\"remove\"}]}}}}";

          MessagePtr message = MessageFactoryP2PDatabase::singleton()->create(UseServicesHelper::toJSON(messageStr), IMessageSourcePtr());
          TESTING_CHECK(message)

          DataGetResultPtr result = DataGetResult::convert(message);

          TESTING_CHECK(result)
          TESTING_CHECK(MessageFactoryP2PDatabase::Method_DataGet == ((MessageFactoryP2PDatabase::Methods) result->method()))
          TESTING_EQUAL("data-get", result->methodAsString())

          TESTING_EQUAL("", result->appID())
          TESTING_EQUAL("test-id", result->messageID())
          TESTING_EQUAL(Seconds(10000).count(), zsLib::timeSinceEpoch<Seconds>(result->time()).count())
          TESTING_CHECK((bool) result->entries())
          TESTING_CHECK(result->hasAttribute(DataGetResult::AttributeType_DatabaseEntries))

          TESTING_CHECK(result->entries())

          int loop = 0;
          for (auto iter = result->entries()->begin(); iter != result->entries()->end(); ++iter, ++loop) {
            auto info = (*iter);
            switch (loop) {
              case 0:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Add)
                TESTING_EQUAL(info.mEntryID, "foo-a")
                TESTING_EQUAL(info.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":{\"meta\":\"data-a\"}}")
                TESTING_EQUAL(UseServicesHelper::toString(info.mData), "{\"data\":\"bar-a\"}")
                TESTING_EQUAL(info.mDataSize, 0)
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(1)).count())
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mUpdated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(2)).count())
                break;
              }
              case 1:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Update)
                TESTING_EQUAL(info.mEntryID, "foo-b")
                TESTING_EQUAL(info.mVersion, 2)
                TESTING_EQUAL(UseServicesHelper::toString(info.mMetaData), "{\"metaData\":\"data-b\"}")
                TESTING_EQUAL(UseServicesHelper::toString(info.mData), "{\"data\":{\"data2\":\"bar-b\"}}")
                TESTING_EQUAL(info.mDataSize, 17)
                TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(info.mCreated).count(), zsLib::timeSinceEpoch<Seconds>(now + Seconds(3)).count())
                TESTING_CHECK(Time() == info.mUpdated)
                break;
              }
              case 2:
              {
                TESTING_EQUAL(info.mDisposition, DatabaseEntryInfo::Disposition_Remove)
                TESTING_EQUAL(info.mEntryID, "foo-c")
                TESTING_EQUAL(info.mVersion, 0)
                TESTING_CHECK(! ((bool)info.mMetaData))
                TESTING_CHECK(! ((bool)info.mData))
                TESTING_EQUAL(info.mDataSize, 0)
                TESTING_CHECK(Time() == info.mCreated)
                TESTING_CHECK(Time() == info.mUpdated)
                break;
              }
              default: {
                TESTING_CHECK(false)
                break;
              }
            }
          }
          
          TESTING_EQUAL(loop, 3)
        }
      }

    }
  }
}

void doTestP2PDatabaseMessage()
{
  if (!OPENPEER_STACK_TEST_DO_P2P_DATABASE_MESSAGE_TEST) return;

  TESTING_INSTALL_LOGGER();

  TestSetupPtr setup = TestSetup::singleton();

  TestP2PDatabaseMessagePtr testObject = TestP2PDatabaseMessage::create();

  testObject->testMessage();

  std::cout << "COMPLETE:     P2P database message complete.\n";

  TESTING_UNINSTALL_LOGGER()
}
