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

#include <openpeer/stack/internal/stack_MessageMonitor.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Location.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/message/Message.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>


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
      #pragma mark IMessageMonitorForAccount
      #pragma mark


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMontiorForAccountPeerLocation
      #pragma mark


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark
      #pragma mark

      //-----------------------------------------------------------------------
      MessageMonitor::MessageMonitor(
                                     MessageMonitorManagerPtr manager,
                                     IMessageQueuePtr queue
                                     ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(manager ? *manager : SharedRecursiveLock::create()),
        mManager(manager),
        mWasHandled(false),
        mTimeoutFired(false),
        mPendingHandled(0)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::init(Seconds timeout)
      {
        AutoRecursiveLock lock(*this);

        if (Seconds() != timeout) {
          mExpires = zsLib::now() + timeout;
          mTimer = Timer::create(mThisWeak.lock(), timeout, false);
        }

        UseMessageMonitorManagerPtr manager = mManager.lock();
        if (manager) {
          manager->monitorStart(mThisWeak.lock());
        }
      }

      //-----------------------------------------------------------------------
      MessageMonitor::~MessageMonitor()
      {
        if(isNoop()) return;
        
        AutoRecursiveLock lock(*this);
        mThisWeak.reset();
        cancel();

        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr MessageMonitor::convert(IMessageMonitorPtr monitor)
      {
        return ZS_DYNAMIC_PTR_CAST(MessageMonitor, monitor);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => IMessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr MessageMonitor::toDebug(IMessageMonitorPtr monitor)
      {
        if (!monitor) return ElementPtr();
        return MessageMonitor::convert(monitor)->toDebug();
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr MessageMonitor::monitor(
                                                IMessageMonitorDelegatePtr delegate,
                                                message::MessagePtr requestMessage,
                                                Seconds timeout,
                                                PromisePtr sendPromise
                                                )
      {
        if (!requestMessage) return MessageMonitorPtr();

        MessageMonitorPtr pThis(new MessageMonitor(
                                                   MessageMonitorManager::convert(UseMessageMonitorManager::singleton()),
                                                   UseStack::queueStack()
                                                   ));
        pThis->mThisWeak = pThis;
        pThis->mMessageID = requestMessage->messageID();
        pThis->mOriginalMessage = requestMessage;
        pThis->mSendPromise = sendPromise;
        pThis->mDelegate = IMessageMonitorDelegateProxy::createWeak(UseStack::queueDelegate(), delegate);
        pThis->init(timeout);
        if (pThis->mSendPromise) {
          pThis->mSendPromise->then(pThis);
        }

        ZS_THROW_INVALID_USAGE_IF(!pThis->mDelegate)

        return pThis;
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr MessageMonitor::monitorAndSendToLocation(
                                                                 IMessageMonitorDelegatePtr delegate,
                                                                 ILocationPtr inLocation,
                                                                 message::MessagePtr message,
                                                                 Seconds timeout
                                                                 )
      {
        if (!message) return MessageMonitorPtr();

        ZS_THROW_INVALID_ARGUMENT_IF(!inLocation)

        UseLocationPtr location = Location::convert(inLocation);

        MessageMonitorPtr pThis = monitor(delegate, message, timeout, Promise::create(UseStack::queueDelegate()));

        PromisePtr sendPromise = location->send(message);
        sendPromise->then(pThis->mSendPromise);
        sendPromise->background();

        return pThis;
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr MessageMonitor::monitorAndSendToService(
                                                                IMessageMonitorDelegatePtr delegate,
                                                                IBootstrappedNetworkPtr bootstrappedNetwork,
                                                                const char *serviceType,
                                                                const char *serviceMethodName,
                                                                message::MessagePtr message,
                                                                Seconds timeout
                                                                )
      {
        if (!message) return MessageMonitorPtr();

        ZS_THROW_INVALID_ARGUMENT_IF(!bootstrappedNetwork)

        MessageMonitorPtr pThis = monitor(delegate, message, timeout, Promise::create(UseStack::queueDelegate()));
        PromisePtr promise = bootstrappedNetwork->sendServiceMessage(serviceType, serviceMethodName, message);
        promise->then(pThis->mSendPromise);
        promise->background();
        return pThis;
      }

      //-----------------------------------------------------------------------
      bool MessageMonitor::handleMessageReceived(message::MessagePtr message)
      {
        UseMessageMonitorManagerPtr manager = UseMessageMonitorManager::singleton();
        if (!manager) return false;
        return manager->handleMessage(message);
      }

      //-----------------------------------------------------------------------
      bool MessageMonitor::isComplete() const
      {
        AutoRecursiveLock lock(*this);
        return !mDelegate;
      }

      //-----------------------------------------------------------------------
      bool MessageMonitor::wasHandled() const
      {
        AutoRecursiveLock lock(*this);
        return mWasHandled;
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) return;

        UseMessageMonitorManagerPtr manager = UseMessageMonitorManager::singleton();
        if (manager) {
          manager->monitorEnd(*this);
        }

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }

        mSendPromise.reset();

        mDelegate.reset();
      }

      //-----------------------------------------------------------------------
      message::MessagePtr MessageMonitor::getMonitoredMessage() const
      {
        AutoRecursiveLock lock(*this);
        return mOriginalMessage;
      }

      //-----------------------------------------------------------------------
      String MessageMonitor::getMonitoredMessageID() const
      {
        AutoRecursiveLock lock(*this);
        return mOriginalMessage->messageID();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => IMessageMonitorAsyncDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitor::onHandleMessage(
                                           IMessageMonitorDelegatePtr delegate,
                                           message::MessagePtr message
                                           )
      {
        bool handled = delegate->handleMessageMonitorMessageReceived(mThisWeak.lock(), message);

        {
          AutoRecursiveLock lock(*this);
          --mPendingHandled;

          mWasHandled = handled;

          if (handled) {
            cancel();
            return;
          }

          if (mTimeoutFired) {
            delegate->onMessageMonitorTimedOut(mThisWeak.lock());

            cancel();
            return;
          }
        }
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::onAutoHandleFailureResult(MessageResultPtr result)
      {
        ZS_LOG_DEBUG(log("auto handle error") + Message::toDebug(result))

        handleMessageReceived(result);
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::onAutoHandleTimeout()
      {
        ZS_LOG_DEBUG(log("auto handle timeout"))

        UseMessageMonitorManagerPtr manager = UseMessageMonitorManager::singleton();
        if (!manager) {
          ZS_LOG_WARNING(Detail, log("manager is gone"))
          handleTimeout();
          return;
        }
        manager->handleTimeoutAll(mOriginalMessage);
      }

      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => IMessageMonitorForMessageMonitorManager
      #pragma mark

      //-----------------------------------------------------------------------
      bool MessageMonitor::handleMessage(message::MessagePtr message)
      {
        AutoRecursiveLock lock(*this);
        if (!mDelegate) return false;
        if (!message) return false;
        if (mMessageID != message->messageID()) return false;
        if (mTimeoutFired) return false;  // message arrived too late

        // create a strong reference proxy from a weak reference proxy
        IMessageMonitorDelegatePtr delegate = IMessageMonitorDelegateProxy::create(UseStack::queueDelegate(), mDelegate);
        if (!delegate) return false;

        ++mPendingHandled;
        (IMessageMonitorAsyncDelegateProxy::create(mThisWeak.lock()))->onHandleMessage(delegate, message);

        // NOTE:  Do not cancel the requester since it is possible to receive
        //        more than one request/notify/response for each message ID
        return true;
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::handleTimeout()
      {
        AutoRecursiveLock lock(*this);

        if (!mDelegate) return;

        mTimeoutFired = true;
        if (0 != mPendingHandled) return;

        try {
          mDelegate->onMessageMonitorTimedOut(mThisWeak.lock());
        } catch (IMessageMonitorDelegateProxy::Exceptions::DelegateGone &) {
        }

        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitor::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_TRACE(log("on timer"))
        handleTimeout();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => IPromiseCatchDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitor::onPromiseRejected(PromisePtr promise)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_WARNING(Detail, debug("notifying of message send failure (because sender could not send message)") + message::Message::toDebug(mOriginalMessage))
        PromiseRejectionStatusPtr status = promise->reason<PromiseRejectionStatus>();
        if (status) {
          notifyWithError(status->mStatus, status->mMessage.hasData() ? status->mMessage.c_str() : NULL);
        } else {
          notifyWithError(IHTTP::HTTPStatusCode_Gone, NULL);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitor => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params MessageMonitor::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::MessageMonitor");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params MessageMonitor::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr MessageMonitor::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("stack::MessageMonitor");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "message id", mMessageID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "handled", mWasHandled);
        IHelper::debugAppend(resultEl, "timeout", mTimeoutFired);
        IHelper::debugAppend(resultEl, "pending handled", mPendingHandled);

        IHelper::debugAppend(resultEl, "timer", (bool)mTimer);
        IHelper::debugAppend(resultEl, "expires", mExpires);
        IHelper::debugAppend(resultEl, "original message", (bool)mOriginalMessage);

        IHelper::debugAppend(resultEl, "send promise", mSendPromise ? mSendPromise->getID() : 0);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void MessageMonitor::notifyWithError(
                                           IHTTP::HTTPStatusCodes code,
                                           const char *reason
                                           )
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_WARNING(Detail, log("notifying of message send failure") + ZS_PARAM("message id", mMessageID) + ZS_PARAM("code", code) + message::Message::toDebug(mOriginalMessage))

        MessageResultPtr result = MessageResult::create(mOriginalMessage, code, reason);
        if (result) {
          // create a fake error result since the send failed
          IMessageMonitorAsyncDelegateProxy::create(UseStack::queueStack(), mThisWeak.lock())->onAutoHandleFailureResult(result);
          return;
        }

        IMessageMonitorAsyncDelegateProxy::create(UseStack::queueStack(), mThisWeak.lock())->onAutoHandleTimeout();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageMonitorFactory &IMessageMonitorFactory::singleton()
      {
        return MessageMonitorFactory::singleton();
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitor(
                                                        IMessageMonitorDelegatePtr delegate,
                                                        message::MessagePtr requestMessage,
                                                        Seconds timeout,
                                                        PromisePtr sendPromise
                                                        )
      {
        if (this) {}
        return MessageMonitor::monitor(delegate, requestMessage, timeout, sendPromise);
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitorAndSendToLocation(
                                                                         IMessageMonitorDelegatePtr delegate,
                                                                         ILocationPtr peerLocation,
                                                                         message::MessagePtr message,
                                                                         Seconds timeout
                                                                         )
      {
        if (this) {}
        return MessageMonitor::monitorAndSendToLocation(delegate, peerLocation, message, timeout);
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitorAndSendToService(
                                                                        IMessageMonitorDelegatePtr delegate,
                                                                        IBootstrappedNetworkPtr bootstrappedNetwork,
                                                                        const char *serviceType,
                                                                        const char *serviceMethodName,
                                                                        message::MessagePtr message,
                                                                        Seconds timeout
                                                                        )
      {
        if (this) {}
        return MessageMonitor::monitorAndSendToService(delegate, bootstrappedNetwork, serviceType, serviceMethodName, message, timeout);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IMessageMonitor
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IMessageMonitor::toDebug(IMessageMonitorPtr monitor)
    {
      return internal::MessageMonitor::toDebug(monitor);
    }

    //-------------------------------------------------------------------------
    IMessageMonitorPtr IMessageMonitor::monitor(
                                                IMessageMonitorDelegatePtr delegate,
                                                message::MessagePtr requestMessage,
                                                Seconds timeout,
                                                PromisePtr sendPromise
                                                )
    {
      return internal::IMessageMonitorFactory::singleton().monitor(delegate, requestMessage, timeout, sendPromise);
    }

    //-------------------------------------------------------------------------
    IMessageMonitorPtr IMessageMonitor::monitorAndSendToLocation(
                                                                 IMessageMonitorDelegatePtr delegate,
                                                                 ILocationPtr peerLocation,
                                                                 message::MessagePtr message,
                                                                 Seconds timeout
                                                                 )
    {
      return internal::IMessageMonitorFactory::singleton().monitorAndSendToLocation(delegate, peerLocation, message, timeout);
    }

    //-------------------------------------------------------------------------
    IMessageMonitorPtr IMessageMonitor::monitorAndSendToService(
                                                                IMessageMonitorDelegatePtr delegate,
                                                                IBootstrappedNetworkPtr bootstrappedNetwork,
                                                                const char *serviceType,
                                                                const char *serviceMethodName,
                                                                message::MessagePtr message,
                                                                Seconds timeout
                                                                )
    {
      return internal::IMessageMonitorFactory::singleton().monitorAndSendToService(delegate, bootstrappedNetwork, serviceType, serviceMethodName, message, timeout);
    }

    //-------------------------------------------------------------------------
    bool IMessageMonitor::handleMessageReceived(message::MessagePtr message)
    {
      return internal::MessageMonitor::handleMessageReceived(message);
    }
  }
}
