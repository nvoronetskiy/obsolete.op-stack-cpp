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

#include <openpeer/stack/message/internal/stack_message_MessageFactoryManager.h>
#include <openpeer/stack/message/IMessageFactory.h>
#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/MessageResult.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>
#include <zsLib/Log.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      typedef stack::internal::IStackForInternal UseStack;

      using services::IHelper;

      using stack::internal::IStackForInternal;
      using stack::internal::Helper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      static Log::Params slog(const char *message)
      {
        return Log::Params(message, "stack::message::Message");
      }

      //-----------------------------------------------------------------------
      const char *Message::toString(MessageTypes type)
      {
        switch (type)
        {
          case MessageType_Invalid:     return "";
          case MessageType_Request:     return "request";
          case MessageType_Result:      return "result";
          case MessageType_Notify:      return "notify";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      Message::MessageTypes Message::toMessageType(const char *inType)
      {
        typedef zsLib::ULONG ULONG;
        String type(inType ? inType : "");

        for (ULONG loop = (ULONG)(Message::MessageType_Invalid+1); loop <= ((ULONG)Message::MessageType_Last); ++loop)
        {
          if (type == toString((Message::MessageTypes)loop)) {
            return (Message::MessageTypes)loop;
          }
        }
        return Message::MessageType_Invalid;
      }

      //-----------------------------------------------------------------------
      Message::Message() :
        mAppID(services::ISettings::getString(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID)),
        mID(IHelper::randomString(32)),
        mTime(zsLib::now())
      {
      }

      //-----------------------------------------------------------------------
      ElementPtr Message::toDebug(MessagePtr message)
      {
        if (!message) return ElementPtr();

        const char *messageTypeStr = Message::toString(message->messageType());
        const char *methodStr = message->methodAsString();
        String domain = message->domain();
        String appID = message->appID();
        String messageID = message->messageID();
        Time time = message->time();

        String handler;
        IMessageFactoryPtr factory = message->factory();
        if (factory) {
          const char *handlerStr = factory->getHandler();
          handler = (handlerStr ? String(handlerStr) : String());
        }

        String messageType(messageTypeStr ? String(messageTypeStr) : String());
        String method(methodStr ? String(methodStr) : String());

        MessageResult::ErrorCodeType errorCode = 0;
        String errorReason;

        if (message->isResult()) {
          MessageResultPtr result = MessageResult::convert(message);
          errorCode = result->errorCode();
          errorReason = result->errorReason();
        }

        ElementPtr resultEl = Element::create("message::Message");

        IHelper::debugAppend(resultEl, "message type", messageType);
        IHelper::debugAppend(resultEl, "handler", handler);
        IHelper::debugAppend(resultEl, "method", method);
        IHelper::debugAppend(resultEl, "domain", domain);
        IHelper::debugAppend(resultEl, "appid", appID);
        IHelper::debugAppend(resultEl, "id", messageID);
        IHelper::debugAppend(resultEl, "time", time);
        IHelper::debugAppend(resultEl, "error code", errorCode);
        IHelper::debugAppend(resultEl, "error reason", errorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      MessagePtr Message::create(
                                 DocumentPtr document,
                                 IMessageSourcePtr messageSource
                                 )
      {
        MessagePtr result;

        try
        {
          ElementPtr root = document->getFirstChildElementChecked();
          return Message::create(root, messageSource);
        } catch(CheckFailed &) {
          ZS_LOG_ERROR(Detail, slog("expected element is missing"))
        }

        return result;
      }

      //-----------------------------------------------------------------------
      MessagePtr Message::create(
                                 ElementPtr root,
                                 IMessageSourcePtr messageSource
                                 )
      {
        return internal::MessageFactoryManager::create(root, messageSource);
      }

      //-----------------------------------------------------------------------
      const char *Message::methodAsString() const
      {
        IMessageFactoryPtr originalFactory = factory();
        if (!originalFactory) {
          return "UNDEFINED";
        }

        return originalFactory->toString(method());
      }

      //-----------------------------------------------------------------------
      DocumentPtr Message::encode()
      {
        ZS_THROW_NOT_IMPLEMENTED("The encoder for this method is not implemented")
        return DocumentPtr();
      }
    }
  }
}
