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

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabasesForLocationDatabase)

      //-----------------------------------------------------------------------
      interaction ILocationDatabaseForLocationDatabases
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseForLocationDatabases, ForLocationDatabases)

        virtual ~ILocationDatabaseForLocationDatabases() {}
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabase
      #pragma mark

      class LocationDatabase : public SharedRecursiveLock,
                               public Noop,
                               public ILocationDatabaseLocal,
                               public ILocationDatabaseForLocationDatabases
      {
      public:
        friend interaction ILocationDatabaseFactory;
        friend interaction ILocationDatabase;
        friend interaction ILocationDatabaseLocal;
        friend interaction ILocationDatabaseForLocationDatabases;

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabase, UseLocationDatabases)

      protected:
        LocationDatabase(UseLocationDatabasesPtr databases);
        
        LocationDatabase(Noop) :
          SharedRecursiveLock(SharedRecursiveLock::create()),
          Noop(true) {}

        void init();

      public:
        ~LocationDatabase();

        static LocationDatabasePtr convert(ILocationDatabasePtr object);
        static LocationDatabasePtr convert(ForLocationDatabasesPtr object);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabase
        #pragma mark

        static ElementPtr toDebug(ILocationDatabasePtr object);

        static ILocationDatabasePtr open(
                                         ILocationDatabaseDelegatePtr inDelegate,
                                         ILocationPtr location,
                                         const char *databaseID,
                                         bool inAutomaticallyDownloadDatabaseData
                                         );

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getLocation() const;

        virtual String getDatabaseID() const;

        virtual ElementPtr getMetaData() const;

        virtual Time getExpires() const;

        virtual EntryListPtr getUpdates(
                                        const String &inExistingVersion,
                                        String &outNewVersion
                                        ) const;

        virtual EntryPtr getEntry(const char *inUniqueID) const;

        virtual void notifyWhenDataReady(
                                         const UniqueIDList &needingEntryData,
                                         ILocationDatabaseDataReadyDelegatePtr inDelegate
                                         );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabaseLocation
        #pragma mark


        static ILocationDatabaseLocalPtr toLocal(ILocationDatabasePtr location);

        static ILocationDatabaseLocalPtr create(
                                                ILocationDatabaseLocalDelegatePtr inDelegate,
                                                IAccountPtr account,
                                                const char *inDatabaseID,
                                                ElementPtr inMetaData,
                                                const PeerList &inPeerAccessList,
                                                Time expires = Time()
                                                );

        virtual PeerListPtr getPeerAccessList() const;

        virtual void setExpires(const Time &time);

        virtual void add(
                         const char *uniqueID,
                         ElementPtr inMetaData,
                         ElementPtr inData
                         );

        virtual void update(
                            const char *uniqueID,
                            ElementPtr inData
                            );

        virtual void remove(const char *uniqueID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => ILocationDatabaseForLocationDatabases
        #pragma mark

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        ILocationDatabaseSubscriptionPtr subscribe(ILocationDatabaseDelegatePtr originalDelegate);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabase => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabasesWeakPtr mThisWeak;

        UseLocationDatabasesPtr mDatabases;

        ILocationDatabaseDelegateSubscriptions mSubscriptions;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabaseFactory
      #pragma mark

      interaction ILocationDatabaseFactory
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabaseLocal::PeerList, PeerList)

        static ILocationDatabaseFactory &singleton();

        virtual ILocationDatabasePtr open(
                                          ILocationDatabaseDelegatePtr inDelegate,
                                          ILocationPtr location,
                                          const char *databaseID,
                                          bool inAutomaticallyDownloadDatabaseData
                                          );

        virtual ILocationDatabaseLocalPtr create(
                                                 ILocationDatabaseLocalDelegatePtr inDelegate,
                                                 IAccountPtr account,
                                                 const char *inDatabaseID,
                                                 ElementPtr inMetaData,
                                                 const PeerList &inPeerAccessList,
                                                 Time expires = Time()
                                                 );
      };

      class LocationDatabaseFactory : public IFactory<ILocationDatabaseFactory> {};

    }
  }
}
