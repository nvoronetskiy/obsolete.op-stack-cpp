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

#include <openpeer/stack/message/push-mailbox/FolderGetResult.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <zsLib/XML.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace push_mailbox
      {
        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

        using message::internal::MessageHelper;

        //---------------------------------------------------------------------
        FolderGetResultPtr FolderGetResult::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<FolderGetResult>(message);
        }

        //---------------------------------------------------------------------
        FolderGetResult::FolderGetResult()
        {
        }

        //---------------------------------------------------------------------
        FolderGetResultPtr FolderGetResult::create(
                                                   ElementPtr rootEl,
                                                   IMessageSourcePtr messageSource
                                                   )
        {
          FolderGetResultPtr ret(new FolderGetResult);

          IMessageHelper::fill(*ret, rootEl, messageSource);

          ElementPtr folderEl = rootEl->findFirstChildElement("foldder");

          ret->mFolderInfo = FolderInfo::create(folderEl);

          if (folderEl) {
            ElementPtr messagesEl = folderEl->findFirstChildElement("messages");

            if (messagesEl) {
              ElementPtr messageEl = messagesEl->findFirstChildElement("message");
              while (messageEl) {
                MessageInfo info = MessageInfo::create(messageEl);
                if (info.hasData()) {
                  ret->mMessages.push_back(info);
                }
                messageEl = messageEl->findFirstChildElement("message");
              }
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool FolderGetResult::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_FolderInfo:        return mFolderInfo.hasData();
            case AttributeType_Messages:          return (mMessages.size() > 0);
            default:                                            break;
          }
          return MessageResult::hasAttribute((MessageResult::AttributeTypes)type);
        }

      }
    }
  }
}
