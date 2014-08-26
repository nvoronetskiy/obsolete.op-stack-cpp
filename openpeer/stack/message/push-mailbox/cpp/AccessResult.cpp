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

#include <openpeer/stack/message/push-mailbox/AccessResult.h>
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
        AccessResultPtr AccessResult::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(AccessResult, message);
        }

        //---------------------------------------------------------------------
        AccessResult::AccessResult()
        {
        }

        //---------------------------------------------------------------------
        AccessResultPtr AccessResult::create(
                                             ElementPtr rootEl,
                                             IMessageSourcePtr messageSource
                                             )
        {
          AccessResultPtr ret(new AccessResult);

          IMessageHelper::fill(*ret, rootEl, messageSource);

          ret->mNamespaceGrantChallengeInfo = NamespaceGrantChallengeInfo::create(rootEl->findFirstChildElement("namespaceGrantChallenge"));

          ret->mPeerURI = IMessageHelper::getElementTextAndDecode(rootEl->findFirstChildElement("peer"));

          ElementPtr uploadMessageEl = rootEl->findFirstChildElement("transferMessage");
          if (uploadMessageEl) {
            ret->mUploadMessageURL = IMessageHelper::getElementTextAndDecode(uploadMessageEl->findFirstChildElement("upload"));
            ret->mDownloadMessageURL = IMessageHelper::getElementTextAndDecode(uploadMessageEl->findFirstChildElement("download"));
            ElementPtr stringReplacementEl = uploadMessageEl->findFirstChildElement("stringReplacements");
            if (stringReplacementEl) {
              ret->mStringReplacementMessageID = IMessageHelper::getElementTextAndDecode(stringReplacementEl->findFirstChildElement("id"));
              ret->mStringReplacementMessageSize = IMessageHelper::getElementTextAndDecode(stringReplacementEl->findFirstChildElement("sizeRemaining"));
            }
          }

          return ret;
        }

        //---------------------------------------------------------------------
        bool AccessResult::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_NamespaceGrantChallengeInfo:               return mNamespaceGrantChallengeInfo.hasData();
            case AttributeType_PeerURI:                                   return mPeerURI.hasData();
            case AttributeType_UploadMessageURL:                          return mUploadMessageURL.hasData();
            case AttributeType_DownloadMessageURL:                        return mDownloadMessageURL.hasData();
            case AttributeType_StringReplacementMessageID:                return mStringReplacementMessageID.hasData();
            case AttributeType_StringReplacementMessageSize:              return mStringReplacementMessageSize.hasData();
            default:                                                      break;
          }
          return MessageResult::hasAttribute((MessageResult::AttributeTypes)type);
        }

      }
    }
  }
}
