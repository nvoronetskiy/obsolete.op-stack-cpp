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
      using services::IHelper;
      using stack::internal::Helper;
      using zsLib::Numeric;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //---------------------------------------------------------------------
      static Log::Params Finder_slog(const char *message)
      {
        return Log::Params(message, "Finder");
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
      static void merge(IPeerFilePublicPtr &result, const IPeerFilePublicPtr &source, bool overwrite)
      {
        if (!source) return;
        if (result) {
          if (!overwrite) return;
        }
        result = source;
      }

      //-----------------------------------------------------------------------
      static void merge(FolderInfo::Dispositions &result, FolderInfo::Dispositions source, bool overwrite)
      {
        if (FolderInfo::Disposition_NA == source) return;
        if (FolderInfo::Disposition_NA != result) {
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
      Finder Finder::create(ElementPtr elem)
      {
        Finder ret;

        if (!elem) return ret;

        ret.mID = IMessageHelper::getAttributeID(elem);

        ElementPtr protocolsEl = elem->findFirstChildElement("protocols");
        if (protocolsEl) {
          ElementPtr protocolEl = protocolsEl->findFirstChildElement("protocol");
          while (protocolEl) {
            Finder::Protocol protocol;
            protocol.mTransport = IMessageHelper::getElementText(protocolEl->findFirstChildElement("transport"));
            protocol.mHost = IMessageHelper::getElementText(protocolEl->findFirstChildElement("host"));

            if ((protocol.mTransport.hasData()) ||
                (protocol.mHost.hasData())) {
              ret.mProtocols.push_back(protocol);
            }

            protocolEl = protocolEl->findNextSiblingElement("protocol");
          }
        }

        ret.mRegion = IMessageHelper::getElementText(elem->findFirstChildElement("region"));
        ret.mCreated = IHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("created")));
        ret.mExpires = IHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("expires")));

        try
        {
          ret.mPublicKey = IRSAPublicKey::load(*IHelper::convertFromBase64(IMessageHelper::getElementText(elem->findFirstChildElementChecked("key")->findFirstChildElementChecked("x509Data"))));
          try {
            ret.mPriority = Numeric<decltype(ret.mPriority)>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("priority")));
          } catch(Numeric<decltype(ret.mPriority)>::ValueOutOfRange &) {
          }
          try {
            ret.mWeight = Numeric<decltype(ret.mWeight)>(IMessageHelper::getElementText(elem->findFirstChildElementChecked("weight")));
          } catch(Numeric<decltype(ret.mWeight)>::ValueOutOfRange &) {
          }
        }
        catch(CheckFailed &) {
          ZS_LOG_BASIC(Finder_slog("createFinder XML check failure"))
        }

        return ret;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark message::FolderInfo
      #pragma mark

      //-----------------------------------------------------------------------
      const char *FolderInfo::toString(Dispositions disposition)
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
      FolderInfo::Dispositions FolderInfo::toDisposition(const char *inStr)
      {
        if (!inStr) return Disposition_NA;
        String str(inStr);
        if ("update" == str) return Disposition_Update;
        if ("remove" == str) return Disposition_Remove;
        if ("reset" == str) return Disposition_Reset;
        return Disposition_NA;
      }

      //-----------------------------------------------------------------------
      bool FolderInfo::hasData() const
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
      ElementPtr FolderInfo::toDebug() const
      {
        ElementPtr resultEl = Element::create("message::FolderInfo");

        IHelper::debugAppend(resultEl, "disposition", mDisposition != Disposition_NA ? String(toString(mDisposition)) : String());
        IHelper::debugAppend(resultEl, "name", mName);
        IHelper::debugAppend(resultEl, "rename", mRenamed);
        IHelper::debugAppend(resultEl, "version", mVersion);
        IHelper::debugAppend(resultEl, "unread", mUnread);
        IHelper::debugAppend(resultEl, "total", mTotal);
        IHelper::debugAppend(resultEl, "update next", mUpdateNext);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void FolderInfo::mergeFrom(
                                 const FolderInfo &source,
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
      FolderInfo FolderInfo::create(ElementPtr elem)
      {
        FolderInfo info;

        if (!elem) return info;

        info.mDisposition = toDisposition(IMessageHelper::getAttribute(elem, "disposition"));
        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mRenamed = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("rename"));
        info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));

        try {
          info.mUnread = Numeric<decltype(info.mUnread)>(IMessageHelper::getElementText(elem->findFirstChildElement("unread")));
        } catch (Numeric<decltype(info.mUnread)>::ValueOutOfRange &) {
        }
        try {
          info.mTotal = Numeric<decltype(info.mTotal)>(IMessageHelper::getElementText(elem->findFirstChildElement("unread")));
        } catch (Numeric<decltype(info.mTotal)>::ValueOutOfRange &) {
        }

        info.mUpdateNext = services::IHelper::stringToTime(IMessageHelper::getElementText(elem->findFirstChildElement("updateNext")));

        return info;
      }

      //-----------------------------------------------------------------------
      ElementPtr FolderInfo::createElement() const
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
          folderEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updateNext", services::IHelper::timeToString(mUpdateNext)));
        }

        return folderEl;
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

      //---------------------------------------------------------------------
      IdentityInfo IdentityInfo::create(ElementPtr elem)
      {
        IdentityInfo info;

        if (!elem) return info;

        info.mDisposition = IdentityInfo::toDisposition(elem->getAttributeValue("disposition"));

        info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
        info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
        info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
        info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
        info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

        info.mReloginKey = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reloginKey"));

        info.mBase = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("base"));
        info.mURI = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("uri"));
        info.mProvider = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("provider"));

        info.mStableID = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("stableID"));
        ElementPtr peerEl = elem->findFirstChildElement("peer");
        if (peerEl) {
          info.mPeerFilePublic = IPeerFilePublic::loadFromElement(peerEl);
        }

        try {
          info.mPriority = Numeric<WORD>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("priority")));
        } catch(Numeric<WORD>::ValueOutOfRange &) {
        }
        try {
          info.mWeight = Numeric<WORD>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("weight")));
        } catch(Numeric<WORD>::ValueOutOfRange &) {
        }

        info.mUpdated = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("created")));
        info.mUpdated = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));
        info.mExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("expires")));

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
            try {
              avatar.mWidth = Numeric<int>(IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("width")));
            } catch(Numeric<int>::ValueOutOfRange &) {
            }
            try {
              avatar.mHeight = Numeric<int>(IMessageHelper::getElementTextAndDecode(avatarEl->findFirstChildElement("height")));
            } catch(Numeric<int>::ValueOutOfRange &) {
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

        if (!mAccessToken.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", mAccessToken));
        }
        if (!mAccessSecret.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", mAccessSecret));
        }
        if (Time() != mAccessSecretExpires) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(mAccessSecretExpires)));
        }
        if (!mAccessSecretProof.isEmpty()) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", mAccessSecretProof));
        }
        if (Time() != mAccessSecretProofExpires) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(mAccessSecretProofExpires)));
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
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("created", IHelper::timeToString(mCreated)));
        }
        if (Time() != mUpdated) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updated", IHelper::timeToString(mUpdated)));
        }
        if (Time() != mExpires) {
          identityEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", IHelper::timeToString(mExpires)));
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
      ElementPtr LockboxInfo::createElement() const
      {
        ElementPtr lockboxEl = Element::create("lockbox");

        if (mDomain.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("domain", mDomain));
        }
        if (mAccountID.hasData()) {
          lockboxEl->setAttribute("id", mAccountID);
        }
        if (mAccessToken.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", mAccessToken));
        }
        if (mAccessSecret.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", mAccessSecret));
        }
        if (Time() != mAccessSecretExpires) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(mAccessSecretExpires)));
        }
        if (mAccessSecretProof.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", mAccessSecretProof));
        }
        if (Time() != mAccessSecretProofExpires) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(mAccessSecretProofExpires)));
        }

        if (mKey) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("key", IHelper::convertToBase64(*mKey)));
        }
        if (mHash.hasData()) {
          lockboxEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("hash", mHash));
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
        info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
        info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
        info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
        info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
        info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

        String key = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("key"));

        if (key.hasData()) {
          info.mKey = IHelper::convertFromBase64(key);
          if (IHelper::isEmpty(info.mKey)) info.mKey = SecureByteBlockPtr();
        }
        info.mHash = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("hash"));

        try {
          info.mResetFlag = Numeric<bool>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("reset")));
        } catch(Numeric<bool>::ValueOutOfRange &) {
        }

        return info;
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
      AgentInfo AgentInfo::create(ElementPtr elem)
      {
        AgentInfo info;

        if (!elem) return info;

        info.mUserAgent = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("userAgent"));
        info.mName = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("name"));
        info.mImageURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("image"));
        info.mAgentURL = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("url"));

        return info;
      }
      
      //-----------------------------------------------------------------------
      ElementPtr AgentInfo::createElement() const
      {
        ElementPtr agentEl = Element::create("agent");

        if (mUserAgent.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("userAgent", mUserAgent));
        }
        if (mName.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("name", mName));
        }
        if (mImageURL.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("image", mImageURL));
        }
        if (mAgentURL.hasData()) {
          agentEl->adoptAsLastChild(IMessageHelper::createElementWithText("url", mAgentURL));
        }

        return agentEl;
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
      NamespaceInfo NamespaceInfo::create(ElementPtr elem)
      {
        NamespaceInfo info;

        if (!elem) return info;

        info.mURL = IMessageHelper::getAttributeID(elem);
        info.mLastUpdated = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updated")));

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
          namespaceEl->setAttribute("updated", IHelper::timeToString(mLastUpdated));
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

        IHelper::debugAppend(resultEl, "grant challenge ID", mID);
        IHelper::debugAppend(resultEl, "service name", mName);
        IHelper::debugAppend(resultEl, "image url", mImageURL);
        IHelper::debugAppend(resultEl, "service url", mServiceURL);
        IHelper::debugAppend(resultEl, "domains", mDomains);
        IHelper::debugAppend(resultEl, "namespaces", mNamespaces.size());

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
            namespaceInfo.mLastUpdated = services::IHelper::stringToTime(IMessageHelper::getAttribute(namespaceEl, "updated"));

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

        IMessageHelper::setAttributeID(namespaceGrantChallengeEl, mID);
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
            ElementPtr namespaceEl = IMessageHelper::createElementWithID("namespace", url);
            if (namespaceInfo.mLastUpdated != Time()) {
              namespaceEl->setAttribute("updated", services::IHelper::timeToString(namespaceInfo.mLastUpdated));
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

      //-----------------------------------------------------------------------
      RolodexInfo RolodexInfo::create(ElementPtr elem)
      {
        RolodexInfo info;

        if (!elem) return info;

        info.mServerToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("serverToken"));

        info.mAccessToken = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessToken"));
        info.mAccessSecret = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecret"));
        info.mAccessSecretExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretExpires")));
        info.mAccessSecretProof = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProof"));
        info.mAccessSecretProofExpires = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("accessSecretProofExpires")));

        info.mVersion = IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("version"));
        info.mUpdateNext = IHelper::stringToTime(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("updateNext")));

        try {
          info.mRefreshFlag = Numeric<bool>(IMessageHelper::getElementTextAndDecode(elem->findFirstChildElement("refresh")));
        } catch(Numeric<bool>::ValueOutOfRange &) {
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

        if (!mAccessToken.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessToken", mAccessToken));
        }
        if (!mAccessSecret.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecret", mAccessSecret));
        }
        if (Time() != mAccessSecretExpires) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretExpires", IHelper::timeToString(mAccessSecretExpires)));
        }
        if (!mAccessSecretProof.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("accessSecretProof", mAccessSecretProof));
        }
        if (Time() != mAccessSecretExpires) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("accessSecretProofExpires", IHelper::timeToString(mAccessSecretProofExpires)));
        }

        if (!mVersion.isEmpty()) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithText("version", mVersion));
        }
        if (Time() != mUpdateNext) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("updateNext", IHelper::timeToString(mUpdateNext)));
        }

        if (mRefreshFlag) {
          rolodexEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("refresh", "true"));
        }

        return rolodexEl;
      }

    }
  }
}
