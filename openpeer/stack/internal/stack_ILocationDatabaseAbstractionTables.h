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

#include <openpeer/stack/internal/types.h>

#include <easySQLite/SqlField.h>
#include <easySQLite/SqlTable.h>
#include <easySQLite/SqlRecord.h>
#include <easySQLite/SqlValue.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseAbstractionTables
      #pragma mark

      interaction ILocationDatabaseAbstractionTables
      {
        ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
        ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
        ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
        ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

        static const char *version;
        static const char *indexDatabase;
        static const char *indexPeerLocation;
        static const char *peerLocationHash;
        static const char *peerURI;
        static const char *locationID;
        static const char *databaseID;
        static const char *lastDownloadedVersion;
        static const char *downloadComplete;
        static const char *notified;
        static const char *metaData;
        static const char *expires;
        static const char *entryID;
        static const char *data;
        static const char *dataLength;
        static const char *dataFetched;
        static const char *disposition;
        static const char *created;
        static const char *updated;
        static const char *lastAccessed;


        //---------------------------------------------------------------------
        static SqlField *Version()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(version, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Version_name() {return "version";}

        //---------------------------------------------------------------------
        static SqlField *PeerLocation()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(peerLocationHash, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(peerURI, sql::type_text, sql::flag_not_null),
            SqlField(locationID, sql::type_text, sql::flag_not_null),
            SqlField(lastDownloadedVersion, sql::type_text, sql::flag_not_null),
            SqlField(downloadComplete, sql::type_bool, sql::flag_not_null),
            SqlField(notified, sql::type_bool, sql::flag_not_null),
            SqlField(lastAccessed, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *PeerLocation_name() {return "peerLocation";}

        //---------------------------------------------------------------------
        static SqlField *Database()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexPeerLocation, sql::type_int, sql::flag_not_null),
            SqlField(databaseID, sql::type_text, sql::flag_not_null),
            SqlField(lastDownloadedVersion, sql::type_text, sql::flag_not_null),
            SqlField(metaData, sql::type_text, sql::flag_not_null),
            SqlField(expires, sql::type_int, sql::flag_not_null),
            SqlField(downloadComplete, sql::type_bool, sql::flag_not_null),
            SqlField(notified, sql::type_bool, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Database_name() {return "database";}

        //---------------------------------------------------------------------
        static SqlField *DatabaseChange()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexPeerLocation, sql::type_int, sql::flag_not_null),
            SqlField(indexDatabase, sql::type_int, sql::flag_not_null),
            SqlField(disposition, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *DatabaseChange_name() {return "database_change";}
        
        //---------------------------------------------------------------------
        static SqlField *Permission()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexPeerLocation, sql::type_int, sql::flag_not_null),
            SqlField(indexDatabase, sql::type_int, sql::flag_not_null),
            SqlField(peerURI, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Permission_name() {return "permission";}
        
        //---------------------------------------------------------------------
        static SqlField *Entry()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(entryID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(version, sql::type_int, sql::flag_not_null),
            SqlField(metaData, sql::type_text, sql::flag_not_null),
            SqlField(data, sql::type_text, sql::flag_not_null),
            SqlField(dataLength, sql::type_int, sql::flag_not_null),
            SqlField(dataFetched, sql::type_bool, sql::flag_not_null),
            SqlField(created, sql::type_int, sql::flag_not_null),
            SqlField(updated, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Entry_name() {return "entry";}
        
        //---------------------------------------------------------------------
        static SqlField *EntryChange()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(disposition, sql::type_int, sql::flag_not_null),
            SqlField(entryID, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *EntryChange_name() {return "entry_change";}

      };

    }
  }
}
