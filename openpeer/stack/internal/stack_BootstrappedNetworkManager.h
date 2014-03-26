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

#include <openpeer/stack/internal/types.h>

#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForBootstrappedNetworkManager;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkManagerForBootstrappedNetwork
      #pragma mark

      interaction IBootstrappedNetworkManagerForBootstrappedNetwork
      {
        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkManagerForBootstrappedNetwork, ForBootstrappedNetwork)

        static ForBootstrappedNetworkPtr singleton();

        virtual const SharedRecursiveLock &getLock() const = 0;

        virtual BootstrappedNetworkPtr findExistingOrUse(BootstrappedNetworkPtr network) = 0;

        virtual void registerDelegate(
                                      BootstrappedNetworkPtr bootstrappedNetwork,
                                      IBootstrappedNetworkDelegatePtr delegate
                                      ) = 0;

        virtual void notifyComplete(BootstrappedNetworkPtr bootstrappedNetwork) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetworkManager
      #pragma mark

      class BootstrappedNetworkManager : public Noop,
                                         public SharedRecursiveLock,
                                         public IBootstrappedNetworkManagerForBootstrappedNetwork
      {
      public:
        friend interaction IBootstrappedNetworkManagerFactory;
        friend interaction IBootstrappedNetworkManagerForBootstrappedNetwork;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForBootstrappedNetworkManager, UseBootstrappedNetwork)

        typedef String Domain;
        typedef std::map<Domain, UseBootstrappedNetworkPtr> BootstrappedNetworkMap;

        typedef std::pair<UseBootstrappedNetworkPtr, IBootstrappedNetworkDelegatePtr> PendingDelegatePair;
        typedef std::list<PendingDelegatePair> PendingDelegateList;
        
      protected:
        BootstrappedNetworkManager();
        
        BootstrappedNetworkManager(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create())
          {}

        void init();

      public  :
        ~BootstrappedNetworkManager();

      protected:
        static BootstrappedNetworkManagerPtr create();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetworkManager => IBootstrappedNetworkManagerForBootstrappedNetwork
        #pragma mark

        static BootstrappedNetworkManagerPtr singleton();

        // (duplicate) virtual const SharedRecursiveLock &getLock() const;

        virtual BootstrappedNetworkPtr findExistingOrUse(BootstrappedNetworkPtr network);

        virtual void registerDelegate(
                                      BootstrappedNetworkPtr bootstrappedNetwork,
                                      IBootstrappedNetworkDelegatePtr delegate
                                      );

        virtual void notifyComplete(BootstrappedNetworkPtr bootstrappedNetwork);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetworkManager => (internal)
        #pragma mark

        const SharedRecursiveLock &getLock() const {return *this;}

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark BootstrappedNetworkManager => (data)
        #pragma mark

        AutoPUID mID;
        BootstrappedNetworkManagerWeakPtr mThisWeak;

        BootstrappedNetworkMap mBootstrappedNetworks;
        PendingDelegateList mPendingDelegates;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkManagerFactory
      #pragma mark

      interaction IBootstrappedNetworkManagerFactory
      {
        static IBootstrappedNetworkManagerFactory &singleton();

        virtual BootstrappedNetworkManagerPtr createBootstrappedNetworkManager();
      };

    }
  }
}
