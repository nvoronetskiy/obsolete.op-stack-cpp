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

#pragma once

#include <openpeer/stack/message/MessageNotify.h>
#include <openpeer/stack/message/peer-finder/MessageFactoryPeerFinder.h>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace peer_finder
      {
        class PeerLocationFindNotify : public MessageNotify
        {
        public:
          enum AttributeTypes
          {
            AttributeType_RequestfindProofBundleDigestValue,
            AttributeType_Context,
            AttributeType_ICEUsernameFrag,
            AttributeType_ICEPassword,
            AttributeType_ICEFinal,
            AttributeType_LocationInfo,
            AttributeType_PeerFiles,
          };

        public:
          static PeerLocationFindNotifyPtr convert(MessagePtr message);

          static PeerLocationFindNotifyPtr create(PeerLocationFindRequestPtr request);
          static PeerLocationFindNotifyPtr create(
                                                 ElementPtr root,
                                                 IMessageSourcePtr messageSource
                                                 );

          virtual DocumentPtr encode();

          virtual Methods method() const                                {return (Message::Methods)MessageFactoryPeerFinder::Method_PeerLocationFind;}
          virtual IMessageFactoryPtr factory() const                    {return MessageFactoryPeerFinder::singleton();}

          bool hasAttribute(AttributeTypes type) const;

          const String &context() const                                 {return mContext;}
          void context(const String &secret)                            {mContext = secret;}

          bool validated() const                                        {return mValidated;}
          void validated(bool val)                                      {get(mValidated) = val;}

          const String &iceUsernameFrag() const                         {return mICEUsernameFrag;}
          void iceUsernameFrag(const String &val)                       {mICEUsernameFrag = val;}

          const String &icePassword() const                             {return mICEPassword;}
          void icePassword(const String &val)                           {mICEPassword = val;}

          bool final() const                                            {return mFinal;}
          void final(bool val)                                          {get(mFinal) = val;}

          const String &requestFindProofBundleDigestValue() const       {return mRequestFindProofBundleDigestValue;}
          void requestFindProofBundleDigestValue(const String &secret)  {mRequestFindProofBundleDigestValue = secret;}

          LocationInfoPtr locationInfo() const                          {return mLocationInfo;}
          void locationInfo(LocationInfoPtr locationInfo)               {mLocationInfo = locationInfo;}

          IPeerFilesPtr peerFiles() const                               {return mPeerFiles;}
          void peerFiles(IPeerFilesPtr peerFiles)                       {mPeerFiles = peerFiles;}

        protected:
          PeerLocationFindNotify();

          String mContext;

          AutoBool mValidated;

          String mICEUsernameFrag;
          String mICEPassword;

          AutoBool mFinal;

          String mRequestFindProofBundleDigestValue;
          LocationInfoPtr mLocationInfo;

          IPeerFilesPtr mPeerFiles;
        };
      }
    }
  }
}
