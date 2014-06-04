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

#include <openpeer/stack/message/push-mailbox/AccessRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

//#include <zsLib/Stringize.h>
//#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_STACK_MESSAGE_PUSH_MAILBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace push_mailbox
      {
        using internal::MessageHelper;

        typedef stack::internal::IStackForInternal UseStack;

        using zsLib::Seconds;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "AccessRequest");
        }

        //---------------------------------------------------------------------
        AccessRequestPtr AccessRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<AccessRequest>(message);
        }

        //---------------------------------------------------------------------
        AccessRequest::AccessRequest()
        {
        }

        //---------------------------------------------------------------------
        AccessRequestPtr AccessRequest::create()
        {
          AccessRequestPtr ret(new AccessRequest);
          return ret;
        }

        //---------------------------------------------------------------------
        bool AccessRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_LockboxInfo:       return mLockboxInfo.hasData();
            case AttributeType_AgentInfo:         return mAgentInfo.hasData();
            case AttributeType_GrantID:           return mGrantID.hasData();
            case AttributeType_PeerFiles:         return (bool)mPeerFiles;
            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr AccessRequest::encode()
        {
          IPeerFilePublicPtr peerFilePublic;
          if (mPeerFiles) {
            peerFilePublic =  mPeerFiles->getPeerFilePublic();
          }
          
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          String clientNonce = IHelper::randomString(32);

          LockboxInfo lockboxInfo;

          lockboxInfo.mAccountID = mLockboxInfo.mAccountID;
          lockboxInfo.mAccessToken = mLockboxInfo.mAccessToken;
          if (mLockboxInfo.mAccessSecret.hasData()) {
            lockboxInfo.mAccessSecretProofExpires = zsLib::now() + Seconds(OPENPEER_STACK_MESSAGE_PUSH_MAILBOX_ACCESS_REQUEST_EXPIRES_TIME_IN_SECONDS);
            lockboxInfo.mAccessSecretProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(mLockboxInfo.mAccessSecret), "lockbox-access-validate:" + clientNonce + ":" + IHelper::timeToString(lockboxInfo.mAccessSecretProofExpires) + ":" + lockboxInfo.mAccessToken + ":push-mailbox-access"));
          }

          rootEl->adoptAsLastChild(IMessageHelper::createElementWithText("nonce", clientNonce));
          if (lockboxInfo.hasData()) {
            rootEl->adoptAsLastChild(lockboxInfo.createElement());
          }

          AgentInfo agentInfo = UseStack::agentInfo();
          agentInfo.mergeFrom(mAgentInfo, true);

          if (agentInfo.hasData()) {
            rootEl->adoptAsLastChild(agentInfo.createElement());
          }
          
          if (mGrantID.hasData()) {
            rootEl->adoptAsLastChild(IMessageHelper::createElementWithID("grant", mGrantID));
          }

          if (peerFilePublic) {
            String peerURI = peerFilePublic->getPeerURI();
            ElementPtr uriEl = MessageHelper::createElementWithText("uri", peerURI);
            ElementPtr peerEl = Element::create("peer");
            peerEl->adoptAsLastChild(uriEl);
            rootEl->adoptAsLastChild(peerEl);
          }

          return ret;
        }


      }
    }
  }
}