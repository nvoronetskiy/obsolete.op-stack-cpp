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

#include <openpeer/stack/types.h>
#include <openpeer/services/IICESocket.h>

#include <list>
#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using zsLib::string;
      using zsLib::DWORD;
      using zsLib::XML::Document;
      using zsLib::XML::Element;
      using zsLib::XML::Text;
      using zsLib::XML::TextPtr;
      using zsLib::XML::Attribute;
      using zsLib::XML::AttributePtr;
      using zsLib::Singleton;
      using zsLib::SingletonLazySharedPtr;

      using services::IICESocket;
      using services::IRSAPrivateKey;
      using services::IRSAPrivateKeyPtr;
      using services::IRSAPublicKey;
      using services::IRSAPublicKeyPtr;
      using services::IDHKeyDomain;
      using services::IDHKeyDomainPtr;
      using services::IDHPrivateKey;
      using services::IDHPrivateKeyPtr;
      using services::IDHPublicKey;
      using services::IDHPublicKeyPtr;

      typedef std::list<String> RouteList;
      typedef std::list<String> StringList;

      typedef stack::LocationInfo LocationInfo;
      typedef stack::LocationInfoList LocationInfoList;

      ZS_DECLARE_STRUCT_PTR(Service)
      ZS_DECLARE_STRUCT_PTR(IdentityInfo)

      ZS_DECLARE_TYPEDEF_PTR(std::list<IdentityInfo>, IdentityInfoList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark AgentInfo
      #pragma mark

      struct AgentInfo
      {
        String mUserAgent;
        String mName;
        String mImageURL;
        String mAgentURL;

        Log::Level mLogLevel;

        AgentInfo() :
          mLogLevel(Log::None) {}

        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const AgentInfo &source,
                       bool overwriteExisting = true
                       );

        static AgentInfo create(ElementPtr elem);
        ElementPtr createElement(bool forceLogLevelOutput = false) const;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Certificate
      #pragma mark

      struct Certificate
      {
        typedef String CertificateID;
        String mID;
        String mService;
        Time mExpires;
        IRSAPublicKeyPtr mPublicKey;

        bool hasData() const;
        ElementPtr toDebug() const;
      };

      typedef std::map<Certificate::CertificateID, Certificate> TCertificateMap;
      ZS_DECLARE_TYPEDEF_PTR(TCertificateMap, CertificateMap)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IdentityInfo
      #pragma mark

      struct IdentityInfo
      {
        enum Dispositions
        {
          Disposition_NA, // not applicable
          Disposition_Update,
          Disposition_Remove,
        };

        struct Avatar
        {
          String mName;
          String mURL;
          int mWidth;
          int mHeight;

          Avatar() : mWidth(0), mHeight(0) {}
          bool hasData() const;
          ElementPtr toDebug() const;
        };
        typedef std::list<Avatar> AvatarList;

        static const char *toString(Dispositions diposition);
        static Dispositions toDisposition(const char *str);

        Dispositions mDisposition;

        Token mToken;

        String mReloginKey;
        String mReloginKeyEncrypted;

        String mBase;
        String mURI;
        String mProvider;

        String mStableID;
        IPeerFilePublicPtr mPeerFilePublic;

        WORD mPriority;
        WORD mWeight;

        Time mCreated;
        Time mUpdated;
        Time mExpires;

        String mName;
        String mProfile;
        String mVProfile;

        AvatarList mAvatars;

        ElementPtr mContactProofBundle;
        ElementPtr mIdentityProofBundle;

        IdentityInfo() : mDisposition(Disposition_NA), mPriority(0), mWeight(0) {}
        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const IdentityInfo &source,
                       bool overwriteExisting = true
                       );

        static IdentityInfo create(ElementPtr elem);
        ElementPtr createElement(bool forcePriorityWeightOutput = false) const;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark LockboxInfo
      #pragma mark

      struct LockboxInfo
      {
        String mDomain;
        String mAccountID;

        String mKeyName;
        String mKeyEncrypted;
        String mKeyHash;

        SecureByteBlockPtr mKey;

        Token mToken;

        bool mResetFlag;

        LockboxInfo() : mResetFlag(false) {}
        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const LockboxInfo &source,
                       bool overwriteExisting = true
                       );

        static LockboxInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark NamespaceInfo
      #pragma mark

      struct NamespaceInfo
      {
        typedef String NamespaceURL;

        NamespaceURL mURL;
        Time mLastUpdated;

        NamespaceInfo() {}
        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const NamespaceInfo &source,
                       bool overwriteExisting = true
                       );

        static NamespaceInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      typedef std::map<NamespaceInfo::NamespaceURL, NamespaceInfo> TNamespaceInfoMap;
      ZS_DECLARE_TYPEDEF_PTR(TNamespaceInfoMap, NamespaceInfoMap)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark NamespaceGrantChallengeInfo
      #pragma mark

      struct NamespaceGrantChallengeInfo
      {
        String mID;
        String mName;
        String mImageURL;
        String mServiceURL;
        String mDomains;

        NamespaceInfoMap mNamespaces;

        NamespaceGrantChallengeInfo() {}
        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const NamespaceGrantChallengeInfo &source,
                       bool overwriteExisting = true
                       );

        static NamespaceGrantChallengeInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<NamespaceGrantChallengeInfo>, NamespaceGrantChallengeInfoList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessageInfo
      #pragma mark

      struct PushMessageInfo
      {
        struct PushInfo
        {
          String mServiceType;

          ElementPtr mValues;
          ElementPtr mCustom;

          bool hasData() const;
          ElementPtr toDebug() const;

          void mergeFrom(
                         const PushInfo &source,
                         bool overwriteExisting = true
                         );
        };

        typedef std::list<PushInfo> PushInfoList;

        struct FlagInfo
        {
          struct URIInfo
          {
            String mURI;

            WORD mErrorCode;
            String mErrorReason;

            bool hasData() const;
          };
          typedef std::list<URIInfo> URIInfoList;

          enum Dispositions
          {
            Disposition_NA,

            Disposition_Subscribe,
            Disposition_Update,
            Disposition_Remove,
          };
          static Dispositions toDisposition(const char *disposition);
          static const char *toString(Dispositions disposition);

          enum Flags
          {
            Flag_NA,

            Flag_Read,
            Flag_Answered,
            Flag_Flagged,
            Flag_Deleted,
            Flag_Draft,
            Flag_Recent,
            Flag_Delivered,
            Flag_Sent,
            Flag_Pushed,
            Flag_Error,
          };
          static Flags toFlag(const char *flagName);
          static const char *toString(Flags flag);

          Dispositions mDisposition;

          Flags mFlag;
          URIInfoList mFlagURIInfos;

          FlagInfo() :
            mDisposition(Disposition_NA),
            mFlag(Flag_NA) {}

          bool hasData() const;
        };

        typedef std::map<FlagInfo::Flags, FlagInfo> FlagInfoMap;

        struct FolderInfo
        {
          enum Dispositions
          {
            Disposition_NA,

            Disposition_Update,
            Disposition_Remove,
          };

          static Dispositions toDisposition(const char *disposition);
          static const char *toString(Dispositions disposition);

          enum Where
          {
            Where_NA,

            Where_Local,
            Where_Remote,
          };

          static Where toWhere(const char *where);
          static const char *toString(Where where);

          Dispositions mDisposition;
          Where mWhere;

          String mName;

          FolderInfo() :
            mDisposition(Disposition_NA),
            mWhere(Where_NA) {}

          bool hasData() const;
        };
        typedef std::list<FolderInfo> FolderInfoList;

        enum Dispositions
        {
          Disposition_NA,

          Disposition_Update,
          Disposition_Remove,
        };

        static Dispositions toDisposition(const char *disposition);
        static const char *toString(Dispositions disposition);

        String mID;
        Dispositions mDisposition;

        String mVersion;

        DWORD mChannelID;

        String mTo;
        String mCC;
        String mBCC;

        String mFrom;

        String mType;
        String mMimeType;
        String mEncoding;

        String mPushType;
        PushInfoList mPushInfos;

        Time mSent;
        Time mExpires;

        size_t mLength;
        size_t mRemaining;

        FolderInfoList mFolders;

        FlagInfoMap mFlags;

        PushMessageInfo() :
          mDisposition(Disposition_NA),
          mChannelID(0),
          mLength(0),
          mRemaining(0) {}

        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const PushMessageInfo &source,
                       bool overwriteExisting = true
                       );

        static PushMessageInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushMessageInfo>, PushMessageInfoList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushMessageFolderInfo
      #pragma mark

      struct PushMessageFolderInfo
      {
        enum Dispositions
        {
          Disposition_NA, // not applicable
          Disposition_Update,
          Disposition_Remove,
          Disposition_Reset,
        };

        static const char *toString(Dispositions diposition);
        static Dispositions toDisposition(const char *str);

        Dispositions mDisposition;
        String mName;
        String mRenamed;
        String mVersion;
        ULONG mUnread;
        ULONG mTotal;

        Time mUpdateNext;

        PushMessageFolderInfo() :
          mDisposition(Disposition_NA),
          mUnread(0),
          mTotal(0) {}

        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const PushMessageFolderInfo &source,
                       bool overwriteExisting = true
                       );

        static PushMessageFolderInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushMessageFolderInfo>, PushMessageFolderInfoList)
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PushSubscriptionInfo
      #pragma mark

      struct PushSubscriptionInfo
      {
        typedef String ValueName;
        typedef std::list<ValueName> ValueNameList;

        String mFolder;

        Time mExpires;

        String mType;

        String mMapped;

        bool mUnreadBadge;

        String mSound;

        String mAction;

        String mLaunchImage;

        ULONG mPriority;

        ValueNameList mValueNames;

        PushSubscriptionInfo() :
          mUnreadBadge(false),
          mPriority(0) {}

        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const PushSubscriptionInfo &source,
                       bool overwriteExisting = true
                       );

        static PushSubscriptionInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      ZS_DECLARE_TYPEDEF_PTR(std::list<PushSubscriptionInfo>, PushSubscriptionInfoList)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Service
      #pragma mark

      struct Service
      {
        ZS_DECLARE_STRUCT_PTR(Method)
        ZS_DECLARE_TYPEDEF_PTR(std::list<MethodPtr>, MethodList)

        typedef String ServiceID;
        typedef String Type;

        struct Method
        {
          typedef String MethodName;

          MethodName mName;
          String mURI;
          String mUsername;
          String mPassword;

          bool hasData() const;
          ElementPtr toDebug() const;
        };
        typedef std::map<Method::MethodName, Method> MethodMap;

        ServiceID mID;
        String mType;
        String mVersion;

        MethodMap mMethods;

        bool hasData() const;
        ElementPtr toDebug() const;

        static Service create(ElementPtr elem);
      };

      typedef std::map<Service::ServiceID, Service> TServiceMap;
      ZS_DECLARE_TYPEDEF_PTR(TServiceMap, ServiceMap)

      typedef std::map<Service::Type, ServiceMap> TServiceTypeMap;
      ZS_DECLARE_TYPEDEF_PTR(TServiceTypeMap, ServiceTypeMap)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark RolodexInfo
      #pragma mark

      struct RolodexInfo
      {
        String mServerToken;

        Token mToken;

        String mVersion;
        Time mUpdateNext;

        bool mRefreshFlag;

        RolodexInfo() : mRefreshFlag(false) {}
        bool hasData() const;
        ElementPtr toDebug() const;

        void mergeFrom(
                       const RolodexInfo &source,
                       bool overwriteExisting = true
                       );

        static RolodexInfo create(ElementPtr elem);
        ElementPtr createElement() const;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (forwards)
      #pragma mark

      ZS_DECLARE_CLASS_PTR(Message)
      ZS_DECLARE_CLASS_PTR(MessageRequest)
      ZS_DECLARE_CLASS_PTR(MessageResult)
      ZS_DECLARE_CLASS_PTR(MessageNotify)

      ZS_DECLARE_INTERACTION_PTR(IMessageHelper)
      ZS_DECLARE_INTERACTION_PTR(IMessageFactory)


      namespace bootstrapper
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryBootstrapper)
        ZS_DECLARE_CLASS_PTR(ServicesGetRequest)
        ZS_DECLARE_CLASS_PTR(ServicesGetResult)
      }

      namespace bootstrapped_servers
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryBootstrappedServers)
        ZS_DECLARE_CLASS_PTR(ServersGetRequest)
        ZS_DECLARE_CLASS_PTR(ServersGetResult)
      }

      namespace certificates
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryCertificates)
        ZS_DECLARE_CLASS_PTR(CertificatesGetRequest)
        ZS_DECLARE_CLASS_PTR(CertificatesGetResult)
      }

      namespace identity
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryIdentity)
        ZS_DECLARE_CLASS_PTR(IdentityAccessWindowRequest)
        ZS_DECLARE_CLASS_PTR(IdentityAccessWindowResult)
        ZS_DECLARE_CLASS_PTR(IdentityAccessStartNotify)
        ZS_DECLARE_CLASS_PTR(IdentityAccessCompleteNotify)
        ZS_DECLARE_CLASS_PTR(IdentityAccessNamespaceGrantChallengeValidateRequest)
        ZS_DECLARE_CLASS_PTR(IdentityAccessNamespaceGrantChallengeValidateResult)
        ZS_DECLARE_CLASS_PTR(IdentityAccessRolodexCredentialsGetRequest)
        ZS_DECLARE_CLASS_PTR(IdentityAccessRolodexCredentialsGetResult)
        ZS_DECLARE_CLASS_PTR(IdentityLookupUpdateRequest)
        ZS_DECLARE_CLASS_PTR(IdentityLookupUpdateResult)
      }

      namespace identity_lockbox
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryIdentityLockbox)
        ZS_DECLARE_CLASS_PTR(LockboxAccessRequest)
        ZS_DECLARE_CLASS_PTR(LockboxAccessResult)
        ZS_DECLARE_CLASS_PTR(LockboxAccessValidateRequest)
        ZS_DECLARE_CLASS_PTR(LockboxAccessValidateResult)
        ZS_DECLARE_CLASS_PTR(LockboxNamespaceGrantChallengeValidateRequest)
        ZS_DECLARE_CLASS_PTR(LockboxNamespaceGrantChallengeValidateResult)
        ZS_DECLARE_CLASS_PTR(LockboxIdentitiesUpdateRequest)
        ZS_DECLARE_CLASS_PTR(LockboxIdentitiesUpdateResult)
        ZS_DECLARE_CLASS_PTR(LockboxContentGetRequest)
        ZS_DECLARE_CLASS_PTR(LockboxContentGetResult)
        ZS_DECLARE_CLASS_PTR(LockboxContentSetRequest)
        ZS_DECLARE_CLASS_PTR(LockboxContentSetResult)
      }

      namespace identity_lookup
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryIdentityLookup)
        ZS_DECLARE_CLASS_PTR(IdentityLookupCheckRequest)
        ZS_DECLARE_CLASS_PTR(IdentityLookupCheckResult)
        ZS_DECLARE_CLASS_PTR(IdentityLookupRequest)
        ZS_DECLARE_CLASS_PTR(IdentityLookupResult)
      }

      namespace namespace_grant
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryNamespaceGrant)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantWindowRequest)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantWindowResult)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantStartNotify)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantCompleteNotify)
      }
      
      namespace rolodex
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryRolodex)
        ZS_DECLARE_CLASS_PTR(RolodexAccessRequest)
        ZS_DECLARE_CLASS_PTR(RolodexAccessResult)
        ZS_DECLARE_CLASS_PTR(RolodexNamespaceGrantChallengeValidateRequest)
        ZS_DECLARE_CLASS_PTR(RolodexNamespaceGrantChallengeValidateResult)
        ZS_DECLARE_CLASS_PTR(RolodexContactsGetRequest)
        ZS_DECLARE_CLASS_PTR(RolodexContactsGetResult)
      }

      namespace peer
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPeer)
        ZS_DECLARE_CLASS_PTR(PeerServicesGetRequest)
        ZS_DECLARE_CLASS_PTR(PeerServicesGetResult)
        ZS_DECLARE_CLASS_PTR(PeerFilesGetRequest)
        ZS_DECLARE_CLASS_PTR(PeerFilesGetResult)
        ZS_DECLARE_CLASS_PTR(PeerFileSetRequest)
        ZS_DECLARE_CLASS_PTR(PeerFileSetResult)
      }

      namespace peer_common
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPeerCommon)
        ZS_DECLARE_CLASS_PTR(PeerPublishRequest)
        ZS_DECLARE_CLASS_PTR(PeerPublishResult)
        ZS_DECLARE_CLASS_PTR(PeerGetRequest)
        ZS_DECLARE_CLASS_PTR(PeerGetResult)
        ZS_DECLARE_CLASS_PTR(PeerDeleteRequest)
        ZS_DECLARE_CLASS_PTR(PeerDeleteResult)
        ZS_DECLARE_CLASS_PTR(PeerSubscribeRequest)
        ZS_DECLARE_CLASS_PTR(PeerSubscribeResult)
        ZS_DECLARE_CLASS_PTR(PeerPublishNotify)
      }

      namespace peer_finder
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPeerFinder)
        ZS_DECLARE_CLASS_PTR(ChannelMapRequest)
        ZS_DECLARE_CLASS_PTR(ChannelMapResult)
        ZS_DECLARE_CLASS_PTR(ChannelMapNotify)
        ZS_DECLARE_CLASS_PTR(PeerLocationFindRequest)
        ZS_DECLARE_CLASS_PTR(PeerLocationFindResult)
        ZS_DECLARE_CLASS_PTR(PeerLocationFindNotify)
        ZS_DECLARE_CLASS_PTR(SessionCreateRequest)
        ZS_DECLARE_CLASS_PTR(SessionCreateResult)
        ZS_DECLARE_CLASS_PTR(SessionDeleteRequest)
        ZS_DECLARE_CLASS_PTR(SessionDeleteResult)
        ZS_DECLARE_CLASS_PTR(SessionKeepAliveRequest)
        ZS_DECLARE_CLASS_PTR(SessionKeepAliveResult)
      }

      namespace push_mailbox
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPushMailbox)
        ZS_DECLARE_CLASS_PTR(AccessRequest)
        ZS_DECLARE_CLASS_PTR(AccessResult)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantChallengeValidateRequest)
        ZS_DECLARE_CLASS_PTR(NamespaceGrantChallengeValidateResult)
        ZS_DECLARE_CLASS_PTR(PeerValidateRequest)
        ZS_DECLARE_CLASS_PTR(PeerValidateResult)
        ZS_DECLARE_CLASS_PTR(FoldersGetRequest)
        ZS_DECLARE_CLASS_PTR(FoldersGetResult)
        ZS_DECLARE_CLASS_PTR(FolderUpdateRequest)
        ZS_DECLARE_CLASS_PTR(FolderUpdateResult)
        ZS_DECLARE_CLASS_PTR(FolderGetRequest)
        ZS_DECLARE_CLASS_PTR(FolderGetResult)
        ZS_DECLARE_CLASS_PTR(MessagesDataGetRequest)
        ZS_DECLARE_CLASS_PTR(MessagesDataGetResult)
        ZS_DECLARE_CLASS_PTR(MessagesMetaDataGetRequest)
        ZS_DECLARE_CLASS_PTR(MessagesMetaDataGetResult)
        ZS_DECLARE_CLASS_PTR(MessageUpdateRequest)
        ZS_DECLARE_CLASS_PTR(MessageUpdateResult)
        ZS_DECLARE_CLASS_PTR(ListFetchRequest)
        ZS_DECLARE_CLASS_PTR(ListFetchResult)
        ZS_DECLARE_CLASS_PTR(ChangedNotify)
        ZS_DECLARE_CLASS_PTR(RegisterPushRequest)
        ZS_DECLARE_CLASS_PTR(RegisterPushResult)
      }

      namespace peer_salt
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPeerSalt)
        ZS_DECLARE_CLASS_PTR(SignedSaltGetRequest)
        ZS_DECLARE_CLASS_PTR(SignedSaltGetResult)

        typedef ElementPtr SignedSaltElementPtr;
        typedef std::list<SignedSaltElementPtr> SaltBundleList;
      }

      namespace peer_to_peer
      {
        ZS_DECLARE_CLASS_PTR(MessageFactoryPeerToPeer)
        ZS_DECLARE_CLASS_PTR(PeerIdentifyRequest)
        ZS_DECLARE_CLASS_PTR(PeerIdentifyResult)
        ZS_DECLARE_CLASS_PTR(PeerKeepAliveRequest)
        ZS_DECLARE_CLASS_PTR(PeerKeepAliveResult)
      }
    }
  }
}
