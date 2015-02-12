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

#include "TestLocationDB.h"
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

ZS_DECLARE_USING_PTR(openpeer::stack::test, TestLocationDB)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)

ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationDatabaseAbstractionFactory, IUseAbstractionFactory)
ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationDatabaseAbstractionFactory, UseAbstractionFactory)


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
      #pragma mark OverrideLocationAbstractionFactory
      #pragma mark

      interaction OverrideLocationAbstractionFactory : public IUseAbstractionFactory
      {
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationDatabaseAbstraction, IUseAbstraction)
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationDatabaseAbstraction, UseAbstraction)

        static OverrideLocationAbstractionFactoryPtr create()
        {
          return OverrideLocationAbstractionFactoryPtr(new OverrideLocationAbstractionFactory);
        }

        virtual UseAbstractionPtr createMaster(
                                               const char *inHashRepresentingUser,
                                               const char *inUserStorageFilePath
                                               )
        {
          return OverrideLocationAbstraction::createMaster(inHashRepresentingUser, inUserStorageFilePath);
        }

        virtual UseAbstractionPtr openDatabase(
                                               IUseAbstractionPtr masterDatabase,
                                               const char *peerURI,
                                               const char *locationID,
                                               const char *databaseID
                                               )
        {
          return OverrideLocationAbstraction::openDatabase(masterDatabase, peerURI, locationID, databaseID);
        }
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark OverrideLocationAbstraction
      #pragma mark

      //-----------------------------------------------------------------------
      OverrideLocationAbstraction::OverrideLocationAbstraction(
                                                               const char *inHashRepresentingUser,
                                                               const char *inUserStorageFilePath
                                                               ) :
        UseAbstraction(inHashRepresentingUser, inUserStorageFilePath)
      {
      }

      //-----------------------------------------------------------------------
      OverrideLocationAbstraction::OverrideLocationAbstraction(
                                                               IUseAbstractionPtr masterDatabase,
                                                               const char *peerURI,
                                                               const char *locationID,
                                                               const char *databaseID
                                                               ) :
        UseAbstraction(masterDatabase, peerURI, locationID, databaseID, false)
      {
      }

      //-----------------------------------------------------------------------
      OverrideLocationAbstractionPtr OverrideLocationAbstraction::convert(IUseAbstractionPtr abstraction)
      {
        return ZS_DYNAMIC_PTR_CAST(OverrideLocationAbstraction, abstraction);
      }

      //-----------------------------------------------------------------------
      OverrideLocationAbstractionPtr OverrideLocationAbstraction::createMaster(
                                                                               const char *inHashRepresentingUser,
                                                                               const char *inUserStorageFilePath
                                                                               )
      {
        OverrideLocationAbstractionPtr pThis(new OverrideLocationAbstraction(inHashRepresentingUser, inUserStorageFilePath));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      OverrideLocationAbstractionPtr OverrideLocationAbstraction::openDatabase(
                                                                               IUseAbstractionPtr masterDatabase,
                                                                               const char *peerURI,
                                                                               const char *locationID,
                                                                               const char *databaseID
                                                                               )
      {
        OverrideLocationAbstractionPtr pThis(new OverrideLocationAbstraction(masterDatabase, peerURI, locationID, databaseID));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void OverrideLocationAbstraction::init()
      {
        UseAbstraction::init();
      }

      //-----------------------------------------------------------------------
      OverrideLocationAbstraction::SqlDatabasePtr OverrideLocationAbstraction::getDatabase()
      {
        return mDB;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark TestLocationDB
      #pragma mark

      //-----------------------------------------------------------------------
      TestLocationDB::TestLocationDB() :
        mUserHash(UseServicesHelper::randomString(8))
      {
        mFactory = OverrideLocationAbstractionFactory::create();

        UseAbstractionFactory::override(mFactory);
      }

      //-----------------------------------------------------------------------
      TestLocationDB::TestLocationDB(TestLocationDBPtr master)
      {
        TESTING_CHECK(master)

        mFactory = OverrideLocationAbstractionFactory::create();

        mMaster = master;
        mUserHash = master->mUserHash;

        UseAbstractionFactory::override(mFactory);
      }

      //-----------------------------------------------------------------------
      TestLocationDB::~TestLocationDB()
      {
        mThisWeak.reset();

        UseAbstractionFactory::override(UseAbstractionFactoryPtr());
      }

      //-----------------------------------------------------------------------
      TestLocationDBPtr TestLocationDB::create()
      {
        TestLocationDBPtr pThis(new TestLocationDB());
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      TestLocationDBPtr TestLocationDB::create(TestLocationDBPtr master)
      {
        TestLocationDBPtr pThis(new TestLocationDB(master));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::init()
      {
        ISettings::applyDefaults();
        ISettings::setString("openpeer/stack/cache-database-path", "/tmp");
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testCreate()
      {
        if (!mMaster) {
          mkdir("/tmp/testlocationdb", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
          mkdir("/tmp/testlocationdb/users", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

          mAbstraction = IUseAbstraction::createMaster(mUserHash, "/tmp/testlocationdb/users");
          mOverride = OverrideLocationAbstraction::convert(mAbstraction);
        } else {
          mAbstraction = IUseAbstraction::openDatabase(mMaster->mAbstraction, "peer://foo.com/abcdef", "bogus-location-a", "db1");
          mOverride = OverrideLocationAbstraction::convert(mAbstraction);
        }

        TESTING_CHECK(mAbstraction)
        TESTING_CHECK(mOverride)

        checkIndexValue(UseTables::Version(), UseTables::Version_name(), 0, UseTables::version, 1);
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testPeerLocation()
      {
        auto peerLocation = mAbstraction->peerLocationTable();
        TESTING_CHECK(peerLocation)

        String originalA;

        checkCount(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0);

        {
          Seconds now = zsLib::timeSinceEpoch<Seconds>(zsLib::now());
          Seconds end = now + Seconds(2);

          IUseAbstraction::PeerLocationRecord record;
          peerLocation->createOrObtain("peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706", "location-a", record);

          TESTING_EQUAL(record.mIndex, 1)
          TESTING_EQUAL(record.mPeerURI, "peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706")
          TESTING_EQUAL(record.mLocationID, "location-a")
          TESTING_EQUAL(record.mLastDownloadedVersion, String())
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          originalA = record.mUpdateVersion;

          String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash("peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706" ":" "location-a"));

          checkCount(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1);

          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::peerLocationHash, hash);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::peerURI, "peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::locationID, "location-a");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::lastDownloadedVersion, String());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::downloadComplete, 0);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::lastAccessed, (int) now.count(), (int) end.count());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Seconds now = zsLib::timeSinceEpoch<Seconds>(zsLib::now());
          Seconds end = now + Seconds(2);

          IUseAbstraction::PeerLocationRecord record;
          peerLocation->createOrObtain("peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a", "location-b", record);

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mPeerURI, "peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a")
          TESTING_EQUAL(record.mLocationID, "location-b")
          TESTING_EQUAL(record.mLastDownloadedVersion, String())
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash("peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a" ":" "location-b"));

          checkCount(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 2);

          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::peerLocationHash, hash);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::peerURI, "peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::locationID, "location-b");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::lastDownloadedVersion, String());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::downloadComplete, 0);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::lastAccessed, (int) now.count(), (int) end.count());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Seconds now = zsLib::timeSinceEpoch<Seconds>(zsLib::now()) - Seconds(2);
          Seconds end = now + Seconds(4);

          IUseAbstraction::PeerLocationRecord record;
          peerLocation->createOrObtain("peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706", "location-a", record);

          TESTING_EQUAL(record.mIndex, 1)
          TESTING_EQUAL(record.mPeerURI, "peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706")
          TESTING_EQUAL(record.mLocationID, "location-a")
          TESTING_EQUAL(record.mLastDownloadedVersion, String())
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())
          TESTING_EQUAL(record.mUpdateVersion, originalA)

          String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash("peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706" ":" "location-a"));

          checkCount(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 2);

          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::peerLocationHash, hash);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::peerURI, "peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::locationID, "location-a");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::lastDownloadedVersion, String());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::downloadComplete, 0);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::lastAccessed, (int) now.count(), (int) end.count());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Seconds now = zsLib::timeSinceEpoch<Seconds>(zsLib::now()) - Seconds(2);
          Seconds end = now + Seconds(4);

          peerLocation->notifyDownloaded(2, "b-version", true);

          checkCount(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 2);

          String hash = UseServicesHelper::convertToHex(*UseServicesHelper::hash("peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a" ":" "location-b"));

          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::peerLocationHash, hash);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::peerURI, "peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::locationID, "location-b");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::lastDownloadedVersion, "b-version");
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::downloadComplete, 1);
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 1, UseTables::lastAccessed, (int) now.count(), (int) end.count());

          // verify no other record changed
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::lastDownloadedVersion, String());
          checkIndexValue(UseTables::PeerLocation(), UseTables::PeerLocation_name(), 0, UseTables::downloadComplete, 0);
        }

        {
          Time now = zsLib::now() + Seconds(1);

          Time before = now - Seconds(3);
          Time end = now + Seconds(1);

          auto result = peerLocation->getUnusedLocationsBatch(now);

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto value = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(value.mIndex, 1)
                TESTING_EQUAL(value.mPeerURI, "peer://domain.com/7837b3ed27d15ff56af48f772e735175de29b9b7fe5d0e83e74140a212045706")
                TESTING_EQUAL(value.mLocationID, "location-a")
                TESTING_EQUAL(value.mLastDownloadedVersion, String())
                TESTING_CHECK(!value.mDownloadComplete)
                TESTING_CHECK(Time() != value.mLastAccessed)
                TESTING_CHECK(value.mUpdateVersion.hasData())

                TESTING_CHECK((value.mLastAccessed > before) && (value.mLastAccessed < end))
                TESTING_CHECK(value.mLastAccessed < now)
                break;
              }
              case 1: {
                TESTING_EQUAL(value.mIndex, 2)
                TESTING_EQUAL(value.mPeerURI, "peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a")
                TESTING_EQUAL(value.mLocationID, "location-b")
                TESTING_EQUAL(value.mLastDownloadedVersion, "b-version")
                TESTING_CHECK(value.mDownloadComplete)
                TESTING_CHECK(value.mUpdateVersion.hasData())

                TESTING_CHECK((value.mLastAccessed > before) && (value.mLastAccessed < end))
                TESTING_CHECK(value.mLastAccessed < now)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }

        {
          Time now = zsLib::now() - Seconds(2);

          auto result = peerLocation->getUnusedLocationsBatch(now);
          TESTING_EQUAL(result->size(), 0)
        }

        {
          peerLocation->remove(1);

          Time now = zsLib::now() + Seconds(1);

          Time before = now - Seconds(3);
          Time end = now + Seconds(1);

          auto result = peerLocation->getUnusedLocationsBatch(now);

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto value = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(value.mIndex, 2)
                TESTING_EQUAL(value.mPeerURI, "peer://domain.com/8947b93fbdae8997d380d189d28743fd0b8dccf27acf65c48b4920cac9f1e47a")
                TESTING_EQUAL(value.mLocationID, "location-b")
                TESTING_EQUAL(value.mLastDownloadedVersion, "b-version")
                TESTING_CHECK(value.mDownloadComplete)
                TESTING_CHECK(value.mUpdateVersion.hasData())

                TESTING_CHECK((value.mLastAccessed > before) && (value.mLastAccessed < end))
                TESTING_CHECK(value.mLastAccessed < now)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(1, loop)
        }
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testDatabase()
      {
        auto database = mAbstraction->databaseTable();
        TESTING_CHECK(database)

        auto permission = mAbstraction->permissionTable();
        TESTING_CHECK(permission)

        checkCount(UseTables::Database(), UseTables::Database_name(), 0);

        Time nowRemembered;

        {
          Time now = zsLib::now();
          Time end = now + Seconds(2);

          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 2;
          record.mDatabaseID = "foo1";
          record.mLastDownloadedVersion = String();
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value1a\",\"value1b\" ] } }");
          record.mExpires = now;
          record.mDownloadComplete = false;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 1)
          TESTING_EQUAL(record.mIndexPeerLocation, 2)
          TESTING_EQUAL(record.mDatabaseID, "foo1")
          TESTING_EQUAL(record.mLastDownloadedVersion, String())
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}")
          TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(now).count(), zsLib::timeSinceEpoch<Seconds>(record.mExpires).count())
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 2)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 1)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add);

          checkCount(UseTables::Database(), UseTables::Database_name(), 1);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::databaseID, "foo1");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::lastDownloadedVersion, String());
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(now).count(), (int) zsLib::timeSinceEpoch<Seconds>(end).count());
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::downloadComplete, 0);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Time now = zsLib::now();
          Time end = now + Seconds(2);

          nowRemembered = now;

          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 3;
          record.mDatabaseID = "foo2";
          record.mLastDownloadedVersion = "ver-2";
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2a\",\"value2b\" ] } }");
          record.mExpires = now;
          record.mDownloadComplete = true;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mIndexPeerLocation, 3)
          TESTING_EQUAL(record.mDatabaseID, "foo2")
          TESTING_EQUAL(record.mLastDownloadedVersion, "ver-2")
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}")
          TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(now).count(), zsLib::timeSinceEpoch<Seconds>(record.mExpires).count())
          TESTING_EQUAL(record.mDownloadComplete, true)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 2)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add);

          checkCount(UseTables::Database(), UseTables::Database_name(), 2);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::databaseID, "foo2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::lastDownloadedVersion, "ver-2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::metaData, "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(now).count(), (int) zsLib::timeSinceEpoch<Seconds>(end).count());
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::downloadComplete, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Time now = zsLib::now();
          Time end = now + Seconds(2);

          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 2;
          record.mDatabaseID = "foo3";
          record.mLastDownloadedVersion = "ver-3";
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value3a\",\"value3b\" ] } }");
          record.mExpires = now;
          record.mDownloadComplete = false;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 3)
          TESTING_EQUAL(record.mIndexPeerLocation, 2)
          TESTING_EQUAL(record.mDatabaseID, "foo3")
          TESTING_EQUAL(record.mLastDownloadedVersion, "ver-3")
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value3a\",\"value3b\"]}}")
          TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(now).count(), zsLib::timeSinceEpoch<Seconds>(record.mExpires).count())
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 2)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 3)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add);

          checkCount(UseTables::Database(), UseTables::Database_name(), 3);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::databaseID, "foo3");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::lastDownloadedVersion, "ver-3");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::metaData, "{\"values\":{\"value\":[\"value3a\",\"value3b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(now).count(), (int) zsLib::timeSinceEpoch<Seconds>(end).count());
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::downloadComplete, 0);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          Time now = nowRemembered;
          Time end = now + Seconds(2);

          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 3;
          record.mDatabaseID = "foo2";
          record.mLastDownloadedVersion = "ver-2";
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2a\",\"value2b\" ] } }");
          record.mExpires = now;
          record.mDownloadComplete = true;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mIndexPeerLocation, 3)
          TESTING_EQUAL(record.mDatabaseID, "foo2")
          TESTING_EQUAL(record.mLastDownloadedVersion, "ver-2")
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}")
          TESTING_EQUAL(zsLib::timeSinceEpoch<Seconds>(now).count(), zsLib::timeSinceEpoch<Seconds>(record.mExpires).count())
          TESTING_EQUAL(record.mDownloadComplete, true)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 2)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_None);

          checkCount(UseTables::Database(), UseTables::Database_name(), 3);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::databaseID, "foo2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::lastDownloadedVersion, "ver-2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::metaData, "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::expires, (int) zsLib::timeSinceEpoch<Seconds>(now).count(), (int) zsLib::timeSinceEpoch<Seconds>(end).count());
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::downloadComplete, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 3;
          record.mDatabaseID = "foo2";
          record.mLastDownloadedVersion = "ver-2";
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2a\",\"value2b\" ] } }");
          record.mExpires = Time();
          record.mDownloadComplete = true;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mIndexPeerLocation, 3)
          TESTING_EQUAL(record.mDatabaseID, "foo2")
          TESTING_EQUAL(record.mLastDownloadedVersion, "ver-2")
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}")
          TESTING_CHECK(Time() == record.mExpires)
          TESTING_EQUAL(record.mDownloadComplete, true)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 2)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Update);

          checkCount(UseTables::Database(), UseTables::Database_name(), 3);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::databaseID, "foo2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::lastDownloadedVersion, "ver-2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::metaData, "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::expires, 0);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::downloadComplete, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndex = 2;

          database->remove(record, changeRecord);

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 2)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove);

          checkCount(UseTables::Database(), UseTables::Database_name(), 2);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::databaseID, "foo1");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::databaseID, "foo3");
        }

        {
          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 3;
          record.mDatabaseID = "foo2";
          record.mLastDownloadedVersion = "ver-2";
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"value2a\",\"value2b\" ] } }");
          record.mExpires = Time();
          record.mDownloadComplete = true;

          database->addOrUpdate(record, changeRecord);

          TESTING_EQUAL(record.mIndex, 4)
          TESTING_EQUAL(record.mIndexPeerLocation, 3)
          TESTING_EQUAL(record.mDatabaseID, "foo2")
          TESTING_EQUAL(record.mLastDownloadedVersion, "ver-2")
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}")
          TESTING_CHECK(Time() == record.mExpires)
          TESTING_EQUAL(record.mDownloadComplete, true)
          TESTING_CHECK(record.mUpdateVersion.hasData())

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 4)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add);

          checkCount(UseTables::Database(), UseTables::Database_name(), 3);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, SqlField::id, 4);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::databaseID, "foo2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::lastDownloadedVersion, "ver-2");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::metaData, "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::expires, 0);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::downloadComplete, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 2, UseTables::updateVersion, record.mUpdateVersion);
        }

        {
//          Time now = zsLib::now() + Seconds(1);

          IUseAbstraction::PeerURIList uris;

          uris.push_back("peer://domain.com/a/");
          uris.push_back("peer://domain.com/b/");
          uris.push_back("peer://domain.com/c/");

          permission->insert(3, 4, uris);

          {
            auto result = database->getBatchByPeerLocationIndexForPeerURI("peer://domain.com/a/", 3);
            TESTING_CHECK(result)

            int loop = 0;
            for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
            {
              auto record = (*iter);
              switch (loop) {
                case 0: {
                  TESTING_EQUAL(record.mIndex, 4)
                  TESTING_EQUAL(record.mIndexPeerLocation, 3)
                  TESTING_EQUAL(record.mDatabaseID, "foo2")
                  TESTING_EQUAL(record.mLastDownloadedVersion, "ver-2")
                  TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value2a\",\"value2b\"]}}")
                  TESTING_CHECK(Time() == record.mExpires)
                  TESTING_EQUAL(record.mDownloadComplete, true)
                  TESTING_CHECK(record.mUpdateVersion.hasData())
                  break;
                }
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(1, loop)
          }

          {
            auto result = database->getBatchByPeerLocationIndexForPeerURI("peer://domain.com/d/", 3);
            TESTING_CHECK(result)

            int loop = 0;
            for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
            {
              auto record = (*iter);
              switch (loop) {
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(0, loop)
          }

          {
            auto result = database->getBatchByPeerLocationIndexForPeerURI("peer://domain.com/a/", 2);
            TESTING_CHECK(result)

            int loop = 0;
            for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
            {
              auto record = (*iter);
              switch (loop) {
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(0, loop)
          }
        }
        
        {
          IUseAbstraction::DatabaseRecord record;
          IUseAbstraction::DatabaseChangeRecord changeRecord;

          record.mIndexPeerLocation = 3;
          record.mDatabaseID = "foo2";

          database->remove(record, changeRecord);

          TESTING_EQUAL(changeRecord.mIndexPeerLocation, 3)
          TESTING_EQUAL(changeRecord.mIndexDatabase, 4)
          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove);

          checkCount(UseTables::Database(), UseTables::Database_name(), 2);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::databaseID, "foo1");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::databaseID, "foo3");
        }
        
        {
          Time now = zsLib::now() - Seconds(2);
          Time end = now + Seconds(4);

          IUseAbstraction::DatabaseRecord record;
          database->getByIndex(1, record);

          TESTING_EQUAL(record.mIndex, 1)
          TESTING_EQUAL(record.mIndexPeerLocation, 2)
          TESTING_EQUAL(record.mDatabaseID, "foo1")
          TESTING_EQUAL(record.mLastDownloadedVersion, String())
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}")
          TESTING_CHECK((record.mExpires > now) && (record.mExpires < end))
          TESTING_EQUAL(record.mDownloadComplete, false)
          TESTING_CHECK(record.mUpdateVersion.hasData())
        }

        {
          Time now = zsLib::now() + Seconds(1);

          Time before = now - Seconds(3);
          Time end = now + Seconds(1);

          auto result = database->getBatchByPeerLocationIndex(2);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mDatabaseID, "foo1")
                TESTING_EQUAL(record.mLastDownloadedVersion, String())
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}")
                TESTING_CHECK((record.mExpires > before) && (record.mExpires < end))
                TESTING_EQUAL(record.mDownloadComplete, false)
                TESTING_CHECK(record.mUpdateVersion.hasData())
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mDatabaseID, "foo3")
                TESTING_EQUAL(record.mLastDownloadedVersion, "ver-3")
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value3a\",\"value3b\"]}}")
                TESTING_CHECK((record.mExpires > before) && (record.mExpires < end))
                TESTING_EQUAL(record.mDownloadComplete, false)
                TESTING_CHECK(record.mUpdateVersion.hasData())
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }

        {
          Time now = zsLib::now() + Seconds(1);

          Time before = now - Seconds(3);
          Time end = now + Seconds(1);

          auto result = database->getBatchByPeerLocationIndex(2, 1);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mDatabaseID, "foo3")
                TESTING_EQUAL(record.mLastDownloadedVersion, "ver-3")
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value3a\",\"value3b\"]}}")
                TESTING_CHECK((record.mExpires > before) && (record.mExpires < end))
                TESTING_EQUAL(record.mDownloadComplete, false)
                TESTING_CHECK(record.mUpdateVersion.hasData())
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(1, loop)
        }

        {
          Time now = zsLib::now() + Seconds(1);

          Time before = now - Seconds(3);
          Time end = now + Seconds(1);

          IUseAbstraction::PeerURIList uris;

          uris.push_back("peer://domain.com/d/");
          uris.push_back("peer://domain.com/e/");
          uris.push_back("peer://domain.com/f/");

          permission->insert(2, 1, uris);
          permission->insert(2, 3, uris);

          {
            auto result = database->getBatchByPeerLocationIndexForPeerURI("peer://domain.com/f/", 2);
            TESTING_CHECK(result)

            int loop = 0;
            for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
            {
              auto record = (*iter);
              switch (loop) {
                case 0: {
                  TESTING_EQUAL(record.mIndex, 1)
                  TESTING_EQUAL(record.mIndexPeerLocation, 2)
                  TESTING_EQUAL(record.mDatabaseID, "foo1")
                  TESTING_EQUAL(record.mLastDownloadedVersion, String())
                  TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}")
                  TESTING_CHECK((record.mExpires > before) && (record.mExpires < end))
                  TESTING_EQUAL(record.mDownloadComplete, false)
                  TESTING_CHECK(record.mUpdateVersion.hasData())
                  break;
                }
                case 1: {
                  TESTING_EQUAL(record.mIndex, 3)
                  TESTING_EQUAL(record.mIndexPeerLocation, 2)
                  TESTING_EQUAL(record.mDatabaseID, "foo3")
                  TESTING_EQUAL(record.mLastDownloadedVersion, "ver-3")
                  TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"value3a\",\"value3b\"]}}")
                  TESTING_CHECK((record.mExpires > before) && (record.mExpires < end))
                  TESTING_EQUAL(record.mDownloadComplete, false)
                  TESTING_CHECK(record.mUpdateVersion.hasData())
                  break;
                }
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(2, loop)
          }

          {
            auto result = database->getBatchByPeerLocationIndexForPeerURI("peer://domain.com/f/", 3);
            TESTING_CHECK(result)

            int loop = 0;
            for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
            {
              auto record = (*iter);
              switch (loop) {
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(0, loop)
          }
        }

        {
          database->notifyDownloaded(1, "ver-1b", true);

          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::databaseID, "foo1");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::lastDownloadedVersion, "ver-1b");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"value1a\",\"value1b\"]}}");
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 0, UseTables::downloadComplete, 1);

          // check nothing else was changed
          checkIndexValue(UseTables::Database(), UseTables::Database_name(), 1, UseTables::lastDownloadedVersion, "ver-3");
        }

        {
          database->flushAllForPeerLocation(2);

          checkCount(UseTables::Database(), UseTables::Database_name(), 0);
        }
        
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testDatabaseChange()
      {
        auto databaseChange = mAbstraction->databaseChangeTable();
        TESTING_CHECK(databaseChange)

        auto permission = mAbstraction->permissionTable();
        TESTING_CHECK(permission)

        {
          permission->flushAllForPeerLocation(3);
          permission->flushAllForPeerLocation(2);
        }

        checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 0);
        checkCount(UseTables::Permission(), UseTables::Permission_name(), 0);

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Add;
          record.mIndexPeerLocation = 2;
          record.mIndexDatabase = 10;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 1);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 0, UseTables::disposition, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 0, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 0, UseTables::indexDatabase, 10);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Add;
          record.mIndexPeerLocation = 2;
          record.mIndexDatabase = 11;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 2);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 1, UseTables::disposition, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 1, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 1, UseTables::indexDatabase, 11);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Update;
          record.mIndexPeerLocation = 2;
          record.mIndexDatabase = 10;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 3);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 2, UseTables::disposition, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 2, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 2, UseTables::indexDatabase, 10);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Remove;
          record.mIndexPeerLocation = 2;
          record.mIndexDatabase = 10;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 4);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 3, SqlField::id, 4);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 3, UseTables::disposition, 3);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 3, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 3, UseTables::indexDatabase, 10);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Add;
          record.mIndexPeerLocation = 3;
          record.mIndexDatabase = 12;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 5);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 4, SqlField::id, 5);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 4, UseTables::disposition, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 4, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 4, UseTables::indexDatabase, 12);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Add;
          record.mIndexPeerLocation = 3;
          record.mIndexDatabase = 13;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 6);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 5, SqlField::id, 6);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 5, UseTables::disposition, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 5, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 5, UseTables::indexDatabase, 13);
        }

        {
          IUseAbstraction::DatabaseChangeRecord record;
          record.mDisposition = IUseAbstraction::DatabaseChangeRecord::Disposition_Add;
          record.mIndexPeerLocation = 3;
          record.mIndexDatabase = 14;
          databaseChange->insert(record);

          checkCount(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 7);

          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 6, SqlField::id, 7);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 6, UseTables::disposition, 1);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 6, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::DatabaseChange(), UseTables::DatabaseChange_name(), 6, UseTables::indexDatabase, 14);
        }

        {
          auto result = databaseChange->getChangeBatch(2);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 11)
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 3: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(4, loop)
        }
        {
          auto result = databaseChange->getChangeBatch(2,3);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(1, loop)
        }
        {
          auto result = databaseChange->getChangeBatch(3);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 5)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 3)
                TESTING_EQUAL(record.mIndexDatabase, 12)
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 6)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 3)
                TESTING_EQUAL(record.mIndexDatabase, 13)
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 7)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 3)
                TESTING_EQUAL(record.mIndexDatabase, 14)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }

        {
          auto result = databaseChange->getChangeBatchForPeerURI("peer://domain.com/a", 2);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            switch (loop) {
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(0, loop)
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/a");
          uris.push_back("peer://domain.com/b");
          uris.push_back("peer://domain.com/c");
          permission->insert(2, 10, uris);

          auto result = databaseChange->getChangeBatchForPeerURI("peer://domain.com/a", 2);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }
        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/c");
          permission->insert(2, 11, uris);

          auto result = databaseChange->getChangeBatchForPeerURI("peer://domain.com/c", 2);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 11)
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 3: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(4, loop)
        }
        {
          auto result = databaseChange->getChangeBatchForPeerURI("peer://domain.com/a", 2, 1);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::DatabaseChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mIndexPeerLocation, 2)
                TESTING_EQUAL(record.mIndexDatabase, 10)
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testPermission()
      {
        auto permission = mAbstraction->permissionTable();
        TESTING_CHECK(permission)

        {
          permission->flushAllForPeerLocation(2);
          checkCount(UseTables::Permission(), UseTables::Permission_name(), 0);
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/a");
          uris.push_back("peer://domain.com/b");
          uris.push_back("peer://domain.com/c");
          permission->insert(2, 10, uris);

          checkCount(UseTables::Permission(), UseTables::Permission_name(), 3);

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 0, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 0, UseTables::indexDatabase, 10);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 0, UseTables::peerURI, "peer://domain.com/a");

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 1, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 1, UseTables::indexDatabase, 10);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 1, UseTables::peerURI, "peer://domain.com/b");

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 2, UseTables::indexPeerLocation, 2);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 2, UseTables::indexDatabase, 10);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 2, UseTables::peerURI, "peer://domain.com/c");
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/e");
          uris.push_back("peer://domain.com/a");
          permission->insert(3, 11, uris);

          checkCount(UseTables::Permission(), UseTables::Permission_name(), 5);

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 3, SqlField::id, 4);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 3, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 3, UseTables::indexDatabase, 11);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 3, UseTables::peerURI, "peer://domain.com/e");

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 4, SqlField::id, 5);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 4, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 4, UseTables::indexDatabase, 11);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 4, UseTables::peerURI, "peer://domain.com/a");
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/f");
          uris.push_back("peer://domain.com/g");
          permission->insert(3, 12, uris);

          checkCount(UseTables::Permission(), UseTables::Permission_name(), 7);

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 5, SqlField::id, 6);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 5, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 5, UseTables::indexDatabase, 12);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 5, UseTables::peerURI, "peer://domain.com/f");

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 6, SqlField::id, 7);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 6, UseTables::indexPeerLocation, 3);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 6, UseTables::indexDatabase, 12);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 6, UseTables::peerURI, "peer://domain.com/g");
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/h");
          uris.push_back("peer://domain.com/i");
          permission->insert(4, 13, uris);

          checkCount(UseTables::Permission(), UseTables::Permission_name(), 9);

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 7, SqlField::id, 8);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 7, UseTables::indexPeerLocation, 4);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 7, UseTables::indexDatabase, 13);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 7, UseTables::peerURI, "peer://domain.com/h");

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 8, SqlField::id, 9);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 8, UseTables::indexPeerLocation, 4);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 8, UseTables::indexDatabase, 13);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 8, UseTables::peerURI, "peer://domain.com/i");
        }

        {
          IUseAbstraction::PeerURIList uris;
          uris.push_back("peer://domain.com/h");
          permission->insert(4, 14, uris);

          checkCount(UseTables::Permission(), UseTables::Permission_name(), 10);

          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 9, SqlField::id, 10);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 9, UseTables::indexPeerLocation, 4);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 9, UseTables::indexDatabase, 14);
          checkIndexValue(UseTables::Permission(), UseTables::Permission_name(), 9, UseTables::peerURI, "peer://domain.com/h");
        }

        {
          {
            IUseAbstraction::PeerURIList result;
            permission->getByDatabaseIndex(12, result);

            int loop = 0;
            for (auto iter = result.begin(); iter != result.end(); ++loop, ++iter)
            {
              auto uri = (*iter);
              switch (loop) {
                case 0: {
                  TESTING_EQUAL(uri, "peer://domain.com/f")
                  break;
                }
                case 1: {
                  TESTING_EQUAL(uri, "peer://domain.com/g")
                  break;
                }
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(2, loop)
          }
        }

        {
          permission->flushAllForDatabase(12);
          checkCount(UseTables::Permission(), UseTables::Permission_name(), 8);

          {

            IUseAbstraction::PeerURIList result;
            permission->getByDatabaseIndex(12, result);

            int loop = 0;
            for (auto iter = result.begin(); iter != result.end(); ++loop, ++iter)
            {
              auto uri = (*iter);
              switch (loop) {
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(0, loop)
          }
        }

        {
          IUseAbstraction::PeerURIList result;
          permission->getByDatabaseIndex(10, result);

          int loop = 0;
          for (auto iter = result.begin(); iter != result.end(); ++loop, ++iter)
          {
            auto uri = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(uri, "peer://domain.com/a")
                break;
              }
              case 1: {
                TESTING_EQUAL(uri, "peer://domain.com/b")
                break;
              }
              case 2: {
                TESTING_EQUAL(uri, "peer://domain.com/c")
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }
        {
          IUseAbstraction::PeerURIList result;
          permission->getByDatabaseIndex(11, result);

          int loop = 0;
          for (auto iter = result.begin(); iter != result.end(); ++loop, ++iter)
          {
            auto uri = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(uri, "peer://domain.com/e")
                break;
              }
              case 1: {
                TESTING_EQUAL(uri, "peer://domain.com/a")
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }

        {
          permission->flushAllForPeerLocation(3);
          checkCount(UseTables::Permission(), UseTables::Permission_name(), 6);

          {
            IUseAbstraction::PeerURIList result;
            permission->getByDatabaseIndex(11, result);

            int loop = 0;
            for (auto iter = result.begin(); iter != result.end(); ++loop, ++iter)
            {
              auto uri = (*iter);
              switch (loop) {
                default: TESTING_CHECK(false); break;
              }
            }
            TESTING_EQUAL(0, loop)
          }
        }
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testEntry()
      {
        auto entry = mAbstraction->entryTable();
        TESTING_CHECK(entry)

        checkCount(UseTables::Entry(), UseTables::Entry_name(), 0);

        {
          Time now = zsLib::now();

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 5;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea1\",\"valuea2\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"boguses\": { \"bogus\": [\"valuea1\",\"valuea2\" ] } }");
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = record.mCreated;

          auto result = entry->add(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 1);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 5);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "{\"boguses\":{\"bogus\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valuea1\",\"valuea2\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        {
          Time now = zsLib::now();

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 2;
          record.mMetaData = UseServicesHelper::toJSON("{ \"fudge\": { \"value\": [\"valuea1\",\"valuea2\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"bar\": { \"bogus\": [\"valuea1\",\"valuea2\" ] } }");
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = record.mCreated;

          auto result = entry->add(record, changeRecord);
          TESTING_CHECK(!result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_None)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 1);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 5);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "{\"boguses\":{\"bogus\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valuea1\",\"valuea2\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        {
          Time now = zsLib::now();

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-b";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valueb1\",\"valueb2\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"boguses\": { \"bogus\": [\"valueb1\",\"valueb2\" ] } }");
          record.mDataLength = 100;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = record.mCreated;

          auto result = entry->add(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-b")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::entryID, "foo-b");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::version, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::metaData, "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::data, "{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 1, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        Time nowA;
        
        {
          Time now = zsLib::now() + Seconds(2);
          nowA = now;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea3\",\"valuea4\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"boguses\": { \"bogus\": [\"valuea3\",\"valuea4\" ] } }");
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = record.mCreated;

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 6);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "{\"boguses\":{\"bogus\":[\"valuea3\",\"valuea4\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valuea3\",\"valuea4\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }
        
        {
          Time now = zsLib::now() + Seconds(4);
          Time oldNow = nowA;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 2;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea3\",\"valuea4\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"boguses\": { \"bogus\": [\"valuea5\",\"valuea6\" ] } }");
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = record.mCreated;

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(!result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_None)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 6);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "{\"boguses\":{\"bogus\":[\"valuea3\",\"valuea4\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valuea3\",\"valuea4\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(oldNow).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(oldNow).count());
        }

        {
          Time now = zsLib::now() + Seconds(2);
          Time oldNow = nowA;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 8;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea5\",\"valuea5\" ] } }");
          record.mData = UseServicesHelper::toJSON("{ \"boguses\": { \"bogus\": [\"valuea5\",\"valuea6\" ] } }");
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = Time();
          record.mUpdated = record.mCreated;

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 8);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "{\"boguses\":{\"bogus\":[\"valuea5\",\"valuea6\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, strlen("{\"boguses\":{\"bogus\":[\"valuea5\",\"valuea6\"]}}"));
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(oldNow).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        {
          Time now = zsLib::now() + Seconds(15);
          nowA = now;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea5\",\"valuea5\" ] } }");
          record.mData = ElementPtr();
          record.mDataLength = 100;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = now;

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 9);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, 100);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 0);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }
        {
          Time oldNowA = nowA;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea5\",\"valuea5\" ] } }");
          record.mData = ElementPtr();
          record.mDataLength = 100;
          record.mDataFetched = false;
          record.mCreated = Time();
          record.mUpdated = Time();

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(!result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_None)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 9);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, 100);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 0);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(oldNowA).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(oldNowA).count());
        }

        {
          Time now = zsLib::now() + Seconds(15);
          nowA = now;

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-a";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuea5\",\"valuea5\" ] } }");
          record.mData = ElementPtr();
          record.mDataLength = 0;
          record.mDataFetched = false;
          record.mCreated = now;
          record.mUpdated = now;

          auto result = entry->update(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-a")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 2);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::entryID, "foo-a");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::version, 10);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::metaData, "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::data, "");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataLength, 0);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::dataFetched, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::created, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 0, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        {
          Time now = zsLib::now();

          IUseAbstraction::EntryRecord record;
          IUseAbstraction::EntryChangeRecord changeRecord;

          record.mEntryID = "foo-c";
          record.mVersion = 0;
          record.mMetaData = UseServicesHelper::toJSON("{ \"values\": { \"value\": [\"valuec1\",\"valuec2\" ] } }");
          record.mData = ElementPtr();
          record.mDataLength = 100;
          record.mDataFetched = true;
          record.mCreated = Time();
          record.mUpdated = now;

          auto result = entry->add(record, changeRecord);
          TESTING_CHECK(result)

          TESTING_EQUAL(changeRecord.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
          TESTING_EQUAL(changeRecord.mEntryID, "foo-c")

          checkCount(UseTables::Entry(), UseTables::Entry_name(), 3);

          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::entryID, "foo-c");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::version, 1);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::metaData, "{\"values\":{\"value\":[\"valuec1\",\"valuec2\"]}}");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::data, "");
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::dataLength, 100);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::dataFetched, 0);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::created, (int) 0);
          checkIndexValue(UseTables::Entry(), UseTables::Entry_name(), 2, UseTables::updated, (int) zsLib::timeSinceEpoch<Seconds>(now).count());
        }

        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          auto result = entry->getBatch(true);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mEntryID, "foo-a")
                TESTING_EQUAL(record.mVersion, 10)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 0)
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mEntryID, "foo-c")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuec1\",\"valuec2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 100)
                TESTING_CHECK(!record.mDataFetched)
                TESTING_CHECK(Time() == record.mCreated)
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }

        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          auto result = entry->getBatch(false);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mEntryID, "foo-a")
                TESTING_EQUAL(record.mVersion, 10)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuea1\",\"valuea2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 0)
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mEntryID, "foo-c")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuec1\",\"valuec2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 100)
                TESTING_CHECK(!record.mDataFetched)
                TESTING_CHECK(Time() == record.mCreated)
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }

        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          auto result = entry->getBatch(true, 1);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mEntryID, "foo-c")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuec1\",\"valuec2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 100)
                TESTING_CHECK(!record.mDataFetched)
                TESTING_CHECK(Time() == record.mCreated)
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }

        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          auto result = entry->getBatch(false, 1);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
                TESTING_CHECK(record.mDataFetched)
                TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mEntryID, "foo-c")
                TESTING_EQUAL(record.mVersion, 1)
                TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valuec1\",\"valuec2\"]}}")
                TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
                TESTING_EQUAL(record.mDataLength, 100)
                TESTING_CHECK(!record.mDataFetched)
                TESTING_CHECK(Time() == record.mCreated)
                TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(2, loop)
        }

        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          IUseAbstraction::EntryRecord record;
          record.mIndex = 2;

          auto result = entry->getEntry(record, true);
          TESTING_CHECK(result)

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mEntryID, "foo-b")
          TESTING_EQUAL(record.mVersion, 1)
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(UseServicesHelper::toString(record.mData), "{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
          TESTING_CHECK(record.mDataFetched)
          TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
          TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
        }
        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          IUseAbstraction::EntryRecord record;
          record.mIndex = 2;

          auto result = entry->getEntry(record, false);
          TESTING_CHECK(result)

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mEntryID, "foo-b")
          TESTING_EQUAL(record.mVersion, 1)
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
          TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
          TESTING_CHECK(record.mDataFetched)
          TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
          TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
        }
        {
          IUseAbstraction::EntryRecord record;
          record.mIndex = 4;

          auto result = entry->getEntry(record, true);
          TESTING_CHECK(!result)
        }
        {
          IUseAbstraction::EntryRecord record;
          record.mEntryID = "foo-d";

          auto result = entry->getEntry(record, true);
          TESTING_CHECK(!result)
        }
        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          IUseAbstraction::EntryRecord record;
          record.mEntryID = "foo-b";

          auto result = entry->getEntry(record, true);
          TESTING_CHECK(result)

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mEntryID, "foo-b")
          TESTING_EQUAL(record.mVersion, 1)
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(UseServicesHelper::toString(record.mData), "{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
          TESTING_CHECK(record.mDataFetched)
          TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
          TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
        }
        {
          Time now = zsLib::now() - Seconds(2);
          Time end = zsLib::now() + Seconds(17);

          IUseAbstraction::EntryRecord record;
          record.mEntryID = "foo-b";

          auto result = entry->getEntry(record, false);
          TESTING_CHECK(result)

          TESTING_EQUAL(record.mIndex, 2)
          TESTING_EQUAL(record.mEntryID, "foo-b")
          TESTING_EQUAL(record.mVersion, 1)
          TESTING_EQUAL(UseServicesHelper::toString(record.mMetaData), "{\"values\":{\"value\":[\"valueb1\",\"valueb2\"]}}")
          TESTING_EQUAL(UseServicesHelper::toString(record.mData), "")
          TESTING_EQUAL(record.mDataLength, strlen("{\"boguses\":{\"bogus\":[\"valueb1\",\"valueb2\"]}}"))
          TESTING_CHECK(record.mDataFetched)
          TESTING_CHECK((record.mCreated > now) && (record.mCreated < end))
          TESTING_CHECK((record.mUpdated > now) && (record.mUpdated < end))
        }
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testEntryChange()
      {
        auto entryChange = mAbstraction->entryChangeTable();
        TESTING_CHECK(entryChange)

        checkCount(UseTables::EntryChange(), UseTables::EntryChange_name(), 0);

        {
          IUseAbstraction::EntryChangeRecord record;
          record.mDisposition = IUseAbstraction::EntryChangeRecord::Disposition_Add;
          record.mEntryID = "foo-a";

          entryChange->insert(record);

          checkCount(UseTables::EntryChange(), UseTables::EntryChange_name(), 1);

          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 0, SqlField::id, 1);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 0, UseTables::disposition, 1);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 0, UseTables::entryID, "foo-a");
        }

        {
          IUseAbstraction::EntryChangeRecord record;
          record.mDisposition = IUseAbstraction::EntryChangeRecord::Disposition_Add;
          record.mEntryID = "foo-b";

          entryChange->insert(record);

          checkCount(UseTables::EntryChange(), UseTables::EntryChange_name(), 2);

          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 1, SqlField::id, 2);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 1, UseTables::disposition, 1);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 1, UseTables::entryID, "foo-b");
        }

        {
          IUseAbstraction::EntryChangeRecord record;
          record.mDisposition = IUseAbstraction::EntryChangeRecord::Disposition_Update;
          record.mEntryID = "foo-a";

          entryChange->insert(record);

          checkCount(UseTables::EntryChange(), UseTables::EntryChange_name(), 3);

          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 2, SqlField::id, 3);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 2, UseTables::disposition, 2);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 2, UseTables::entryID, "foo-a");
        }

        {
          IUseAbstraction::EntryChangeRecord record;
          record.mDisposition = IUseAbstraction::EntryChangeRecord::Disposition_Remove;
          record.mEntryID = "foo-b";

          entryChange->insert(record);

          checkCount(UseTables::EntryChange(), UseTables::EntryChange_name(), 4);

          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 3, SqlField::id, 4);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 3, UseTables::disposition, 3);
          checkIndexValue(UseTables::EntryChange(), UseTables::EntryChange_name(), 3, UseTables::entryID, "foo-b");
        }

        {
          auto result = entryChange->getBatch();
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 1)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mEntryID, "foo-a")
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mEntryID, "foo-a")
                break;
              }
              case 3: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(4, loop)
        }

        {
          auto result = entryChange->getBatch(1);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 2)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Add)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                break;
              }
              case 1: {
                TESTING_EQUAL(record.mIndex, 3)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Update)
                TESTING_EQUAL(record.mEntryID, "foo-a")
                break;
              }
              case 2: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(3, loop)
        }

        {
          auto result = entryChange->getBatch(3);
          TESTING_CHECK(result)

          int loop = 0;
          for (auto iter = result->begin(); iter != result->end(); ++loop, ++iter)
          {
            auto record = (*iter);
            switch (loop) {
              case 0: {
                TESTING_EQUAL(record.mIndex, 4)
                TESTING_EQUAL(record.mDisposition, IUseAbstraction::EntryChangeRecord::Disposition_Remove)
                TESTING_EQUAL(record.mEntryID, "foo-b")
                break;
              }
              default: TESTING_CHECK(false); break;
            }
          }
          TESTING_EQUAL(1, loop)
        }
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::testDelete()
      {
        auto result = IUseAbstraction::deleteDatabase(mMaster->mAbstraction, "peer://foo.com/abcdef", "bogus-location-a", "db1");

        TESTING_CHECK(result)
      }
      
      //-----------------------------------------------------------------------
      void TestLocationDB::tableDump(
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
      void TestLocationDB::checkCount(
                                      OverrideLocationAbstractionPtr abstraction,
                                      SqlField *definition,
                                      const char *tableName,
                                      int total
                                      )
      {
        TESTING_CHECK(definition)
        TESTING_CHECK(tableName)

        TESTING_CHECK(abstraction)
        if (!abstraction) return;

        SqlDatabasePtr database = abstraction->getDatabase();
        TESTING_CHECK(database)
        if (!database) return;

        SqlTable table(database->getHandle(), tableName, definition);
        table.open();

        TESTING_EQUAL(table.recordCount(), total);
      }
      
      //-----------------------------------------------------------------------
      void TestLocationDB::checkCount(
                                      SqlField *definition,
                                      const char *tableName,
                                      int total
                                      )
      {
        checkCount(mOverride, definition, tableName, total);
      }

      //-----------------------------------------------------------------------
      void TestLocationDB::checkIndexValue(
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
      void TestLocationDB::checkIndexValue(
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
      void TestLocationDB::checkIndexValue(
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
      void TestLocationDB::checkIndexValue(
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
      void TestLocationDB::checkIndexValue(
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
      void TestLocationDB::checkValue(
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
      void TestLocationDB::checkValue(
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

void doTestLocationDB()
{
  if (!OPENPEER_STACK_TEST_DO_LOCATION_DB_TEST) return;

  TESTING_INSTALL_LOGGER();

  TestSetupPtr setup = TestSetup::singleton();

  TestLocationDBPtr testObject = TestLocationDB::create();

  testObject->testCreate();
  testObject->testPeerLocation();
  testObject->testDatabase();
  testObject->testDatabaseChange();
  testObject->testPermission();

  {
    TestLocationDBPtr testObject2 = TestLocationDB::create(testObject);
    testObject2->testCreate();
    testObject2->testEntry();
    testObject2->testEntryChange();
  }

  {
    TestLocationDBPtr testObject3 = TestLocationDB::create(testObject);
    testObject3->testDelete();
  }

  std::cout << "COMPLETE:     Location db complete.\n";

  TESTING_UNINSTALL_LOGGER()
}
