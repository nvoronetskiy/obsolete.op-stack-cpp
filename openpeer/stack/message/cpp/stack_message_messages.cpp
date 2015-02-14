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

#include <openpeer/stack/message/internal/stack_message_messages.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/IRSAPublicKey.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>
#include <zsLib/Numeric.h>

namespace openpeer { namespace stack { namespace message { ZS_IMPLEMENT_SUBSYSTEM(openpeer_stack_message) } } }
namespace openpeer { namespace stack { namespace message { ZS_DECLARE_SUBSYSTEM(openpeer_stack_message) } } }

namespace openpeer
{
  namespace stack
  {
    namespace message
    {
      using stack::internal::Helper;
      using zsLib::Numeric;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

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
      static void merge(DWORD &result, DWORD source, bool overwrite)
      {
        if (0 == source) return;
        if (0 != result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(ULONG &result, ULONG source, bool overwrite)
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
      static void merge(Log::Level &result, Log::Level source, bool overwrite)
      {
        if (Log::None == source) return;
        if (Log::None != result) {
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
      static void merge(PushMessageFolderInfo::Dispositions &result, PushMessageFolderInfo::Dispositions source, bool overwrite)
      {
        if (PushMessageFolderInfo::Disposition_NA == source) return;
        if (PushMessageFolderInfo::Disposition_NA != result) {
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
      static void merge(NamespaceInfoMap &result, const NamespaceInfoMap &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }

        for (NamespaceInfoMap::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          NamespaceInfoMap::iterator found = result.find(iter->first);
          if (found == result.end()) {
            result[iter->first] = iter->second;
          } else {
            (found->second).mergeFrom(iter->second, overwrite);
          }
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
      static void merge(std::list<String> &result, const std::list<String> &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(PushMessageInfo::PushInfoList &result, const PushMessageInfo::PushInfoList &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(PushMessageInfo::FlagInfoMap &result, const PushMessageInfo::FlagInfoMap &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(PushMessageInfo::FolderInfoList &result, const PushMessageInfo::FolderInfoList &source, bool overwrite)
      {
        if (source.size() < 1) return;
        if (result.size() > 0) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(PushMessageInfo::Dispositions &result, PushMessageInfo::Dispositions source, bool overwrite)
      {
        if (PushMessageInfo::Disposition_NA == source) return;
        if (PushMessageInfo::Disposition_NA != result) {
          if (!overwrite) return;
        }
        result = source;
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
                (mAgentURL.hasData()) ||
                (Log::None != mLogLevel));
      }

      //-----------------------------------------------------------------------
      ElementPtr AgentInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::AgentInfo");

        UseServicesHelper::debugAppend(resultEl, "user agent", mUserAgent);
        UseServicesHelper::debugAppend(resultEl, "name", mName);
        UseServicesHelper::debugAppend(resultEl, "image url", mImageURL);
        UseServicesHelper::debugAppend(resultEl, "agent url", mAgentURL);
        UseServicesHelper::debugAppend(resultEl, "log level", Log::toString(mLogLevel));

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
        merge(mLogLevel, source.mLogLevel, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      AgentInfo AgentInfo::create(ElementPtr elem)
      {
        AgentInfo info;

        if (!elem) return info;

        info.mUserAgent = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("userAgent"));
        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mImageURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("image"));
        info.mAgentURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));

        info.mLogLevel = Log::toLevel(IMessageHelper::getElementText(elem->findFirstChildElement("log")));

        return info;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr AgentInfo::createElement(bool forceLogLevelOutput) const
      {
        ElementPtr agentEl = Element::create("agent");

        if (mUserAgent.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("userAgent", mUserAgent));
        }
        if (mName.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", mName));
        }
        if (mImageURL.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("image", mImageURL));
        }
        if (mAgentURL.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("url", mAgentURL));
        }

        if ((Log::None != mLogLevel) ||
            (forceLogLevelOutput)) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("log", Log::toString(mLogLevel)));
        }

        return agentEl;
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

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "service", mService);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);
        UseServicesHelper::debugAppend(resultEl, "public key", (bool)mPublicKey);

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

        UseServicesHelper::debugAppend(resultEl, "name", mName);
        UseServicesHelper::debugAppend(resultEl, "url", mURL);
        UseServicesHelper::debugAppend(resultEl, "width", mWidth);
        UseServicesHelper::debugAppend(resultEl, "height", mHeight);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::IdentityInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_IdentityInfo(const char *message)
      {
        return Log::Params(message, "IdentityInfo");
      }

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

                (mToken.hasData()) ||

                (mReloginKey.hasData()) ||
                (mReloginKeyEncrypted.hasData()) ||

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

        UseServicesHelper::debugAppend(resultEl, "identity disposition", IdentityInfo::Disposition_NA != mDisposition ? String(toString(mDisposition)) : String());
        UseServicesHelper::debugAppend(resultEl, mToken.toDebug());
        UseServicesHelper::debugAppend(resultEl, "identity relogin key", mReloginKey);
        UseServicesHelper::debugAppend(resultEl, "identity relogin key encrypted", mReloginKeyEncrypted);
        UseServicesHelper::debugAppend(resultEl, "identity base", mBase);
        UseServicesHelper::debugAppend(resultEl, "identity", mURI);
        UseServicesHelper::debugAppend(resultEl, "identity provider", mProvider);
        UseServicesHelper::debugAppend(resultEl, "identity stable ID", mStableID);
        UseServicesHelper::debugAppend(resultEl, IPeerFilePublic::toDebug(mPeerFilePublic));
        UseServicesHelper::debugAppend(resultEl, "priority", mPriority);
        UseServicesHelper::debugAppend(resultEl, "weight", mWeight);
        UseServicesHelper::debugAppend(resultEl, "created", mCreated);
        UseServicesHelper::debugAppend(resultEl, "updated", mUpdated);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);
        UseServicesHelper::debugAppend(resultEl, "name", mName);
        UseServicesHelper::debugAppend(resultEl, "profile", mProfile);
        UseServicesHelper::debugAppend(resultEl, "vprofile", mVProfile);
        UseServicesHelper::debugAppend(resultEl, "avatars", mAvatars.size());
        UseServicesHelper::debugAppend(resultEl, "identity contact proof", (bool)mContactProofBundle);
        UseServicesHelper::debugAppend(resultEl, "identity proof", (bool)mIdentityProofBundle);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void IdentityInfo::mergeFrom(
                                   const IdentityInfo &source,
                                   bool overwriteExisting
                                   )
      {
        merge(mDisposition, source.mDisposition, overwriteExisting);

        mToken.mergeFrom(source.mToken, overwriteExisting);

        merge(mReloginKey, source.mReloginKey, overwriteExisting);
        merge(mReloginKeyEncrypted, source.mReloginKeyEncrypted, overwriteExisting);

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

      //---------------------------------------------------------------------
      IdentityInfo IdentityInfo::create(ElementPtr elem)
      {
        IdentityInfo info;

        if (!elem) return info;

        info.mDisposition = IdentityInfo::toDisposition(elem->getAttributeValue("disposition"));

        info.mToken = Token::create(elem->findFirstChildElement("token"));

        info.mReloginKey = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reloginKey"));
        info.mReloginKeyEncrypted = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reloginKeyEncrypted"));

        info.mBase = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("base"));
        info.mURI = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("uri"));
        info.mProvider = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("provider"));

