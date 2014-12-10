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

#include "TestPushMailboxDB.h"
#include "TestSetup.h"

#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Proxy.h>

#include <openpeer/stack/IStack.h>
#include <openpeer/stack/ICache.h>
#include <openpeer/stack/ISettings.h>

#include "config.h"
#include "testing.h"

#include <list>
#include <sys/stat.h>

using zsLib::ULONG;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack, ICache)
ZS_DECLARE_USING_PTR(openpeer::stack, ISettings)

ZS_DECLARE_USING_PTR(openpeer::stack::test, TestPushMailboxDB)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)

ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IServicePushMailboxSessionDatabaseAbstractionFactory, IUseAbstractionFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServicePushMailboxSessionDatabaseAbstractionFactory, UseAbstractionFactory)


namespace openpeer { namespace stack { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_stack_test) } } }

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark OverrideAbstractionFactory
      #pragma mark

      interaction OverrideAbstractionFactory : public IUseAbstractionFactory
      {
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServicePushMailboxSessionDatabaseAbstraction, UseAbstraction)

        static OverrideAbstractionFactoryPtr create()
        {
          return OverrideAbstractionFactoryPtr(new OverrideAbstractionFactory);
        }

        virtual UseAbstractionPtr create(
                                         const char *inHashRepresentingUser,
                                         const char *inUserTemporaryFilePath,
                                         const char *inUserStorageFilePath
                                         )
        {
          return OverridePushMailboxAbstraction::create(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath);
        }

      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark OverridePushMailboxAbstraction
      #pragma mark

      //-----------------------------------------------------------------------
      OverridePushMailboxAbstraction::OverridePushMailboxAbstraction(
                                                                     const char *inHashRepresentingUser,
                                                                     const char *inUserTemporaryFilePath,
                                                                     const char *inUserStorageFilePath
                                                                     ) :
        UseAbstraction(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath)
      {
      }

      //-----------------------------------------------------------------------
      OverridePushMailboxAbstractionPtr OverridePushMailboxAbstraction::convert(IUseAbstractionPtr abstraction)
      {
        return ZS_DYNAMIC_PTR_CAST(OverridePushMailboxAbstraction, abstraction);
      }

      //-----------------------------------------------------------------------
      OverridePushMailboxAbstractionPtr OverridePushMailboxAbstraction::create(
                                                                               const char *inHashRepresentingUser,
                                                                               const char *inUserTemporaryFilePath,
                                                                               const char *inUserStorageFilePath
                                                                               )
      {
        OverridePushMailboxAbstractionPtr pThis(new OverridePushMailboxAbstraction(inHashRepresentingUser, inUserTemporaryFilePath, inUserStorageFilePath));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void OverridePushMailboxAbstraction::init()
      {
        UseAbstraction::init();
      }

      //-----------------------------------------------------------------------
      OverridePushMailboxAbstraction::SqlDatabasePtr OverridePushMailboxAbstraction::getDatabase()
      {
        return mDB;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark TestPushMailboxDB
      #pragma mark

      //-----------------------------------------------------------------------
      TestPushMailboxDB::TestPushMailboxDB()
      {
        mFactory = OverrideAbstractionFactory::create();

        UseAbstractionFactory::override(mFactory);
      }

      //-----------------------------------------------------------------------
      TestPushMailboxDB::~TestPushMailboxDB()
      {
        mThisWeak.reset();

        UseAbstractionFactory::override(UseAbstractionFactoryPtr());
      }

      //-----------------------------------------------------------------------
      TestPushMailboxDBPtr TestPushMailboxDB::create()
      {
        TestPushMailboxDBPtr pThis(new TestPushMailboxDB());
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::init()
      {
        ISettings::applyDefaults();
        ISettings::setString("openpeer/stack/cache-database-path", "/tmp");
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testCreate()
      {
        mkdir("/tmp/testpushdb", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        mkdir("/tmp/testpushdb/tmp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        mkdir("/tmp/testpushdb/users", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        mAbstraction = IUseAbstraction::create("abcdef", "/tmp/testpushdb/tmp", "/tmp/testpushdb/users");
        mOverride = OverridePushMailboxAbstraction::convert(mAbstraction);

        TESTING_CHECK(mAbstraction)
        TESTING_CHECK(mOverride)

        checkIndexValue(UseTables::Version(), UseTables::Version_name(), 0, UseTables::version, 1);
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkIndexValue(
                                              SqlField *definition,
                                              const char *tableName,
                                              int index,
                                              const char *fieldName,
                                              const char *value
                                              )
      {
        TESTING_CHECK(definition)
        TESTING_CHECK(tableName)

        TESTING_CHECK(mOverride)
        if (!mOverride) return;

        SqlDatabasePtr database = mOverride->getDatabase();
        TESTING_CHECK(database)
        if (!database) return;

        SqlTable table(database->getHandle(), tableName, definition);
        table.open();
        checkIndexValue(&table, index, fieldName, value);
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkIndexValue(
                                              SqlField *definition,
                                              const char *tableName,
                                              int index,
                                              const char *fieldName,
                                              int value
                                              )
      {
        TESTING_CHECK(definition)
        TESTING_CHECK(tableName)

        TESTING_CHECK(mOverride)
        if (!mOverride) return;

        SqlDatabasePtr database = mOverride->getDatabase();
        TESTING_CHECK(database)
        if (!database) return;

        SqlTable table(database->getHandle(), tableName, definition);
        table.open();
        checkIndexValue(&table, index, fieldName, value);
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkIndexValue(
                                              SqlTable *table,
                                              int index,
                                              const char *fieldName,
                                              const char *value
                                              )
      {
        TESTING_CHECK(table)

        SqlRecord *record = table->getRecord(index);

        checkValue(record, fieldName, value);
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkIndexValue(
                                              SqlTable *table,
                                              int index,
                                              const char *fieldName,
                                              int value
                                              )
      {
        TESTING_CHECK(table)

        SqlRecord *record = table->getRecord(index);

        checkValue(record, fieldName, value);
      }
      
      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkValue(
                                         SqlRecord *record,
                                         const char *fieldName,
                                         const char *inValue
                                         )
      {
        TESTING_CHECK(record)
        if (!record) return;

        SqlValue *value = record->getValue(fieldName);
        TESTING_CHECK(value)

        if (!value) return;

        String result = value->asString();

        TESTING_EQUAL(result, String(inValue))
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkValue(
                                         SqlRecord *record,
                                         const char *fieldName,
                                         int inValue
                                         )
      {
        TESTING_CHECK(record)
        if (!record) return;

        SqlValue *value = record->getValue(fieldName);
        TESTING_CHECK(value)

        if (!value) return;

        auto result = value->asInteger();

        TESTING_EQUAL(result, inValue)
      }
    }
  }
}

void doTestPushMailboxDB()
{
  if (!OPENPEER_STACK_TEST_DO_PUSH_MAILBOX_DB_TEST) return;

  TESTING_INSTALL_LOGGER();

  TestSetupPtr setup = TestSetup::singleton();

  TestPushMailboxDBPtr testObject = TestPushMailboxDB::create();

  testObject->testCreate();

  std::cout << "COMPLETE:     Push mailbox db complete.\n";

//  TESTING_CHECK(testObject->mNetworkDone)
//  TESTING_CHECK(testObject->mNetwork->isPreparationComplete())
//  TESTING_CHECK(testObject->mNetwork->wasSuccessful())


  TESTING_UNINSTALL_LOGGER()
}
