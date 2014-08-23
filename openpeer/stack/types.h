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

#include <zsLib/types.h>
#include <zsLib/String.h>
#include <zsLib/IPAddress.h>
#include <zsLib/Proxy.h>
#include <zsLib/ProxySubscriptions.h>

#include <cryptopp/secblock.h>

#include <openpeer/services/IICESocket.h>

#define OPENPEER_COMMON_SETTING_APPLICATION_NAME              "openpeer/common/application-name"
#define OPENPEER_COMMON_SETTING_APPLICATION_IMAGE_URL         "openpeer/common/application-image-url"
#define OPENPEER_COMMON_SETTING_APPLICATION_URL               "openpeer/common/application-url"

#define OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID  "openpeer/calculated/authorizated-application-id"
#define OPENPEER_COMMON_SETTING_USER_AGENT                    "openpeer/calculated/user-agent"
#define OPENPEER_COMMON_SETTING_DEVICE_ID                     "openpeer/calculated/device-id"
#define OPENPEER_COMMON_SETTING_OS                            "openpeer/calculated/os"
#define OPENPEER_COMMON_SETTING_SYSTEM                        "openpeer/calculated/system"
#define OPENPEER_COMMON_SETTING_INSTANCE_ID                   "openpeer/calculated/instance-id"

namespace openpeer
{
  namespace stack
  {
    using zsLib::PUID;
    using zsLib::BYTE;
    using zsLib::WORD;
    using zsLib::UINT;
    using zsLib::LONG;
    using zsLib::ULONG;
    using zsLib::Time;
    using zsLib::Duration;
    using zsLib::Seconds;
    using zsLib::String;
    using zsLib::AutoBool;
    using zsLib::IPAddress;
    using zsLib::Log;

    using boost::dynamic_pointer_cast;

    ZS_DECLARE_TYPEDEF_PTR(zsLib::RecursiveLock, RecursiveLock)
    ZS_DECLARE_TYPEDEF_PTR(zsLib::AutoRecursiveLock, AutoRecursiveLock)

    ZS_DECLARE_USING_PTR(zsLib, IMessageQueue)

    ZS_DECLARE_USING_PTR(zsLib::XML, Element)
    ZS_DECLARE_USING_PTR(zsLib::XML, Document)
    ZS_DECLARE_USING_PTR(zsLib::XML, Node)

    using openpeer::services::SharedRecursiveLock;
    using openpeer::services::LockedValue;

    ZS_DECLARE_USING_PTR(services, IHTTP)
    ZS_DECLARE_USING_PTR(services, IICESocket)
    ZS_DECLARE_USING_PTR(services, IRSAPrivateKey)
    ZS_DECLARE_USING_PTR(services, IRSAPublicKey)
    ZS_DECLARE_USING_PTR(services, IDHKeyDomain)

    ZS_DECLARE_TYPEDEF_PTR(services::SecureByteBlock, SecureByteBlock)

    ZS_DECLARE_INTERACTION_PTR(ILocation)
    ZS_DECLARE_INTERACTION_PTR(IMessageSource)

    ZS_DECLARE_STRUCT_PTR(Candidate)
    ZS_DECLARE_STRUCT_PTR(LocationInfo)
    ZS_DECLARE_STRUCT_PTR(Token)

    ZS_DECLARE_TYPEDEF_PTR(std::list<Candidate>, CandidateList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<LocationInfoPtr>, LocationInfoList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<ElementPtr>, IdentityBundleElementList)

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark Token
    #pragma mark

    struct Token
    {
      String mID;
      String mSecret;
      String mSecretEncrypted;
      Time mExpires;

      String mProof;
      String mNonce;
      String mResource;

      Token() {}
      bool hasData() const;
      ElementPtr toDebug() const;

      void mergeFrom(
                     const Token &source,
                     bool overwriteExisting = true
                     );

