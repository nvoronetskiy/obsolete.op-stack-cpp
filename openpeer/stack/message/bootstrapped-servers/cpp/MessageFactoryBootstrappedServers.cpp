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

#include <openpeer/stack/message/bootstrapped-servers/MessageFactoryBootstrappedServers.h>
#include <openpeer/stack/message/bootstrapped-servers/ServersGetResult.h>
#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageFactoryManager.h>

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/IHelper.h>

#define OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_BOOTSTRAPPED_SERVER_HANDLER "bootstrapped-servers"


namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace bootstrapped_servers
      {
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryBootstrappedServers
        #pragma mark

        //---------------------------------------------------------------------
        MessageFactoryBootstrappedServersPtr MessageFactoryBootstrappedServers::create()
        {
          MessageFactoryBootstrappedServersPtr pThis(new MessageFactoryBootstrappedServers);
          IMessageFactoryManager::registerFactory(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        MessageFactoryBootstrappedServersPtr MessageFactoryBootstrappedServers::singleton()
        {
          static SingletonLazySharedPtr<MessageFactoryBootstrappedServers> singleton(create());
          return singleton.singleton();
        }

        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        //-----------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryPeerSalt => IMessageFactory
        #pragma mark

        //---------------------------------------------------------------------
        const char *MessageFactoryBootstrappedServers::getHandler() const
        {
          return OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_BOOTSTRAPPED_SERVER_HANDLER;
        }

        //---------------------------------------------------------------------
        Message::Methods MessageFactoryBootstrappedServers::toMethod(const char *inMethod) const
        {
          typedef zsLib::ULONG ULONG;
          String methodStr(inMethod ? inMethod : "");

          for (ULONG loop = (ULONG)(MessageFactoryBootstrappedServers::Method_Invalid+1); loop <= ((ULONG)MessageFactoryBootstrappedServers::Method_Last); ++loop)
          {
            if (methodStr == toString((Message::Methods)loop)) {
              return (Message::Methods)loop;
            }
          }
          return Message::Method_Invalid;
        }

        //---------------------------------------------------------------------
        const char *MessageFactoryBootstrappedServers::toString(Message::Methods method) const
        {
          switch ((MessageFactoryBootstrappedServers::Methods)method)
          {
            case Method_Invalid:            return "";

            case Method_FindersGet:         return "finders-get";
          }
          return "";
        }

        //---------------------------------------------------------------------
        MessagePtr MessageFactoryBootstrappedServers::create(
                                                            ElementPtr root,
                                                            IMessageSourcePtr messageSource
                                                            )
        {
          if (!root) return MessagePtr();

          Message::MessageTypes msgType = IMessageHelper::getMessageType(root);
          Methods msgMethod = (MessageFactoryBootstrappedServers::Methods)toMethod(IMessageHelper::getAttribute(root, "method"));

          switch (msgType) {
            case Message::MessageType_Invalid:                return MessagePtr();

            case Message::MessageType_Request:                return MessagePtr();
            case Message::MessageType_Result:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_FindersGet:                       return ServersGetResult::create(root, messageSource);
              }
              break;
            }
            case Message::MessageType_Notify:                 return MessagePtr();
          }

          return MessagePtr();
        }

      }
    }
  }
}
