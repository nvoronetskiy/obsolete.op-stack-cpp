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

#include <openpeer/stack/message/internal/stack_message_messages.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { namespace message { ZS_IMPLEMENT_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using services::IHelper;
      using stack::internal::Helper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::Service
      #pragma mark

      //-----------------------------------------------------------------------
      bool Service::hasData() const
      {
        return ((mID.hasData()) ||
                (mType.hasData()) ||
                (mVersion.hasData()) ||
                (mMethods.size() > 0));
      }

      //-----------------------------------------------------------------------
      ElementPtr Service::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::Service");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "type", mType);
        IHelper::debugAppend(resultEl, "version", mVersion);
        IHelper::debugAppend(resultEl, "methods", mMethods.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      bool Service::Method::hasData() const
      {
        return ((mName.hasData()) ||
                (mURI.hasData()) ||
                (mUsername.hasData()) ||
                (mPassword.hasData()));
      }

      //-----------------------------------------------------------------------
      ElementPtr Service::Method::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::Service::Method");

        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "uri", mURI);
        IHelper::debugAppend(resultEl, "username", mUsername);
        IHelper::debugAppend(resultEl, "password", mPassword);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::Certificate
      #pragma mark

      //-----------------------------------------------------------------------
      bool Certificate::hasData() const
      {
        return (mID.hasData()) ||
               (mService.hasData()) ||
               (Time() != mExpires) ||
               ((bool)mPublicKey);
      }

      //-----------------------------------------------------------------------
      ElementPtr Certificate::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::Certificate");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, "service", mService);
        IHelper::debugAppend(resultEl, "expires", mExpires);
        IHelper::debugAppend(resultEl, "public key", (bool)mPublicKey);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::Finder
      #pragma mark

      //-----------------------------------------------------------------------
      bool Finder::hasData() const
      {
        return (mID.hasData()) ||
               (mProtocols.size() > 0) ||
               ((bool)mPublicKey) ||
               (mPriority != 0) ||
               (mWeight != 0) ||
               (mRegion.hasData()) ||
               (Time() != mCreated) ||
               (Time() != mExpires);
      }

      //-----------------------------------------------------------------------
      static ElementPtr getProtocolsDebugValueString(const Finder::ProtocolList &protocols)
      {
        if (protocols.size() < 1) return ElementPtr();

        ElementPtr resultEl = Element::create("Finder::ProtocolList");

        for (Finder::ProtocolList::const_iterator iter = protocols.begin(); iter != protocols.end(); ++iter)
        {
          ElementPtr protocolEl = Element::create("Finder::Protocol");
          const Finder::Protocol &protocol = (*iter);

          IHelper::debugAppend(protocolEl, "transport", protocol.mTransport);
          IHelper::debugAppend(protocolEl, "host", protocol.mHost);

          IHelper::debugAppend(resultEl, protocolEl);
        }

        return resultEl;
      }

      //-----------------------------------------------------------------------
      ElementPtr Finder::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::Finder");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, getProtocolsDebugValueString(mProtocols));
        IHelper::debugAppend(resultEl, "public key", (bool)mPublicKey);
        IHelper::debugAppend(resultEl, "priority", mPriority);
        IHelper::debugAppend(resultEl, "weight", mWeight);
        IHelper::debugAppend(resultEl, "region", mRegion);
        IHelper::debugAppend(resultEl, "created", mCreated);
        IHelper::debugAppend(resultEl, "expires", mExpires);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::IdentityInfo::Avatar
      #pragma mark

      //-----------------------------------------------------------------------
      bool IdentityInfo::Avatar::hasData() const
      {
        return ((mName.hasData()) ||
                (mURL.hasData()) ||
                (0 != mWidth) ||
                (0 != mHeight));
      }

      //-----------------------------------------------------------------------
      ElementPtr IdentityInfo::Avatar::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::IdentityInfo::Avatar");

        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "url", mURL);
        IHelper::debugAppend(resultEl, "width", mWidth);
        IHelper::debugAppend(resultEl, "height", mHeight);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::IdentityInfo
      #pragma mark

      //-----------------------------------------------------------------------
      const char *IdentityInfo::toString(Dispositions disposition)
      {
        switch (disposition)
        {
          case Disposition_NA:        return "";
          case Disposition_Update:    return "update";
          case Disposition_Remove:    return "remove";
        }
        return "";
      }

      //-----------------------------------------------------------------------
      IdentityInfo::Dispositions IdentityInfo::toDisposition(const char *inStr)
      {
        if (!inStr) return Disposition_NA;
        String str(inStr);
        if ("update" == str) return Disposition_Update;
        if ("remove" == str) return Disposition_Remove;
        return Disposition_NA;
      }
      
      //-----------------------------------------------------------------------
      bool IdentityInfo::hasData() const
      {
        return ((Disposition_NA != mDisposition) ||

                (mAccessToken.hasData()) ||
                (mAccessSecret.hasData()) ||
                (Time() != mAccessSecretExpires) ||
                (mAccessSecretProof.hasData()) ||
                (Time() != mAccessSecretProofExpires) ||

                (mReloginKey.hasData()) ||

                (mBase.hasData()) ||
                (mURI.hasData()) ||
                (mProvider.hasData()) ||

                (mStableID.hasData()) ||
                (mPeerFilePublic) ||

                (0 != mPriority) ||
                (0 != mWeight) ||

                (Time() != mCreated) ||
                (Time() != mUpdated) ||
                (Time() != mExpires) ||

                (mName.hasData()) ||
                (mProfile.hasData()) ||
                (mVProfile.hasData()) ||

                (mAvatars.size() > 0) ||

                (mContactProofBundle) ||
                (mIdentityProofBundle));
      }

      //-----------------------------------------------------------------------
      ElementPtr IdentityInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::IdentityInfo");

        IHelper::debugAppend(resultEl, "identity disposition", IdentityInfo::Disposition_NA != mDisposition ? String(toString(mDisposition)) : String());
        IHelper::debugAppend(resultEl, "identity access token", mAccessToken);
        IHelper::debugAppend(resultEl, "identity access secret", mAccessSecret);
        IHelper::debugAppend(resultEl, "identity access secret expires", mAccessSecretExpires);
        IHelper::debugAppend(resultEl, "identity access secret proof", mAccessSecretProof);
        IHelper::debugAppend(resultEl, "identity access secret expires", mAccessSecretProofExpires);
        IHelper::debugAppend(resultEl, "identity relogin key", mReloginKey);
        IHelper::debugAppend(resultEl, "identity base", mBase);
        IHelper::debugAppend(resultEl, "identity", mURI);
        IHelper::debugAppend(resultEl, "identity provider", mProvider);
        IHelper::debugAppend(resultEl, "identity stable ID", mStableID);
        IHelper::debugAppend(resultEl, IPeerFilePublic::toDebug(mPeerFilePublic));
        IHelper::debugAppend(resultEl, "priority", mPriority);
        IHelper::debugAppend(resultEl, "weight", mWeight);
        IHelper::debugAppend(resultEl, "created", mCreated);
        IHelper::debugAppend(resultEl, "updated", mUpdated);
        IHelper::debugAppend(resultEl, "expires", mExpires);
        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "profile", mProfile);
        IHelper::debugAppend(resultEl, "vprofile", mVProfile);
        IHelper::debugAppend(resultEl, "avatars", mAvatars.size());
        IHelper::debugAppend(resultEl, "identity contact proof", (bool)mContactProofBundle);
        IHelper::debugAppend(resultEl, "identity proof", (bool)mIdentityProofBundle);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      static void merge(String &result, const String &source, bool overwrite)
      {
        if (source.isEmpty()) return;
        if (result.hasData()) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(SecureByteBlockPtr &result, const SecureByteBlockPtr &source, bool overwrite)
      {
        if (!source) return;
        if (result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(Time &result, const Time &source, bool overwrite)
      {
        if (Time() == source) return;
        if (Time() != result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(WORD &result, WORD source, bool overwrite)
      {
        if (0 == source) return;
        if (0 != result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(bool &result, bool source, bool overwrite)
      {
        if (!source) return;
        if (result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(IPeerFilePublicPtr &result, const IPeerFilePublicPtr &source, bool overwrite)
      {
        if (!source) return;
        if (result) {
          if (!overwrite) return;
        }
        result = source;
      }
      
      //-----------------------------------------------------------------------
      static void merge(IdentityInfo::Dispositions &result, IdentityInfo::Dispositions source, bool overwrite)
      {
        if (IdentityInfo::Disposition_NA == source) return;
        if (IdentityInfo::Disposition_NA != result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(IdentityInfo::AvatarList &result, const IdentityInfo::AvatarList &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(ElementPtr &result, const ElementPtr &source, bool overwrite)
      {
        if (!source) return;
        if (result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      void IdentityInfo::mergeFrom(
                                   const IdentityInfo &source,
                                   bool overwriteExisting
                                   )
      {
        merge(mDisposition, source.mDisposition, overwriteExisting);

        merge(mAccessToken, source.mAccessToken, overwriteExisting);
        merge(mAccessSecret, source.mAccessSecret, overwriteExisting);
        merge(mAccessSecretExpires, source.mAccessSecretExpires, overwriteExisting);
        merge(mAccessSecretProof, source.mAccessSecretProof, overwriteExisting);
        merge(mAccessSecretProofExpires, source.mAccessSecretProofExpires, overwriteExisting);

        merge(mReloginKey, source.mReloginKey, overwriteExisting);

        merge(mBase, source.mBase, overwriteExisting);
        merge(mURI, source.mURI, overwriteExisting);
        merge(mProvider, source.mProvider, overwriteExisting);

        merge(mStableID, source.mStableID, overwriteExisting);
        merge(mPeerFilePublic, source.mPeerFilePublic, overwriteExisting);

        merge(mPriority, source.mPriority, overwriteExisting);
        merge(mWeight, source.mWeight, overwriteExisting);

        merge(mCreated, source.mCreated, overwriteExisting);
        merge(mUpdated, source.mUpdated, overwriteExisting);
        merge(mExpires, source.mExpires, overwriteExisting);

        merge(mName, source.mName, overwriteExisting);
        merge(mProfile, source.mProfile, overwriteExisting);
        merge(mVProfile, source.mVProfile, overwriteExisting);
        merge(mAvatars, source.mAvatars, overwriteExisting);

        merge(mContactProofBundle, source.mContactProofBundle, overwriteExisting);
        merge(mIdentityProofBundle, source.mIdentityProofBundle, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::LockboxInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool LockboxInfo::hasData() const
      {
        return ((mDomain.hasData()) ||
                (mAccountID.hasData()) ||

                (mAccessToken.hasData()) ||
                (mAccessSecret.hasData()) ||
                (Time() != mAccessSecretExpires) ||
                (mAccessSecretProof.hasData()) ||
                (Time() != mAccessSecretProofExpires) ||

                (mKey) ||
                (mHash.hasData()) ||

                (mResetFlag));
      }

      //-----------------------------------------------------------------------
      ElementPtr LockboxInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::LockboxInfo");

        IHelper::debugAppend(resultEl, "domain", mDomain);
        IHelper::debugAppend(resultEl, "account ID", mAccountID);
        IHelper::debugAppend(resultEl, "access token", mAccessToken);
        IHelper::debugAppend(resultEl, "access secret", mAccessSecret);
        IHelper::debugAppend(resultEl, "access secret expires", mAccessSecretExpires);
        IHelper::debugAppend(resultEl, "access secret proof", mAccessSecretProof);
        IHelper::debugAppend(resultEl, "access secret expires", mAccessSecretProofExpires);
        IHelper::debugAppend(resultEl, "key", mKey ? IHelper::convertToBase64(*mKey) : String());
        IHelper::debugAppend(resultEl, "hash", mHash);
        IHelper::debugAppend(resultEl, "reset", mResetFlag);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void LockboxInfo::mergeFrom(
                                  const LockboxInfo &source,
                                  bool overwriteExisting
                                  )
      {
        merge(mDomain, source.mDomain, overwriteExisting);
        merge(mAccountID, source.mAccountID, overwriteExisting);

        merge(mAccessToken, source.mAccessToken, overwriteExisting);
        merge(mAccessSecret, source.mAccessSecret, overwriteExisting);
        merge(mAccessSecretExpires, source.mAccessSecretExpires, overwriteExisting);
        merge(mAccessSecretProof, source.mAccessSecretProof, overwriteExisting);
        merge(mAccessSecretProofExpires, source.mAccessSecretProofExpires, overwriteExisting);

        merge(mKey, source.mKey, overwriteExisting);
        merge(mHash, source.mHash, overwriteExisting);
        merge(mResetFlag, source.mResetFlag, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::AgentInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool AgentInfo::hasData() const
      {
        return ((mUserAgent.hasData()) ||
                (mName.hasData()) ||
                (mImageURL.hasData()) ||
                (mAgentURL.hasData()));
      }

      //-----------------------------------------------------------------------
      ElementPtr AgentInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::AgentInfo");

        IHelper::debugAppend(resultEl, "user agent", mUserAgent);
        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "image url", mImageURL);
        IHelper::debugAppend(resultEl, "agent url", mAgentURL);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void AgentInfo::mergeFrom(
                                const AgentInfo &source,
                                bool overwriteExisting
                                )
      {
        merge(mUserAgent, source.mUserAgent, overwriteExisting);
        merge(mName, source.mName, overwriteExisting);
        merge(mImageURL, source.mImageURL, overwriteExisting);
        merge(mAgentURL, source.mAgentURL, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::NamespaceGrantChallengeInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool NamespaceGrantChallengeInfo::hasData() const
      {
        return ((mID.hasData()) ||
                (mName.hasData()) ||
                (mImageURL.hasData()) ||
                (mServiceURL.hasData()) ||
                (mDomains.hasData()));
      }

      //-----------------------------------------------------------------------
      ElementPtr NamespaceGrantChallengeInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::NamespaceGrantChallengeInfo");

        IHelper::debugAppend(resultEl, "grant challenge ID", mID);
        IHelper::debugAppend(resultEl, "service name", mName);
        IHelper::debugAppend(resultEl, "image url", mImageURL);
        IHelper::debugAppend(resultEl, "service url", mServiceURL);
        IHelper::debugAppend(resultEl, "domains", mDomains);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void NamespaceGrantChallengeInfo::mergeFrom(
                                                  const NamespaceGrantChallengeInfo &source,
                                                  bool overwriteExisting
                                                  )
      {
        merge(mID, source.mID, overwriteExisting);
        merge(mName, source.mName, overwriteExisting);
        merge(mImageURL, source.mImageURL, overwriteExisting);
        merge(mServiceURL, source.mServiceURL, overwriteExisting);
        merge(mDomains, source.mDomains, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::NamespaceInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool NamespaceInfo::hasData() const
      {
        return ((mURL.hasData()) ||
                (Time() != mLastUpdated));
      }

      //-----------------------------------------------------------------------
      ElementPtr NamespaceInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::NamespaceInfo");

        IHelper::debugAppend(resultEl, "namespace url", mURL);
        IHelper::debugAppend(resultEl, "last updated", mLastUpdated);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void NamespaceInfo::mergeFrom(
                                    const NamespaceInfo &source,
                                    bool overwriteExisting
                                    )
      {
        merge(mURL, source.mURL, overwriteExisting);
        merge(mLastUpdated, source.mLastUpdated, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::RolodexInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool RolodexInfo::hasData() const
      {
        return ((mServerToken.hasData()) ||

                (mAccessToken.hasData()) ||
                (mAccessSecret.hasData()) ||
                (Time() != mAccessSecretExpires) ||
                (mAccessSecretProof.hasData()) ||
                (Time() != mAccessSecretProofExpires) ||

                (mVersion.hasData()) ||
                (Time() != mUpdateNext) ||

                (mRefreshFlag));
      }

      //-----------------------------------------------------------------------
      ElementPtr RolodexInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::RolodexInfo");

        IHelper::debugAppend(resultEl, "server token", mServerToken);
        IHelper::debugAppend(resultEl, "access token", mAccessToken);
        IHelper::debugAppend(resultEl, "access secret", mAccessSecret);
        IHelper::debugAppend(resultEl, "access secret expires", mAccessSecretExpires);
        IHelper::debugAppend(resultEl, "access secret proof", mAccessSecretProof);
        IHelper::debugAppend(resultEl, "access secret expires", mAccessSecretProofExpires);
        IHelper::debugAppend(resultEl, "version", mVersion);
        IHelper::debugAppend(resultEl, "update next", mUpdateNext);
        IHelper::debugAppend(resultEl, "refresh", mRefreshFlag);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void RolodexInfo::mergeFrom(
                                  const RolodexInfo &source,
                                  bool overwriteExisting
                                  )
      {
        merge(mServerToken, source.mServerToken, overwriteExisting);

        merge(mAccessToken, source.mAccessToken, overwriteExisting);
        merge(mAccessSecret, source.mAccessSecret, overwriteExisting);
        merge(mAccessSecretExpires, source.mAccessSecretExpires, overwriteExisting);
        merge(mAccessSecretProof, source.mAccessSecretProof, overwriteExisting);
        merge(mAccessSecretProofExpires, source.mAccessSecretProofExpires, overwriteExisting);

        merge(mVersion, source.mVersion, overwriteExisting);
        merge(mUpdateNext, source.mUpdateNext, overwriteExisting);

        merge(mRefreshFlag, source.mRefreshFlag, overwriteExisting);
      }

    }
  }
}