      static Token create(
                          const String &masterSecret,
                          const String &associatedID,
                          Duration validDuration
                          );


      static Token create(ElementPtr elem);
      Token createProof(
                        const char *resource,
                        Duration validDuration
                        );

      ElementPtr createElement() const;
      bool validate(const Token &proofToken) const;
      bool validate(
                    const String &inMasterSecret,
                    String &outAssociatedID
                    ) const;
    };

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark Candidate
    #pragma mark

    struct Candidate : public IICESocket::Candidate
    {
      struct Protocol
      {
        String mTransport;
        String mHost;

        bool hasData() const;
        ElementPtr toDebug() const;

        static Protocol create(ElementPtr elem);
        ElementPtr createElement() const;
      };
      typedef std::list<Protocol> ProtocolList;

      String mNamespace;
      String mTransport;
      ProtocolList mProtocols;

      Token mToken;

      Candidate();
      Candidate(const Candidate &candidate);
      Candidate(const IICESocket::Candidate &candidate);
      bool hasData() const;
      ElementPtr toDebug() const;

      static Candidate create(
                              ElementPtr elem,
                              const char *encryptionPassphrase = NULL
                              );
      ElementPtr createElement(const char *encryptionPassphrase = NULL) const;

      static ProtocolList createProtocolList(ElementPtr protocolsEl);
      static ElementPtr createElement(const ProtocolList &protocols);

      static ElementPtr toDebug(
                                const ProtocolList &protocols,
                                const char *groupingElementName = NULL
                                );
    };

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark LocationInfo
    #pragma mark

    struct LocationInfo
    {
      ILocationPtr mLocation;

      String    mDeviceID;
      IPAddress mIPAddress;
      String    mUserAgent;
      String    mOS;
      String    mSystem;
      String    mHost;

      AutoBool  mCandidatesFinal;
      String    mCandidatesVersion;
      CandidateList mCandidates;

      bool hasData() const;
      ElementPtr toDebug() const;

      static LocationInfoPtr create();

      static LocationInfoPtr create(
                                    ElementPtr elem,
                                    IMessageSourcePtr messageSource,
                                    const char *encryptionPassphrase = NULL
                                    );

      ElementPtr createElement(const char *encryptionPassphrase = NULL) const;

    protected:
      LocationInfo() {}
      LocationInfo(const LocationInfo &info) {} // not legal
    };

    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    #pragma mark
    #pragma mark Server
    #pragma mark

    struct Server
    {
      typedef Candidate::Protocol Protocol;
      typedef Candidate::ProtocolList ProtocolList;

      String mID;
      String mType;
      ProtocolList mProtocols;
      IRSAPublicKeyPtr mPublicKey;
      WORD  mPriority;
      WORD  mWeight;
      String mRegion;
      Time mCreated;
      Time mExpires;

      Server() :
        mPriority(0),
        mWeight(0)
        {}

      bool hasData() const;
      ElementPtr toDebug() const;

      static Server create(ElementPtr elem);
    };

    ZS_DECLARE_TYPEDEF_PTR(std::list<Server>, ServerList)

