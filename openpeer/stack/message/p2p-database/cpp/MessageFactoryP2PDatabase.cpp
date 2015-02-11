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

#include <openpeer/stack/message/p2p-database/MessageFactoryP2PDatabase.h>
#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageFactoryManager.h>

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/message/p2p-database/ListSubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeResult.h>
#include <openpeer/stack/message/p2p-database/ListSubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/SubscribeRequest.h>
#include <openpeer/stack/message/p2p-database/SubscribeResult.h>
#include <openpeer/stack/message/p2p-database/SubscribeNotify.h>
#include <openpeer/stack/message/p2p-database/DataGetRequest.h>
#include <openpeer/stack/message/p2p-database/DataGetResult.h>

#include <openpeer/stack/IHelper.h>

#define OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_PEER_COMMON_HANDLER "p2p-database"

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace p2p_database
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryP2PDatabase
        #pragma mark

        //---------------------------------------------------------------------
        MessageFactoryP2PDatabasePtr MessageFactoryP2PDatabase::create()
        {
          MessageFactoryP2PDatabasePtr pThis(new MessageFactoryP2PDatabase);
          IMessageFactoryManager::registerFactory(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        MessageFactoryP2PDatabasePtr MessageFactoryP2PDatabase::singleton()
        {
          static SingletonLazySharedPtr<MessageFactoryP2PDatabase> singleton(create());
          return singleton.singleton();
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryP2PDatabase => IMessageFactory
        #pragma mark

        //---------------------------------------------------------------------
        const char *MessageFactoryP2PDatabase::getHandler() const
        {
          return OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_PEER_COMMON_HANDLER;
        }

        //---------------------------------------------------------------------
        Message::Methods MessageFactoryP2PDatabase::toMethod(const char *inMethod) const
        {
          typedef zsLib::ULONG ULONG;
          String methodStr(inMethod ? inMethod : "");

          for (ULONG loop = (ULONG)(MessageFactoryP2PDatabase::Method_Invalid+1); loop <= ((ULONG)MessageFactoryP2PDatabase::Method_Last); ++loop)
          {
            if (methodStr == toString((Message::Methods)loop)) {
              return (Message::Methods)loop;
            }
          }
          return Message::Method_Invalid;
        }

        //---------------------------------------------------------------------
        const char *MessageFactoryP2PDatabase::toString(Message::Methods method) const
        {
          switch ((MessageFactoryP2PDatabase::Methods)method)
          {
            case Method_Invalid:                        return "";

            case Method_ListSubscribe:                  return "list-subscribe";
            case Method_Subscribe:                      return "subscribe";
            case Method_DataGet:                        return "data-get";
          }
          return "";
        }

        //---------------------------------------------------------------------
        MessagePtr MessageFactoryP2PDatabase::create(
                                                    ElementPtr rootEl,
                                                    IMessageSourcePtr messageSource
                                                    )
        {
          if (!rootEl) return MessagePtr();

          Message::MessageTypes msgType = IMessageHelper::getMessageType(rootEl);
          Methods msgMethod = (MessageFactoryP2PDatabase::Methods)toMethod(IMessageHelper::getAttribute(rootEl, "method"));

          switch (msgType) {
            case Message::MessageType_Invalid:                return MessagePtr();

            case Message::MessageType_Request:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_ListSubscribe:                    return ListSubscribeRequest::create(rootEl, messageSource);
                case Method_Subscribe:                        return SubscribeRequest::create(rootEl, messageSource);
                case Method_DataGet:                          return DataGetRequest::create(rootEl, messageSource);
              }
              break;
            }
            case Message::MessageType_Result:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_ListSubscribe:                    return ListSubscribeResult::create(rootEl, messageSource);
                case Method_Subscribe:                        return SubscribeResult::create(rootEl, messageSource);
                case Method_DataGet:                          return DataGetResult::create(rootEl, messageSource);
              }
              break;
            }
            case Message::MessageType_Notify:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_ListSubscribe:                    return ListSubscribeNotify::create(rootEl, messageSource);
                case Method_Subscribe:                        return SubscribeNotify::create(rootEl, messageSource);
                case Method_DataGet:                          return MessagePtr();
              }
              break;
            }
          }

          return MessagePtr();
        }
      }
    }
  }
}
