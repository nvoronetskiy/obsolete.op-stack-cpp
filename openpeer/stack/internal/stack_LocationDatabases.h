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
#include <openpeer/stack/ILocationDatabases.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      ZS_DECLARE_INTERACTION_PTR(ILocationForLocationDatabases)
      ZS_DECLARE_INTERACTION_PTR(ILocationDatabasesManagerForLocationDatabases)

      //-----------------------------------------------------------------------
      interaction ILocationDatabasesForLocationDatabasesManager
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesForLocationDatabasesManager, ForLocationDatabasesManager)

        static ElementPtr toDebug(ForLocationDatabasesManagerPtr object);

        static ForLocationDatabasesManagerPtr create(ILocationPtr location);

        virtual PUID getID() const = 0;

        virtual ILocationPtr getLocation() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationDatabases
      #pragma mark

      class LocationDatabases : public SharedRecursiveLock,
                                public Noop,
                                public ILocationDatabases,
                                public ILocationDatabasesForLocationDatabasesManager
      {
      public:
        friend interaction ILocationDatabasesFactory;
        friend interaction ILocationDatabases;
        friend interaction ILocationDatabasesForLocationDatabasesManager;

      protected:
        LocationDatabases(ILocationPtr location);
        
        LocationDatabases(Noop) :
          SharedRecursiveLock(SharedRecursiveLock::create()),
          Noop(true) {}

        void init();

      public:
        ~LocationDatabases();

        static LocationDatabasesPtr convert(ILocationDatabasesPtr object);
        static LocationDatabasesPtr convert(ForLocationDatabasesManagerPtr object);

        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationDatabases, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(ILocationDatabasesManagerForLocationDatabases, UseLocationDatabasesManager)

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationDatabases
        #pragma mark

        static ElementPtr toDebug(ILocationDatabasesPtr object);

        static ILocationDatabasesPtr open(
                                          ILocationPtr inLocation,
                                          ILocationDatabasesDelegatePtr inDelegate
                                          );

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getLocation() const;

        virtual LocationDatabaseListPtr getUpdates(
                                                   const String &inExistingVersion,
                                                   String &outNewVersion
                                                   ) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => ILocationDatabasesForLocationDatabasesManager
        #pragma mark

        // (duplicate) virtual ElementPtr toDebug() const;

        static LocationDatabasesPtr create(ILocationPtr location);

        // (duplicate) virtual PUID getID() const = 0;
        // (duplicate) virtual ILocationPtr getLocation() const = 0;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        ILocationDatabasesSubscriptionPtr subscribe(ILocationDatabasesDelegatePtr originalDelegate);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationDatabases => (data)
        #pragma mark

        AutoPUID mID;
        LocationDatabasesWeakPtr mThisWeak;

        UseLocationPtr mLocation;

        ILocationDatabasesDelegateSubscriptions mSubscriptions;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationDatabasesFactory
      #pragma mark

      interaction ILocationDatabasesFactory
      {
        static ILocationDatabasesFactory &singleton();

        virtual ILocationDatabasesPtr open(
                                           ILocationPtr inLocation,
                                           ILocationDatabasesDelegatePtr inDelegate
                                           );
      };

      class LocationDatabasesFactory : public IFactory<ILocationDatabasesFactory> {};

    }
  }
}
