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

#include <openpeer/stack/message/peer-finder/ChannelMapRequest.h>
#include <openpeer/stack/message/internal/stack_message_MessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>


#define OPENPEER_STACK_MESSAGE_CHANNEL_MAP_REQUEST_PROOF_EXPIRES_IN_SECONDS (60*2)

namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;

      namespace peer_finder
      {
        using zsLib::Seconds;
        typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

        //---------------------------------------------------------------------
        static Log::Params slog(const char *message)
        {
          return Log::Params(message, "ChannelMapRequest");
        }

        //---------------------------------------------------------------------
        ChannelMapRequestPtr ChannelMapRequest::convert(MessagePtr message)
        {
          return dynamic_pointer_cast<ChannelMapRequest>(message);
        }

        //---------------------------------------------------------------------
        ChannelMapRequest::ChannelMapRequest()
        {
        }

        //---------------------------------------------------------------------
        bool ChannelMapRequest::hasAttribute(AttributeTypes type) const
        {
          switch (type)
          {
            case AttributeType_ChannelNumber:           return (0 != mChannelNumber);
            case AttributeType_LocalContextID:          return mLocalContextID.hasData();
            case AttributeType_RemoteContextID:         return mLocalContextID.hasData();
            case AttributeType_RelayToken:              return mRelayToken.hasData();
            default:                                    break;
          }
          return false;
        }

        //---------------------------------------------------------------------
        DocumentPtr ChannelMapRequest::encode()
        {
          DocumentPtr ret = IMessageHelper::createDocumentWithRoot(*this);
          ElementPtr rootEl = ret->getFirstChildElement();

          ElementPtr relayEl = Element::create("relay");

          if (hasAttribute(AttributeType_ChannelNumber)) {
            rootEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("channel", string(mChannelNumber)));
          }

          if (hasAttribute(AttributeType_RelayToken)) {
            Token relayToken = mRelayToken.createProof("peer-finder-channel-map", Seconds(OPENPEER_STACK_MESSAGE_CHANNEL_MAP_REQUEST_PROOF_EXPIRES_IN_SECONDS));
            if (relayToken.hasData()) {
              relayEl->adoptAsLastChild(relayToken.createElement());
            }
          }
          if (hasAttribute(AttributeType_LocalContextID)) {
            relayEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("localContext", mLocalContextID));
          }
          if (hasAttribute(AttributeType_RemoteContextID)) {
            relayEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("remoteContext", mRemoteContextID));
          }

          if (relayEl->hasChildren()) {
            rootEl->adoptAsLastChild(relayEl);
          }

          return ret;
        }

        //---------------------------------------------------------------------
        ChannelMapRequestPtr ChannelMapRequest::create()
        {
          ChannelMapRequestPtr ret(new ChannelMapRequest);
          return ret;
        }

      }
    }
  }
}
