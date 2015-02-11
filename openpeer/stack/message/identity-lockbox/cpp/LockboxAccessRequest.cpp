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

#include <openpeer/stack/message/identity-lockbox/LockboxAccessRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define OPENPEER_STACK_MESSAGE_LOCKBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace identity_lockbox
      {
        typedef stack::internal::IStackForInternal UseStack;

        using zsLib::Seconds;
        using internal::MessageHelper;
        using stack::internal::IStackForInternal;

        //---------------------------------------------------------------------
        LockboxAccessRequestPtr LockboxAccessRequest::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(LockboxAccessRequest, message);
        }

        //---------------------------------------------------------------------
        LockboxAccessRequest::LockboxAccessRequest()
        {
        }

        //---------------------------------------------------------------------
        LockboxAccessRequestPtr LockboxAccessRequest::create()
        {
          LockboxAccessRequestPtr ret(new LockboxAccessRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool LockboxAccessRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_IdentityInfo:      return mIdentityInfo.hasData();
            case AttributeType_LockboxInfo:       return mLockboxInfo.hasData();
            case AttributeType_AgentInfo:         return mAgentInfo.hasData();
            case AttributeType_GrantID:           return mGrantID.hasData();
            case AttributeType_NamespaceInfos:    return (mNamespaceInfos.size() > 0);
            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr LockboxAccessRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          IdentityInfo identityInfo;

          identityInfo.mURI = mIdentityInfo.mURI;
          identityInfo.mProvider = mIdentityInfo.mProvider;

          identityInfo.mToken = mIdentityInfo.mToken.createProof("identity-lockbox-access", Seconds(OPENPEER_STACK_MESSAGE_LOCKBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS));

          LockboxInfo lockboxInfo;
          lockboxInfo.mDomain = mLockboxInfo.mDomain;
          lockboxInfo.mAccountID = mLockboxInfo.mAccountID;
          lockboxInfo.mKeyName = mLockboxInfo.mKeyName;
          lockboxInfo.mKey = mLockboxInfo.mKey;
          lockboxInfo.mResetFlag = mLockboxInfo.mResetFlag;

          if (identityInfo.hasData()) {
            rootEl->adoptAsLastChild(identityInfo.createElement());
          }

          if ((lockboxInfo.mKey) &&
              (lockboxInfo.mKeyName.hasData())) {
            // `<lockbox-hash>` = hex(hmac(`<lockbox-passphrase>`, "lockbox:" + `<lockbox-passphrase-id>`))
            String hash = IHelper::convertToHex(*IHelper::hmac(*lockboxInfo.mKey, "lockbox:" + lockboxInfo.mKeyName));
            if (identityInfo.hasData()) {
              lockboxInfo.mKeyHash = hash;
            } else {
              Token lockboxHashToken;

              lockboxHashToken.mID = lockboxInfo.mKeyName;
              lockboxHashToken.mSecret = hash;
              lockboxHashToken.mExpires = zsLib::now() + Seconds(OPENPEER_STACK_MESSAGE_LOCKBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS);

              lockboxInfo.mToken = lockboxHashToken.createProof("identity-lockbox-access", Seconds(OPENPEER_STACK_MESSAGE_LOCKBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS));
            }
          }

          if (lockboxInfo.hasData()) {
            rootEl->adoptAsLastChild(lockboxInfo.createElement());  // when info is present, it's only the key hash
          }

          AgentInfo agentInfo = UseStack::agentInfo();
          agentInfo.mLogLevel = Log::None;
          agentInfo.mergeFrom(mAgentInfo, true);

          if (agentInfo.hasData()) {
            rootEl->adoptAsLastChild(agentInfo.createElement());
          }

          if (mGrantID.hasData()) {
            rootEl->adoptAsLastChild(IMessageHelper::createElementWithTextID("grant", mGrantID));
          }

          if (hasAttribute(AttributeType_NamespaceInfos)) {
            ElementPtr namespacesEl = IMessageHelper::createElement("namespaces");

            for (NamespaceInfoMap::iterator iter = mNamespaceInfos.begin(); iter != mNamespaceInfos.end(); ++iter)
            {
              const NamespaceInfo &namespaceInfo = (*iter).second;
              namespacesEl->adoptAsLastChild(namespaceInfo.createElement());
            }

            if (namespacesEl->hasChildren()) {
              rootEl->adoptAsLastChild(namespacesEl);
            }
          }
          return ret;
        }
      }
    }
  }
}
