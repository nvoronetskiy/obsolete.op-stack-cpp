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

#include <openpeer/stack/message/identity/IdentityAccessCompleteNotify.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace identity
      {
        using internal::MessageHelper;

        //---------------------------------------------------------------------
        IdentityAccessCompleteNotifyPtr IdentityAccessCompleteNotify::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(IdentityAccessCompleteNotify, message);
        }

        //---------------------------------------------------------------------
        IdentityAccessCompleteNotify::IdentityAccessCompleteNotify()
        {
        }

        //---------------------------------------------------------------------
        IdentityAccessCompleteNotifyPtr IdentityAccessCompleteNotify::create(
                                                                             ElementPtr rootEl,
                                                                             IMessageSourcePtr messageSource
                                                                             )
        {
          IdentityAccessCompleteNotifyPtr ret(new IdentityAccessCompleteNotify);
          IMessageHelper::fill(*ret, rootEl, messageSource);

          ret->mIdentityInfo = IdentityInfo::create(rootEl->findFirstChildElement("identity"));
          ret->mLockboxInfo = LockboxInfo::create(rootEl->findFirstChildElement("lockbox"));
          ret->mNamespaceGrantChallengeInfo = NamespaceGrantChallengeInfo::create(rootEl->findFirstChildElement("namespaceGrantChallenge"));

          ret->mEncryptedUserSpecificPassphrase = IMessageHelper::getElementTextAndDecode(rootEl->findFirstChildElement("encryptedUserSpecificKey"));

          ret->mEncryptionKeyUponGrantProofHash = IMessageHelper::getElementTextAndDecode(rootEl->findFirstChildElement("encryptionKeyUponGrantProofHash"));
          ret->mEncryptionKeyUponGrantProof = IMessageHelper::getElementTextAndDecode(rootEl->findFirstChildElement("encryptionKeyUponGrantProof"));

          return ret;
        }

        //---------------------------------------------------------------------
        bool IdentityAccessCompleteNotify::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_IdentityInfo:                    return (mIdentityInfo.hasData());
            case AttributeType_LockboxInfo:                     return (mLockboxInfo.hasData());
            case AttributeType_NamespaceGrantChallengeInfo:     return (mNamespaceGrantChallengeInfo.hasData());
            case AttributeType_EncryptedUserSpecificPassphrase: return (mEncryptedUserSpecificPassphrase.hasData());
            case AttributeType_EncryptionKeyUponGrantProofHash: return (mEncryptionKeyUponGrantProofHash.hasData());
            case AttributeType_EncryptionKeyUponGrantProof:     return (mEncryptionKeyUponGrantProof.hasData());
            default:                                      break;
          }
          return false;
        }

      }
    }
  }
}
