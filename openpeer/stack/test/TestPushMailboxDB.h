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

#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_ServicePushMailboxSessionDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstractionTables.h>

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      using zsLib::ULONG;

      ZS_DECLARE_CLASS_PTR(OverrideAbstractionFactory)

      ZS_DECLARE_CLASS_PTR(OverridePushMailboxAbstraction)

      ZS_DECLARE_CLASS_PTR(TestPushMailboxDB)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark OverridePushMailboxAbstraction
      #pragma mark

      class OverridePushMailboxAbstraction : public openpeer::stack::internal::ServicePushMailboxSessionDatabaseAbstraction
      {
      public:
        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)

        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IServicePushMailboxSessionDatabaseAbstraction, IUseAbstraction)
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::ServicePushMailboxSessionDatabaseAbstraction, UseAbstraction)

      public:
        OverridePushMailboxAbstraction(
                                       const char *inHashRepresentingUser,
                                       const char *inUserTemporaryFilePath,
                                       const char *inUserStorageFilePath
                                       );

        static OverridePushMailboxAbstractionPtr convert(IUseAbstractionPtr abstraction);

        static OverridePushMailboxAbstractionPtr create(
                                                        const char *inHashRepresentingUser,
                                                        const char *inUserTemporaryFilePath,
                                                        const char *inUserStorageFilePath
                                                        );

        void init();

        SqlDatabasePtr getDatabase();
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark TestPushMailboxDB
      #pragma mark

      class TestPushMailboxDB
      {
      public:
        ZS_DECLARE_TYPEDEF_PTR(zsLib::RecursiveLock, RecursiveLock)

        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IServicePushMailboxSessionDatabaseAbstraction, IUseAbstraction)
        ZS_DECLARE_TYPEDEF_PTR(openpeer::stack::internal::IServicePushMailboxSessionDatabaseAbstractionTables, UseTables)
        
        ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
        ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
        ZS_DECLARE_TYPEDEF_PTR(sql::RecordSet, SqlRecordSet)
        ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
        ZS_DECLARE_TYPEDEF_PTR(sql::Database, SqlDatabase)

      private:
        TestPushMailboxDB();

        void init();

      public:
        ~TestPushMailboxDB();

        static TestPushMailboxDBPtr create();

        void testCreate();
        void testSettings();
        void testFolder();
        void testFolderMessage();
        void testFolderVersionedMessage();
        void testMessage();
        void testDeliveryState();

      protected:
        void tableDump(
                       SqlField *definition,
                       const char *tableName
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
        TestPushMailboxDBWeakPtr mThisWeak;

        ULONG mCount {};

        String mUserHash;

        IUseAbstractionPtr mAbstraction;

        OverridePushMailboxAbstractionPtr mOverride;

        OverrideAbstractionFactoryPtr mFactory;
      };
      
    }
  }
}

