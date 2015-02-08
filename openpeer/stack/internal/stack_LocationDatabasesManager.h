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
#include <openpeer/stack/ILocation.h>


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction ILocationDatabasesForLocationDatabasesManager;

      interaction ILocationDatabasesManagerForLocationDatabases
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesManagerForLocationDatabases, ForLocationDatabases)

        static ForLocationDatabasesPtr singleton();

        static LocationDatabasesPtr getOrCreateForLocation(ILocationPtr location);

        static void notifyDestroyed(LocationDatabases &databases);

        virtual ~ILocationDatabasesManagerForLocationDatabases() {}
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location
      #pragma mark

      class LocationDatabasesManager : public SharedRecursiveLock,
                                       public Noop,
                                       public ILocationDatabasesManagerForLocationDatabases
      {
      public:
        friend interaction ILocationDatabasesManagerFactory;
        friend interaction ILocationDatabasesManagerForLocationDatabases;

        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabasesManager, UseLocationDatabases)

      protected:
        LocationDatabasesManager();
        
        LocationDatabasesManager(Noop) :
          SharedRecursiveLock(SharedRecursiveLock::create()),
          Noop(true) {}

        void init();

        static LocationDatabasesManagerPtr create();
        static LocationDatabasesManagerPtr singleton();

        typedef PUID LocationID;
        typedef std::map<LocationID, UseLocationDatabasesWeakPtr> LocationDatabasesMap;

      public:
        ~LocationDatabasesManager();

        static LocationDatabasesManagerPtr convert(ForLocationDatabasesPtr object);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabasesManager => ILocationDatabasesManagerForLocationDatabases
        #pragma mark

        virtual LocationDatabasesPtr getOrCreateForLocation(ILocationPtr location);
        virtual void notifyDestroyed(LocationDatabases &databases);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabasesManager => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabasesManager => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabasesManagerWeakPtr mThisWeak;

        LocationDatabasesMap mDatabases;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesManagerFactory
      #pragma mark

      interaction ILocationDatabasesManagerFactory
      {
        static ILocationDatabasesManagerFactory &singleton();

        virtual LocationDatabasesManagerPtr singletonManager();
      };

      class LocationDatabasesManagerFactory : public IFactory<ILocationDatabasesManagerFactory> {};

    }
  }
}