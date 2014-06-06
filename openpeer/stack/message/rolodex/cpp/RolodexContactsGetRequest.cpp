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

#include <openpeer/stack/message/rolodex/RolodexContactsGetRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define OPENPEER_STACK_MESSAGE_ROLODEX_CONTACTS_GET_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace rolodex
      {
        using zsLib::Seconds;
        using internal::MessageHelper;

        //---------------------------------------------------------------------
        RolodexContactsGetRequestPtr RolodexContactsGetRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<RolodexContactsGetRequest>(message);
        }

        //---------------------------------------------------------------------
        RolodexContactsGetRequest::RolodexContactsGetRequest()
        {
        }

        //---------------------------------------------------------------------
        RolodexContactsGetRequestPtr RolodexContactsGetRequest::create()
        {
          RolodexContactsGetRequestPtr ret(new RolodexContactsGetRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool RolodexContactsGetRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_RolodexInfo:      return mRolodexInfo.hasData();
            default:                             break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr RolodexContactsGetRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          String clientNonce = IHelper::randomString(32);

          RolodexInfo rolodexInfo;
          rolodexInfo.mServerToken = mRolodexInfo.mServerToken;
          rolodexInfo.mVersion = mRolodexInfo.mVersion;
          rolodexInfo.mRefreshFlag = mRolodexInfo.mRefreshFlag;

          rolodexInfo.mAccessToken = mRolodexInfo.mAccessToken;
          if (mRolodexInfo.mAccessSecret.hasData()) {
            rolodexInfo.mAccessSecretProofExpires = zsLib::now() + Seconds(OPENPEER_STACK_MESSAGE_ROLODEX_CONTACTS_GET_REQUEST_EXPIRES_TIME_IN_SECONDS);
            rolodexInfo.mAccessSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hash(mRolodexInfo.mAccessSecret), "rolodex-access-validate:" + clientNonce + ":" + IHelper::timeToString(rolodexInfo.mAccessSecretProofExpires) + ":" + rolodexInfo.mAccessToken + ":rolodex-contacts-get"));
          }

          rootEl->adoptAsLastChild(IMessageHelper::createElementWithText("nonce", clientNonce));
          if (rolodexInfo.hasData()) {
            rootEl->adoptAsLastChild(rolodexInfo.createElement());
          }

          return ret;
        }
      }
    }
  }
}
