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
#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/ISettings.h>

#include <openpeer/services/IHelper.h>

#include "config.h"
#include "testing.h"

#include <list>
#include <sys/stat.h>

using zsLib::ULONG;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack, ICache)
ZS_DECLARE_USING_PTR(openpeer::stack, ISettings)

ZS_DECLARE_TYPEDEF_PTR(openpeer::services::IHelper, UseServicesHelper)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::IHelper, UseStackHelper)

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
      #pragma mark OverridePushMailboxAbstractionFactory
      #pragma mark

      interaction OverridePushMailboxAbstractionFactory : public IUseAbstractionFactory
      {
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServicePushMailboxSessionDatabaseAbstraction, UseAbstraction)

        static OverridePushMailboxAbstractionFactoryPtr create()
        {
          return OverridePushMailboxAbstractionFactoryPtr(new OverridePushMailboxAbstractionFactory);
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
      TestPushMailboxDB::TestPushMailboxDB() :
        mUserHash(UseServicesHelper::randomString(8))
      {
        mFactory = OverridePushMailboxAbstractionFactory::create();

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

        mAbstraction = IUseAbstraction::create(mUserHash, "/tmp/testpushdb/tmp", "/tmp/testpushdb/users");
        mOverride = OverridePushMailboxAbstraction::convert(mAbstraction);

        TESTING_CHECK(mAbstraction)
        TESTING_CHECK(mOverride)

        checkIndexValue(UseTables::Version(), UseTables::Version_name(), 0, UseTables::version, 1);
        checkIndexValue(UseTables::Settings(), UseTables::Settings_name(), 0, UseTables::lastDownloadedVersionForFolders, String());
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testSettings()
      {
        auto settings = mAbstraction->settingsTable();
        TESTING_CHECK(settings)

        {
          String result = settings->getLastDownloadedVersionForFolders();
          TESTING_EQUAL(result, String())
          checkIndexValue(UseTables::Settings(), UseTables::Settings_name(), 0, UseTables::lastDownloadedVersionForFolders, String());
        }
        {
          String value("foo-1");
          settings->setLastDownloadedVersionForFolders(value);
          String result = settings->getLastDownloadedVersionForFolders();
          TESTING_EQUAL(result, value)
          checkIndexValue(UseTables::Settings(), UseTables::Settings_name(), 0, UseTables::lastDownloadedVersionForFolders, value);
        }
        {
          String value("foo-2");
          settings->setLastDownloadedVersionForFolders(value);
          String result = settings->getLastDownloadedVersionForFolders();
          TESTING_EQUAL(result, value)
          checkIndexValue(UseTables::Settings(), UseTables::Settings_name(), 0, UseTables::lastDownloadedVersionForFolders, value);
        }
        {
          String value;
          settings->setLastDownloadedVersionForFolders(value);
          String result = settings->getLastDownloadedVersionForFolders();
          TESTING_EQUAL(result, value)
          checkIndexValue(UseTables::Settings(), UseTables::Settings_name(), 0, UseTables::lastDownloadedVersionForFolders, value);
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testFolder()
      {
        auto folder = mAbstraction->folderTable();
        TESTING_CHECK(folder)

        {
          folder->resetUniqueID(5); // this will not do anything
        }

        String originalUniqueID;

        {
          IUseAbstraction::FolderRecord value;
          value.mFolderName = "inbox";
          value.mTotalUnreadMessages = 5;
          value.mTotalMessages = 7;
          value.mUpdateNext = zsLib::now() + Seconds(5);
          folder->addOrUpdate(value);

          auto index = folder->getIndex("inbox");
          TESTING_EQUAL(index, 1)

          auto resultRecord = folder->getIndexAndUniqueID("inbox");
          TESTING_CHECK(resultRecord)
          TESTING_EQUAL(resultRecord->mIndex, 1)
          TESTING_EQUAL(resultRecord->mFolderName, "inbox")
          TESTING_CHECK(resultRecord->mUniqueID.hasData())
          TESTING_EQUAL(resultRecord->mServerVersion, String())
          TESTING_EQUAL(resultRecord->mDownloadedVersion, String())
          TESTING_EQUAL(resultRecord->mTotalUnreadMessages, 5)
          TESTING_EQUAL(resultRecord->mTotalMessages, 7)
          TESTING_CHECK(zsLib::now() < resultRecord->mUpdateNext)
          TESTING_CHECK(zsLib::now() + Seconds(10) > resultRecord->mUpdateNext)

          originalUniqueID = resultRecord->mUniqueID;

          auto name = folder->getName(index);
          TESTING_EQUAL(name, "inbox")

          folder->updateFolderName(index, "inbox2");

          auto name2 = folder->getName(index);
          TESTING_EQUAL(name2, "inbox2")
        }

        {
          folder->updateServerVersionIfFolderExists("inbox2", "server-1");

          auto resultRecord = folder->getIndexAndUniqueID("inbox2");
          TESTING_EQUAL(resultRecord->mIndex, 1)
          TESTING_EQUAL(resultRecord->mFolderName, "inbox2")
          TESTING_CHECK(resultRecord->mUniqueID.hasData())
          TESTING_EQUAL(resultRecord->mServerVersion, "server-1")
          TESTING_EQUAL(resultRecord->mDownloadedVersion, String())
          TESTING_EQUAL(resultRecord->mTotalUnreadMessages, 5)
          TESTING_EQUAL(resultRecord->mTotalMessages, 7)
          TESTING_CHECK(zsLib::now() < resultRecord->mUpdateNext)
          TESTING_CHECK(zsLib::now() + Seconds(10) > resultRecord->mUpdateNext)
        }

        {
          folder->updateServerVersionIfFolderExists("bogus", "server-2"); // noop
        }

        {
          folder->updateDownloadInfo(1, "download-1", zsLib::now() + Seconds(15));
        }

        {
          folder->resetUniqueID(1);

          auto resultRecord = folder->getIndexAndUniqueID("inbox2");
          TESTING_EQUAL(resultRecord->mIndex, 1)
          TESTING_EQUAL(resultRecord->mFolderName, "inbox2")
          TESTING_CHECK(resultRecord->mUniqueID.hasData())
          TESTING_CHECK(resultRecord->mUniqueID != originalUniqueID)
          TESTING_CHECK(resultRecord->mUniqueID.hasData())
          TESTING_EQUAL(resultRecord->mServerVersion, "server-1")
          TESTING_EQUAL(resultRecord->mDownloadedVersion, "download-1")
          TESTING_EQUAL(resultRecord->mTotalUnreadMessages, 5)
          TESTING_EQUAL(resultRecord->mTotalMessages, 7)
          TESTING_CHECK(zsLib::now() < resultRecord->mUpdateNext)
          TESTING_CHECK(zsLib::now() + Seconds(10) < resultRecord->mUpdateNext)
        }

      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testFolderMessage()
      {
        auto folderMessage = mAbstraction->folderMessageTable();
        TESTING_CHECK(folderMessage)

        {
          folderMessage->flushAll();            // noop
        }
        
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 5;
          record.mMessageID = "foo";
          folderMessage->addToFolderIfNotPresent(record);

          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::indexFolderRecord, 5);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::messageID, "foo");
        }
        
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 6;
          record.mMessageID = "foo";
          folderMessage->addToFolderIfNotPresent(record);

          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::indexFolderRecord, 6);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::messageID, "foo");
        }

        {
          IUseAbstraction::FolderMessageRecord record;      // noop
          record.mIndexFolderRecord = 5;
          record.mMessageID = "foo";
          folderMessage->addToFolderIfNotPresent(record);
        }

        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 4;
          record.mMessageID = "bar";
          folderMessage->addToFolderIfNotPresent(record);

          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, UseTables::indexFolderRecord, 4);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, UseTables::messageID, "bar");
        }

        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 6;
          record.mMessageID = "bar";
          folderMessage->addToFolderIfNotPresent(record);

          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 3, SqlField::id, 4);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 3, UseTables::indexFolderRecord, 6);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 3, UseTables::messageID, "bar");
        }
        
        checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 4);
        
        {
          IUseAbstraction::IFolderMessageTable::IndexListPtr indexes = folderMessage->getWithMessageID("foo");

          int loop = 0;
          for (auto iter = indexes->begin(); iter != indexes->end(); ++iter, ++loop)
          {
            auto value = (*iter);
            switch (loop) {
              case 0:   TESTING_EQUAL(5, value); break;
              case 1:   TESTING_EQUAL(6, value); break;
              default:  TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(loop, 2)
        }

        {
          folderMessage->flushAll();            // remove all four
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0);
        }
        
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 9;
          record.mMessageID = "foo2";
          folderMessage->addToFolderIfNotPresent(record);
          
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::indexFolderRecord, 9);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::messageID, "foo2");
        }
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 8;
          record.mMessageID = "foo2";
          folderMessage->addToFolderIfNotPresent(record);
          
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::indexFolderRecord, 8);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::messageID, "foo2");
        }
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 9;
          record.mMessageID = "foo";
          folderMessage->addToFolderIfNotPresent(record);
          
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 3);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, UseTables::indexFolderRecord, 9);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2, UseTables::messageID, "foo");
        }
        
        {
          folderMessage->removeAllFromFolder(9);
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, SqlField::id, 2);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::indexFolderRecord, 8);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 0, UseTables::messageID, "foo2");
        }
        {
          IUseAbstraction::FolderMessageRecord record;
          record.mIndexFolderRecord = 9;
          record.mMessageID = "foo";
          folderMessage->addToFolderIfNotPresent(record);
          
          checkCount(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 2);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, SqlField::id, 3);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::indexFolderRecord, 9);
          checkIndexValue(UseTables::FolderMessage(), UseTables::FolderMessage_name(), 1, UseTables::messageID, "foo");
        }
      }
      
      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testFolderVersionedMessage()
      {
        auto folderVersionedMessage = mAbstraction->folderVersionedMessageTable();
        TESTING_CHECK(folderVersionedMessage)

        {
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0);
          folderVersionedMessage->flushAll();   // noop
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0);
        }

        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 5;
          record.mMessageID = "foo";
          record.mRemovedFlag = false;

          folderVersionedMessage->add(record);
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::indexFolderRecord, 5);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::messageID, "foo");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::removedFlag, 0);
        }

        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 5;
          record.mMessageID = "foo";
          
          folderVersionedMessage->addRemovedEntryIfNotAlreadyRemoved(record);
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::indexFolderRecord, 5);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::messageID, "foo");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::removedFlag, 1);
        }

        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 6;
          record.mMessageID = "foo2";
          
          folderVersionedMessage->add(record);   // noop
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::indexFolderRecord, 6);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::messageID, "foo2");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::removedFlag, 0);
        }
        
        {
          folderVersionedMessage->removeAllRelatedToFolder(6);
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2);
          
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 6;
          record.mMessageID = "foo2";
          folderVersionedMessage->addRemovedEntryIfNotAlreadyRemoved(record); // noop
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2);
        }
        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 8;
          record.mMessageID = "foo3";
          
          folderVersionedMessage->add(record);   // noop

          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, SqlField::id, 4);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::indexFolderRecord, 8);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::messageID, "foo3");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::removedFlag, 0);
        }

        {
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3);
          folderVersionedMessage->flushAll();   // noop
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0);
        }

        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 10;
          record.mMessageID = "foo4";
          
          folderVersionedMessage->add(record);   // noop
          
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, SqlField::id, 5);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::indexFolderRecord, 10);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::messageID, "foo4");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 0, UseTables::removedFlag, 0);
        }
        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 10;
          record.mMessageID = "foo5";
          
          folderVersionedMessage->add(record);   // noop
          
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, SqlField::id, 6);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::indexFolderRecord, 10);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::messageID, "foo5");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 1, UseTables::removedFlag, 0);
        }
        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 8;
          record.mMessageID = "foo6";
          
          folderVersionedMessage->add(record);   // noop
          
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, SqlField::id, 7);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::indexFolderRecord, 8);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::messageID, "foo6");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 2, UseTables::removedFlag, 0);
        }
        {
          IUseAbstraction::FolderVersionedMessageRecord record;
          record.mIndexFolderRecord = 8;
          record.mMessageID = "foo6";
          
          folderVersionedMessage->addRemovedEntryIfNotAlreadyRemoved(record);   // noop
          
          checkCount(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 4);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3, SqlField::id, 8);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3, UseTables::indexFolderRecord, 8);
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3, UseTables::messageID, "foo6");
          checkIndexValue(UseTables::FolderVersionedMessage(), UseTables::FolderVersionedMessage_name(), 3, UseTables::removedFlag, 1);
        }
        
        {
          auto result = folderVersionedMessage->getBatchAfterIndex(5, 10);
          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto value = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(value.mIndex, 6)
                TESTING_EQUAL(value.mIndexFolderRecord, 10)
                TESTING_EQUAL(value.mMessageID, "foo5")
                TESTING_CHECK(!value.mRemovedFlag)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(1, loop)
        }

        {
          auto result = folderVersionedMessage->getBatchAfterIndex(-1, 8);
          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto value = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(value.mIndex, 7)
                TESTING_EQUAL(value.mIndexFolderRecord, 8)
                TESTING_EQUAL(value.mMessageID, "foo6")
                TESTING_CHECK(!value.mRemovedFlag)
                break;
              }
              case 1: {
                TESTING_EQUAL(value.mIndex, 8)
                TESTING_EQUAL(value.mIndexFolderRecord, 8)
                TESTING_EQUAL(value.mMessageID, "foo6")
                TESTING_CHECK(value.mRemovedFlag)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testMessage()
      {
        auto message = mAbstraction->messageTable();
        TESTING_CHECK(message)
        
        {
          message->addOrUpdate("fooa", "v1a");
          checkCount(UseTables::Message(), UseTables::Message_name(), 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::messageID, "fooa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::serverVersion, "v1a");
        }

        {
          message->addOrUpdate("fooa", "v2a");
          checkCount(UseTables::Message(), UseTables::Message_name(), 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::messageID, "fooa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::serverVersion, "v2a");
        }
        {
          message->addOrUpdate("foob", "v1b");
          checkCount(UseTables::Message(), UseTables::Message_name(), 2);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::messageID, "foob");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::serverVersion, "v1b");
        }
        {
          message->addOrUpdate("fooc", "v1c");
          checkCount(UseTables::Message(), UseTables::Message_name(), 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::messageID, "fooc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::serverVersion, "v1c");
        }

        {
          auto index = message->getIndex("fooa");
          TESTING_EQUAL(1, index)
        }
        {
          auto index = message->getIndex("foob");
          TESTING_EQUAL(2, index)
        }
        {
          auto index = message->getIndex("fooc");
          TESTING_EQUAL(3, index)
        }
        {
          message->remove(2);
          auto index = message->getIndex("foob");
          TESTING_CHECK(index < 0)

          checkCount(UseTables::Message(), UseTables::Message_name(), 2);

          index = message->getIndex("fooa");
          TESTING_EQUAL(1, index)

          index = message->getIndex("fooc");
          TESTING_EQUAL(3, index)
        }

        {
          IUseAbstraction::MessageRecord record;
          record.mIndex = 3;
          record.mDownloadedVersion = "v2c";
          record.mServerVersion = "v2c";
          record.mTo = "alicec";
          record.mFrom = "bobc";
          record.mCC = "debbiec";
          record.mBCC = "charlesc";
          record.mType = "typec";
          record.mMimeType = "text/c-text";
          record.mEncoding = "bogusc";
          record.mPushType = "pushc";

          record.mPushType = "pushc";

          IServicePushMailboxSession::PushInfo info1;
          info1.mServiceType = "apns";
          info1.mCustom = UseServicesHelper::toJSON("{ \"fooc\": \"customc\" }");
          info1.mValues = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value1ca\",\"value1cb\" ] } }");

          IServicePushMailboxSession::PushInfo info2;
          info2.mServiceType = "gcm";
          info2.mCustom = UseServicesHelper::toJSON("{ \"custom2c\": \"custom2c\" }");
          info2.mValues = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2ca\",\"value2cb\" ] } }");

          record.mPushInfos.push_back(info1);
          record.mPushInfos.push_back(info2);

          record.mEncryptedDataLength = 100;
          record.mNeedsNotification = true;

          message->update(record);

          checkCount(UseTables::Message(), UseTables::Message_name(), 2);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, SqlField::id, 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::messageID, "fooc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::downloadedVersion, "v2c");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::serverVersion, "v2c");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::to, "alicec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::from, "bobc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::cc, "debbiec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::bcc, "charlesc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::type, "typec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::mimeType, "text/c-text");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encoding, "bogusc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::pushType, "pushc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::pushInfo, "{\"pushes\":{\"push\":[{\"serviceType\":\"apns\",\"values\":{\"value\":[\"value1ca\",\"value1cb\"]},\"custom\":{\"fooc\":\"customc\"}},{\"serviceType\":\"gcm\",\"values\":{\"value\":[\"value2ca\",\"value2cb\"]},\"custom\":{\"custom2c\":\"custom2c\"}}]}}");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedDataLength, 100);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::needsNotification, 1);

          // make sure other record is not touched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::messageID, "fooa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::serverVersion, "v2a");
        }

        {
          message->updateEncodingAndEncryptedDataLength(1, "bogusa", 1001);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encoding, "bogusa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encryptedDataLength, 1001);

          // check to make sure other record is untouched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encoding, "bogusc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedDataLength, 100);
        }

        {
          message->updateEncodingAndFileNames(3, "bogusc2", "encryptc.enc", "decryptc.dec", 999);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, SqlField::id, 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encoding, "bogusc2");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedFileName, "encryptc.enc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::decryptedFileName, "decryptc.dec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedDataLength, 999);

          // check to make sure other record is untouched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encoding, "bogusa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encryptedFileName, "");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::decryptedFileName, "");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encryptedDataLength, 1001);
        }

        {
          message->updateEncryptionFileName(3, "encryptc2.enc");

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, SqlField::id, 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encoding, "bogusc2");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedFileName, "encryptc2.enc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::decryptedFileName, "decryptc.dec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 1, UseTables::encryptedDataLength, 999);

          // check to make sure other record is untouched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encoding, "bogusa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encryptedFileName, "");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::decryptedFileName, "");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::encryptedDataLength, 1001);
        }

        {
          IUseAbstraction::MessageRecord record;
          record.mMessageID = "food";
          record.mTo = "juand";
          record.mFrom = "edwardd";
          record.mCC = "frankd";
          record.mBCC = "georged";
          record.mType = "simpled";
          record.mMimeType = "text/text-d";
          record.mEncoding = "bogusd";
          record.mPushType = "pushd";

          IServicePushMailboxSession::PushInfo info1;
          info1.mServiceType = "apns";
          info1.mCustom = UseServicesHelper::toJSON("{ \"food\": \"customd\" }");
          info1.mValues = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value1da\",\"value1db\" ] } }");

          IServicePushMailboxSession::PushInfo info2;
          info2.mServiceType = "gcm";
          info2.mCustom = UseServicesHelper::toJSON("{ \"custom2d\": \"custom2d\" }");
          info2.mValues = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2da\",\"value2db\" ] } }");

          record.mPushInfos.push_back(info1);
          record.mPushInfos.push_back(info2);

          Time now = zsLib::now();

          record.mSent = now;
          record.mExpires = now + Hours(2);

          record.mEncryptedData = UseServicesHelper::convertToBuffer("encryptedbufferd");
          record.mEncryptedFileName = "encryptd.enc";
          record.mDecryptedData = UseServicesHelper::convertToBuffer("decryptedbufferd");
          record.mNeedsNotification = true;

          message->insert(record);

          checkCount(UseTables::Message(), UseTables::Message_name(), 3);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, SqlField::id, 4);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::messageID, "food");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::to, "juand");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::from, "edwardd");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::cc, "frankd");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::bcc, "georged");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::type, "simpled");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::mimeType, "text/text-d");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encoding, "bogusd");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::pushType, "pushd");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::pushInfo, "{\"pushes\":{\"push\":[{\"serviceType\":\"apns\",\"values\":{\"value\":[\"value1da\",\"value1db\"]},\"custom\":{\"food\":\"customd\"}},{\"serviceType\":\"gcm\",\"values\":{\"value\":[\"value2da\",\"value2db\"]},\"custom\":{\"custom2d\":\"custom2d\"}}]}}");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::sent, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::expires, (int) (zsLib::timeSinceEpoch<Seconds>(now).count() + zsLib::toSeconds(Hours(2)).count()));
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encryptedData, UseServicesHelper::convertToBase64(*UseServicesHelper::convertToBuffer("encryptedbufferd")));
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encryptedDataLength, (int) UseServicesHelper::convertToBuffer("encryptedbufferd")->SizeInBytes());
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encryptedFileName, "encryptd.enc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::decryptedData, UseServicesHelper::convertToBase64(*UseServicesHelper::convertToBuffer("decryptedbufferd")));
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::hasDecryptedData, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::needsNotification, 1);

          // make sure other record is not touched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::messageID, "fooa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::serverVersion, "v2a");
        }

        {
          IUseAbstraction::MessageRecordPtr record = message->getByIndex(2);
          TESTING_CHECK(!record)
        }

        {
          IUseAbstraction::MessageRecordPtr record = message->getByIndex(4);
          TESTING_CHECK(record)

          TESTING_EQUAL(record->mIndex, 4)
          TESTING_EQUAL(record->mMessageID, "food")
          TESTING_EQUAL(record->mTo, "juand")
          TESTING_EQUAL(record->mFrom, "edwardd")
          TESTING_EQUAL(record->mCC, "frankd")
          TESTING_EQUAL(record->mBCC, "georged")
          TESTING_EQUAL(record->mType, "simpled")
          TESTING_EQUAL(record->mMimeType, "text/text-d")
          TESTING_EQUAL(record->mEncoding, "bogusd")
          TESTING_EQUAL(record->mPushType, "pushd")

          TESTING_EQUAL(record->mPushInfos.size(), 2)

          IServicePushMailboxSession::PushInfo info1 = record->mPushInfos.front();
          record->mPushInfos.pop_front();

          IServicePushMailboxSession::PushInfo info2 = record->mPushInfos.front();
          record->mPushInfos.pop_front();

          TESTING_EQUAL(info1.mServiceType, "apns")
          TESTING_EQUAL(UseServicesHelper::toString(info1.mCustom), "{\"custom\":{\"food\":\"customd\"}}");
          TESTING_EQUAL(UseServicesHelper::toString(info1.mValues), "{\"values\":{\"value\":[\"value1da\",\"value1db\"]}}")

          TESTING_EQUAL(info2.mServiceType, "gcm")
          TESTING_EQUAL(UseServicesHelper::toString(info2.mCustom), "{\"custom\":{\"custom2d\":\"custom2d\"}}")
          TESTING_EQUAL(UseServicesHelper::toString(info2.mValues), "{\"values\":{\"value\":[\"value2da\",\"value2db\"]}}")

          Time now = zsLib::now();

          TESTING_CHECK(now >= record->mSent)
          TESTING_CHECK(now < record->mSent + Seconds(3))

          TESTING_CHECK(now < record->mExpires)
          TESTING_CHECK(now + Hours(2) > record->mExpires)
          TESTING_CHECK(now + Hours(2) < record->mExpires + Seconds(3))

          TESTING_EQUAL(record->mEncryptedFileName, "encryptd.enc")
          TESTING_EQUAL(record->mDecryptedFileName, "")
        }

        {
          IUseAbstraction::MessageRecordPtr record = message->getByMessageID("bogus");
          TESTING_CHECK(!record)
        }

        {
          IUseAbstraction::MessageRecordPtr record = message->getByMessageID("food");
          TESTING_CHECK(record)

          TESTING_EQUAL(record->mIndex, 4)
          TESTING_EQUAL(record->mMessageID, "food")
          TESTING_EQUAL(record->mTo, "juand")
          TESTING_EQUAL(record->mFrom, "edwardd")
          TESTING_EQUAL(record->mCC, "frankd")
          TESTING_EQUAL(record->mBCC, "georged")
          TESTING_EQUAL(record->mType, "simpled")
          TESTING_EQUAL(record->mMimeType, "text/text-d")

          TESTING_CHECK(record->mDecryptedData)
          TESTING_EQUAL(UseServicesHelper::convertToString(*record->mDecryptedData), "decryptedbufferd");
          TESTING_EQUAL(record->mDecryptedFileName, String())

          TESTING_EQUAL(record->mPushType, "pushd")
          TESTING_EQUAL(record->mPushInfos.size(), 2)

          IServicePushMailboxSession::PushInfo info1 = record->mPushInfos.front();
          record->mPushInfos.pop_front();

          IServicePushMailboxSession::PushInfo info2 = record->mPushInfos.front();
          record->mPushInfos.pop_front();

          TESTING_EQUAL(info1.mServiceType, "apns")
          TESTING_EQUAL(UseServicesHelper::toString(info1.mCustom), "{\"custom\":{\"food\":\"customd\"}}");
          TESTING_EQUAL(UseServicesHelper::toString(info1.mValues), "{\"values\":{\"value\":[\"value1da\",\"value1db\"]}}")

          TESTING_EQUAL(info2.mServiceType, "gcm")
          TESTING_EQUAL(UseServicesHelper::toString(info2.mCustom), "{\"custom\":{\"custom2d\":\"custom2d\"}}")
          TESTING_EQUAL(UseServicesHelper::toString(info2.mValues), "{\"values\":{\"value\":[\"value2da\",\"value2db\"]}}")

          Time now = zsLib::now();

          TESTING_CHECK(now >= record->mSent)
          TESTING_CHECK(now < record->mSent + Seconds(3))

          TESTING_CHECK(now < record->mExpires)
          TESTING_CHECK(now + Hours(2) > record->mExpires)
          TESTING_CHECK(now + Hours(2) < record->mExpires + Seconds(3))
        }

        {
          message->updateServerVersionIfExists("bogus", "foobar");

          IUseAbstraction::MessageRecordPtr record = message->getByMessageID("bogus");
          TESTING_CHECK(!record)  // this shoudl not exist (i.e. not created)
        }

        {
          message->updateServerVersionIfExists("food", "server-v2");

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, SqlField::id, 4);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::messageID, "food");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::serverVersion, "server-v2");
        }

        {
          SecureByteBlockPtr encryptedData = UseServicesHelper::convertToBuffer("encrypted-data-test");
          message->updateEncryptedData(4, *encryptedData);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, SqlField::id, 4);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::messageID, "food");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encryptedData, UseServicesHelper::convertToBase64(*encryptedData));
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 2, UseTables::encryptedDataLength, (int) encryptedData->SizeInBytes());
        }

        {
          IUseAbstraction::IMessageTable::MessageNeedingUpdateListPtr updateList = message->getBatchNeedingUpdate();
          TESTING_CHECK(updateList)

          TESTING_EQUAL(updateList->size(), 2)

          int index = 0;
          for (auto iter = updateList->begin(); iter != updateList->end(); ++iter, ++index)
          {
            const IUseAbstraction::IMessageTable::MessageNeedingUpdateInfo &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndexMessageRecord, 1)
                TESTING_EQUAL(info.mMessageID, "fooa")
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mIndexMessageRecord, 4)
                TESTING_EQUAL(info.mMessageID, "food")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          message->addOrUpdate("fooe", "v1e");
          checkCount(UseTables::Message(), UseTables::Message_name(), 4);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::serverVersion, "v1e");
        }
        {
          IUseAbstraction::MessageRecord record;
          record.mIndex = 5;
          record.mDownloadedVersion = "v2e";
          record.mServerVersion = "v2e";
          record.mTo = "alicee";
          record.mFrom = "bobe";
          record.mCC = "debbiee";
          record.mBCC = "charlese";
          record.mType = "typee";
          record.mMimeType = "text/e-text";
          record.mEncoding = "boguse";
          record.mPushType = "pushe";

          record.mEncryptedDataLength = 100;
          record.mNeedsNotification = true;

          message->update(record);

          checkCount(UseTables::Message(), UseTables::Message_name(), 4);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadedVersion, "v2e");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::serverVersion, "v2e");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::to, "alicee");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::from, "bobe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::cc, "debbiee");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::bcc, "charlese");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::type, "typee");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::mimeType, "text/e-text");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::encoding, "boguse");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::encryptedDataLength, 100);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::needsNotification, 1);

          // make sure other record is not touched
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::messageID, "fooa");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 0, UseTables::serverVersion, "v2a");
        }

        {
          IUseAbstraction::MessageRecordListPtr needingData = message->getBatchNeedingData(zsLib::now());
          TESTING_CHECK(needingData)

          TESTING_EQUAL(needingData->size(), 1)

          int index = 0;
          for (auto iter = needingData->begin(); iter != needingData->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                TESTING_EQUAL(info.mEncryptedDataLength, 100)
                TESTING_EQUAL(info.mEncryptedFileName, "")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          Time retryTime = zsLib::now();

          Seconds minRetryAfter = zsLib::timeSinceEpoch<Seconds>(retryTime);

          minRetryAfter = minRetryAfter + Seconds(58);
          Seconds maxRetryAfter = minRetryAfter + Seconds(4);

          message->notifyDownload(5, false);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadFailures, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadRetryAfter, (int) minRetryAfter.count(), (int) maxRetryAfter.count());
        }

        {
          Time retryTime = zsLib::now();

          Seconds minRetryAfter = zsLib::timeSinceEpoch<Seconds>(retryTime);

          minRetryAfter = minRetryAfter + Seconds(60*2) - Seconds(2);
          Seconds maxRetryAfter = minRetryAfter + Seconds(4);

          message->notifyDownload(5, false);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadFailures, 2);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadRetryAfter, (int) minRetryAfter.count(), (int) maxRetryAfter.count());
        }
        {
          Time retryTime = zsLib::now();

          Seconds minRetryAfter = zsLib::timeSinceEpoch<Seconds>(retryTime);

          minRetryAfter = minRetryAfter + Seconds(60*4) - Seconds(2);
          Seconds maxRetryAfter = minRetryAfter + Seconds(4);

          message->notifyDownload(5, false);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadFailures, 3);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadRetryAfter, (int) minRetryAfter.count(), (int) maxRetryAfter.count());
        }

        {
          message->notifyDownload(5, true);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadFailures, 0);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::downloadRetryAfter, 0);
        }

        {
          auto folderMessage = mAbstraction->folderMessageTable();
          TESTING_CHECK(folderMessage)

          {
            folderMessage->flushAll();            // ensure folder message table is empty

            {
              IUseAbstraction::FolderMessageRecord record;
              record.mIndexFolderRecord = 2;
              record.mMessageID = "fooa";
              folderMessage->addToFolderIfNotPresent(record);
            }
            {
              IUseAbstraction::FolderMessageRecord record;
              record.mIndexFolderRecord = 2;
              record.mMessageID = "foob";
              folderMessage->addToFolderIfNotPresent(record);
            }
            {
              IUseAbstraction::FolderMessageRecord record;
              record.mIndexFolderRecord = 3;
              record.mMessageID = "fooc";
              folderMessage->addToFolderIfNotPresent(record);
            }
            {
              IUseAbstraction::FolderMessageRecord record;
              record.mIndexFolderRecord = 3;
              record.mMessageID = "food";
              folderMessage->addToFolderIfNotPresent(record);
            }
            {
              IUseAbstraction::FolderMessageRecord record;
              record.mIndexFolderRecord = 3;
              record.mMessageID = "fooe";
              folderMessage->addToFolderIfNotPresent(record);
            }

            message->updateEncryptionFileName(5, "fooe.enc");
          }

          IUseAbstraction::MessageRecordListPtr needingKeyProcessing = message->getBatchNeedingKeysProcessing(3, "", "");
          TESTING_CHECK(needingKeyProcessing)

          TESTING_EQUAL(needingKeyProcessing->size(), 2)

          int index = 0;
          for (auto iter = needingKeyProcessing->begin(); iter != needingKeyProcessing->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "food")
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          message->notifyKeyingProcessed(5);

          IUseAbstraction::MessageRecordListPtr needingKeyProcessing = message->getBatchNeedingKeysProcessing(3, "", "");
          TESTING_CHECK(needingKeyProcessing)

          TESTING_EQUAL(needingKeyProcessing->size(), 1)

          int index = 0;
          for (auto iter = needingKeyProcessing->begin(); iter != needingKeyProcessing->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "food")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          IUseAbstraction::MessageRecordListPtr needingDecrypting = message->getBatchNeedingDecrypting("");
          TESTING_CHECK(needingDecrypting)

          TESTING_EQUAL(needingDecrypting->size(), 1)

          int index = 0;
          for (auto iter = needingDecrypting->begin(); iter != needingDecrypting->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          message->notifyDecryptLater(5, "keye");

          IUseAbstraction::MessageRecordListPtr needingDecrypting = message->getBatchNeedingDecrypting("");
          TESTING_CHECK(needingDecrypting)

          TESTING_EQUAL(needingDecrypting->size(), 0)
        }

        {
          message->notifyDecryptNowForKeys("keye");

          IUseAbstraction::MessageRecordListPtr needingDecrypting = message->getBatchNeedingDecrypting("");
          TESTING_CHECK(needingDecrypting)

          TESTING_EQUAL(needingDecrypting->size(), 1)

          int index = 0;
          for (auto iter = needingDecrypting->begin(); iter != needingDecrypting->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          message->notifyDecrypted(5, UseServicesHelper::convertToBuffer("decrypted-fooe"), true);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptedData, UseServicesHelper::convertToBase64(*UseServicesHelper::convertToBuffer("decrypted-fooe")));
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptFailure, 0);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::needsNotification, 1);

          {
            IUseAbstraction::MessageRecordListPtr needingDecrypting = message->getBatchNeedingDecrypting("");
            TESTING_CHECK(needingDecrypting)

            TESTING_EQUAL(needingDecrypting->size(), 0)
          }

          message->notifyDecryptNowForKeys("keye");

          {
            IUseAbstraction::MessageRecordListPtr needingDecrypting = message->getBatchNeedingDecrypting("");
            TESTING_CHECK(needingDecrypting)

            TESTING_EQUAL(needingDecrypting->size(), 0)
          }
        }

        {
          SqlDatabasePtr database = mOverride->getDatabase();
          TESTING_CHECK(database)

          SqlRecordSet recordSet(*database);

          recordSet.query(String("UPDATE ") + UseTables::Message_name() + " SET " + UseTables::decryptedData + "='', " + UseTables::needsNotification + "=0, " + UseTables::decryptFailure + "=1 WHERE " + SqlField::id + " = 5");

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptedData, String());
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptFailure, 1);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::needsNotification, 0);

          message->notifyDecrypted(5, "fooe.enc", "fooe.dec", 1);

          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, SqlField::id, 5);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::messageID, "fooe");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::encryptedFileName, "fooe.enc");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptedFileName, "fooe.dec");
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::decryptFailure, 0);
          checkIndexValue(UseTables::Message(), UseTables::Message_name(), 3, UseTables::needsNotification, 1);
        }

        {
          IUseAbstraction::MessageRecordListPtr needingNotification = message->getBatchNeedingNotification();
          TESTING_CHECK(needingNotification)

          TESTING_EQUAL(needingNotification->size(), 3)

          int index = 0;
          for (auto iter = needingNotification->begin(); iter != needingNotification->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooc")
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mMessageID, "food")
                break;
              }
              case 2: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          message->clearNeedsNotification(3);
          message->clearNeedsNotification(4);

          IUseAbstraction::MessageRecordListPtr needingNotification = message->getBatchNeedingNotification();
          TESTING_CHECK(needingNotification)

          TESTING_EQUAL(needingNotification->size(), 1)

          int index = 0;
          for (auto iter = needingNotification->begin(); iter != needingNotification->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          IUseAbstraction::MessageRecordListPtr needingExpiry = message->getBatchNeedingExpiry(zsLib::now());
          TESTING_CHECK(needingExpiry)

          TESTING_EQUAL(needingExpiry->size(), 0)
        }

        {
          Time now = zsLib::now();
          Seconds expires = zsLib::timeSinceEpoch<Seconds>(now) - Seconds(1);

          SqlDatabasePtr database = mOverride->getDatabase();
          TESTING_CHECK(database)

          SqlRecordSet recordSet(*database);

          recordSet.query(String("UPDATE ") + UseTables::Message_name() + " SET " + UseTables::expires + "=" + zsLib::string(expires.count()) + " WHERE " + SqlField::id + " = 5");

          IUseAbstraction::MessageRecordListPtr needingExpiry = message->getBatchNeedingExpiry(now);
          TESTING_CHECK(needingExpiry)

          TESTING_EQUAL(needingExpiry->size(), 1)

          int index = 0;
          for (auto iter = needingExpiry->begin(); iter != needingExpiry->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mMessageID, "fooe")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testDeliveryState()
      {
        auto deliveryState = mAbstraction->messageDeliveryStateTable();
        TESTING_CHECK(deliveryState)

        {
          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0);
          deliveryState->removeForMessage(3); // noop
          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0);
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordListPtr states = deliveryState->getForMessage(1);
          TESTING_CHECK(states)

          TESTING_EQUAL(states->size(), 0)
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordList states;
          deliveryState->updateForMessage(2, states);

          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0);
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordList states;
          deliveryState->updateForMessage(2, states);

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mIndexMessage = 2;
            record.mFlag = "read";
            record.mURI = "identity://foo.com/a1";
            states.push_back(record);
          }

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mFlag = "read";
            record.mURI = "identity://foo.com/a2";
            states.push_back(record);
          }

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mFlag = "error";
            record.mURI = "identity://foo.com/a3";
            record.mErrorCode = 403;
            record.mErrorReason = "Not found";
            states.push_back(record);
          }

          deliveryState->updateForMessage(2, states);

          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 3);

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::indexMessageRecord, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::flag, "read");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::uri, "identity://foo.com/a1");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorCode, 0);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorReason, String());

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::indexMessageRecord, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::flag, "read");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::uri, "identity://foo.com/a2");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::errorCode, 0);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::errorReason, String());

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::indexMessageRecord, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::flag, "error");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::uri, "identity://foo.com/a3");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::errorCode, 403);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::errorReason, "Not found");
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordList states;
          deliveryState->updateForMessage(2, states);

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mIndexMessage = 2;
            record.mFlag = "delivered";
            record.mURI = "identity://foo.com/a4";
            states.push_back(record);
          }

          deliveryState->updateForMessage(2, states);

          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1);

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::indexMessageRecord, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::flag, "delivered");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::uri, "identity://foo.com/a4");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorCode, 0);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorReason, String());
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordList states;
          deliveryState->updateForMessage(3, states);

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mFlag = "found";
            record.mURI = "identity://foo.com/b1";
            states.push_back(record);
          }

          {
            IUseAbstraction::MessageDeliveryStateRecord record;
            record.mFlag = "pending";
            record.mURI = "identity://foo.com/b2";
            record.mErrorCode = 500;
            record.mErrorReason = "Internal error";
            states.push_back(record);
          }

          deliveryState->updateForMessage(3, states);

          checkCount(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 3);

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::indexMessageRecord, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::flag, "delivered");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::uri, "identity://foo.com/a4");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorCode, 0);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 0, UseTables::errorReason, String());

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::indexMessageRecord, 3);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::flag, "found");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::uri, "identity://foo.com/b1");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::errorCode, 0);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 1, UseTables::errorReason, String());

          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::indexMessageRecord, 3);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::flag, "pending");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::uri, "identity://foo.com/b2");
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::errorCode, 500);
          checkIndexValue(UseTables::MessageDeliveryState(), UseTables::MessageDeliveryState_name(), 2, UseTables::errorReason, "Internal error");
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordListPtr result = deliveryState->getForMessage(2);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageDeliveryStateRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 1)
                TESTING_EQUAL(info.mIndexMessage, 2)
                TESTING_EQUAL(info.mFlag, "delivered")
                TESTING_EQUAL(info.mURI, "identity://foo.com/a4")
                TESTING_EQUAL(info.mErrorCode, 0)
                TESTING_EQUAL(info.mErrorReason, String())
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          IUseAbstraction::MessageDeliveryStateRecordListPtr result = deliveryState->getForMessage(3);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 2)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::MessageDeliveryStateRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mIndexMessage, 3)
                TESTING_EQUAL(info.mFlag, "found")
                TESTING_EQUAL(info.mURI, "identity://foo.com/b1")
                TESTING_EQUAL(info.mErrorCode, 0)
                TESTING_EQUAL(info.mErrorReason, String())
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mIndex, 3)
                TESTING_EQUAL(info.mIndexMessage, 3)
                TESTING_EQUAL(info.mFlag, "pending")
                TESTING_EQUAL(info.mURI, "identity://foo.com/b2")
                TESTING_EQUAL(info.mErrorCode, 500)
                TESTING_EQUAL(info.mErrorReason, "Internal error")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testPendingDelivery()
      {
        auto pendingDelivery = mAbstraction->pendingDeliveryMessageTable();
        TESTING_CHECK(pendingDelivery)

        {
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0);
          pendingDelivery->remove(1); // noop
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0);
        }

        {
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0);
          pendingDelivery->removeByMessageIndex(2); // noop
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0);
        }

        {
          IUseAbstraction::PendingDeliveryMessageRecord record;

          record.mIndexMessage = 3;
          record.mRemoteFolder = "inbox1";
          record.mCopyToSent = true;
          record.mSubscribeFlags = 16;
          record.mEncryptedDataLength = 9999;

          pendingDelivery->insert(record);

          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1);

          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, UseTables::indexMessageRecord, 3);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, UseTables::remoteFolder, "inbox1");
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, UseTables::copyToSent, 1);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, UseTables::subscribeFlags, 16);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 0, UseTables::encryptedDataLength, 9999);
        }

        {
          IUseAbstraction::PendingDeliveryMessageRecord record;

          record.mIndexMessage = 4;
          record.mRemoteFolder = "inbox2";
          record.mCopyToSent = false;
          record.mSubscribeFlags = 64;
          record.mEncryptedDataLength = 555;

          pendingDelivery->insert(record);

          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2);

          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, UseTables::indexMessageRecord, 4);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, UseTables::remoteFolder, "inbox2");
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, UseTables::copyToSent, 0);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, UseTables::subscribeFlags, 64);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1, UseTables::encryptedDataLength, 555);
        }

        {
          IUseAbstraction::PendingDeliveryMessageRecord record;

          record.mIndexMessage = 5;
          record.mRemoteFolder = "inbox3";
          record.mCopyToSent = true;
          record.mSubscribeFlags = 128;
          record.mEncryptedDataLength = 1555;

          pendingDelivery->insert(record);

          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 3);

          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, UseTables::indexMessageRecord, 5);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, UseTables::remoteFolder, "inbox3");
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, UseTables::copyToSent, 1);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, UseTables::subscribeFlags, 128);
          checkIndexValue(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2, UseTables::encryptedDataLength, 1555);
        }

        {
          IUseAbstraction::PendingDeliveryMessageRecordListPtr result = pendingDelivery->getBatchToDeliver();
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 3)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::PendingDeliveryMessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 1)
                TESTING_EQUAL(info.mIndexMessage, 3)
                TESTING_EQUAL(info.mRemoteFolder, "inbox1")
                TESTING_EQUAL(info.mCopyToSent, true)
                TESTING_EQUAL(info.mSubscribeFlags, 16)
                TESTING_EQUAL(info.mEncryptedDataLength, 9999)
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mIndexMessage, 4)
                TESTING_EQUAL(info.mRemoteFolder, "inbox2")
                TESTING_EQUAL(info.mCopyToSent, false)
                TESTING_EQUAL(info.mSubscribeFlags, 64)
                TESTING_EQUAL(info.mEncryptedDataLength, 555)
                break;
              }
              case 2: {
                TESTING_EQUAL(info.mIndex, 3)
                TESTING_EQUAL(info.mIndexMessage, 5)
                TESTING_EQUAL(info.mRemoteFolder, "inbox3")
                TESTING_EQUAL(info.mCopyToSent, true)
                TESTING_EQUAL(info.mSubscribeFlags, 128)
                TESTING_EQUAL(info.mEncryptedDataLength, 1555)
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          pendingDelivery->remove(2);
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 2);

          IUseAbstraction::PendingDeliveryMessageRecordListPtr result = pendingDelivery->getBatchToDeliver();
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 2)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::PendingDeliveryMessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 1)
                TESTING_EQUAL(info.mIndexMessage, 3)
                TESTING_EQUAL(info.mRemoteFolder, "inbox1")
                TESTING_EQUAL(info.mCopyToSent, true)
                TESTING_EQUAL(info.mSubscribeFlags, 16)
                TESTING_EQUAL(info.mEncryptedDataLength, 9999)
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mIndex, 3)
                TESTING_EQUAL(info.mIndexMessage, 5)
                TESTING_EQUAL(info.mRemoteFolder, "inbox3")
                TESTING_EQUAL(info.mCopyToSent, true)
                TESTING_EQUAL(info.mSubscribeFlags, 128)
                TESTING_EQUAL(info.mEncryptedDataLength, 1555)
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          pendingDelivery->removeByMessageIndex(3);
          checkCount(UseTables::PendingDeliveryMessage(), UseTables::PendingDeliveryMessage_name(), 1);

          IUseAbstraction::PendingDeliveryMessageRecordListPtr result = pendingDelivery->getBatchToDeliver();
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::PendingDeliveryMessageRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 3)
                TESTING_EQUAL(info.mIndexMessage, 5)
                TESTING_EQUAL(info.mRemoteFolder, "inbox3")
                TESTING_EQUAL(info.mCopyToSent, true)
                TESTING_EQUAL(info.mSubscribeFlags, 128)
                TESTING_EQUAL(info.mEncryptedDataLength, 1555)
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testList()
      {
        auto listTable = mAbstraction->listTable();
        TESTING_CHECK(listTable)

        {
          IUseAbstraction::index result = listTable->addOrUpdateListID("foo");
          TESTING_EQUAL(result, 1)

          checkCount(UseTables::List(), UseTables::List_name(), 1);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::listID, "foo");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::needsDownload, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadFailures, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadRetryAfter, 0);
        }
        {
          IUseAbstraction::index result = listTable->addOrUpdateListID("bar");
          TESTING_EQUAL(result, 2)

          checkCount(UseTables::List(), UseTables::List_name(), 2);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::listID, "bar");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::needsDownload, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadFailures, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadRetryAfter, 0);
        }
        {
          IUseAbstraction::index result = listTable->addOrUpdateListID("foo");
          TESTING_EQUAL(result, 1)

          checkCount(UseTables::List(), UseTables::List_name(), 2);
        }
        {
          IUseAbstraction::index result = listTable->addOrUpdateListID("bar");
          TESTING_EQUAL(result, 2)

          checkCount(UseTables::List(), UseTables::List_name(), 2);
        }

        {
          bool result = listTable->hasListID("foo");
          TESTING_CHECK(result)
        }
        {
          bool result = listTable->hasListID("bar");
          TESTING_CHECK(result)
        }
        {
          bool result = listTable->hasListID("bogus");
          TESTING_CHECK(!result)
        }

        {
          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(zsLib::now());
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 2)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::ListRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 1)
                TESTING_EQUAL(info.mListID, "foo")
                TESTING_EQUAL(info.mNeedsDownload, true)
                TESTING_EQUAL(info.mDownloadFailures, 0)
                TESTING_CHECK(info.mDownloadRetryAfter == Time())
                break;
              }
              case 1: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mListID, "bar")
                TESTING_EQUAL(info.mNeedsDownload, true)
                TESTING_CHECK(info.mDownloadRetryAfter == Time())
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          listTable->notifyFailedToDownload(1);

          Time now = zsLib::now();
          Seconds min = zsLib::timeSinceEpoch<Seconds>(now) + Seconds(60) - Seconds(2);
          Seconds max = min + Seconds(4);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::listID, "foo");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::needsDownload, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadFailures, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadRetryAfter, (int) min.count(), (int) max.count());

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::ListRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mListID, "bar")
                TESTING_EQUAL(info.mNeedsDownload, true)
                TESTING_CHECK(info.mDownloadRetryAfter == Time())
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          listTable->notifyFailedToDownload(1);

          Time now = zsLib::now();
          Seconds min = zsLib::timeSinceEpoch<Seconds>(now) + Seconds(60*2) - Seconds(2);
          Seconds max = min + Seconds(4);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::listID, "foo");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::needsDownload, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadFailures, 2);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadRetryAfter, (int) min.count(), (int) max.count());

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::ListRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mListID, "bar")
                TESTING_EQUAL(info.mNeedsDownload, true)
                TESTING_CHECK(info.mDownloadRetryAfter == Time())
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          listTable->notifyDownloaded(1);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::listID, "foo");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::needsDownload, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadFailures, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 0, UseTables::downloadRetryAfter, 0);

          Time now = zsLib::now();

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::ListRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mListID, "bar")
                TESTING_EQUAL(info.mNeedsDownload, true)
                TESTING_CHECK(info.mDownloadRetryAfter == Time())
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          listTable->notifyFailedToDownload(2);

          Time now = zsLib::now();
          Seconds min = zsLib::timeSinceEpoch<Seconds>(now) + Seconds(60) - Seconds(2);
          Seconds max = min + Seconds(4);

          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::listID, "bar");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::needsDownload, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadFailures, 1);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadRetryAfter, (int) min.count(), (int) max.count());

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 0)
        }

        {
          Time now = zsLib::now();
          Time fakeNow = zsLib::timeSinceEpoch(zsLib::timeSinceEpoch<Seconds>(now) + Seconds(61));

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(fakeNow);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 1)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::ListRecord &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info.mIndex, 2)
                TESTING_EQUAL(info.mListID, "bar")
                TESTING_EQUAL(info.mNeedsDownload, true)
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          listTable->notifyDownloaded(2);

          Time now = zsLib::now();
          Time fakeNow = zsLib::timeSinceEpoch(zsLib::timeSinceEpoch<Seconds>(now) + Seconds(61));

          IUseAbstraction::ListRecordListPtr result = listTable->getBatchNeedingDownload(fakeNow);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 0)

          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::listID, "bar");
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::needsDownload, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadFailures, 0);
          checkIndexValue(UseTables::List(), UseTables::List_name(), 1, UseTables::downloadRetryAfter, 0);
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testListURI()
      {
        auto listTable = mAbstraction->listTable();
        TESTING_CHECK(listTable)

        auto listURI = mAbstraction->listURITable();
        TESTING_CHECK(listURI)

        {
          int order = listURI->getOrder("bogus", "identity://bogus1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }

        {
          IUseAbstraction::IListURITable::URIList uris;

          IUseAbstraction::index foundIndex = listTable->addOrUpdateListID("foo");

          uris.push_back("identity://fooa1");
          uris.push_back("identity://fooa2");
          uris.push_back("identity://fooa3");

          listURI->update(foundIndex, uris);

          checkCount(UseTables::ListURI(), UseTables::ListURI_name(), 3);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::uri, "identity://fooa1");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::order, 0);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::uri, "identity://fooa2");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::order, 1);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::uri, "identity://fooa3");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::order, 2);
        }

        {
          IUseAbstraction::IListURITable::URIList uris;

          IUseAbstraction::index foundIndex = listTable->addOrUpdateListID("foo");

          uris.push_back("identity://foob1");
          uris.push_back("identity://foob2");
          uris.push_back("identity://foob3");

          listURI->update(foundIndex, uris);

          checkCount(UseTables::ListURI(), UseTables::ListURI_name(), 3);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::uri, "identity://foob1");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 0, UseTables::order, 0);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::uri, "identity://foob2");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 1, UseTables::order, 1);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::uri, "identity://foob3");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 2, UseTables::order, 2);
        }

        {
          IUseAbstraction::IListURITable::URIList uris;

          IUseAbstraction::index foundIndex = listTable->addOrUpdateListID("bar");
          {
            IUseAbstraction::index fooIndex = listTable->addOrUpdateListID("foo");
            TESTING_CHECK(foundIndex != fooIndex)
          }

          uris.push_back("identity://bara1");
          uris.push_back("identity://bara2");

          listURI->update(foundIndex, uris);

          checkCount(UseTables::ListURI(), UseTables::ListURI_name(), 5);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 3, SqlField::id, 4);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 3, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 3, UseTables::uri, "identity://bara1");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 3, UseTables::order, 0);

          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 4, SqlField::id, 5);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 4, UseTables::indexListRecord, foundIndex);
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 4, UseTables::uri, "identity://bara2");
          checkIndexValue(UseTables::ListURI(), UseTables::ListURI_name(), 4, UseTables::order, 1);
        }

        {
          IUseAbstraction::IListURITable::URIListPtr result = listURI->getURIs("foo");
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 3)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::IListURITable::URI &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info, "identity://foob1")
                break;
              }
              case 1: {
                TESTING_EQUAL(info, "identity://foob2")
                break;
              }
              case 2: {
                TESTING_EQUAL(info, "identity://foob3")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          IUseAbstraction::IListURITable::URIListPtr result = listURI->getURIs("bar");
          TESTING_CHECK(result)

          TESTING_EQUAL(result->size(), 2)

          int index = 0;
          for (auto iter = result->begin(); iter != result->end(); ++iter, ++index)
          {
            const IUseAbstraction::IListURITable::URI &info = *iter;

            switch (index)
            {
              case 0: {
                TESTING_EQUAL(info, "identity://bara1")
                break;
              }
              case 1: {
                TESTING_EQUAL(info, "identity://bara2")
                break;
              }
              default:
              {
                TESTING_CHECK(false)  // should not reach here
                break;
              }
            }
          }
        }

        {
          int order = listURI->getOrder("foo", "identity://foob1");
          TESTING_EQUAL(order, 0)
        }
        {
          int order = listURI->getOrder("foo", "identity://foob2");
          TESTING_EQUAL(order, 1)
        }
        {
          int order = listURI->getOrder("foo", "identity://foob3");
          TESTING_EQUAL(order, 2)
        }
        {
          int order = listURI->getOrder("bar", "identity://bara1");
          TESTING_EQUAL(order, 0)
        }
        {
          int order = listURI->getOrder("bar", "identity://bara2");
          TESTING_EQUAL(order, 1)
        }
        {
          int order = listURI->getOrder("bar", "identity://foob1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
        {
          int order = listURI->getOrder("foo", "identity://bara1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
        {
          int order = listURI->getOrder("bogus", "identity://foob1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
        {
          int order = listURI->getOrder("bogus", "identity://bara1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
        {
          int order = listURI->getOrder("foo", "identity://bogus1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
        {
          int order = listURI->getOrder("bar", "identity://bogus1");
          TESTING_EQUAL(order, OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN)
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testKeyDomain()
      {
        auto keyDomain = mAbstraction->keyDomainTable();
        TESTING_CHECK(keyDomain)

        {
          IUseAbstraction::KeyDomainRecordPtr result = keyDomain->getByKeyDomain(4096);
          TESTING_CHECK(!result)
        }

        {
          IUseAbstraction::KeyDomainRecord record;
          record.mKeyDomain = 4096;
          record.mDHStaticPrivateKey = "private1";
          record.mDHStaticPublicKey = "public1";
          keyDomain->add(record);

          checkCount(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 1);

          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 0, UseTables::keyDomain, 4096);
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 0, UseTables::dhStaticPrivateKey, "private1");
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 0, UseTables::dhStaticPublicKey, "public1");
        }

        {
          IUseAbstraction::KeyDomainRecordPtr result = keyDomain->getByKeyDomain(4096);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 1)
          TESTING_EQUAL(result->mKeyDomain, 4096)
          TESTING_EQUAL(result->mDHStaticPrivateKey, "private1")
          TESTING_EQUAL(result->mDHStaticPublicKey, "public1")
        }

        {
          IUseAbstraction::KeyDomainRecordPtr result = keyDomain->getByKeyDomain(1024);
          TESTING_CHECK(!result)
        }

        {
          IUseAbstraction::KeyDomainRecord record;
          record.mKeyDomain = 1024;
          record.mDHStaticPrivateKey = "private2";
          record.mDHStaticPublicKey = "public2";
          keyDomain->add(record);

          checkCount(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 2);

          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 1, UseTables::keyDomain, 1024);
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 1, UseTables::dhStaticPrivateKey, "private2");
          checkIndexValue(UseTables::KeyDomain(), UseTables::KeyDomain_name(), 1, UseTables::dhStaticPublicKey, "public2");
        }

        {
          IUseAbstraction::KeyDomainRecordPtr result = keyDomain->getByKeyDomain(1024);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 2)
          TESTING_EQUAL(result->mKeyDomain, 1024)
          TESTING_EQUAL(result->mDHStaticPrivateKey, "private2")
          TESTING_EQUAL(result->mDHStaticPublicKey, "public2")
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testSendingKey()
      {
        auto sendingKey = mAbstraction->sendingKeyTable();
        TESTING_CHECK(sendingKey)

        {
          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getByKeyID("key1");
          TESTING_CHECK(!result)
        }

        {
          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://foo1", zsLib::now());
          TESTING_CHECK(!result)
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecord record;
          record.mKeyID = "key1";
          record.mURI = "identity://foo1";
          record.mRSAPassphrase = "rsa1";
          record.mDHPassphrase = "dh1";
          record.mKeyDomain = 4096;
          record.mDHEphemeralPrivateKey = "private1";
          record.mDHEphemeralPublicKey = "public1";
          record.mListSize = 5;
          record.mTotalWithDHPassphrase = 3;
          record.mAckDHPassphraseSet = "ack1";
          record.mActiveUntil = activeUntil;
          record.mExpires = expires;

          sendingKey->addOrUpdate(record);

          checkCount(UseTables::SendingKey(), UseTables::SendingKey_name(), 1);

          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::keyID, "key1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::uri, "identity://foo1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::rsaPassphrase, "rsa1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::dhPassphrase, "dh1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::keyDomain, 4096);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::dhEphemeralPrivateKey, "private1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::dhEphemeralPublicKey, "public1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::listSize, 5);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::totalWithDHPassphraseSet, 3);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::ackDHPassphraseSet, "ack1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::activeUntil, (int) zsLib::timeSinceEpoch<Seconds>(activeUntil).count());
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 0, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getByKeyID("key1");
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 1)
          TESTING_EQUAL(result->mKeyID, "key1")
          TESTING_EQUAL(result->mURI, "identity://foo1")
          TESTING_EQUAL(result->mRSAPassphrase, "rsa1")
          TESTING_EQUAL(result->mDHPassphrase, "dh1")
          TESTING_EQUAL(result->mKeyDomain, 4096)
          TESTING_EQUAL(result->mDHEphemeralPrivateKey, "private1")
          TESTING_EQUAL(result->mDHEphemeralPublicKey, "public1")
          TESTING_EQUAL(result->mListSize, 5)
          TESTING_EQUAL(result->mTotalWithDHPassphrase, 3)
          TESTING_EQUAL(result->mAckDHPassphraseSet, "ack1")

          TESTING_CHECK(result->mActiveUntil > (activeUntil - Seconds(2)))
          TESTING_CHECK(result->mActiveUntil < (activeUntil + Seconds(2)))

          TESTING_CHECK(result->mExpires > (expires - Seconds(2)))
          TESTING_CHECK(result->mExpires < (expires + Seconds(2)))
        }

        {
          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getByKeyID("key2");
          TESTING_CHECK(!result)
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://foo1", now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 1)
          TESTING_EQUAL(result->mKeyID, "key1")
          TESTING_EQUAL(result->mURI, "identity://foo1")
          TESTING_EQUAL(result->mRSAPassphrase, "rsa1")
          TESTING_EQUAL(result->mDHPassphrase, "dh1")
          TESTING_EQUAL(result->mKeyDomain, 4096)
          TESTING_EQUAL(result->mDHEphemeralPrivateKey, "private1")
          TESTING_EQUAL(result->mDHEphemeralPublicKey, "public1")
          TESTING_EQUAL(result->mListSize, 5)
          TESTING_EQUAL(result->mTotalWithDHPassphrase, 3)
          TESTING_EQUAL(result->mAckDHPassphraseSet, "ack1")

          TESTING_CHECK(result->mActiveUntil > (activeUntil - Seconds(2)))
          TESTING_CHECK(result->mActiveUntil < (activeUntil + Seconds(2)))

          TESTING_CHECK(result->mExpires > (expires - Seconds(2)))
          TESTING_CHECK(result->mExpires < (expires + Seconds(2)))
        }

        {
          Time now = zsLib::now();
          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://bogus", now);
          TESTING_CHECK(!result)
        }

        {
          Time now = zsLib::now() + Seconds(60) + Seconds(2);

          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://foo1", now);
          TESTING_CHECK(!result)
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecord record;
          record.mKeyID = "key2";
          record.mURI = "identity://foo2";
          record.mRSAPassphrase = "rsa2";
          record.mDHPassphrase = "dh2";
          record.mKeyDomain = 8192;
          record.mDHEphemeralPrivateKey = "private2";
          record.mDHEphemeralPublicKey = "public2";
          record.mListSize = 11;
          record.mTotalWithDHPassphrase = 7;
          record.mAckDHPassphraseSet = "ack2";
          record.mActiveUntil = activeUntil;
          record.mExpires = expires;

          sendingKey->addOrUpdate(record);

          checkCount(UseTables::SendingKey(), UseTables::SendingKey_name(), 2);

          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::keyID, "key2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::uri, "identity://foo2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::rsaPassphrase, "rsa2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::dhPassphrase, "dh2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::keyDomain, 8192);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::dhEphemeralPrivateKey, "private2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::dhEphemeralPublicKey, "public2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::listSize, 11);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::totalWithDHPassphraseSet, 7);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::ackDHPassphraseSet, "ack2");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::activeUntil, (int) zsLib::timeSinceEpoch<Seconds>(activeUntil).count());
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 1, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecord record;
          record.mKeyID = "key3";
          record.mURI = "identity://foo1";
          record.mRSAPassphrase = "rsa3";
          record.mDHPassphrase = "dh3";
          record.mKeyDomain = 1024;
          record.mDHEphemeralPrivateKey = "private3";
          record.mDHEphemeralPublicKey = "public3";
          record.mListSize = 21;
          record.mTotalWithDHPassphrase = 19;
          record.mAckDHPassphraseSet = "ack3";
          record.mActiveUntil = activeUntil;
          record.mExpires = expires;

          sendingKey->addOrUpdate(record);

          checkCount(UseTables::SendingKey(), UseTables::SendingKey_name(), 3);

          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::keyID, "key3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::uri, "identity://foo1");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::rsaPassphrase, "rsa3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhPassphrase, "dh3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::keyDomain, 1024);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhEphemeralPrivateKey, "private3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhEphemeralPublicKey, "public3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::listSize, 21);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::totalWithDHPassphraseSet, 19);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::ackDHPassphraseSet, "ack3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::activeUntil, (int) zsLib::timeSinceEpoch<Seconds>(activeUntil).count());
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }
        
        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(60);
          Time expires = now + Seconds(120);

          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://foo1", now);
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 3)
          TESTING_EQUAL(result->mKeyID, "key3")
          TESTING_EQUAL(result->mURI, "identity://foo1")
          TESTING_EQUAL(result->mRSAPassphrase, "rsa3")
          TESTING_EQUAL(result->mDHPassphrase, "dh3")
          TESTING_EQUAL(result->mKeyDomain, 1024)
          TESTING_EQUAL(result->mDHEphemeralPrivateKey, "private3")
          TESTING_EQUAL(result->mDHEphemeralPublicKey, "public3")
          TESTING_EQUAL(result->mListSize, 21)
          TESTING_EQUAL(result->mTotalWithDHPassphrase, 19)
          TESTING_EQUAL(result->mAckDHPassphraseSet, "ack3")

          TESTING_CHECK(result->mActiveUntil > (activeUntil - Seconds(2)))
          TESTING_CHECK(result->mActiveUntil < (activeUntil + Seconds(2)))

          TESTING_CHECK(result->mExpires > (expires - Seconds(2)))
          TESTING_CHECK(result->mExpires < (expires + Seconds(2)))
        }

        {
          Time now = zsLib::now();

          Time activeUntil = now + Seconds(300);
          Time expires = now + Seconds(360);

          IUseAbstraction::SendingKeyRecordPtr result = sendingKey->getActive("identity://foo1", now);
          TESTING_CHECK(result)

          result->mKeyID = "key3";
          result->mURI = "identity://foo1b";
          result->mRSAPassphrase = "rsa3b";
          result->mDHPassphrase = "dh3b";
          result->mKeyDomain = 2048;
          result->mDHEphemeralPrivateKey = "private3b";
          result->mDHEphemeralPublicKey = "public3b";
          result->mListSize = 43;
          result->mTotalWithDHPassphrase = 31;
          result->mAckDHPassphraseSet = "ack3b";
          result->mActiveUntil = activeUntil;
          result->mExpires = expires;

          sendingKey->addOrUpdate(*result);

          checkCount(UseTables::SendingKey(), UseTables::SendingKey_name(), 3);

          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::keyID, "key3");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::uri, "identity://foo1b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::rsaPassphrase, "rsa3b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhPassphrase, "dh3b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::keyDomain, 2048);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhEphemeralPrivateKey, "private3b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::dhEphemeralPublicKey, "public3b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::listSize, 43);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::totalWithDHPassphraseSet, 31);
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::ackDHPassphraseSet, "ack3b");
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::activeUntil, (int) zsLib::timeSinceEpoch<Seconds>(activeUntil).count());
          checkIndexValue(UseTables::SendingKey(), UseTables::SendingKey_name(), 2, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }

      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testReceivingKey()
      {
        auto receivingKey = mAbstraction->receivingKeyTable();
        TESTING_CHECK(receivingKey)

        {
          IUseAbstraction::ReceivingKeyRecordPtr result = receivingKey->getByKeyID("rkey1");
          TESTING_CHECK(!result)
        }

        {
          Time now = zsLib::now();

          Time expires = now + Seconds(120);

          IUseAbstraction::ReceivingKeyRecord record;
          record.mKeyID = "rkey1";
          record.mURI = "identity://foo1";
          record.mPassphrase = "passphrase1";
          record.mKeyDomain = 4096;
          record.mDHEphemeralPrivateKey = "private1";
          record.mDHEphemeralPublicKey = "public1";
          record.mExpires = expires;

          receivingKey->addOrUpdate(record);

          checkCount(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1);

          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::keyID, "rkey1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::uri, "identity://foo1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::passphrase, "passphrase1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::keyDomain, 4096);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::dhEphemeralPrivateKey, "private1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::dhEphemeralPublicKey, "public1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }

        {
          Time now = zsLib::now();

          Time expires = now + Seconds(120);

          IUseAbstraction::ReceivingKeyRecordPtr result = receivingKey->getByKeyID("rkey1");
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 1)
          TESTING_EQUAL(result->mKeyID, "rkey1")
          TESTING_EQUAL(result->mURI, "identity://foo1")
          TESTING_EQUAL(result->mPassphrase, "passphrase1")
          TESTING_EQUAL(result->mKeyDomain, 4096)
          TESTING_EQUAL(result->mDHEphemeralPrivateKey, "private1")
          TESTING_EQUAL(result->mDHEphemeralPublicKey, "public1")

          TESTING_CHECK(result->mExpires > (expires - Seconds(2)))
          TESTING_CHECK(result->mExpires < (expires + Seconds(2)))
        }

        {
          Time now = zsLib::now();

          Time expires = now + Seconds(120);

          IUseAbstraction::ReceivingKeyRecord record;
          record.mKeyID = "rkey2";
          record.mURI = "identity://foo2";
          record.mPassphrase = "passphrase2";
          record.mKeyDomain = 8192;
          record.mDHEphemeralPrivateKey = "private2";
          record.mDHEphemeralPublicKey = "public2";
          record.mExpires = expires;

          receivingKey->addOrUpdate(record);

          checkCount(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 2);

          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::keyID, "rkey2");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::uri, "identity://foo2");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::passphrase, "passphrase2");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::keyDomain, 8192);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::dhEphemeralPrivateKey, "private2");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::dhEphemeralPublicKey, "public2");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 1, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }
        
        {
          Time now = zsLib::now();

          Time expires = now + Seconds(120);

          IUseAbstraction::ReceivingKeyRecordPtr result = receivingKey->getByKeyID("rkey2");
          TESTING_CHECK(result)

          TESTING_EQUAL(result->mIndex, 2)
          TESTING_EQUAL(result->mKeyID, "rkey2")
          TESTING_EQUAL(result->mURI, "identity://foo2")
          TESTING_EQUAL(result->mPassphrase, "passphrase2")
          TESTING_EQUAL(result->mKeyDomain, 8192)
          TESTING_EQUAL(result->mDHEphemeralPrivateKey, "private2")
          TESTING_EQUAL(result->mDHEphemeralPublicKey, "public2")

          TESTING_CHECK(result->mExpires > (expires - Seconds(2)))
          TESTING_CHECK(result->mExpires < (expires + Seconds(2)))
        }
        
        {
          Time now = zsLib::now();

          Time expires = now + Seconds(300);

          IUseAbstraction::ReceivingKeyRecordPtr result = receivingKey->getByKeyID("rkey1");
          TESTING_CHECK(result)

          result->mKeyID = "rkey1";
          result->mURI = "identity://foo1b";
          result->mPassphrase = "passphrase1b";
          result->mKeyDomain = 1024;
          result->mDHEphemeralPrivateKey = "private1b";
          result->mDHEphemeralPublicKey = "public1b";
          result->mExpires = expires;

          receivingKey->addOrUpdate(*result);

          checkCount(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 2);

          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::keyID, "rkey1");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::uri, "identity://foo1b");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::passphrase, "passphrase1b");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::keyDomain, 1024);
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::dhEphemeralPrivateKey, "private1b");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::dhEphemeralPublicKey, "public1b");
          checkIndexValue(UseTables::ReceivingKey(), UseTables::ReceivingKey_name(), 0, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(expires).count());
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::testStorage()
      {
        auto storage = mAbstraction->storage();
        TESTING_CHECK(storage)

        String tempPrefix = String("/tmp/testpushdb/tmp/") + mUserHash + "_";
        String storagePrefix = String("/tmp/testpushdb/users/") + mUserHash + "_";

        {
          String path = storage->getStorageFileName();
          TESTING_EQUAL(path.substr(0, storagePrefix.length()), storagePrefix)
        }

        {
          SecureByteBlockPtr buffer = UseServicesHelper::random(5000);
          String path = storage->storeToTemporaryFile(*buffer);
          TESTING_EQUAL(path.substr(0, tempPrefix.length()), tempPrefix)

          FILE *file = fopen(path, "rb");
          TESTING_CHECK(NULL != file)

          SecureByteBlock buffer2(5000);

          size_t read = 0;

          while (read != buffer2.SizeInBytes()) {
            auto actualRead = fread(&(buffer2.BytePtr()[read]), sizeof(BYTE), buffer2.SizeInBytes() - read, file);
            TESTING_CHECK(actualRead > 0)

            read += actualRead;
          }

          TESTING_EQUAL(0, UseServicesHelper::compare(*buffer, buffer2))
        }
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::tableDump(
                                        SqlField *definition,
                                        const char *tableName
                                        )
      {
        TESTING_CHECK(definition)
        TESTING_CHECK(tableName)

        TESTING_CHECK(mOverride)
        if (!mOverride) return;

        SqlDatabasePtr database = mOverride->getDatabase();
        TESTING_CHECK(database)
        if (!database) return;

        SqlTable table(*database, tableName, definition);
        table.open();
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkCount(
                                         SqlField *definition,
                                         const char *tableName,
                                         int total
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
        
        TESTING_EQUAL(table.recordCount(), total);
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
        checkIndexValue(&table, index, fieldName, value, value);
      }

      //-----------------------------------------------------------------------
      void TestPushMailboxDB::checkIndexValue(
                                              SqlField *definition,
                                              const char *tableName,
                                              int index,
                                              const char *fieldName,
                                              int minValue,
                                              int maxValue
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
        checkIndexValue(&table, index, fieldName, minValue, maxValue);
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
                                              int minValue,
                                              int maxValue
                                              )
      {
        TESTING_CHECK(table)

        SqlRecord *record = table->getRecord(index);

        checkValue(record, fieldName, minValue, maxValue);
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
                                         int minValue,
                                         int maxValue
                                         )
      {
        TESTING_CHECK(record)
        if (!record) return;

        SqlValue *value = record->getValue(fieldName);
        TESTING_CHECK(value)

        if (!value) return;

        auto result = value->asInteger();

        if (minValue == maxValue) {
          TESTING_EQUAL(result, minValue)
          return;
        }
        TESTING_CHECK(result >= minValue)
        TESTING_CHECK(result <= maxValue)
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
  testObject->testSettings();
  testObject->testFolder();
  testObject->testFolderMessage();
  testObject->testFolderVersionedMessage();
  testObject->testMessage();
  testObject->testDeliveryState();
  testObject->testPendingDelivery();
  testObject->testList();
  testObject->testListURI();
  testObject->testKeyDomain();
  testObject->testSendingKey();
  testObject->testReceivingKey();
  testObject->testStorage();

  std::cout << "COMPLETE:     Push mailbox db complete.\n";

//  TESTING_CHECK(testObject->mNetworkDone)
//  TESTING_CHECK(testObject->mNetwork->isPreparationComplete())
//  TESTING_CHECK(testObject->mNetwork->wasSuccessful())


  TESTING_UNINSTALL_LOGGER()
}
