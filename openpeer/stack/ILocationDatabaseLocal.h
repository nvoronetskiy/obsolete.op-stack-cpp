/*

 Copyright (c) 2015, Hookflash Inc.
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

#include <openpeer/stack/types.h>
#include <openpeer/stack/ILocationDatabase.h>

namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseLocalTypes
    #pragma mark

    interaction ILocationDatabaseLocalTypes : public ILocationDatabase
    {
      typedef std::list<IPeerPtr> PeerList;
      ZS_DECLARE_PTR(PeerList)
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseLocal
    #pragma mark

    interaction ILocationDatabaseLocal : public ILocationDatabaseLocalTypes
    {
      //-----------------------------------------------------------------------
      // PURPOSE: Convert a location database to a local location database
      //          when the database is known to be a local location database.
      // RETURNS: The converted local location database or a NULL
      //          ILocationDatabaseLocalPtr() if the locaiton
      static ILocationDatabaseLocalPtr toLocal(ILocationDatabasePtr location);

      //-----------------------------------------------------------------------
      // PURPOSE: Creates a local database given a database identifier.
      static ILocationDatabaseLocalPtr create(
                                              ILocationDatabaseLocalDelegatePtr inDelegate,
                                              IAccountPtr account,
                                              const char *inDatabaseID,
                                              ElementPtr inMetaData,
                                              const PeerList &inPeerAccessList,
                                              Time expires = Time()  // Time() means the database should never expire
                                              );

      //-----------------------------------------------------------------------
      // PURPOSE: Obtain list of peers that have access to this database.
      virtual PeerListPtr getPeerAccessList() const = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Updates when a database should automatically expire.
      virtual void setExpires(const Time &time) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Add a unique entry into the database.
      virtual void add(
                       const char *uniqueID,
                       ElementPtr inMetaData,
                       ElementPtr inData
                       ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Updates an existing entry in the database.
      virtual void update(
                          const char *uniqueID,
                          ElementPtr inData
                          ) = 0;

      //-----------------------------------------------------------------------
      // PURPOSE: Remove an entry from the database.
      virtual void remove(const char *uniqueID) = 0;
    };
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationDatabaseLocalDelegate
    #pragma mark

    interaction ILocationDatabaseLocalDelegate : public ILocationDatabaseDelegate
    {
    };

  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::ILocationDatabaseLocalDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationDatabasePtr, ILocationDatabasePtr)
ZS_DECLARE_PROXY_METHOD_1(onLocationDatabaseChanged, ILocationDatabasePtr)
ZS_DECLARE_PROXY_END()
