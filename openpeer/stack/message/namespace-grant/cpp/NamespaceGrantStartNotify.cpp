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

#include <openpeer/stack/message/namespace-grant/NamespaceGrantStartNotify.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>
#include <openpeer/stack/internal/stack_Stack.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#define OPENPEER_STACK_MESSAGE_LOCKBOX_NAMESPACE_GRANT_START_UPDATE_REQUEST_EXPIRES_TIME_IN_SECONDS ((60*60)*24)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace namespace_grant
      {
        typedef stack::internal::IStackForInternal UseStack;
        
        using zsLib::Seconds;
        using internal::MessageHelper;
        using stack::internal::Helper;
        using stack::internal::IStackForInternal;

        //---------------------------------------------------------------------
        const char *NamespaceGrantStartNotify::toString(BrowserVisibilities visibility)
        {
          switch (visibility) {
            case BrowserVisibility_NA:              return "";

            case BrowserVisibility_Hidden:          return "hidden";
            case BrowserVisibility_Visible:         return "visbile";
            case BrowserVisibility_VisibleOnDemand: return "visible-on-demand";
          }
          return "";
        }

        //---------------------------------------------------------------------
        NamespaceGrantStartNotifyPtr NamespaceGrantStartNotify::convert(MessagePtr message)
        {
          return ZS_DYNAMIC_PTR_CAST(NamespaceGrantStartNotify, message);
        }

        //---------------------------------------------------------------------
        NamespaceGrantStartNotify::NamespaceGrantStartNotify() :
          mVisibility(BrowserVisibility_NA),
          mPopup(-1)
        {
        }

        //---------------------------------------------------------------------
        NamespaceGrantStartNotifyPtr NamespaceGrantStartNotify::create()
        {
          NamespaceGrantStartNotifyPtr ret(new NamespaceGrantStartNotify);
          return ret;
        }

        //---------------------------------------------------------------------
        bool NamespaceGrantStartNotify::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_AgentInfo:         return mAgentInfo.hasData();
            case AttributeType_Challenges:        return (mChallenges.size() > 0);
            case AttributeType_BrowserVisibility: return (BrowserVisibility_NA != mVisibility);
            case AttributeType_BrowserPopup:      return (mPopup >= 0);
            case AttributeType_OuterFrameURL:     return mOuterFrameURL.hasData();

            default:                              break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr NamespaceGrantStartNotify::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr browserEl = Element::create("browser");

          String clientNonce = IHelper::randomString(32);

          AgentInfo agentInfo = UseStack::agentInfo();
          agentInfo.mergeFrom(mAgentInfo, true);

          if (agentInfo.hasData()) {
            rootEl->adoptAsLastChild(agentInfo.createElement());
          }

          if (mChallenges.size() > 0) {
            ElementPtr namespaceGrantChallengesEl = Element::create("namespaceGrantChallenges");

            for (NamespaceGrantChallengeInfoList::iterator iterChallenge = mChallenges.begin(); iterChallenge != mChallenges.end(); ++iterChallenge)
            {
              const NamespaceGrantChallengeInfo &namespaceGrantChallengeInfo = (*iterChallenge);

              ElementPtr namespaceGrantChallengeEl = namespaceGrantChallengeInfo.createElement();
              if (!namespaceGrantChallengeEl) {
                continue;
              }

              namespaceGrantChallengesEl->adoptAsLastChild(namespaceGrantChallengeEl);
            }

            if (namespaceGrantChallengesEl->hasChildren()) {
              rootEl->adoptAsLastChild(namespaceGrantChallengesEl);
            }
          }

          
          if (hasAttribute(AttributeType_BrowserVisibility)) {
            browserEl->adoptAsLastChild(IMessageHelper::createElementWithText("visibility", toString(mVisibility)));
          }

          if (hasAttribute(AttributeType_BrowserPopup)) {
            browserEl->adoptAsLastChild(IMessageHelper::createElementWithText("popup", (mPopup ? "allow" : "deny")));
          }

          if (hasAttribute(AttributeType_OuterFrameURL)) {
            browserEl->adoptAsLastChild(IMessageHelper::createElementWithText("outerFrameURL", mOuterFrameURL));
          }

          if (browserEl->hasChildren()) {
            rootEl->adoptAsLastChild(browserEl);
          }

          return ret;
        }
      }
    }
  }
}
