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

#include <openpeer/stack/IMessageMonitor.h>
#include <openpeer/stack/internal/types.h>

#include <zsLib/MessageQueueAssociator.h>
#include <zsLib/Timer.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IMessageMonitorManagerForMessageMonitor;
      interaction ILocationForMessageMonitor;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorForMessageMonitorManager
      #pragma mark

      interaction IMessageMonitorForMessageMonitorManager
      {
        virtual PUID getID() const = 0;

        typedef PUID SentViaObjectID;

        virtual String getMonitoredMessageID() const = 0;

        virtual bool handleMessage(message::MessagePtr message) = 0;

        virtual void notifySendMessageFailure(message::MessagePtr message) = 0;
        virtual void notifySenderObjectGone(PUID senderObjectID) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorAsyncDelegate
      #pragma mark

      interaction IMessageMonitorAsyncDelegate
      {
        virtual void onHandleMessage(
                                     IMessageMonitorDelegatePtr delegate,
                                     MessagePtr message
                                     ) = 0;

        virtual void onAutoHandleFailureResult(MessageResultPtr result) = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark
      #pragma mark

      class MessageMonitor : public Noop,
                             public MessageQueueAssociator,
                             public SharedRecursiveLock,
                             public IMessageMonitor,
                             public IMessageMonitorAsyncDelegate,
                             public IMessageMonitorForMessageMonitorManager,
                             public ITimerDelegate
      {
      public:
        friend interaction IMessageMonitorFactory;
        friend interaction IMessageMonitor;

        ZS_DECLARE_TYPEDEF_PTR(IMessageMonitorManagerForMessageMonitor, UseMessageMonitorManager)
        ZS_DECLARE_TYPEDEF_PTR(ILocationForMessageMonitor, UseLocation)

      protected:
        MessageMonitor(
                       MessageMonitorManagerPtr manager,
                       IMessageQueuePtr queue
                       );
        
        MessageMonitor(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init(Duration timeout);

      public:
        ~MessageMonitor();

        static MessageMonitorPtr convert(IMessageMonitorPtr monitor);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitor => IMessageMonitor
        #pragma mark

        static ElementPtr toDebug(IMessageMonitorPtr monitor);

        static MessageMonitorPtr monitor(
                                         IMessageMonitorDelegatePtr delegate,
                                         message::MessagePtr requestMessage,
                                         Duration timeout
                                         );

        static MessageMonitorPtr monitorAndSendToLocation(
                                                          IMessageMonitorDelegatePtr delegate,
                                                          ILocationPtr peerLocation,
                                                          message::MessagePtr message,
                                                          Duration timeout
                                                          );

        static MessageMonitorPtr monitorAndSendToService(
                                                         IMessageMonitorDelegatePtr delegate,
                                                         IBootstrappedNetworkPtr bootstrappedNetwork,
                                                         const char *serviceType,
                                                         const char *serviceMethodName,
                                                         message::MessagePtr message,
                                                         Duration timeout
                                                         );

        static bool handleMessageReceived(message::MessagePtr message);

        virtual PUID getID() const {return mID;}

        virtual bool isComplete() const;
        virtual bool wasHandled() const;

        virtual void cancel();

        virtual String getMonitoredMessageID() const;
        virtual message::MessagePtr getMonitoredMessage() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitor => IMessageMonitorAsyncDelegate
        #pragma mark

        virtual void onHandleMessage(
                                     IMessageMonitorDelegatePtr delegate,
                                     message::MessagePtr message
                                     );

        virtual void onAutoHandleFailureResult(MessageResultPtr result);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitor => IMessageMonitorForMessageMonitorManager
        #pragma mark

        // (duplicate) virtual PUID getID() const;
        // (duplicate) virtual String getMonitoredMessageID();

        virtual bool handleMessage(message::MessagePtr message);

        virtual void notifySendMessageFailure(message::MessagePtr message);
        virtual void notifySenderObjectGone(PUID senderObjectID);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitor => ITimerDelegate
        #pragma mark

        virtual void onTimer(TimerPtr timer);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitor => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void notifyWithError(IHTTP::HTTPStatusCodes code);

      protected:
        AutoPUID mID;
        MessageMonitorWeakPtr mThisWeak;

        UseMessageMonitorManagerWeakPtr mManager;

        String mMessageID;

        IMessageMonitorDelegatePtr mDelegate;

        bool mWasHandled;
        bool mTimeoutFired;
        ULONG mPendingHandled;

        TimerPtr mTimer;
        Time mExpires;
        message::MessagePtr mOriginalMessage;

        SentViaObjectID mSentViaObjectID;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorFactory
      #pragma mark

      interaction IMessageMonitorFactory
      {
        static IMessageMonitorFactory &singleton();

        virtual MessageMonitorPtr monitor(
                                          IMessageMonitorDelegatePtr delegate,
                                          message::MessagePtr requestMessage,
                                          Duration timeout
                                          );

        virtual MessageMonitorPtr monitorAndSendToLocation(
                                                           IMessageMonitorDelegatePtr delegate,
                                                           ILocationPtr peerLocation,
                                                           message::MessagePtr message,
                                                           Duration timeout
                                                           );

        virtual MessageMonitorPtr monitorAndSendToService(
                                                          IMessageMonitorDelegatePtr delegate,
                                                          IBootstrappedNetworkPtr bootstrappedNetwork,
                                                          const char *serviceType,
                                                          const char *serviceMethodName,
                                                          message::MessagePtr message,
                                                          Duration timeout
                                                          );
      };

      class MessageMonitorFactory : public IFactory<IMessageMonitorFactory> {};
      
    }
  }
}

ZS_DECLARE_PROXY_BEGIN(openpeer::stack::internal::IMessageMonitorAsyncDelegate)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::IMessageMonitorDelegatePtr, IMessageMonitorDelegatePtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::message::MessagePtr, MessagePtr)
ZS_DECLARE_PROXY_TYPEDEF(openpeer::stack::message::MessageResultPtr, MessageResultPtr)
ZS_DECLARE_PROXY_METHOD_2(onHandleMessage, IMessageMonitorDelegatePtr, MessagePtr)
ZS_DECLARE_PROXY_METHOD_1(onAutoHandleFailureResult, MessageResultPtr)
ZS_DECLARE_PROXY_END()
