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

#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_Helper.h>
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
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageIncomingForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr IMessageIncomingForAccount::toDebug(ForAccountPtr messageIncoming)
      {
        return MessageIncoming::toDebug(MessageIncoming::convert(messageIncoming));
      }

      //-----------------------------------------------------------------------
      IMessageIncomingForAccount::ForAccountPtr IMessageIncomingForAccount::create(
                                                                                   AccountPtr account,
                                                                                   LocationPtr location,
                                                                                   message::MessagePtr message
                                                                                   )
      {
        return IMessageIncomingFactory::singleton().create(account, location, message);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageIncoming
      #pragma mark

      //-----------------------------------------------------------------------
      MessageIncoming::MessageIncoming(
                                       AccountPtr account,
                                       LocationPtr location,
                                       message::MessagePtr message
                                       ) :
        SharedRecursiveLock(*account),
        mAccount(account),
        mLocation(location),
        mMessage(message),
        mResponseSent(false)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      void MessageIncoming::init()
      {
      }

      //-----------------------------------------------------------------------
      MessageIncoming::~MessageIncoming()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))

        if (mResponseSent) {
          ZS_LOG_DEBUG(debug("response already sent"))
          return;
        }

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("automatic failure response cannot be sent as account gone"))
          return;
        }

        account->notifyMessageIncomingResponseNotSent(*this);
      }

      //-----------------------------------------------------------------------
      MessageIncomingPtr MessageIncoming::convert(IMessageIncomingPtr messageIncoming)
      {
        return dynamic_pointer_cast<MessageIncoming>(messageIncoming);
      }

      //-----------------------------------------------------------------------
      MessageIncomingPtr MessageIncoming::convert(ForAccountPtr messageIncoming)
      {
        return dynamic_pointer_cast<MessageIncoming>(messageIncoming);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageIncoming => IMessageIncoming
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr MessageIncoming::toDebug(IMessageIncomingPtr messageIncoming)
      {
        if (!messageIncoming) return ElementPtr();

        MessageIncomingPtr pThis = MessageIncoming::convert(messageIncoming);
        return pThis->toDebug();
      }

      //-----------------------------------------------------------------------
      ILocationPtr MessageIncoming::getLocation() const
      {
        return mLocation;
      }

      //-----------------------------------------------------------------------
      message::MessagePtr MessageIncoming::getMessage() const
      {
        return mMessage;
      }

      //-----------------------------------------------------------------------
      bool MessageIncoming::sendResponse(message::MessagePtr message)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(*this);

        UseAccountPtr account = mAccount.lock();
        if (!account) {
          ZS_LOG_WARNING(Detail, debug("failed to send incoming message response as account is gone"))
          return false;
        }
        bool sent = account->send(mLocation, message);
        mResponseSent = mResponseSent ||  sent;
        return sent;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageIncoming => IMessageIncomingForAccount
      #pragma mark

      //-----------------------------------------------------------------------
      MessageIncoming::ForAccountPtr MessageIncoming::create(
                                                             AccountPtr account,
                                                             LocationPtr location,
                                                             message::MessagePtr message
                                                             )
      {
        MessageIncomingPtr pThis(new MessageIncoming(account, location, message));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      LocationPtr MessageIncoming::getLocation(bool) const
      {
        return mLocation;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageIncoming => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params MessageIncoming::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("MessageIncoming");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params MessageIncoming::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr MessageIncoming::toDebug() const
      {
        AutoRecursiveLock lock(*this);
        ElementPtr resultEl = Element::create("MessageIncoming");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, ILocation::toDebug(mLocation));
        IHelper::debugAppend(resultEl, Message::toDebug(mMessage));

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageIncomingFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageIncomingFactory &IMessageIncomingFactory::singleton()
      {
        return MessageIncomingFactory::singleton();
      }

      //-----------------------------------------------------------------------
      IMessageIncomingForAccount::ForAccountPtr IMessageIncomingFactory::create(
                                                                                AccountPtr account,
                                                                                LocationPtr location,
                                                                                message::MessagePtr message
                                                                                )
      {
        if (this) {}
        return MessageIncoming::create(account, location, message);
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    ElementPtr IMessageIncoming::toDebug(IMessageIncomingPtr messageIncoming)
    {
      return internal::MessageIncoming::toDebug(messageIncoming);
    }

  }
}
