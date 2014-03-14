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
#include <openpeer/stack/IStack.h>

#define OPENPEER_STACK_SETTING_STACK_STACK_THREAD_PRIORITY "openpeer/stack/stack-thread-priority"
#define OPENPEER_STACK_SETTING_STACK_KEY_GENERATION_THREAD_PRIORITY "openpeer/stack/key-generation-thread-priority"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IStackForInternal
      #pragma mark

      interaction IStackForInternal
      {
        static AgentInfo agentInfo();

        static IMessageQueuePtr queueDelegate();
        static IMessageQueuePtr queueStack();
        static IMessageQueuePtr queueServices();
        static IMessageQueuePtr queueKeyGeneration();
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Stack
      #pragma mark

      class Stack : public IStack,
                    public IStackForInternal
      {
      public:
        friend interaction IStack;
        friend interaction IStackForInternal;

      protected:
        Stack();

      public:
        ~Stack();

        static StackPtr create();
        static StackPtr convert(IStackPtr stack);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStack
        #pragma mark

        static StackPtr singleton();

        virtual void setup(
                           IMessageQueuePtr defaultDelegateMessageQueue,
                           IMessageQueuePtr stackMessageQueue,
                           IMessageQueuePtr servicesMessageQueue,
                           IMessageQueuePtr keyGenerationQueue
                           );

        virtual PUID getID() const {return mID;}

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => IStackForInternal
        #pragma mark

        virtual void getAgentInfo(AgentInfo &result) const;

        virtual IMessageQueuePtr queueDelegate() const;
        virtual IMessageQueuePtr queueStack();
        virtual IMessageQueuePtr queueServices();
        virtual IMessageQueuePtr queueKeyGeneration();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);
        void verifySettingIsSet(const char *settingName);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Stack => (data)
        #pragma mark

        PUID mID;
        StackWeakPtr mThisWeak;
        mutable RecursiveLock mLock;

        IMessageQueuePtr mDelegateQueue;
        IMessageQueuePtr mStackQueue;
        IMessageQueuePtr mServicesQueue;
        IMessageQueuePtr mKeyGenerationQueue;
      };
    }
  }
}
