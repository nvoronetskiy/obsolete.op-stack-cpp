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

#include <openpeer/stack/internal/stack_ILocationDatabaseAbstractionTables.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

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

      const char *ILocationDatabaseAbstractionTables::version = "version";
      const char *ILocationDatabaseAbstractionTables::indexDatabase = "indexDatabase";
      const char *ILocationDatabaseAbstractionTables::indexPeerLocation = "indexPeerLocation";
      const char *ILocationDatabaseAbstractionTables::peerLocationHash = "peerLocationHash";
      const char *ILocationDatabaseAbstractionTables::peerURI = "peerURI";
      const char *ILocationDatabaseAbstractionTables::locationID = "locationID";
      const char *ILocationDatabaseAbstractionTables::databaseID = "databaseID";
      const char *ILocationDatabaseAbstractionTables::lastDownloadedVersion = "lastDownloadedVersion";
      const char *ILocationDatabaseAbstractionTables::metaData = "metaData";
      const char *ILocationDatabaseAbstractionTables::expires = "expires";
      const char *ILocationDatabaseAbstractionTables::entryID = "entryID";
      const char *ILocationDatabaseAbstractionTables::data = "data";
      const char *ILocationDatabaseAbstractionTables::dataLength = "dataLength";
      const char *ILocationDatabaseAbstractionTables::dataFetched = "dataFetched";
      const char *ILocationDatabaseAbstractionTables::disposition = "disposition";
      const char *ILocationDatabaseAbstractionTables::created = "created";
      const char *ILocationDatabaseAbstractionTables::updated = "updated";
      const char *ILocationDatabaseAbstractionTables::lastAccessed = "lastAccessed";
      const char *ILocationDatabaseAbstractionTables::updateVersion = "updateVersion";
    }
  }
}
