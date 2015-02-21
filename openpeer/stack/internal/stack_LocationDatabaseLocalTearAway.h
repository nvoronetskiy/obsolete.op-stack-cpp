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
#include <openpeer/stack/ILocationDatabaseLocal.h>

#include <zsLib/TearAway.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_STRUCT_PTR(LocationDatabaseLocalTearAwayData)

      struct LocationDatabaseLocalTearAwayData
      {
        ILocationDatabaseSubscriptionPtr mSubscription;
      };

    }
  }
}

ZS_DECLARE_TEAR_AWAY_BEGIN(openpeer::stack::ILocationDatabaseLocal, openpeer::stack::internal::LocationDatabaseLocalTearAwayData)
ZS_DECLARE_TEAR_AWAY_TYPEDEF(openpeer::stack::ILocationPtr, ILocationPtr)
ZS_DECLARE_TEAR_AWAY_TYPEDEF(zsLib::XML::ElementPtr, ElementPtr)
ZS_DECLARE_TEAR_AWAY_TYPEDEF(zsLib::PromisePtr, PromisePtr)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getID, PUID)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getLocation, ILocationPtr)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getDatabaseID, String)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getMetaData, ElementPtr)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getExpires, Time)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_2(getUpdates, EntryInfoListPtr, const String &, String &)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_1(getEntry, EntryInfoPtr, const char *)
ZS_DECLARE_TEAR_AWAY_METHOD_RETURN_1(notifyWhenDataReady, PromisePtr, const UniqueIDList &)
ZS_DECLARE_TEAR_AWAY_METHOD_CONST_RETURN_0(getPeerAccessList, PeerListPtr)
ZS_DECLARE_TEAR_AWAY_METHOD_1(setExpires, const Time &)
ZS_DECLARE_TEAR_AWAY_METHOD_RETURN_3(add, bool, const char *, ElementPtr, ElementPtr)
ZS_DECLARE_TEAR_AWAY_METHOD_RETURN_2(update, bool, const char *, ElementPtr)
ZS_DECLARE_TEAR_AWAY_METHOD_RETURN_1(remove, bool, const char *)
ZS_DECLARE_TEAR_AWAY_END()