    ZS_DECLARE_INTERACTION_PTR(IAccount)
    ZS_DECLARE_INTERACTION_PTR(IBootstrappedNetwork)
    ZS_DECLARE_INTERACTION_PTR(ICache)
    ZS_DECLARE_INTERACTION_PTR(ICacheDelegate)
    ZS_DECLARE_INTERACTION_PTR(IDiff)
    ZS_DECLARE_INTERACTION_PTR(IHelper)
    ZS_DECLARE_INTERACTION_PTR(IKeyGenerator)
    ZS_DECLARE_INTERACTION_PTR(IMessageIncoming)
    ZS_DECLARE_INTERACTION_PTR(IMessageMonitor)
    ZS_DECLARE_INTERACTION_PTR(IMessageSource)
    ZS_DECLARE_INTERACTION_PTR(ILocation)
    ZS_DECLARE_INTERACTION_PTR(IPeerFiles)
    ZS_DECLARE_INTERACTION_PTR(IPeerFilePublic)
    ZS_DECLARE_INTERACTION_PTR(IPeerFilePrivate)
    ZS_DECLARE_INTERACTION_PTR(IPeer)
    ZS_DECLARE_INTERACTION_PTR(IPeerSubscription)
    ZS_DECLARE_INTERACTION_PTR(IPublication)
    ZS_DECLARE_INTERACTION_PTR(IPublicationLocker)
    ZS_DECLARE_INTERACTION_PTR(IPublicationMetaData)
    ZS_DECLARE_INTERACTION_PTR(IPublicationPublisher)
    ZS_DECLARE_INTERACTION_PTR(IPublicationFetcher)
    ZS_DECLARE_INTERACTION_PTR(IPublicationRemover)
    ZS_DECLARE_INTERACTION_PTR(IPublicationRepository)
    ZS_DECLARE_INTERACTION_PTR(IPublicationRepositoryPeerCache)
    ZS_DECLARE_INTERACTION_PTR(IPublicationSubscription)
    ZS_DECLARE_INTERACTION_PTR(IServerMessaging)
    ZS_DECLARE_INTERACTION_PTR(IServiceCertificates)
    ZS_DECLARE_INTERACTION_PTR(IServiceCertificatesValidateQuery)
    ZS_DECLARE_INTERACTION_PTR(IServiceSalt)
    ZS_DECLARE_INTERACTION_PTR(IServiceSaltFetchSignedSaltQuery)
    ZS_DECLARE_INTERACTION_PTR(IServiceIdentity)
    ZS_DECLARE_INTERACTION_PTR(IServiceIdentitySession)
    ZS_DECLARE_INTERACTION_PTR(IServiceIdentityProofBundleQuery)
    ZS_DECLARE_INTERACTION_PTR(IServiceLockbox)
    ZS_DECLARE_INTERACTION_PTR(IServiceLockboxSession)
    ZS_DECLARE_INTERACTION_PTR(IServiceNamespaceGrantSession)
    ZS_DECLARE_INTERACTION_PTR(IServicePeerFileLookupQuery)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailbox)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailboxDatabaseAbstractionDelegate)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailboxDatabaseAbstractionNotifier)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailboxSession)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailboxSendQuery)
    ZS_DECLARE_INTERACTION_PTR(IServicePushMailboxRegisterQuery)
    ZS_DECLARE_INTERACTION_PTR(ISettings)
    ZS_DECLARE_INTERACTION_PTR(ISettingsDelegate)
    ZS_DECLARE_INTERACTION_PTR(IStack)

    ZS_DECLARE_INTERACTION_PROXY(IAccountDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IBootstrappedNetworkDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IKeyGeneratorDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IMessageMonitorDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPeerSubscriptionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPublicationPublisherDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPublicationFetcherDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPublicationRemoverDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IPublicationSubscriptionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServerMessagingDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceCertificatesValidateQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceSaltFetchSignedSaltQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceIdentitySessionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceIdentityProofBundleQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceLockboxSessionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServiceNamespaceGrantSessionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServicePeerFileLookupQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServicePushMailboxSessionDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServicePushMailboxSendQueryDelegate)
    ZS_DECLARE_INTERACTION_PROXY(IServicePushMailboxRegisterQueryDelegate)

    ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IKeyGeneratorSubscription, IKeyGeneratorDelegate)
    ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IServerMessagingSubscription, IServerMessagingDelegate)
    ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IServicePushMailboxSessionSubscription, IServicePushMailboxSessionDelegate)

    ZS_DECLARE_TYPEDEF_PTR(std::list<ILocationPtr>, LocationList)
    ZS_DECLARE_TYPEDEF_PTR(std::list<IServiceIdentitySessionPtr>, ServiceIdentitySessionList)
  }
}
