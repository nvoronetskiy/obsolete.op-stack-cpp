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

#include <openpeer/stack/IAccount.h>
#include <openpeer/stack/internal/types.h>
#include <openpeer/stack/IPeerSubscription.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForPeerSubscription;
      interaction ILocationForPeerSubscription;
      interaction IPeerForPeerSubscription;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerSubscriptionForAccount
      #pragma mark

      interaction IPeerSubscriptionForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerSubscriptionForAccount, ForAccount)

        typedef IPeer::PeerFindStates PeerFindStates;
        typedef ILocation::LocationConnectionStates LocationConnectionStates;

        static ElementPtr toDebug(ForAccountPtr subscription);

        virtual PUID getID() const = 0;

        virtual IPeerPtr getSubscribedToPeer() const = 0;

        virtual void notifyFindStateChanged(
                                            PeerPtr peer,
                                            PeerFindStates state
                                            ) = 0;

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
      #pragma mark PeerSubscription
      #pragma mark

      class PeerSubscription : public Noop,
                               public SharedRecursiveLock,
                               public IPeerSubscription,
                               public IPeerSubscriptionForAccount
      {
      public:
        friend interaction IPeerSubscriptionFactory;
        friend interaction IPeerSubscription;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForPeerSubscription, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForPeerSubscription, UseLocation)
        ZS_DECLARE_TYPEDEF_PTR(IPeerForPeerSubscription, UsePeer)

      protected:
        PeerSubscription(
                         AccountPtr account,
                         IPeerSubscriptionDelegatePtr delegate
                         );
        
        PeerSubscription(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~PeerSubscription();

        static PeerSubscriptionPtr convert(IPeerSubscriptionPtr subscription);
        static PeerSubscriptionPtr convert(ForAccountPtr subscription);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerSubscription => IPeerSubscription
        #pragma mark

        static ElementPtr toDebug(IPeerSubscriptionPtr subscription);

        static PeerSubscriptionPtr subscribeAll(
                                                AccountPtr account,
                                                IPeerSubscriptionDelegatePtr delegate
                                                );

        static PeerSubscriptionPtr subscribe(
                                             IPeerPtr peer,
                                             IPeerSubscriptionDelegatePtr delegate
                                             );

        virtual PUID getID() const {return mID;}

        virtual IPeerPtr getSubscribedToPeer() const;

        virtual bool isShutdown() const;

        virtual void cancel();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerSubscription => IPeerSubscriptionForAccount
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual IPeerPtr getSubscribedToPeer() const;

        virtual void notifyFindStateChanged(
                                            PeerPtr peer,
                                            PeerFindStates state
                                            );

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
        #pragma mark PeerSubscription => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerSubscription => (data)
        #pragma mark

        AutoPUID mID;
        PeerSubscriptionWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;

        UsePeerPtr mPeer;

        IPeerSubscriptionDelegatePtr mDelegate;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerSubscriptionFactory
      #pragma mark

      interaction IPeerSubscriptionFactory
      {
        static IPeerSubscriptionFactory &singleton();

        virtual PeerSubscriptionPtr subscribeAll(
                                                 AccountPtr account,
                                                 IPeerSubscriptionDelegatePtr delegate
                                                 );

        virtual PeerSubscriptionPtr subscribe(
                                              IPeerPtr peer,
                                              IPeerSubscriptionDelegatePtr delegate
                                              );
      };

      class PeerSubscriptionFactory : public IFactory<IPeerSubscriptionFactory> {};
      
    }
  }
}