        info.mStableID = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("stableID"));
        ElementPtr peerEl = elem->findFirstChildElement("peer");
        if (peerEl) {
          info.mPeerFilePublic = IPeerFilePublic::loadFromElement(peerEl);
        }

        String priorityStr = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("priority"));
        try {
          info.mPriority = Numeric<decltype(info.mPriority)>(priorityStr);
        } catch(Numeric<decltype(info.mPriority)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_IdentityInfo("failed to convert") + ZS_PARAMIZE(priorityStr))
        }

        String weightStr = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("weight"));
        try {
          info.mWeight = Numeric<decltype(info.mWeight)>(weightStr);
        } catch(Numeric<decltype(info.mWeight)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_IdentityInfo("failed to convert") + ZS_PARAMIZE(weightStr))
        }

        info.mUpdated = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("created")));
        info.mUpdated = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));
        info.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("expires")));

        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mProfile = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("profile"));
        info.mVProfile = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("vprofile"));

        ElementPtr avatarsEl = elem->findFirstChildElement("avatars");
        if (avatarsEl) {
          ElementPtr avatarEl = avatarsEl->findFirstChildElement("avatar");
          while (avatarEl) {
            IdentityInfo::Avatar avatar;
            avatar.mName = IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("name"));
            avatar.mURL = IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("url"));

            String widthStr = IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("width"));
            try {
              avatar.mWidth = Numeric<decltype(avatar.mWidth)>(widthStr);
            } catch(Numeric<decltype(avatar.mWidth)>::ValueOutOfRange &) {
              ZS_LOG_WARNING(Detail, slog_IdentityInfo("failed to convert") + ZS_PARAMIZE(widthStr))
            }

            String heightStr = IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("height"));
            try {
              avatar.mHeight = Numeric<decltype(avatar.mHeight)>(heightStr);
            } catch(Numeric<decltype(avatar.mHeight)>::ValueOutOfRange &) {
              ZS_LOG_WARNING(Detail, slog_IdentityInfo("failed to convert") + ZS_PARAMIZE(heightStr))
            }

            if (avatar.hasData()) {
              info.mAvatars.push_back(avatar);
            }
            avatarEl = avatarEl->findNextSiblingElement("avatar");
          }
        }

        ElementPtr contactProofBundleEl = elem->findFirstChildElement("contactProofBundle");
        if (contactProofBundleEl) {
          info.mContactProofBundle = contactProofBundleEl->clone()->toElement();
        }
        ElementPtr identityProofBundleEl = elem->findFirstChildElement("identityProofBundle");
        if (contactProofBundleEl) {
          info.mIdentityProofBundle = identityProofBundleEl->clone()->toElement();
        }

        return info;
      }

      //---------------------------------------------------------------------
      ElementPtr IdentityInfo::createElement(bool forcePriorityWeightOutput) const
      {
        ElementPtr identityEl = Element::create("identity");
        if (Disposition_NA != mDisposition) {
          identityEl->setAttribute("disposition", IdentityInfo::toString(mDisposition));
        }

        if (mToken.hasData()) {
          identityEl->adoptAsLastChild(mToken.createElement());
        }

        if (!mReloginKey.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("reloginKey", mReloginKey));
        }

        if (!mBase.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("base", mBase));
        }
        if (!mURI.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("uri", mURI));
        }
        if (!mProvider.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("provider", mProvider));
        }

        if (!mStableID.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("stableID", mStableID));
        }

        if (mPeerFilePublic) {
          identityEl->adoptAsLastChild(mPeerFilePublic->saveToElement());
        }

        if ((0 != mPriority) ||
            (forcePriorityWeightOutput)) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("priority", string(mPriority)));
        }
        if ((0 != mWeight) ||
            (forcePriorityWeightOutput)) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("weight", string(mWeight)));
        }

        if (Time() != mCreated) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("created", UseServicesHelper::timeToString(mCreated)));
        }
        if (Time() != mUpdated) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updated", UseServicesHelper::timeToString(mUpdated)));
        }
        if (Time() != mExpires) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
        }

        if (!mName.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", mName));
        }
        if (!mProfile.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("profile", mProfile));
        }
        if (!mVProfile.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("vprofile", mVProfile));
        }

        if (mAvatars.size() > 0) {
          ElementPtr avatarsEl = Element::create("avatars");
          for (IdentityInfo::AvatarList::const_iterator iter = mAvatars.begin(); iter != mAvatars.end(); ++iter)
          {
            const IdentityInfo::Avatar &avatar = (*iter);
            ElementPtr avatarEl = Element::create("avatar");

            if (!avatar.mName.isEmpty()) {
              avatarEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", avatar.mName));
            }
            if (!avatar.mURL.isEmpty()) {
              avatarEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", avatar.mURL));
            }
            if (0 != avatar.mWidth) {
              avatarEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("width", string(avatar.mWidth)));
            }
            if (0 != avatar.mHeight) {
              avatarEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("height", string(avatar.mHeight)));
            }

            if (avatarEl->hasChildren()) {
              avatarsEl->adoptAsLastChild(avatarEl);
            }
          }
          if (avatarsEl->hasChildren()) {
            identityEl->adoptAsLastChild(avatarsEl);
          }
        }

        if (mContactProofBundle) {
          identityEl->adoptAsLastChild(mContactProofBundle->clone()->toElement());
        }
        if (mIdentityProofBundle) {
          identityEl->adoptAsLastChild(mIdentityProofBundle->clone()->toElement());
        }

        return identityEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::LockboxInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_LockboxInfo(const char *message)
      {
        return Log::Params(message, "LockboxInfo");
      }

      //-----------------------------------------------------------------------
      bool LockboxInfo::hasData() const
      {
        return ((mDomain.hasData()) ||
                (mAccountID.hasData()) ||

                (mKeyName.hasData()) ||
                (mKeyEncrypted.hasData()) ||
                (mKeyHash.hasData()) ||

                (mKey) ||

                (mToken.hasData()) ||

                (mResetFlag));
      }

      //-----------------------------------------------------------------------
      ElementPtr LockboxInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::LockboxInfo");

        UseServicesHelper::debugAppend(resultEl, "domain", mDomain);
        UseServicesHelper::debugAppend(resultEl, "account ID", mAccountID);

        UseServicesHelper::debugAppend(resultEl, "key name", mKeyName);
        UseServicesHelper::debugAppend(resultEl, "key encrypted", mKeyEncrypted);
        UseServicesHelper::debugAppend(resultEl, "key hash", mKeyHash);
        UseServicesHelper::debugAppend(resultEl, "key", mKey ? UseServicesHelper::convertToBase64(*mKey) : String());

        UseServicesHelper::debugAppend(resultEl, mToken.toDebug());

        UseServicesHelper::debugAppend(resultEl, "reset", mResetFlag);

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

        merge(mKeyName, source.mKeyName, overwriteExisting);
        merge(mKeyEncrypted, source.mKeyEncrypted, overwriteExisting);
        merge(mKeyHash, source.mKeyHash, overwriteExisting);
        merge(mKey, source.mKey, overwriteExisting);

        mToken.mergeFrom(source.mToken);

        merge(mResetFlag, source.mResetFlag, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      ElementPtr LockboxInfo::createElement() const
      {
        ElementPtr lockboxEl = Element::create("lockbox");

        if (mDomain.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("domain", mDomain));
        }
        if (mAccountID.hasData()) {
          lockboxEl->setAttribute("id", mAccountID);
        }

        if (mKeyName.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("keyName", mKeyName));
        }
        if (mKeyEncrypted.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("keyEncrypted", mKeyEncrypted));
        }
        if (mKeyHash.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("keyHash", mKeyHash));
        }
        if (mToken.hasData()) {
          lockboxEl->adoptAsLastChild(mToken.createElement());
        }

        if (mResetFlag) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("reset", "true"));
        }

        return lockboxEl;
      }

      //-----------------------------------------------------------------------
      LockboxInfo LockboxInfo::create(ElementPtr elem)
      {
        LockboxInfo info;

        if (!elem) return info;

        info.mDomain = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("domain"));
        info.mAccountID = IMessageHelper::getAttributeID(elem);

        info.mKeyName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("keyName"));
        info.mKeyEncrypted = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("keyEncrypted"));
        info.mKeyHash = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("keyHash"));
        info.mToken = Token::create(elem->findFirstChildElement("token"));

        String resetStr = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reset"));
        try {
          info.mResetFlag = Numeric<decltype(info.mResetFlag)>();
        } catch(Numeric<decltype(info.mResetFlag)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_LockboxInfo("failed to convert") + ZS_PARAMIZE(resetStr))
        }

        return info;
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::PushMessageInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_PushMessageInfo(const char *message)
      {
        return Log::Params(message, "PushMessageInfo");
      }

      //-----------------------------------------------------------------------
      PushMessageInfo::Dispositions PushMessageInfo::toDisposition(const char *disposition)
      {
        if (!disposition) return Disposition_NA;

        if ('\0' == *disposition) return Disposition_NA;

        if (0 == strcmp(disposition, "update")) return Disposition_Update;
        if (0 == strcmp(disposition, "remove")) return Disposition_Remove;

        return Disposition_NA;
      }

      //-----------------------------------------------------------------------
      const char *PushMessageInfo::toString(Dispositions disposition)
      {
        switch (disposition) {
          case Disposition_NA:          return "";

          case Disposition_Update:      return "update";
          case Disposition_Remove:      return "remove";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      bool PushMessageInfo::hasData() const
      {
        return ((mID.hasData()) ||

                (mDisposition == Disposition_NA) ||

                (mVersion.hasData()) ||

                (0 != mChannelID) ||

                (mTo.hasData()) ||
                (mCC.hasData()) ||
                (mBCC.hasData()) ||

                (mFrom.hasData()) ||

                (mType.hasData()) ||
                (mMimeType.hasData()) ||
                (mEncoding.hasData()) ||

                ((bool)mMetaData) ||

                (mPushType.hasData()) ||
                (mPushInfos.size() > 0) ||

                (Time() != mSent) ||
                (Time() != mExpires) ||

                (0 != mLength) ||
                (0 != mRemaining) ||

                (mFolders.size() > 0) ||

                (mFlags.size() > 0));
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessageInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::PushMessageInfo");

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "disposition", toString(mDisposition));

        UseServicesHelper::debugAppend(resultEl, "version", mVersion);

        UseServicesHelper::debugAppend(resultEl, "channel ID", mChannelID);

        UseServicesHelper::debugAppend(resultEl, "to", mTo);
        UseServicesHelper::debugAppend(resultEl, "cc", mCC);
        UseServicesHelper::debugAppend(resultEl, "bcc", mBCC);

        UseServicesHelper::debugAppend(resultEl, "from", mFrom);

        UseServicesHelper::debugAppend(resultEl, "type", mType);
        UseServicesHelper::debugAppend(resultEl, "mimeType", mMimeType);
        UseServicesHelper::debugAppend(resultEl, "encoding", mEncoding);

        UseServicesHelper::debugAppend(resultEl, "meta data", mMetaData);

        UseServicesHelper::debugAppend(resultEl, "push type", mPushType);
        UseServicesHelper::debugAppend(resultEl, "push infos", mPushInfos.size());

        UseServicesHelper::debugAppend(resultEl, "sent", mSent);
        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        UseServicesHelper::debugAppend(resultEl, "length", mLength);
        UseServicesHelper::debugAppend(resultEl, "length", mRemaining);

        UseServicesHelper::debugAppend(resultEl, "folders", mFolders.size());

        UseServicesHelper::debugAppend(resultEl, "flags", mFlags.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushMessageInfo::mergeFrom(
                                      const PushMessageInfo &source,
                                      bool overwriteExisting
                                      )
      {
        merge(mID, source.mID, overwriteExisting);
        merge(mDisposition, source.mDisposition, overwriteExisting);

        merge(mVersion, source.mVersion, overwriteExisting);

        merge(mChannelID, source.mChannelID, overwriteExisting);

        merge(mTo, source.mTo, overwriteExisting);
        merge(mCC, source.mCC, overwriteExisting);
        merge(mBCC, source.mBCC, overwriteExisting);

        merge(mFrom, source.mFrom, overwriteExisting);

        merge(mType, source.mType, overwriteExisting);
        merge(mMimeType, source.mMimeType, overwriteExisting);
        merge(mEncoding, source.mEncoding, overwriteExisting);

        merge(mMetaData, source.mMetaData, overwriteExisting);

        merge(mPushType, source.mPushType, overwriteExisting);
        merge(mPushInfos, source.mPushInfos, overwriteExisting);

        merge(mSent, source.mSent, overwriteExisting);
        merge(mExpires, source.mExpires, overwriteExisting);

        merge(mLength, source.mLength, overwriteExisting);
        merge(mRemaining, source.mRemaining, overwriteExisting);

        merge(mFolders, source.mFolders, overwriteExisting);

        merge(mFlags, source.mFlags, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessageInfo::createElement() const
      {
        ElementPtr messageEl = Element::create("message");

        if (mID.hasData()) {
          messageEl->setAttribute("id", mID);
        }

        if (Disposition_NA != mDisposition) {
          messageEl->setAttribute("disposition", toString(mDisposition));
        }

        if (mVersion.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("version", mVersion));
        }

        if (0 != mChannelID) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("channel", mID));
        }

        if (mTo.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("to", mTo));
        }
        if (mCC.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("cc", mCC));
        }
        if (mBCC.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("bcc", mBCC));
        }

        if (mFrom.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("from", mFrom));
        }

        if (mType.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("type", mType));
        }

        if (mMimeType.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("mimeType", mMimeType));
        }

        if (mEncoding.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("encoding", mEncoding));
        }

        if (mMetaData.hasData()) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("metaData", mMetaData));
        }

        if ((mPushType.hasData()) ||
            (mPushInfos.size() > 0)) {
          ElementPtr pushEl = Element::create("push");

          if (mPushType.hasData()) {
            pushEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("type", mPushType));
          }

          ElementPtr servicesEl = Element::create("services");

          for (PushInfoList::const_iterator iter = mPushInfos.begin(); iter != mPushInfos.end(); ++iter) {
            ElementPtr serviceEl = Element::create("service");

            const PushInfo &pushInfo = (*iter);

            if (pushInfo.mServiceType.hasData()) {
              serviceEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("type", pushInfo.mServiceType));
            }

            if ((bool)pushInfo.mValues) {
              ElementPtr valuesEl = pushInfo.mValues->clone()->toElement();
              if ("values" != valuesEl->getValue()) {
                valuesEl->setValue("values");
              }

              if (valuesEl->hasChildren()) {
                serviceEl->adoptAsLastChild(valuesEl);
              }
            }

            if ((bool)pushInfo.mCustom) {
              ElementPtr customEl = pushInfo.mCustom->clone()->toElement();
              if ("custom" != customEl->getValue()) {
                customEl->setValue("custom");
              }
              if (customEl->hasChildren()) {
                serviceEl->adoptAsLastChild(customEl);
              }
            }

            if (serviceEl->hasChildren()) {
              servicesEl->adoptAsLastChild(serviceEl);
            }
          }

          if (servicesEl->hasChildren()) {
            pushEl->adoptAsLastChild(servicesEl);
          }

        }

        if (Time() != mSent) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("sent", UseServicesHelper::timeToString(mSent)));
        }

        if (Time() != mExpires) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
        }

        if (0 != mLength) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("length", string(mLength)));
        }
        if (0 != mRemaining) {
          messageEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("remaining", string(mRemaining)));
        }

        if (mFolders.size() > 0) {
          ElementPtr foldersEl = Element::create("folders");

          for (PushMessageInfo::FolderInfoList::const_iterator iter = mFolders.begin(); iter != mFolders.end(); ++iter) {
            const FolderInfo &info = (*iter);

            ElementPtr folderEl = Element::create("folder");

            if (FolderInfo::Disposition_NA != info.mDisposition) {
              folderEl->setAttribute("disposition", FolderInfo::toString(info.mDisposition));
            }

            if (FolderInfo::Where_NA != info.mWhere) {
              folderEl->setAttribute("where", FolderInfo::toString(info.mWhere));
            }

            if (info.mName.hasData()) {
              folderEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", info.mName));
            }

            if ((folderEl->hasChildren()) ||
                (folderEl->hasAttributes())) {
              foldersEl->adoptAsLastChild(folderEl);
            }
          }

          if (foldersEl->hasChildren()) {
            messageEl->adoptAsLastChild(foldersEl);
          }
        }

        if (mFlags.size() > 0) {
          ElementPtr flagsEl = Element::create("flags");

          for (PushMessageInfo::FlagInfoMap::const_iterator iter = mFlags.begin(); iter != mFlags.end(); ++iter) {
            const FlagInfo &info = (*iter).second;

            ElementPtr flagEl = (FlagInfo::Flag_NA != info.mFlag ? IMessageHelper::createElementWithText("flag", FlagInfo::toString(info.mFlag)) : Element::create("flag"));

            if (FlagInfo::Disposition_NA != info.mDisposition) {
              flagEl->setAttribute("disposition", FlagInfo::toString(info.mDisposition));
            }

            ElementPtr detailsEl = Element::create("details");

            if (info.mFlagURIInfos.size() > 0) {
              for (PushMessageInfo::FlagInfo::URIInfoList::const_iterator uriIter = info.mFlagURIInfos.begin(); uriIter != info.mFlagURIInfos.end(); ++uriIter) {

                ElementPtr detailEl = Element::create("detail");

                const FlagInfo::URIInfo &uriInfo = (*uriIter);

                if (uriInfo.mURI.hasData()) {
                  detailEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("to", uriInfo.mURI));
                }

                if ((0 != uriInfo.mErrorCode) ||
                    (uriInfo.mErrorReason.hasData())) {
                  ElementPtr errorEl = uriInfo.mErrorReason.hasData() ? IMessageHelper::createElementWithTextAndJSONEncode("error", uriInfo.mErrorReason) : Element::create("error");

                  if (0 != uriInfo.mErrorCode) {
                    errorEl->setAttribute("id", string(uriInfo.mErrorCode), false);
                  }

                  detailEl->adoptAsLastChild(errorEl);
                }

                if (detailEl->hasChildren()) {
                  detailsEl->adoptAsLastChild(detailEl);
                }
              }
            }

            if (detailsEl->hasChildren()) {
              flagEl->adoptAsLastChild(detailsEl);
            }

            if ((flagEl->hasChildren()) ||
                (flagEl->hasAttributes())) {
              flagsEl->adoptAsLastChild(flagEl);
            }
          }

          if (flagsEl->hasChildren()) {
            messageEl->adoptAsLastChild(flagsEl);
          }
        }

        return messageEl;
      }

      //-----------------------------------------------------------------------
      PushMessageInfo PushMessageInfo::create(ElementPtr elem)
      {
        PushMessageInfo info;

        if (!elem) return info;

        info.mID = IMessageHelper::getAttributeID(elem);
        info.mDisposition = toDisposition(IMessageHelper::getAttribute(elem, "disposition"));

        info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));

        String channelIDStr = IMessageHelper::getElementText(elem->findFirstChildElement("channel"));
        try {
          info.mChannelID = Numeric<decltype(info.mChannelID)>(channelIDStr);
        } catch(Numeric<decltype(info.mChannelID)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushMessageInfo("failed to convert") + ZS_PARAMIZE(channelIDStr))
        }

        info.mTo = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("to"));
        info.mCC = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("cc"));
        info.mBCC = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("bcc"));

        info.mFrom = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("from"));

        info.mType = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("type"));
        info.mMimeType = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("mimeType"));
        info.mEncoding = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("encoding"));

        info.mMetaData = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("metaData"));


        ElementPtr pushEl = elem->findFirstChildElement("push");
        if (pushEl) {
          info.mPushType = IMessageHelper::getElementTextAndDecode(pushEl->findFirstChildElement("type"));

          ElementPtr servicesEl = pushEl->findFirstChildElement("services");
          if (servicesEl) {
            ElementPtr serviceEl = servicesEl->findFirstChildElement("service");

            while (serviceEl) {
              PushInfo pushInfo;

              pushInfo.mServiceType = IMessageHelper::getElementTextAndDecode(serviceEl->findFirstChildElement("type"));

              ElementPtr valuesEl = serviceEl->findFirstChildElement("values");
              if (valuesEl) {
                pushInfo.mValues = valuesEl->clone()->toElement();
              }

              ElementPtr customEl = serviceEl->findFirstChildElement("custom");
              if (customEl) {
                pushInfo.mCustom = customEl->clone()->toElement();
              }

              if (pushInfo.hasData()) {
                info.mPushInfos.push_back(pushInfo);
              }

              serviceEl = serviceEl->findNextSiblingElement("service");
            }
          }
        }

        info.mSent = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("sent")));
        info.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("expires")));

        String lengthStr = IMessageHelper::getElementText(elem->findFirstChildElement("length"));
        try {
          info.mLength = Numeric<decltype(info.mLength)>();
        } catch(Numeric<decltype(info.mLength)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushMessageInfo("failed to convert") + ZS_PARAMIZE(lengthStr))
        }

        String remainingStr = IMessageHelper::getElementText(elem->findFirstChildElement("remaining"));
        try {
          info.mRemaining = Numeric<decltype(info.mRemaining)>();
        } catch(Numeric<decltype(info.mRemaining)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushMessageInfo("failed to convert") + ZS_PARAMIZE(remainingStr))
        }

        ElementPtr foldersEl = elem->findFirstChildElement("folders");
        if (foldersEl) {
          ElementPtr folderEl = foldersEl->findFirstChildElement("folder");

          while (folderEl) {
            FolderInfo folderInfo;

            folderInfo.mDisposition = FolderInfo::toDisposition(IMessageHelper::getAttribute(folderEl, "disposition"));
            folderInfo.mWhere = FolderInfo::toWhere(IMessageHelper::getAttribute(folderEl, "where"));
            folderInfo.mName = IMessageHelper::getElementTextAndDecode(folderEl->findFirstChildElement("name"));

            if (folderInfo.hasData()) {
              info.mFolders.push_back(folderInfo);
            }

            folderEl = folderEl->findNextSiblingElement("folder");
          }
        }

        ElementPtr flagsEl = elem->findFirstChildElement("flags");
        if (flagsEl) {
          ElementPtr flagEl = flagsEl->findFirstChildElement("flag");

          while (flagEl) {
            FlagInfo flagInfo;

            flagInfo.mDisposition = FlagInfo::toDisposition(IMessageHelper::getAttribute(flagEl, "disposition"));
            flagInfo.mFlag = FlagInfo::toFlag(flagEl->getText(true, false));

            ElementPtr detailsEl = flagsEl->findFirstChildElement("details");
            if (detailsEl) {
              ElementPtr detailEl = detailsEl->findFirstChildElement("detail");
              while (detailEl) {

                FlagInfo::URIInfo uriInfo;

                uriInfo.mURI = IMessageHelper::getElementTextAndDecode(detailEl->findFirstChildElement("to"));

                ElementPtr errorEl = detailEl->findFirstChildElement("error");

                if (errorEl) {
                  String errorCodeStr = IMessageHelper::getAttributeID(errorEl);
                  try {
                    uriInfo.mErrorCode = Numeric<decltype(uriInfo.mErrorCode)>(errorCodeStr);
                  } catch(Numeric<decltype(uriInfo.mErrorCode)>::ValueOutOfRange &) {
                    ZS_LOG_WARNING(Detail, slog_PushMessageInfo("failed to convert") + ZS_PARAMIZE(errorCodeStr))
                  }
                  uriInfo.mErrorReason = IMessageHelper::getElementTextAndDecode(errorEl);
                }

                if (uriInfo.hasData()) {
                  flagInfo.mFlagURIInfos.push_back(uriInfo);
                }

                detailEl = detailEl->findNextSiblingElement("detail");
              }
            }

            if ((FlagInfo::Flag_NA != flagInfo.mFlag) &&
                (flagInfo.hasData())) {
              info.mFlags[flagInfo.mFlag] = flagInfo;
            }

            flagEl = flagEl->findNextSiblingElement("flag");
          }
        }

        return info;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::PushMessageInfo::FlagInfo
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessageInfo::FlagInfo::Dispositions PushMessageInfo::FlagInfo::toDisposition(const char *disposition)
      {
        if (!disposition) return Disposition_NA;

        if ('\0' == *disposition) return Disposition_NA;

        if (0 == strcmp(disposition, "subscribe")) return Disposition_Subscribe;
        if (0 == strcmp(disposition, "update")) return Disposition_Update;
        if (0 == strcmp(disposition, "remove")) return Disposition_Remove;

        return Disposition_NA;
      }

      //-----------------------------------------------------------------------
      const char *PushMessageInfo::FlagInfo::toString(Dispositions disposition)
      {
        switch (disposition) {
          case Disposition_NA:          return "";

          case Disposition_Subscribe:   return "subscribe";
          case Disposition_Update:      return "update";
          case Disposition_Remove:      return "remove";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      PushMessageInfo::FlagInfo::Flags PushMessageInfo::FlagInfo::toFlag(const char *flagName)
      {
        if (!flagName) return Flag_NA;

        if ('\0' == *flagName) return Flag_NA;

        if (0 == strcmp(flagName, "read")) return Flag_Read;
        if (0 == strcmp(flagName, "answered")) return Flag_Answered;
        if (0 == strcmp(flagName, "flagged")) return Flag_Flagged;
        if (0 == strcmp(flagName, "deleted")) return Flag_Deleted;
        if (0 == strcmp(flagName, "draft")) return Flag_Draft;
        if (0 == strcmp(flagName, "recent")) return Flag_Recent;
        if (0 == strcmp(flagName, "delivered")) return Flag_Delivered;
        if (0 == strcmp(flagName, "sent")) return Flag_Sent;
        if (0 == strcmp(flagName, "pushed")) return Flag_Pushed;
        if (0 == strcmp(flagName, "error")) return Flag_Error;

        return Flag_NA;
      }

      //-----------------------------------------------------------------------
      const char *PushMessageInfo::FlagInfo::toString(Flags flag)
      {
        switch (flag) {
          case Flag_NA:         return "";

          case Flag_Read:       return "read";
          case Flag_Answered:   return "answered";
          case Flag_Flagged:    return "flagged";
          case Flag_Deleted:    return "deleted";
          case Flag_Draft:      return "draft";
          case Flag_Recent:     return "recent";
          case Flag_Delivered:  return "delivered";
          case Flag_Sent:       return "sent";
          case Flag_Pushed:     return "pushed";
          case Flag_Error:      return "error";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      bool PushMessageInfo::FlagInfo::hasData() const
      {
        return ((Disposition_NA != mDisposition) ||

                (Flag_NA != mFlag) ||

                (mFlagURIInfos.size() > 0));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::PushMessageInfo::FlagInfo::URIInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool PushMessageInfo::FlagInfo::URIInfo::hasData() const
      {
        return ((mURI.hasData()) ||

                (0 != mErrorCode) ||

                (mErrorReason.hasData()));
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::MessageInfo::FolderInfo
      #pragma mark

      //-----------------------------------------------------------------------
      PushMessageInfo::FolderInfo::Dispositions PushMessageInfo::FolderInfo::toDisposition(const char *disposition)
      {
        if (!disposition) return Disposition_NA;

        if ('\0' == *disposition) return Disposition_NA;

        if (0 == strcmp(disposition, "update")) return Disposition_Update;
        if (0 == strcmp(disposition, "remove")) return Disposition_Remove;

        return Disposition_NA;
      }

      //-----------------------------------------------------------------------
      const char *PushMessageInfo::FolderInfo::toString(Dispositions disposition)
      {
        switch (disposition) {
          case Disposition_NA:          return "";

          case Disposition_Update:      return "update";
          case Disposition_Remove:      return "remove";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      PushMessageInfo::FolderInfo::Where PushMessageInfo::FolderInfo::toWhere(const char *where)
      {
        if (!where) return Where_NA;

        if ('\0' == *where) return Where_NA;

        if (0 == strcmp(where, "local")) return Where_Local;
        if (0 == strcmp(where, "remote")) return Where_Remote;

        return Where_NA;
      }

      //-----------------------------------------------------------------------
      const char *PushMessageInfo::FolderInfo::toString(Where where)
      {
        switch (where) {
          case Where_NA:          return "";

          case Where_Local:       return "local";
          case Where_Remote:      return "remote";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      bool PushMessageInfo::FolderInfo::hasData() const
      {
        return ((Disposition_NA != mDisposition) ||

                (Where_NA != mWhere) ||

                (mName.hasData()));
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::MessageInfo::PushInfo
      #pragma mark

      //-----------------------------------------------------------------------
      bool PushMessageInfo::PushInfo::hasData() const
      {
        return ((mServiceType.hasData()) ||

                ((bool)mValues) ||
                ((bool)mCustom));
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessageInfo::PushInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::PushMessageInfo::PushInfo");

        UseServicesHelper::debugAppend(resultEl, "type", mServiceType);
        UseServicesHelper::debugAppend(resultEl, "mValues", (bool)mValues);
        UseServicesHelper::debugAppend(resultEl, "custom", (bool)mCustom);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushMessageInfo::PushInfo::mergeFrom(
                                                const PushInfo &source,
                                                bool overwriteExisting
                                                )
      {
        merge(mServiceType, source.mServiceType, overwriteExisting);
        merge(mValues, source.mValues, overwriteExisting);
        merge(mCustom, source.mCustom, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::PushMessageFolderInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_PushMessageFolderInfo(const char *message)
      {
        return Log::Params(message, "PushMessageFolderInfo");
      }

      //-----------------------------------------------------------------------
      const char *PushMessageFolderInfo::toString(Dispositions disposition)
      {
        switch (disposition)
        {
          case Disposition_NA:        return "";
          case Disposition_Update:    return "update";
          case Disposition_Remove:    return "remove";
          case Disposition_Reset:     return "reset";
        }
        return "";
      }

      //-----------------------------------------------------------------------
      PushMessageFolderInfo::Dispositions PushMessageFolderInfo::toDisposition(const char *inStr)
      {
        if (!inStr) return Disposition_NA;
        String str(inStr);
        if ("update" == str) return Disposition_Update;
        if ("remove" == str) return Disposition_Remove;
        if ("reset" == str) return Disposition_Reset;
        return Disposition_NA;
      }

      //-----------------------------------------------------------------------
      bool PushMessageFolderInfo::hasData() const
      {
        return (mDisposition != Disposition_NA) ||
               (mName.hasData()) ||
               (mRenamed.hasData()) ||
               (mVersion.hasData()) ||
               (mUnread != 0) ||
               (mTotal != 0) ||
               (Time() != mUpdateNext);
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessageFolderInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::PushMessageFolderInfo");

        UseServicesHelper::debugAppend(resultEl, "disposition", mDisposition != Disposition_NA ? String(toString(mDisposition)) : String());
        UseServicesHelper::debugAppend(resultEl, "name", mName);
        UseServicesHelper::debugAppend(resultEl, "rename", mRenamed);
        UseServicesHelper::debugAppend(resultEl, "version", mVersion);
        UseServicesHelper::debugAppend(resultEl, "unread", mUnread);
        UseServicesHelper::debugAppend(resultEl, "total", mTotal);
        UseServicesHelper::debugAppend(resultEl, "update next", mUpdateNext);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushMessageFolderInfo::mergeFrom(
                                            const PushMessageFolderInfo &source,
                                            bool overwriteExisting
                                            )
      {
        merge(mDisposition, source.mDisposition, overwriteExisting);

        merge(mName, source.mName, overwriteExisting);
        merge(mRenamed, source.mRenamed, overwriteExisting);
        merge(mVersion, source.mVersion, overwriteExisting);

        merge(mUnread, source.mUnread, overwriteExisting);
        merge(mTotal, source.mTotal, overwriteExisting);

        merge(mUpdateNext, source.mUpdateNext, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      PushMessageFolderInfo PushMessageFolderInfo::create(ElementPtr elem)
      {
        PushMessageFolderInfo info;

        if (!elem) return info;

        info.mDisposition = toDisposition(IMessageHelper::getAttribute(elem, "disposition"));
        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mRenamed = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("rename"));
        info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));

        String unreadStr = IMessageHelper::getElementText(elem->findFirstChildElement("unread"));
        try {
          info.mUnread = Numeric<decltype(info.mUnread)>(unreadStr);
        } catch (Numeric<decltype(info.mUnread)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushMessageFolderInfo("failed to convert") + ZS_PARAMIZE(unreadStr))
        }

        String totalStr = IMessageHelper::getElementText(elem->findFirstChildElement("unread"));
        try {
          info.mTotal = Numeric<decltype(info.mTotal)>(totalStr);
        } catch (Numeric<decltype(info.mTotal)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushMessageFolderInfo("failed to convert") + ZS_PARAMIZE(totalStr))
        }

        info.mUpdateNext = UseServicesHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("updateNext")));

        return info;
      }

      //-----------------------------------------------------------------------
      ElementPtr PushMessageFolderInfo::createElement() const
      {
        ElementPtr folderEl = Element::create("folder");

        if (Disposition_NA != mDisposition) {
          folderEl->setAttribute("disposition", toString(mDisposition));
        }
        if (mName.hasData()) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("name", mName));
        }
        if (mRenamed.hasData()) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("rename", mRenamed));
        }
        if (mVersion.hasData()) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("version", mVersion));
        }
        if (mVersion.hasData()) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("unread", string(mUnread)));
        }
        if (0 != mUnread) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("unread", string(mUnread)));
        }
        if (0 != mTotal) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("total", string(mTotal)));
        }
        if (Time() != mUpdateNext) {
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updateNext", UseServicesHelper::timeToString(mUpdateNext)));
        }

        return folderEl;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::PushSubscriptionInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_PushSubscriptionInfo(const char *message)
      {
        return Log::Params(message, "PushSubscriptionInfo");
      }

      //-----------------------------------------------------------------------
      bool PushSubscriptionInfo::hasData() const
      {
        return ((mFolder.hasData()) ||

                (mType.hasData()) ||

                (mMapped.hasData()) ||

                (mUnreadBadge) ||

                (mSound.hasData()) ||

                (mAction.hasData()) ||

                (mLaunchImage.hasData()) ||

                (0 != mPriority) ||

                (mValueNames.size() > 0));
      }

      //-----------------------------------------------------------------------
      ElementPtr PushSubscriptionInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::PushSubscriptionInfo");

        UseServicesHelper::debugAppend(resultEl, "folder", mFolder);

        UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

        UseServicesHelper::debugAppend(resultEl, "type", mType);

        UseServicesHelper::debugAppend(resultEl, "mapped", mMapped);

        UseServicesHelper::debugAppend(resultEl, "unread badge", mUnreadBadge);

        UseServicesHelper::debugAppend(resultEl, "sound", mSound);

        UseServicesHelper::debugAppend(resultEl, "action", mAction);

        UseServicesHelper::debugAppend(resultEl, "launch image", mLaunchImage);

        UseServicesHelper::debugAppend(resultEl, "priority", mPriority);

        UseServicesHelper::debugAppend(resultEl, "value names", mValueNames.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void PushSubscriptionInfo::mergeFrom(
                                           const PushSubscriptionInfo &source,
                                           bool overwriteExisting
                                           )
      {
        merge(mFolder, source.mFolder, overwriteExisting);

        merge(mExpires, source.mExpires, overwriteExisting);

        merge(mType, source.mType, overwriteExisting);

        merge(mMapped, source.mMapped, overwriteExisting);

        merge(mUnreadBadge, source.mUnreadBadge, overwriteExisting);

        merge(mSound, source.mSound, overwriteExisting);

        merge(mAction, source.mAction, overwriteExisting);

        merge(mLaunchImage, source.mLaunchImage, overwriteExisting);

        merge(mPriority, source.mPriority, overwriteExisting);

        merge(mValueNames, source.mValueNames, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      ElementPtr PushSubscriptionInfo::createElement() const
      {
        ElementPtr subscriptionEl = Element::create("subscription");

        if (mFolder.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("folder", mFolder));
        }

        if (Time() != mExpires) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(mExpires)));
        }

        if (mType.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("type", mType));
        }

        if (mMapped.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("mapped", mMapped));
        }

        if (mUnreadBadge) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("unreadBadge", mUnreadBadge ? "true" : "false"));
        }

        if (mSound.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("sound", mSound));
        }

        if (mAction.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("action", mAction));
        }

        if (mLaunchImage.hasData()) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("launchImage", mLaunchImage));
        }

        if (0 != mPriority) {
          subscriptionEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("priority", string(mPriority)));
        }

        if (mValueNames.size() > 0) {
          ElementPtr valuesEl = Element::create("values");
          for (ValueNameList::const_iterator iter = mValueNames.begin(); iter != mValueNames.end(); ++iter)
          {
            const ValueName &valueName = (*iter);
            if (valueName.hasData()) {
              valuesEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("value", valueName));
            }
          }
          if (valuesEl->hasChildren()) {
            subscriptionEl->adoptAsLastChild(valuesEl);
          }
        }

        return subscriptionEl;
      }

      //-----------------------------------------------------------------------
      PushSubscriptionInfo PushSubscriptionInfo::create(ElementPtr elem)
      {
        PushSubscriptionInfo info;

        if (!elem) return info;

        info.mFolder = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("folder"));

        info.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("expires")));

        info.mType = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("type"));

        info.mMapped = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("mapped"));

        String unreadBadgeStr = IMessageHelper::getElementText(elem->findFirstChildElement("unreadBadge"));
        try {
          info.mUnreadBadge = Numeric<decltype(info.mUnreadBadge)>(unreadBadgeStr);
        } catch(Numeric<decltype(info.mUnreadBadge)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushSubscriptionInfo("failed to convert") + ZS_PARAMIZE(unreadBadgeStr))
        }

        info.mSound = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("sound"));

        info.mAction = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("action"));

        info.mLaunchImage = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("launchImage"));

        String priorityStr = IMessageHelper::getElementText(elem->findFirstChildElement("priority"));
        try {
          info.mPriority = Numeric<decltype(info.mPriority)>(priorityStr);
        } catch(Numeric<decltype(info.mPriority)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_PushSubscriptionInfo("failed to convert") + ZS_PARAMIZE(priorityStr))
        }

        ElementPtr valuesEl = elem->findFirstChildElement("values");
        if (valuesEl) {
          ElementPtr valueEl = valuesEl->findFirstChildElement("value");
          while (valueEl) {
            ValueName valueName = IMessageHelper::getElementTextAndDecode(valueEl);
            if (valueName.hasData()) {
              info.mValueNames.push_back(valueName);
            }
            valueEl = valueEl->findNextSiblingElement("value");
          }
        }

        return info;
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

        UseServicesHelper::debugAppend(resultEl, "namespace url", mURL);
        UseServicesHelper::debugAppend(resultEl, "last updated", mLastUpdated);

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
      NamespaceInfo NamespaceInfo::create(ElementPtr elem)
      {
        NamespaceInfo info;

        if (!elem) return info;

        info.mURL = IMessageHelper::getAttributeID(elem);
        info.mLastUpdated = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));

        return info;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr NamespaceInfo::createElement() const
      {
        ElementPtr namespaceEl = Element::create("namespace");

        if (mURL.hasData()) {
          namespaceEl->setAttribute("id", mURL);
        }
        if (Time() != mLastUpdated) {
          namespaceEl->setAttribute("updated", UseServicesHelper::timeToString(mLastUpdated), false);
        }

        return namespaceEl;
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
                (mDomains.hasData()) ||
                (mNamespaces.size() > 0));
      }

      //-----------------------------------------------------------------------
      ElementPtr NamespaceGrantChallengeInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::NamespaceGrantChallengeInfo");

        UseServicesHelper::debugAppend(resultEl, "grant challenge ID", mID);
        UseServicesHelper::debugAppend(resultEl, "service name", mName);
        UseServicesHelper::debugAppend(resultEl, "image url", mImageURL);
        UseServicesHelper::debugAppend(resultEl, "service url", mServiceURL);
        UseServicesHelper::debugAppend(resultEl, "domains", mDomains);
        UseServicesHelper::debugAppend(resultEl, "namespaces", mNamespaces.size());

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
        merge(mNamespaces, source.mNamespaces, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      NamespaceGrantChallengeInfo NamespaceGrantChallengeInfo::create(ElementPtr elem)
      {
        NamespaceGrantChallengeInfo info;

        if (!elem) return info;

        info.mID = IMessageHelper::getAttributeID(elem);
        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mImageURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("image"));
        info.mServiceURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));
        info.mDomains = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("domains"));

        ElementPtr namespacesEl = elem->findFirstChildElement("namespaces");
        if (namespacesEl) {
          ElementPtr namespaceEl = namespacesEl->findFirstChildElement("namespace");
          while (namespaceEl) {

            NamespaceInfo namespaceInfo;
            namespaceInfo.mURL = IMessageHelper::getAttributeID(namespaceEl);
            namespaceInfo.mLastUpdated = UseServicesHelper::stringToTime(IMessageHelper::getAttribute(namespaceEl, "updated"));

            if (namespaceInfo.hasData()) {
              info.mNamespaces[namespaceInfo.mURL] = namespaceInfo;
            }

            namespaceEl = namespaceEl->findNextSiblingElement("namespace");
          }
        }

        return info;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr NamespaceGrantChallengeInfo::createElement() const
      {
        ElementPtr namespaceGrantChallengeEl = Element::create("namespaceGrantChallenge");

        IMessageHelper::setAttributeIDWithText(namespaceGrantChallengeEl, mID);
        if (mName.hasData()) {
          namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", mName));
        }
        if (mImageURL.hasData()) {
          namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("image", mName));
        }
        if (mServiceURL.hasData()) {
          namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("url", mName));
        }
        if (mDomains.hasData()) {
          namespaceGrantChallengeEl->adoptAsLastChild(IMessageHelper::createElementWithText("domains", mName));
        }

        if (mNamespaces.size() > 0) {
          ElementPtr namespacesEl = Element::create("namespaces");

          for (NamespaceInfoMap::const_iterator iter = mNamespaces.begin(); iter != mNamespaces.end(); ++iter) {
            const String &url = (*iter).first;
            const NamespaceInfo &namespaceInfo = (*iter).second;

            if (!url.hasData()) {
              continue;
            }
            if (!namespaceInfo.hasData()) {
              continue;
            }
            ElementPtr namespaceEl = IMessageHelper::createElementWithTextID("namespace", url);
            if (namespaceInfo.mLastUpdated != Time()) {
              namespaceEl->setAttribute("updated", UseServicesHelper::timeToString(namespaceInfo.mLastUpdated), false);
            }

            namespacesEl->adoptAsLastChild(namespaceEl);
          }

          if (namespacesEl->hasChildren()) {
            namespaceGrantChallengeEl->adoptAsLastChild(namespacesEl);
          }
        }

        return namespaceGrantChallengeEl;
      }

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

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "type", mType);
        UseServicesHelper::debugAppend(resultEl, "version", mVersion);
        UseServicesHelper::debugAppend(resultEl, "methods", mMethods.size());

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

        UseServicesHelper::debugAppend(resultEl, "name", mName);
        UseServicesHelper::debugAppend(resultEl, "uri", mURI);
        UseServicesHelper::debugAppend(resultEl, "username", mUsername);
        UseServicesHelper::debugAppend(resultEl, "password", mPassword);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      Service Service::create(ElementPtr serviceEl)
      {
        Service service;

        if (!serviceEl) return service;

        service.mID = IMessageHelper::getAttributeID(serviceEl);
        service.mType = IMessageHelper::getElementText(serviceEl->findFirstChildElement("type"));
        service.mVersion = IMessageHelper::getElementText(serviceEl->findFirstChildElement("version"));

        ElementPtr methodsEl = serviceEl->findFirstChildElement("methods");
        if (methodsEl) {
          ElementPtr methodEl = methodsEl->findFirstChildElement("method");
          while (methodEl) {
            Service::Method method;
            method.mName = IMessageHelper::getElementText(methodEl->findFirstChildElement("name"));

            String uri = IMessageHelper::getElementText(methodEl->findFirstChildElement("uri"));
            String host = IMessageHelper::getElementText(methodEl->findFirstChildElement("host"));

            method.mURI = (host.hasData() ? host : uri);
            method.mUsername = IMessageHelper::getElementText(methodEl->findFirstChildElement("username"));
            method.mPassword = IMessageHelper::getElementText(methodEl->findFirstChildElement("password"));

            if (method.hasData()) {
              service.mMethods[method.mName] = method;
            }

            methodEl = methodEl->findNextSiblingElement("method");
          }
        }
        return service;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::RolodexInfo
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params slog_RolodexInfo(const char *message)
      {
        return Log::Params(message, "RolodexInfo");
      }

      //-----------------------------------------------------------------------
      bool RolodexInfo::hasData() const
      {
        return ((mServerToken.hasData()) ||

                (mToken.hasData()) ||

                (mVersion.hasData()) ||
                (Time() != mUpdateNext) ||

                (mRefreshFlag));
      }

      //-----------------------------------------------------------------------
      ElementPtr RolodexInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::RolodexInfo");

        UseServicesHelper::debugAppend(resultEl, "server token", mServerToken);
        UseServicesHelper::debugAppend(resultEl, mToken.toDebug());
        UseServicesHelper::debugAppend(resultEl, "version", mVersion);
        UseServicesHelper::debugAppend(resultEl, "update next", mUpdateNext);
        UseServicesHelper::debugAppend(resultEl, "refresh", mRefreshFlag);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void RolodexInfo::mergeFrom(
                                  const RolodexInfo &source,
                                  bool overwriteExisting
                                  )
      {
        merge(mServerToken, source.mServerToken, overwriteExisting);

        mToken.mergeFrom(source.mToken, overwriteExisting);

        merge(mVersion, source.mVersion, overwriteExisting);
        merge(mUpdateNext, source.mUpdateNext, overwriteExisting);

        merge(mRefreshFlag, source.mRefreshFlag, overwriteExisting);
      }

      //-----------------------------------------------------------------------
      RolodexInfo RolodexInfo::create(ElementPtr elem)
      {
        RolodexInfo info;

        if (!elem) return info;

        info.mServerToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("serverToken"));

        info.mToken = Token::create(elem->findFirstChildElement("token"));

        info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));
        info.mUpdateNext = UseServicesHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updateNext")));

        String refreshFlagStr = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("refresh"));
        try {
          info.mRefreshFlag = Numeric<decltype(info.mRefreshFlag)>(refreshFlagStr);
        } catch(Numeric<decltype(info.mRefreshFlag)>::ValueOutOfRange &) {
          ZS_LOG_WARNING(Detail, slog_RolodexInfo("failed to convert") + ZS_PARAMIZE(refreshFlagStr))
        }

        return info;
      }

      //-----------------------------------------------------------------------
      ElementPtr RolodexInfo::createElement() const
      {
        ElementPtr rolodexEl = Element::create("rolodex");

        if (!mServerToken.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("serverToken", mServerToken));
        }

        if (mToken.hasData()) {
          rolodexEl->adoptAsLastChild(mToken.createElement());
        }

        if (!mVersion.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("version", mVersion));
        }
        if (Time() != mUpdateNext) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updateNext", UseServicesHelper::timeToString(mUpdateNext)));
        }

        if (mRefreshFlag) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("refresh", "true"));
        }

        return rolodexEl;
      }

    }
  }
}
