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

#include <openpeer/stack/message/identity/IdentityAccessNamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

//#include <zsLib/Stringize.h>
//#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_STACK_MESSAGE_IDENTITY_ACCESS_NAMESPACE_GRANT_CHALLENGE_VALIDATE_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace identity
      {
        using internal::MessageHelper;

        typedef stack::internal::IStackForInternal UseStack;

        using zsLib::Seconds;

        //---------------------------------------------------------------------
        IdentityAccessNamespaceGrantChallengeValidateRequestPtr IdentityAccessNamespaceGrantChallengeValidateRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<IdentityAccessNamespaceGrantChallengeValidateRequest>(message);
        }

        //---------------------------------------------------------------------
        IdentityAccessNamespaceGrantChallengeValidateRequest::IdentityAccessNamespaceGrantChallengeValidateRequest()
        {
        }

        //---------------------------------------------------------------------
        IdentityAccessNamespaceGrantChallengeValidateRequestPtr IdentityAccessNamespaceGrantChallengeValidateRequest::create()
        {
          IdentityAccessNamespaceGrantChallengeValidateRequestPtr ret(new IdentityAccessNamespaceGrantChallengeValidateRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool IdentityAccessNamespaceGrantChallengeValidateRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_IdentityInfo:                  return mIdentityInfo.hasData();
            case AttributeType_NamespaceGrantChallengeBundle: return (bool)mNamespaceGrantChallengeBundle;
            default:                                          break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr IdentityAccessNamespaceGrantChallengeValidateRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          IdentityInfo identityInfo;

          identityInfo.mToken = mIdentityInfo.mToken.createProof("identity-access-namespace-grant-challenge-validate", Seconds(OPENPEER_STACK_MESSAGE_IDENTITY_ACCESS_NAMESPACE_GRANT_CHALLENGE_VALIDATE_EXPIRES_TIME_IN_SECONDS));

          if (identityInfo.hasData()) {
            rootEl->adoptAsLastChild(identityInfo.createElement());
          }

          if (mNamespaceGrantChallengeBundle) {
            rootEl->adoptAsLastChild(mNamespaceGrantChallengeBundle->clone()->toElement());
          }

          return ret;
        }


      }
    }
  }
}
