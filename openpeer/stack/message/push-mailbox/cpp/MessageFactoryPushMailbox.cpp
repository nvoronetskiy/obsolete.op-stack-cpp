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

#include <openpeer/stack/message/push-mailbox/MessageFactoryPushMailbox.h>
#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageFactoryManager.h>

#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/message/push-mailbox/AccessResult.h>
#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateResult.h>
#include <openpeer/stack/message/push-mailbox/PeerValidateResult.h>
#include <openpeer/stack/message/push-mailbox/FoldersGetResult.h>
#include <openpeer/stack/message/push-mailbox/FolderUpdateResult.h>
#include <openpeer/stack/message/push-mailbox/FolderGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessagesDataGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessagesMetaDataGetResult.h>
#include <openpeer/stack/message/push-mailbox/MessageUpdateResult.h>
#include <openpeer/stack/message/push-mailbox/ListFetchResult.h>
#include <openpeer/stack/message/push-mailbox/ChangedNotify.h>
#include <openpeer/stack/message/push-mailbox/RegisterPushResult.h>

#include <openpeer/stack/IHelper.h>

#include <zsLib/Log.h>

#define OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_PUSH_MAILBOX_HANDLER "push-mailbox"

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace push_mailbox
      {

        typedef zsLib::String String;

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryPushMailbox
        #pragma mark

        //---------------------------------------------------------------------
        MessageFactoryPushMailboxPtr MessageFactoryPushMailbox::create()
        {
          MessageFactoryPushMailboxPtr pThis(new MessageFactoryPushMailbox);
          IMessageFactoryManager::registerFactory(pThis);
          return pThis;
        }

        //---------------------------------------------------------------------
        MessageFactoryPushMailboxPtr MessageFactoryPushMailbox::singleton()
        {
          static SingletonLazySharedPtr<MessageFactoryPushMailbox> singleton(create());
          return singleton.singleton();
        }

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageFactoryPushMailbox => IMessageFactory
        #pragma mark

        //---------------------------------------------------------------------
        const char *MessageFactoryPushMailbox::getHandler() const
        {
          return OPENPEER_STACK_MESSAGE_MESSAGE_FACTORY_PUSH_MAILBOX_HANDLER;
        }

        //---------------------------------------------------------------------
        Message::Methods MessageFactoryPushMailbox::toMethod(const char *inMethod) const
        {
          typedef zsLib::ULONG ULONG;
          String methodStr(inMethod ? inMethod : "");

          for (ULONG loop = (ULONG)(MessageFactoryPushMailbox::Method_Invalid+1); loop <= ((ULONG)MessageFactoryPushMailbox::Method_Last); ++loop)
          {
            if (methodStr == toString((Message::Methods)loop)) {
              return (Message::Methods)loop;
            }
          }
          return Message::Method_Invalid;
        }

        //---------------------------------------------------------------------
        const char *MessageFactoryPushMailbox::toString(Message::Methods method) const
        {
          switch ((MessageFactoryPushMailbox::Methods)method)
          {
            case Method_Invalid:                          return "";

            case Method_Access:                           return "access";
            case Method_NamespaceGrantChallengeValidate:  return "namespace-grant-challenge-validate";
            case Method_PeerValidate:                     return "peer-validate";
            case Method_FoldersGet:                       return "folders-get";
            case Method_FolderUpdate:                     return "folder-update";
            case Method_FolderGet:                        return "folder-get";
            case Method_MessagesDataGet:                  return "messages-data-get";
            case Method_MessagesMetaDataGet:              return "messages-metadata-get";
            case Method_MessageUpdate:                    return "message-update";
            case Method_ListFetch:                        return "list-fetch";
            case Method_Changed:                          return "changed";
            case Method_RegisterPush:                     return "register-push";
          }
          return "";
        }

        //---------------------------------------------------------------------
        MessagePtr MessageFactoryPushMailbox::create(
                                                    ElementPtr rootEl,
                                                    IMessageSourcePtr messageSource
                                                    )
        {
          if (!rootEl) return MessagePtr();

          Message::MessageTypes msgType = IMessageHelper::getMessageType(rootEl);
          Methods msgMethod = (MessageFactoryPushMailbox::Methods)toMethod(IMessageHelper::getAttribute(rootEl, "method"));

          switch (msgType) {
            case Message::MessageType_Invalid:                return MessagePtr();

            case Message::MessageType_Request:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_Access:                           return MessagePtr();
                case Method_NamespaceGrantChallengeValidate:  return MessagePtr();
                case Method_PeerValidate:                     return MessagePtr();
                case Method_FoldersGet:                       return MessagePtr();
                case Method_FolderUpdate:                     return MessagePtr();
                case Method_FolderGet:                        return MessagePtr();
                case Method_MessagesDataGet:                  return MessagePtr();
                case Method_MessagesMetaDataGet:              return MessagePtr();
                case Method_MessageUpdate:                    return MessagePtr();
                case Method_ListFetch:                        return MessagePtr();
                case Method_Changed:                          return MessagePtr();
                case Method_RegisterPush:                     return MessagePtr();
              }
              break;
            }
            case Message::MessageType_Result:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_Access:                           return AccessResult::create(rootEl, messageSource);
                case Method_NamespaceGrantChallengeValidate:  return NamespaceGrantChallengeValidateResult::create(rootEl, messageSource);
                case Method_PeerValidate:                     return PeerValidateResult::create(rootEl, messageSource);
                case Method_FoldersGet:                       return FoldersGetResult::create(rootEl, messageSource);
                case Method_FolderUpdate:                     return FolderUpdateResult::create(rootEl, messageSource);
                case Method_FolderGet:                        return FolderGetResult::create(rootEl, messageSource);
                case Method_MessagesDataGet:                  return MessagesDataGetResult::create(rootEl, messageSource);
                case Method_MessagesMetaDataGet:              return MessagesMetaDataGetResult::create(rootEl, messageSource);
                case Method_MessageUpdate:                    return MessageUpdateResult::create(rootEl, messageSource);
                case Method_ListFetch:                        return ListFetchResult::create(rootEl, messageSource);
                case Method_Changed:                          return MessagePtr();
                case Method_RegisterPush:                     return RegisterPushResult::create(rootEl, messageSource);
              }
              break;
            }
            case Message::MessageType_Notify:
            {
              switch (msgMethod) {
                case Method_Invalid:                          return MessagePtr();

                case Method_Access:                           return MessagePtr();
                case Method_NamespaceGrantChallengeValidate:  return MessagePtr();
                case Method_PeerValidate:                     return MessagePtr();
                case Method_FoldersGet:                       return MessagePtr();
                case Method_FolderUpdate:                     return MessagePtr();
                case Method_FolderGet:                        return MessagePtr();
                case Method_MessagesDataGet:                  return MessagePtr();
                case Method_MessagesMetaDataGet:              return MessagePtr();
                case Method_MessageUpdate:                    return MessagePtr();
                case Method_ListFetch:                        return MessagePtr();
                case Method_Changed:                          return ChangedNotify::create(rootEl, messageSource);
                case Method_RegisterPush:                     return MessagePtr();
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
