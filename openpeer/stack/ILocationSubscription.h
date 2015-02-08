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

#include <openpeer/stack/types.h>
#include <openpeer/stack/message/types.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/ILocation.h>

namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationSubscription
    #pragma mark

    interaction ILocationSubscription
    {
      static ElementPtr toDebug(ILocationSubscriptionPtr subscription);

      static ILocationSubscriptionPtr subscribeAll(
                                                   IAccountPtr account,
                                                   ILocationSubscriptionDelegatePtr delegate
                                                   );

      static ILocationSubscriptionPtr subscribe(
                                                ILocationPtr location,
                                                ILocationSubscriptionDelegatePtr delegate
                                                );

      virtual PUID getID() const = 0;

      virtual ILocationPtr getSubscribedToLocation() const = 0;

      virtual bool isShutdown() const = 0;

      virtual void cancel() = 0;
    };

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ILocationSubscriptionDelegate
    #pragma mark

    interaction ILocationSubscriptionDelegate
    {
      typedef ILocation::LocationConnectionStates LocationConnectionStates;

      virtual void onLocationSubscriptionShutdown(ILocationSubscriptionPtr subscription) = 0;

      virtual void onLocationSubscriptionLocationConnectionStateChanged(
                                                                        ILocationSubscriptionPtr subscription,
                                                                        ILocationPtr location,
                                                                        LocationConnectionStates state
                                                                        ) = 0;

      virtual void onLocationSubscriptionMessageIncoming(
                                                         ILocationSubscriptionPtr subscription,
                                                         IMessageIncomingPtr message
                                                         ) = 0;
    };
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::ILocationSubscriptionDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationSubscriptionPtr, ILocationSubscriptionPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationPtr, ILocationPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IMessageIncomingPtr, IMessageIncomingPtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::ILocationSubscriptionDelegate::LocationConnectionStates, LocationConnectionStates)
ZS_DECLARE_PROXY_METHOD_1(onLocationSubscriptionShutdown, ILocationSubscriptionPtr)
ZS_DECLARE_PROXY_METHOD_3(onLocationSubscriptionLocationConnectionStateChanged, ILocationSubscriptionPtr, ILocationPtr, LocationConnectionStates)
ZS_DECLARE_PROXY_METHOD_2(onLocationSubscriptionMessageIncoming, ILocationSubscriptionPtr, IMessageIncomingPtr)
ZS_DECLARE_PROXY_END()
