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

#pragma once

#include <zsLib/types.h>
#include <zsLib/MessageQueueAssociator.h>

#include <openpeer/stack/internal/stack_ILocationDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_LocationDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_ILocationDatabaseAbstractionTables.h>

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      using zsLib::ULONG;

      ZS_DECLARE_CLASS_PTR(OverrideLocationAbstractionFactory)

      ZS_DECLARE_CLASS_PTR(OverrideLocationAbstraction)

      ZS_DECLARE_CLASS_PTR(TestLocationDB)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark OverrideLocationAbstraction
      #pragma mark

      class OverrideLocationAbstraction : public openpeer::stack::internal::LocationDatabaseAbstraction
      {
      public:
        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)

        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationDatabaseAbstraction, IUseAbstraction)
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::LocationDatabaseAbstraction, UseAbstraction)

      public:
        OverrideLocationAbstraction(
                                    const char *inHashRepresentingUser,
                                    const char *inUserStorageFilePath
                                    );

        OverrideLocationAbstraction(
                                    IUseAbstractionPtr masterDatabase,
                                    const char *peerURI,
                                    const char *locationID,
                                    const char *databaseID
                                    );

        static OverrideLocationAbstractionPtr convert(IUseAbstractionPtr abstraction);

        static OverrideLocationAbstractionPtr createMaster(
                                                           const char *inHashRepresentingUser,
                                                           const char *inUserStorageFilePath
                                                           );

        static OverrideLocationAbstractionPtr openDatabase(
                                                           IUseAbstractionPtr masterDatabase,
                                                           const char *peerURI,
                                                           const char *locationID,
                                                           const char *databaseID
                                                           );

        void init();

        SqlDatabasePtr getDatabase();
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark TestLocationDB
      #pragma mark

      class TestLocationDB
      {
      public:
        ZS_DECLARE_TYPEDEF_PTR(zsLib::RecursiveLock, RecursiveLock)

        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationDatabaseAbstraction, IUseAbstraction)
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ILocationDatabaseAbstractionTables, UseTables)
        
        ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
        ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
        ZS_DECLARE_TYPEDEF_PTR(sql::RecordSet, SqlRecordSet)
        ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)

      private:
        TestLocationDB();
        TestLocationDB(TestLocationDBPtr master);

        void init();

      public:
        ~TestLocationDB();

        static TestLocationDBPtr create();
        static TestLocationDBPtr create(TestLocationDBPtr master);

        void testCreate();
        void testPeerLocation();
        void testDatabase();
        void testDatabaseChange();
        void testPermission();
        void testEntry();
        void testEntryChange();

      protected:
        void tableDump(
                       SqlField *definition,
                       const char *tableName
                       );

        void checkCount(
                        OverrideLocationAbstractionPtr abstraction,
                        SqlField *definition,
                        const char *tableName,
                        int total
                        );
        void checkCount(
                        SqlField *definition,
                        const char *tableName,
                        int total
                        );

        void checkIndexValue(
                             SqlField *definition,
                             const char *tableName,
                             int index,
                             const char *fieldName,
                             const char *value
                             );
        void checkIndexValue(
                             SqlField *definition,
                             const char *tableName,
                             int index,
                             const char *fieldName,
                             int value
                             );
        void checkIndexValue(
                             SqlField *definition,
                             const char *tableName,
                             int index,
                             const char *fieldName,
                             int minValue,
                             int maxValue
                             );
        void checkIndexValue(
                             SqlTable *table,
                             int index,
                             const char *fieldName,
                             const char *value
                             );
        void checkIndexValue(
                             SqlTable *table,
                             int index,
                             const char *fieldName,
                             int minValue,
                             int maxValue
                             );
        void checkValue(
                        SqlRecord *record,
                        const char *fieldName,
                        const char *value
                        );
        void checkValue(
                        SqlRecord *record,
                        const char *fieldName,
                        int minValue,
                        int maxValue
                        );

      public:
        mutable RecursiveLock mLock;
        TestLocationDBWeakPtr mThisWeak;

        ULONG mCount {};

        String mUserHash;

        IUseAbstractionPtr mAbstraction;

        OverrideLocationAbstractionPtr mOverride;

        OverrideLocationAbstractionFactoryPtr mFactory;

        TestLocationDBPtr mMaster;
      };
      
    }
  }
}

