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

#include <openpeer/stack/IMessageIncoming.h>
#include <openpeer/stack/internal/types.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForMessageIncoming;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageIncomingForAccount
      #pragma mark

      interaction IMessageIncomingForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IMessageIncomingForAccount, ForAccount)

        static ElementPtr toDebug(ForAccountPtr messageIncoming);

        static ForAccountPtr create(
                                    AccountPtr account,
                                    LocationPtr location,
                                    message::MessagePtr message
                                    );

        virtual LocationPtr getLocation(bool internal = true) const = 0;
        virtual message::MessagePtr getMessage() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageIncoming
      #pragma mark

      class MessageIncoming : public Noop,
                              public SharedRecursiveLock,
                              public IMessageIncoming,
                              public IMessageIncomingForAccount
      {
      public:
        friend interaction IMessageIncomingFactory;
        friend interaction IMessageIncoming;
        friend interaction IMessageIncomingForAccount;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForMessageIncoming, UseAccount)

      protected:
        MessageIncoming(
                        AccountPtr account,
                        LocationPtr location,
                        message::MessagePtr message
                        );
        
        MessageIncoming(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~MessageIncoming();

        static MessageIncomingPtr convert(IMessageIncomingPtr peer);
        static MessageIncomingPtr convert(ForAccountPtr peer);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageIncoming => IMessageIncoming
        #pragma mark

        static ElementPtr toDebug(IMessageIncomingPtr messageIncoming);

        virtual PUID getID() const {return mID;}

        virtual ILocationPtr getLocation() const;
        virtual message::MessagePtr getMessage() const;

        virtual PromisePtr sendResponse(message::MessagePtr message);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageIncoming => IMessageIncomingForAccount
        #pragma mark

        // (duplicate) virtual ILocationPtr getLocation() const;
        // (duplicate) virtual message::MessagePtr getMessage() const;

        static ForAccountPtr create(
                                    AccountPtr account,
                                    LocationPtr location,
                                    message::MessagePtr message
                                    );

        virtual LocationPtr getLocation(bool) const;

        // (duplicate) virtual message::MessagePtr getMessage() const;

        // (duplicate) virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageIncoming => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageIncoming => (data)
        #pragma mark

        AutoPUID mID;
        MessageIncomingWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;
        LocationPtr mLocation;
        message::MessagePtr mMessage;

        bool mResponseSent;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageIncomingFactory
      #pragma mark

      interaction IMessageIncomingFactory
      {
        static IMessageIncomingFactory &singleton();

        virtual IMessageIncomingForAccount::ForAccountPtr create(
                                                                 AccountPtr account,
                                                                 LocationPtr location,
                                                                 message::MessagePtr message
                                                                 );
      };

      class MessageIncomingFactory : public IFactory<IMessageIncomingFactory> {};

    }
  }
}
