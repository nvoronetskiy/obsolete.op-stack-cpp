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

#include <openpeer/stack/internal/stack_PeerSubscription.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/stack/IMessageIncoming.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

//#include <algorithm>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }


namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;
      
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerSubscriptionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IPeerSubscriptionForAccount::toDebug(ForAccountPtr subscription)
      {
        return IPeerSubscription::toDebug(PeerSubscription::convert(subscription));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      PeerSubscription::PeerSubscription(
                                         AccountPtr account,
                                         IPeerSubscriptionDelegatePtr delegate
                                         ) :
        SharedRecursiveLock(account ? (*account) : SharedRecursiveLock::create()),
        mAccount(account),
        mDelegate(IPeerSubscriptionDelegateProxy::createWeak(UseStack::queueDelegate(), delegate))
      {
        ZS_LOG_DEBUG(log("constructed"))
      }

      //-----------------------------------------------------------------------
      void PeerSubscription::init()
      {
        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, log("subscribing to a peer attached to a dead account"))
          try {
            mDelegate->onPeerSubscriptionShutdown(mThisWeak.lock());
          } catch (IPeerSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          }
          mDelegate.reset();
          return;
        }
        mAccount.lock()->subscribe(mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      PeerSubscription::~PeerSubscription()
      {
        if(isNoop()) return;
        
        ZS_LOG_DEBUG(log("destroyed"))
        mThisWeak.reset();

        cancel();
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr PeerSubscription::convert(IPeerSubscriptionPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(PeerSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr PeerSubscription::convert(ForAccountPtr subscription)
      {
        return ZS_DYNAMIC_PTR_CAST(PeerSubscription, subscription);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerSubscription => IPeerSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PeerSubscription::toDebug(IPeerSubscriptionPtr subscription)
      {
        if (!subscription) return ElementPtr();
        return PeerSubscription::convert(subscription)->toDebug();
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr PeerSubscription::subscribeAll(
                                                         AccountPtr account,
                                                         IPeerSubscriptionDelegatePtr delegate
                                                         )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!account)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        PeerSubscriptionPtr pThis(new PeerSubscription(account, delegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr PeerSubscription::subscribe(
                                                      IPeerPtr inPeer,
                                                      IPeerSubscriptionDelegatePtr delegate
                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!inPeer)
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)

        UsePeerPtr peer = Peer::convert(inPeer);
        AccountPtr account = peer->getAccount();

        PeerSubscriptionPtr pThis(new PeerSubscription(account, delegate));
        pThis->mPeer = peer;
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IPeerPtr PeerSubscription::getSubscribedToPeer() const
      {
        AutoRecursiveLock lock(*this);
        return Peer::convert(mPeer);
      }

      //-----------------------------------------------------------------------
      bool PeerSubscription::PeerSubscription::isShutdown() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      void PeerSubscription::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_DEBUG(log("cancel called but already shutdown (probably okay)"))
          return;
        }

        PeerSubscriptionPtr subscription = mThisWeak.lock();

        if ((mDelegate) &&
            (subscription)) {
          try {
            mDelegate->onPeerSubscriptionShutdown(subscription);
          } catch(IPeerSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate already gone"))
          }
        }

        mDelegate.reset();

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_DEBUG(log("cancel called but account gone"))
          return;
        }

        account->notifyDestroyed(*this);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerSubscription => IPeerSubscriptionForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      void PeerSubscription::notifyFindStateChanged(
                                                    PeerPtr inPeer,
                                                    PeerFindStates state
                                                    )
      {
        UsePeerPtr peer = inPeer;

        AutoRecursiveLock lock(*this);
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notify of find state changed after shutdown"))
          return;
        }

        if (mPeer) {
          if (mPeer->getPeerURI() != peer->getPeerURI()) {
            ZS_LOG_DEBUG(log("ignoring find state for peer") + ZS_PARAM("notified peer", UsePeer::toDebug(peer)) + ZS_PARAM("subscribing peer", UsePeer::toDebug(mPeer)))
            return;
          }
        }

        try {
          mDelegate->onPeerSubscriptionFindStateChanged(mThisWeak.lock(), Peer::convert(peer), state);
        } catch(IPeerSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void PeerSubscription::notifyLocationConnectionStateChanged(
                                                                  LocationPtr inLocation,
                                                                  LocationConnectionStates state
                                                                  )
      {
        UseLocationPtr location = inLocation;

        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notify of location connection state changed after shutdown"))
          return;
        }

        if (mPeer) {
          UsePeerPtr peer = location->getPeer();
          if (!peer) {
            ZS_LOG_DEBUG(log("ignoring location connection state change from non-peer") + ZS_PARAM("subscribing", UsePeer::toDebug(mPeer)))
            return;
          }
          if (mPeer->getPeerURI() != peer->getPeerURI()) {
            ZS_LOG_DEBUG(log("ignoring location connection state change") + ZS_PARAM("notified peer", UsePeer::toDebug(peer)) + ZS_PARAM("subscribing peer", UsePeer::toDebug(mPeer)))
            return;
          }
        }

        try {
          mDelegate->onPeerSubscriptionLocationConnectionStateChanged(mThisWeak.lock(), Location::convert(location), state);
        } catch(IPeerSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void PeerSubscription::notifyMessageIncoming(IMessageIncomingPtr message)
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("notify of incoming message after shutdown"))
          return;
        }

        UseLocationPtr location = Location::convert(message->getLocation());
        if (!location) {
          ZS_LOG_DEBUG(log("ignoring incoming message missing location"))
          return;
        }
        if (mPeer) {
          UsePeerPtr peer = location->getPeer();
          if (!peer) {
            ZS_LOG_DEBUG(log("ignoring incoming message from non-peer") + ZS_PARAM("subscribing peer", UsePeer::toDebug(mPeer)) + ZS_PARAM("incoming", IMessageIncoming::toDebug(message)))
            return;
          }
          if (mPeer->getPeerURI() != peer->getPeerURI()) {
            ZS_LOG_TRACE(log("ignoring incoming message for peer") + ZS_PARAM("subscribing peer", UsePeer::toDebug(mPeer)) + ZS_PARAM("incoming", IMessageIncoming::toDebug(message)))
            return;
          }
        }

        try {
          ZS_LOG_DEBUG(log("notifying peer subscription of messaging incoming") + ZS_PARAM("subscribing peer", UsePeer::toDebug(mPeer)) + ZS_PARAM("incoming", IMessageIncoming::toDebug(message)))
          mDelegate->onPeerSubscriptionMessageIncoming(mThisWeak.lock(), message);
        } catch(IPeerSubscriptionDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }

      //-----------------------------------------------------------------------
      void PeerSubscription::notifyShutdown()
      {
        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PeerSubscription::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PeerSubscription");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PeerSubscription::toDebug() const
      {
        AutoRecursiveLock lock(*this);
        ElementPtr resultEl = Element::create("PeerSubscription");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "subscribing", mPeer ? "peer" : "all");
        IHelper::debugAppend(resultEl, UsePeer::toDebug(mPeer));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerSubscriptionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerSubscriptionFactory &IPeerSubscriptionFactory::singleton()
      {
        return PeerSubscriptionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr IPeerSubscriptionFactory::subscribeAll(
                                                                 AccountPtr account,
                                                                 IPeerSubscriptionDelegatePtr delegate
                                                                 )
      {
        if (this) {}
        return PeerSubscription::subscribeAll(account, delegate);
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr IPeerSubscriptionFactory::subscribe(
                                                              IPeerPtr peer,
                                                              IPeerSubscriptionDelegatePtr delegate
                                                              )
      {
        if (this) {}
        return PeerSubscription::subscribe(peer, delegate);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPeerSubscription
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPeerSubscription::toDebug(IPeerSubscriptionPtr subscription)
    {
      return internal::PeerSubscription::toDebug(subscription);
    }

    //-------------------------------------------------------------------------
    IPeerSubscriptionPtr IPeerSubscription::subscribeAll(
                                                         IAccountPtr account,
                                                         IPeerSubscriptionDelegatePtr delegate
                                                         )
    {
      return internal::IPeerSubscriptionFactory::singleton().subscribeAll(internal::Account::convert(account), delegate);
    }

    //-------------------------------------------------------------------------
    IPeerSubscriptionPtr IPeerSubscription::subscribe(
                                                      IPeerPtr peer,
                                                      IPeerSubscriptionDelegatePtr delegate
                                                      )
    {
      return internal::IPeerSubscriptionFactory::singleton().subscribe(peer, delegate);
    }
  }
}
