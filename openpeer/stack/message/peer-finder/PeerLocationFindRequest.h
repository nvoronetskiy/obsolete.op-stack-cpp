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

#pragma once

#include <openpeer/stack/message/MessageRequest.h>
#include <openpeer/stack/message/peer-finder/MessageFactoryPeerFinder.h>

#include <list>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      namespace peer_finder
      {
        class PeerLocationFindRequest : public MessageRequest
        {
        public:
          friend class PeerLocationFindNotify;

          typedef Message::Methods Methods;
          typedef std::list<zsLib::String> ExcludedLocationList;

          enum AttributeTypes
          {
            AttributeType_RequestfindProofBundleDigestValue,  // filled when decoding from incoming request or after calling encode
            AttributeType_CreatedTimestamp,
            AttributeType_FindPeer,
            AttributeType_Context,
            AttributeType_PeerSecret,
            AttributeType_DHKeyDomain,
            AttributeType_DHPrivateKey,
            AttributeType_DHPublicKey,
            AttributeType_ICEUsernameFrag,
            AttributeType_ICEPassword,
            AttributeType_Final,
            AttributeType_ExcludedLocations,
            AttributeType_LocationInfo,
            AttributeType_PeerFiles,
          };

        public:
          static PeerLocationFindRequestPtr convert(MessagePtr message);

          static PeerLocationFindRequestPtr create();
          static PeerLocationFindRequestPtr create(
                                                   ElementPtr root,
                                                   IMessageSourcePtr messageSource
                                                   );

          virtual DocumentPtr encode();

          virtual Methods method() const                                  {return (Message::Methods)MessageFactoryPeerFinder::Method_PeerLocationFind;}

          virtual IMessageFactoryPtr factory() const                      {return MessageFactoryPeerFinder::singleton();}

          bool hasAttribute(AttributeTypes type) const;

          const String &requestFindProofBundleDigestValue() const         {return mRequestFindProofBundleDigestValue;}
          void requestFindProofBundleDigestValue(const String &secret)    {mRequestFindProofBundleDigestValue = secret;}

          Time created() const                                            {return mCreated;}
          void created(const Time &value)                                 {mCreated = value;}

          const IPeerPtr &findPeer() const                                {return mFindPeer;}
          void findPeer(const IPeerPtr &peer)                             {mFindPeer = peer;}

          const String &context() const                                   {return mContext;}
          void context(const String &val)                                 {mContext = val;}

          const String &peerSecret() const                                {return mPeerSecret;}
          void peerSecret(const String &secret)                           {mPeerSecret = secret;}

          IDHKeyDomainPtr dhKeyDomain() const;
          void dhKeyDomain(IDHKeyDomainPtr value)                         {mDHKeyDomain = value;}

          IDHPrivateKeyPtr dhPrivateKey() const                           {return mDHPrivateKey;}
          void dhPrivateKey(IDHPrivateKeyPtr value)                       {mDHPrivateKey = value;}

          IDHPublicKeyPtr dhPublicKey() const                             {return mDHPublicKey;}
          void dhPublicKey(IDHPublicKeyPtr value)                         {mDHPublicKey = value;}

          const String &iceUsernameFrag() const                           {return mICEUsernameFrag;}
          void iceUsernameFrag(const String &val)                         {mICEUsernameFrag = val;}

          const String &icePassword() const                               {return mICEPassword;}
          void icePassword(const String &val)                             {mICEPassword = val;}

          bool final() const                                              {return mFinal > 0;}
          void final(bool value)                                          {mFinal = (value ? 1 : 0);}

          const ExcludedLocationList &excludeLocations() const            {return mExcludedLocations;}
          void excludeLocations(const ExcludedLocationList &excludeList)  {mExcludedLocations = excludeList;}

          LocationInfoPtr locationInfo() const                            {return mLocationInfo;}
          void locationInfo(LocationInfoPtr location)                     {mLocationInfo = location;}

          IPeerFilesPtr peerFiles() const                                 {return mPeerFiles;}
          void peerFiles(IPeerFilesPtr peerFiles)                         {mPeerFiles = peerFiles;}

          bool didVerifySignature() const                                 {return mDidVerifySignature;}

        protected:
          PeerLocationFindRequest();

          String mRequestFindProofBundleDigestValue;

          Time mCreated;

          IPeerPtr mFindPeer;

          String mContext;
          String mPeerSecret;

          IDHKeyDomainPtr mDHKeyDomain;
          IDHPrivateKeyPtr mDHPrivateKey;
          IDHPublicKeyPtr mDHPublicKey;

          String mICEUsernameFrag;
          String mICEPassword;

          int mFinal;

          ExcludedLocationList mExcludedLocations;

          LocationInfoPtr mLocationInfo;

          IPeerFilesPtr mPeerFiles;

          bool mDidVerifySignature {};
        };
      }
    }
  }
}
