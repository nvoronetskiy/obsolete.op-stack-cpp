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

#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/internal/types.h>
#include <openpeer/stack/ILocationSubscription.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForLocationSubscription;
      interaction ILocationForLocationSubscription;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionForAccount
      #pragma mark

      interaction ILocationSubscriptionForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationSubscriptionForAccount, ForAccount)

        typedef ILocation::LocationConnectionStates LocationConnectionStates;

        static ElementPtr toDebug(ForAccountPtr subscription);

        virtual PUID getID() const = 0;

        virtual void notifyLocationConnectionStateChanged(
                                                          LocationPtr location,
                                                          LocationConnectionStates state
                                                          ) = 0;

        virtual void notifyMessageIncoming(IMessageIncomingPtr message) = 0;

        virtual void notifyShutdown() = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LocationSubscription
      #pragma mark

      class LocationSubscription : public Noop,
                                   public SharedRecursiveLock,
                                   public ILocationSubscription,
                                   public ILocationSubscriptionForAccount
      {
      public:
        friend interaction ILocationSubscriptionFactory;
        friend interaction ILocationSubscription;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocationSubscription, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationSubscription, UseLocation)

      protected:
        LocationSubscription(
                             AccountPtr account,
                             ILocationSubscriptionDelegatePtr delegate
                             );
        
        LocationSubscription(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~LocationSubscription();

        static LocationSubscriptionPtr convert(ILocationSubscriptionPtr subscription);
        static LocationSubscriptionPtr convert(ForAccountPtr subscription);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationSubscription => ILocationSubscription
        #pragma mark

        static ElementPtr toDebug(ILocationSubscriptionPtr subscription);

        static LocationSubscriptionPtr subscribeAll(
                                                    AccountPtr account,
                                                    ILocationSubscriptionDelegatePtr delegate
                                                    );

        static LocationSubscriptionPtr subscribe(
                                                 ILocationPtr location,
                                                 ILocationSubscriptionDelegatePtr delegate
                                                 );

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getSubscribedToLocation() const;

        virtual bool isShutdown() const;

        virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationSubscription => ILocationSubscriptionForAccount
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        virtual void notifyLocationConnectionStateChanged(
                                                          LocationPtr location,
                                                          LocationConnectionStates state
                                                          );

        virtual void notifyMessageIncoming(IMessageIncomingPtr message);

        virtual void notifyShutdown();

        // (duplicate) virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationSubscription => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark LocationSubscription => (data)
        #pragma mark

        AutoPUID mID;
        LocationSubscriptionWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;

        UseLocationPtr mLocation;

        ILocationSubscriptionDelegatePtr mDelegate;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationSubscriptionFactory
      #pragma mark

      interaction ILocationSubscriptionFactory
      {
        static ILocationSubscriptionFactory &singleton();

        virtual LocationSubscriptionPtr subscribeAll(
                                                     AccountPtr account,
                                                     ILocationSubscriptionDelegatePtr delegate
                                                     );

        virtual LocationSubscriptionPtr subscribe(
                                                  ILocationPtr location,
                                                  ILocationSubscriptionDelegatePtr delegate
                                                  );
      };

      class LocationSubscriptionFactory : public IFactory<ILocationSubscriptionFactory> {};
      
    }
  }
}
