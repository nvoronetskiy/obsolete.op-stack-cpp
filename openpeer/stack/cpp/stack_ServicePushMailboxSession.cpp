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

#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>

#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/message/bootstrapped-servers/ServersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/AccessRequest.h>
#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/push-mailbox/PeerValidateRequest.h>
#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
#include <openpeer/stack/message/push-mailbox/FoldersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/FolderGetRequest.h>
#include <openpeer/stack/message/push-mailbox/FolderUpdateRequest.h>
#include <openpeer/stack/message/push-mailbox/MessagesMetaDataGetRequest.h>
#include <openpeer/stack/message/push-mailbox/MessagesDataGetRequest.h>
#include <openpeer/stack/message/push-mailbox/MessageUpdateRequest.h>
#include <openpeer/stack/message/push-mailbox/ListFetchRequest.h>
#include <openpeer/stack/message/push-mailbox/ChangedNotify.h>

#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IPeerFilePrivate.h>

#include <openpeer/services/IDHKeyDomain.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>
#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>
#include <openpeer/services/ITCPMessaging.h>
#include <openpeer/services/ITransportStream.h>

#include <cryptopp/sha.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Numeric.h>
#include <zsLib/Stringize.h>

#define OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME ".sent"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME "keys"

#define OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN (-1)

#define OPENPEER_STACK_PUSH_MAILBOX_JSON_MIME_TYPE "text/json"
#define OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_TYPE_RSA "RSA"
#define OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_TYPE_DH "DH"

#define OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_LENGTH (25)

#define OPENPEER_STACK_PUSH_MAILBOX_LIST_URI_PREFIX "list:"

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using CryptoPP::SHA1;
      using zsLib::Numeric;

      using services::IHelper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      typedef message::IMessageHelper IMessageHelper;

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      ZS_DECLARE_USING_PTR(openpeer::services, IBackgrounding)

      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, AccessRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, NamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, PeerValidateRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, FoldersGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, FolderGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, FolderUpdateRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, MessagesMetaDataGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, MessagesDataGetRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      static String getMultiplexedJsonTCPTransport(const Server &server)
      {
        for (Server::ProtocolList::const_iterator iter = server.mProtocols.begin(); iter != server.mProtocols.end(); ++iter)
        {
          const Server::Protocol &protocol = (*iter);

          if (OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_TCP == protocol.mTransport) {
            return protocol.mHost;
          }
        }
        return String();
      }

      //-----------------------------------------------------------------------
      static String extractListID(const String &uri)
      {
        size_t listLength = strlen(OPENPEER_STACK_PUSH_MAILBOX_LIST_URI_PREFIX);
        if (0 != strncmp(OPENPEER_STACK_PUSH_MAILBOX_LIST_URI_PREFIX, uri, listLength)) return String();

        return uri.substr(listLength);
      }

      //-----------------------------------------------------------------------
      static String extractRawSendingKeyID(
                                           const String &inKeyID,
                                           bool &outIsDHKey
                                           )
      {
        IHelper::SplitMap split;
        IHelper::split(inKeyID, split, '-');
        if (split.size() != 2) return String();

        if (split[1] == OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_TYPE_DH) {
          outIsDHKey = true;
        } else {
          outIsDHKey = false;
        }

        return split[0];
      }

      //-----------------------------------------------------------------------
      static String generateSendingKeyID()
      {
        return IHelper::randomString(OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_LENGTH);
      }

      //-----------------------------------------------------------------------
      static String makeDHSendingKeyID(const String &inKeyID)
      {
        return inKeyID + "-" + OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_TYPE_DH;
      }

      //-----------------------------------------------------------------------
      static String makeRSASendingKeyID(const String &inKeyID)
      {
        return inKeyID + "-" + OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_TYPE_RSA;
      }

      //-----------------------------------------------------------------------
      // http://www.zedwood.com/article/cpp-urlencode-function
      // from: http://codepad.org/lCypTglt
      static String urlencode(const String &s)
      {
        //RFC 3986 section 2.3 Unreserved Characters (January 2005)
        const std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

        std::string escaped = "";
        for(size_t i=0; i < s.length(); i++) {
          if (unreserved.find_first_of(s[i]) != std::string::npos) {
            escaped.push_back(s[i]);
          } else {
            escaped.append("%");
            char buf[3];
            sprintf(buf, "%.2X", s[i]);
            escaped.append(buf);
          }
        }

        return escaped;
      }

      //-----------------------------------------------------------------------
      static void copy(const message::PushMessageInfo::PushInfoList &source, IServicePushMailboxSession::PushInfoList &dest)
      {
        typedef PushMessageInfo::PushInfo SourceType;
        typedef PushMessageInfo::PushInfoList SourceListType;
        typedef IServicePushMailboxSession::PushInfo DestType;
        typedef IServicePushMailboxSession::PushInfoList DestListType;

        for (SourceListType::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          const SourceType &sourceValue = (*iter);

          DestType destValue;

          destValue.mServiceType = sourceValue.mServiceType;
          destValue.mValues = sourceValue.mValues ? sourceValue.mValues->clone()->toElement() : ElementPtr();
          destValue.mCustom = sourceValue.mCustom ? sourceValue.mCustom->clone()->toElement() : ElementPtr();

          dest.push_back(destValue);
        }
      }

      //-----------------------------------------------------------------------
      static void copy(const IServicePushMailboxSession::PushInfoList &source, message::PushMessageInfo::PushInfoList &dest)
      {
        typedef IServicePushMailboxSession::PushInfo SourceType;
        typedef IServicePushMailboxSession::PushInfoList SourceListType;
        typedef PushMessageInfo::PushInfo DestType;
        typedef PushMessageInfo::PushInfoList DestListType;

        for (SourceListType::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          const SourceType &sourceValue = (*iter);

          DestType destValue;

          destValue.mServiceType = sourceValue.mServiceType;
          destValue.mValues = sourceValue.mValues ? sourceValue.mValues->clone()->toElement() : ElementPtr();
          destValue.mCustom = sourceValue.mCustom ? sourceValue.mCustom->clone()->toElement() : ElementPtr();

          dest.push_back(destValue);
        }
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::ServicePushMailboxSession(
                                                           IMessageQueuePtr queue,
                                                           BootstrappedNetworkPtr network,
                                                           IServicePushMailboxSessionDelegatePtr delegate,
                                                           IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                           IMessageQueuePtr databaseDelegateAsyncQueue,
                                                           AccountPtr account,
                                                           ServiceNamespaceGrantSessionPtr grantSession,
                                                           ServiceLockboxSessionPtr lockboxSession
                                                           ) :
        zsLib::MessageQueueAssociator(queue),

        SharedRecursiveLock(SharedRecursiveLock::create()),

        mDB(databaseDelegate),

        mCurrentState(SessionState_Pending),

        mBootstrappedNetwork(network),

        mSentViaObjectID(mID),

        mAccount(account),

        mLockbox(lockboxSession),
        mGrantSession(grantSession),

        mRequiresConnection(true),
        mInactivityTimeout(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT))),
        mLastActivity(zsLib::now()),

        mDefaultLastRetryDuration(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS))),
        mLastRetryDuration(mDefaultLastRetryDuration),

        mRefreshFolders(true),

        mRefreshFoldersNeedingUpdate(true),

        mRefreshMonitorFolderCreation(true),

        mRefreshMessagesNeedingUpdate(true),

        mRefreshMessagesNeedingData(true),

        mRefreshListsNeedingDownload(true),

        mRefreshKeysFolderNeedsProcessing(true),

        mRefreshMessagesNeedingDecryption(true),

        mRefreshPendingDelivery(true),
        mDeliveryMaxChunkSize(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_CHUNK_SIZE_IN_BYTES)),
        mLastChannel(0),
        mPendingDeliveryPrecheckRequired(true),

        mRefreshVersionedFolders(true),

        mRefreshMessagesNeedingExpiry(true)
      {
        ZS_LOG_BASIC(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate, UseStack::queueDelegate());
        mAsyncDB = AsyncDatabase::create(databaseDelegateAsyncQueue, databaseDelegate);
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::~ServicePushMailboxSession()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::init()
      {
        AutoRecursiveLock lock(*this);

        UseBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
        mBackgroundingSubscription = IBackgrounding::subscribe(mThisWeak.lock(), services::ISettings::getUInt(OPENPEER_STACK_SETTING_BACKGROUNDING_PUSH_MAILBOX_PHASE));
        mReachabilitySubscription = IReachability::subscribe(mThisWeak.lock());

        mMonitoredFolders[OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME] = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        mMonitoredFolders[OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME] = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionPtr ServicePushMailboxSession::convert(IServicePushMailboxSessionPtr session)
      {
        return dynamic_pointer_cast<ServicePushMailboxSession>(session);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::toDebug(IServicePushMailboxSessionPtr session)
      {
        if (!session) return ElementPtr();
        return ServicePushMailboxSession::convert(session)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionPtr ServicePushMailboxSession::create(
                                                                     IServicePushMailboxSessionDelegatePtr delegate,
                                                                     IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                                     IMessageQueuePtr databaseDelegateAsyncQueue,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IAccountPtr account,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!databaseDelegate) // must not be NULL

        ServicePushMailboxSessionPtr pThis(new ServicePushMailboxSession(UseStack::queueStack(), BootstrappedNetwork::convert(servicePushMailbox), delegate, databaseDelegate, databaseDelegateAsyncQueue, Account::convert(account), ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(lockboxSession)));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      PUID ServicePushMailboxSession::getID() const
      {
        return mID;
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionSubscriptionPtr ServicePushMailboxSession::subscribe(IServicePushMailboxSessionDelegatePtr originalDelegate)
      {
        ZS_LOG_DETAIL(log("subscribing to push mailbox state"))

        AutoRecursiveLock lock(*this);
        if (!originalDelegate) return mDefaultSubscription;

        IServicePushMailboxSessionSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate, UseStack::queueDelegate());

        IServicePushMailboxSessionDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          ServicePushMailboxSessionPtr pThis = mThisWeak.lock();

          if (SessionState_Pending != mCurrentState) {
            delegate->onServicePushMailboxSessionStateChanged(pThis, mCurrentState);
          }

          for (FolderNameMap::const_iterator iter = mNotifiedMonitoredFolders.begin(); iter != mNotifiedMonitoredFolders.end(); ++iter)
          {
            const FolderName &folderName = (*iter).first;
            delegate->onServicePushMailboxSessionFolderChanged(pThis, folderName);
          }
        }

        if (isShutdown()) {
          mSubscriptions.clear();
        }

        return subscription;
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxPtr ServicePushMailboxSession::getService() const
      {
        AutoRecursiveLock lock(*this);
        return BootstrappedNetwork::convert(mBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSession::SessionStates ServicePushMailboxSession::getState(
                                                                                    WORD *lastErrorCode,
                                                                                    String *lastErrorReason
                                                                                    ) const
      {
        AutoRecursiveLock lock(*this);
        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::shutdown()
      {
        ZS_LOG_DEBUG(log("shutdown called"))

        AutoRecursiveLock lock(*this);
        cancel();
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxRegisterQueryPtr ServicePushMailboxSession::registerDevice(
                                                                                    IServicePushMailboxRegisterQueryDelegatePtr delegate,
                                                                                    const char *inDeviceToken,
                                                                                    const char *folder,
                                                                                    Time expires,
                                                                                    const char *mappedType,
                                                                                    bool unreadBadge,
                                                                                    const char *sound,
                                                                                    const char *action,
                                                                                    const char *launchImage,
                                                                                    unsigned int priority,
                                                                                    const ValueNameList &valueNames
                                                                                    )
      {
        AutoRecursiveLock lock(*this);

        String deviceToken(inDeviceToken);

        if (mDeviceToken.hasData()) {
          ZS_THROW_INVALID_ARGUMENT_IF(deviceToken != mDeviceToken)  // this should never change within a single run
        }

        PushSubscriptionInfo info;
        info.mFolder = String(folder);
        info.mExpires = expires;
        info.mMapped = String(mappedType);
        info.mUnreadBadge = unreadBadge;
        info.mSound = String(sound);
        info.mAction = String(action);
        info.mLaunchImage = String(launchImage);
        info.mPriority = priority;
        info.mValueNames = valueNames;

        mDeviceToken = deviceToken;

        ZS_LOG_DEBUG(log("registering device") + ZS_PARAM("token", mDeviceToken) + info.toDebug())

        RegisterQueryPtr query = RegisterQuery::create(getAssociatedMessageQueue(), *this, mThisWeak.lock(), delegate, info);
        if (isShutdown()) {
          query->setError(IHTTP::HTTPStatusCode_Gone, "already shutdown");
          query->cancel();
          return query;
        }

        mRegisterQueries.push_back(query);

        mLastActivity = zsLib::now(); // register activity to cause connection to restart if not started

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return query;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::monitorFolder(const char *inFolderName)
      {
        String folderName(inFolderName);

        ZS_THROW_INVALID_ARGUMENT_IF(folderName.isEmpty())

        AutoRecursiveLock lock(*this);

        ZS_LOG_DEBUG(log("monitoring folder") + ZS_PARAM("folder name", folderName))

        if (mMonitoredFolders.find(folderName) != mMonitoredFolders.end()) {
          ZS_LOG_DEBUG(log("already monitoring folder") + ZS_PARAM("folder name", folderName))
          return;
        }

        mMonitoredFolders[folderName] = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

        // need to possible create missing monitored folder
        mRefreshMonitorFolderCreation = true;

        // need to recheck which folders need an update
        mRefreshFoldersNeedingUpdate = true;

        // since we just started monitoring, indicate that the folder has changed
        mSubscriptions.delegate()->onServicePushMailboxSessionFolderChanged(mThisWeak.lock(), folderName);

        mLastActivity = zsLib::now(); // register activity to cause connection to restart if not started

        // wake up and refresh the folders list to start the monitor process on this folder
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::getFolderMessageUpdates(
                                                              const char *inFolder,
                                                              const String &inLastVersionDownloaded,
                                                              String &outUpdatedToVersion,
                                                              PushMessageListPtr &outMessagesAdded,
                                                              MessageIDListPtr &outMessagesRemoved
                                                              )
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderVersionedMessagesInfo FolderVersionedMessagesInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderVersionedMessagesList FolderVersionedMessagesList;

        typedef std::map<MessageID, MessageID> RemovedMap;
        typedef std::map<MessageID, PushMessagePtr> AddedMap;

        outUpdatedToVersion = String();
        outMessagesAdded = PushMessageListPtr();
        outMessagesRemoved = MessageIDListPtr();

        AutoRecursiveLock lock(*this);

        int folderIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        String uniqueID;

        if (!mDB->getFolderIndexAndUniqueID(inFolder, folderIndex, uniqueID)) {
          ZS_LOG_WARNING(Detail, log("no information about the folder exists") + ZS_PARAM("folder", inFolder))
          return false;
        }

        ZS_THROW_INVALID_ASSUMPTION_IF(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == folderIndex)

        int versionIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

        if (inLastVersionDownloaded.hasData()) {
          IHelper::SplitMap split;
          IHelper::split(inLastVersionDownloaded, split, '-');
          if (split.size() != 2) {
            ZS_LOG_WARNING(Detail, log("version string is not legal") + ZS_PARAM("version", inLastVersionDownloaded))
            return false;
          }

          if (split[0] != uniqueID) {
            ZS_LOG_WARNING(Detail, log("version conflict detected") + ZS_PARAM("input version", split[0]) + ZS_PARAM("current version", uniqueID))
            return false;
          }

          try {
            versionIndex = Numeric<int>(split[1]);
          } catch (Numeric<int>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, log("version index is not legal") + ZS_PARAM("version index", split[1]))
            return false;
          }
        }

        FolderVersionedMessagesList versionedMessages;
        mDB->getBatchOfFolderVersionedMessagesAfterIndex(versionIndex, folderIndex, versionedMessages);


        AddedMap allAdded;

        AddedMap added;
        RemovedMap removed;

        int lastVersionFound = versionIndex;

        for (FolderVersionedMessagesList::iterator iter = versionedMessages.begin(); iter != versionedMessages.end(); ++iter)
        {
          FolderVersionedMessagesInfo &info = (*iter);
          lastVersionFound = info.mIndex;

          if (info.mRemovedFlag) {
            AddedMap::iterator found = added.find(info.mMessageID);
            if (found != added.end()) {
              // was added but is now removed
              added.erase(found);
            }

            removed[info.mMessageID] = info.mMessageID;
            continue;
          }

          // scope check to see if the message was removed in this batch because it's now being added (back?)
          {
            RemovedMap::iterator found = removed.find(info.mMessageID);
            if (found != removed.end()) {
              removed.erase(found);
            }
          }

          // scope: check if this was added before during this call
          {
            AddedMap::iterator found = allAdded.find(info.mMessageID);
            if (found != allAdded.end()) {
              added[info.mMessageID] = (*found).second;
              continue;
            }
          }

          PushMessagePtr message = getPushMessage(info.mMessageID);
          if (!message) {
            ZS_LOG_WARNING(Detail, log("contains added version message that does not exist (removed later?)") + ZS_PARAM("message id", info.mMessageID))
            continue;
          }
          added[info.mMessageID] = message;
          allAdded[info.mMessageID] = message;
        }

        allAdded.clear();

        outMessagesAdded = PushMessageListPtr(new PushMessageList);
        for (AddedMap::iterator iter = added.begin(); iter != added.end(); ++iter) {
          outMessagesAdded->push_back((*iter).second);
        }

        added.clear();

        outMessagesRemoved = MessageIDListPtr(new MessageIDList);
        for (RemovedMap::iterator iter = removed.begin(); iter != removed.end(); ++iter) {
          outMessagesRemoved->push_back((*iter).second);
        }

        removed.clear();

        if (lastVersionFound >= 0) {
          outUpdatedToVersion = uniqueID + "-" + string(lastVersionFound);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::getLatestDownloadVersionAvailableForFolder(const char *inFolder)
      {
        ZS_LOG_DEBUG(log("get latest download version available for folder") + ZS_PARAM("folder", inFolder))

        AutoRecursiveLock lock(*this);

        int folderIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        String uniqueID;

        if (!mDB->getFolderIndexAndUniqueID(inFolder, folderIndex, uniqueID)) {
          ZS_LOG_WARNING(Detail, log("no information about the folder exists") + ZS_PARAM("folder", inFolder))
          return String();
        }

        int versionIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        if (!mDB->getLastFolderVersionedMessageForFolder(folderIndex, versionIndex)) {
          ZS_LOG_WARNING(Detail, log("no versioned messages exist for the folder") + ZS_PARAM("folder", inFolder))
          return String();
        }

        return uniqueID + "-" + string(versionIndex);
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSendQueryPtr ServicePushMailboxSession::sendMessage(
                                                                             IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                             const PushMessage &message,
                                                                             const char *remoteFolder,
                                                                             bool copyToSentFolder
                                                                             )
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::URIList URIList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::SendingKeyInfo SendingKeyInfo;

        typedef std::list<size_t> SizeList;
        typedef std::list<PeerOrIdentityListPtr> PeerPtrList;

        AutoRecursiveLock lock(*this);

        String messageID = prepareMessageID(message.mMessageID);

        Time sent = message.mSent;
        if (Time() == sent) sent = zsLib::now();

        ZS_LOG_DEBUG(log("send message called") + ZS_PARAM("message ID", messageID))

        PeerOrIdentityListPtr allPeers;
        addToMasterList(allPeers, message.mTo);
        addToMasterList(allPeers, message.mCC);
        addToMasterList(allPeers, message.mBCC);

        size_t sizeAll = 0;
        String allURI = convertToDatabaseList(allPeers, sizeAll);

        size_t sizeTo = 0;
        String to = convertToDatabaseList(message.mTo, sizeTo);
        size_t sizeCC = 0;
        String cc = convertToDatabaseList(message.mCC, sizeCC);
        size_t sizeBCC = 0;
        String bcc = convertToDatabaseList(message.mBCC, sizeBCC);

        SendQueryPtr query = SendQuery::create(getAssociatedMessageQueue(), *(mThisWeak.lock()), mThisWeak.lock(), delegate, messageID);

        if ((to.isEmpty()) &&
            (cc.isEmpty()) &&
            (bcc.isEmpty())) {
          ZS_LOG_ERROR(Detail, log("message does not have any targets"))
          query->cancel();
          return query;
        }

        if (!mPeerFiles) {
          ZS_LOG_ERROR(Detail, log("peer files are not present thus cannot send a message"))
          query->cancel();
          return query;
        }

        IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();
        ZS_THROW_INVALID_ASSUMPTION_IF(!peerFilePublic)

        URIList uris;
        uris.push_back(to);
        uris.push_back(cc);
        uris.push_back(bcc);

        SizeList sizes;
        sizes.push_back(sizeTo);
        sizes.push_back(sizeCC);
        sizes.push_back(sizeBCC);

        PeerPtrList peerLists;
        peerLists.push_back(message.mTo);
        peerLists.push_back(message.mCC);
        peerLists.push_back(message.mBCC);

        mSendQueries[messageID] = query;

        SendingKeyInfo info;

        // figure out which sending key to use
        if (mDB->getActiveSendingKey(allURI, zsLib::now(), info)) {
          ZS_LOG_TRACE(log("obtained active sending key for URI") + ZS_PARAM("uri", allURI))
        } else {
          deliverNewSendingKey(allURI, allPeers, info.mKeyID, info.mRSAPassphrase, info.mExpires);
          if (info.mKeyID.isEmpty()) {
            ZS_LOG_ERROR(Detail, log("failed to deliver encryption key") + ZS_PARAM("uri", allURI))
          }
          info.mListSize = sizeAll;
          info.mTotalWithPassphrase = 0;
        }

        String keyID = (info.mListSize == info.mTotalWithPassphrase ? makeDHSendingKeyID(info.mKeyID) :makeRSASendingKeyID(info.mKeyID));
        String passphrase = (info.mListSize == info.mTotalWithPassphrase ? info.mDHPassphrase : info.mRSAPassphrase);

        String encoding;
        SecureByteBlockPtr encryptedMessage;
        if (message.mFullMessage) {
          if (!encryptMessage(keyID, passphrase, *message.mFullMessage, encoding, encryptedMessage)) {
            ZS_LOG_ERROR(Detail, log("failed to encrypt emssage"))
            encoding = String();
            encryptedMessage = IHelper::clone(message.mFullMessage);
          }
        }

        int messageIndex = mDB->insertMessage(
                                              messageID,
                                              to,
                                              peerFilePublic->getPeerURI(),
                                              cc,
                                              bcc,
                                              message.mMessageType,
                                              message.mMimeType,
                                              encoding,
                                              message.mPushType,
                                              message.mPushInfos,
                                              message.mSent,
                                              sent,
                                              encryptedMessage,
                                              message.mFullMessage,
                                              true
                                              );

        mRefreshVersionedFolders = true;

        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == messageIndex) {
          ZS_LOG_ERROR(Detail, log("failed to insert message into the database"))
          query->cancel();
          return query;
        }

        int flags = 0;
        for (PushStateDetailMap::const_iterator iter = message.mPushStateDetails.begin(); iter != message.mPushStateDetails.end(); ++iter)
        {
          PushStates flag = (*iter).first;
          flags = flags | ((int)(flag));
        }

        mDB->insertPendingDeliveryMessage(messageIndex, remoteFolder, copyToSentFolder, flags);
        mRefreshPendingDelivery = true;

        mLastActivity = zsLib::now(); // register activity to cause connection to restart if not started

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return query;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::recheckNow()
      {
        ZS_LOG_DEBUG(log("recheck now called"))

        AutoRecursiveLock lock(*this);

        mRequiresConnection = true;

        mDoNotRetryConnectionBefore = Time();

        mRefreshFolders = true;
        mRefreshFoldersNeedingUpdate = true;

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::markPushMessageRead(const char *inMessageID)
      {
        ZS_LOG_DEBUG(log("mark push message as read") + ZS_PARAM("message id", inMessageID))
        AutoRecursiveLock lock(*this);

        String messageID(inMessageID);
        ZS_THROW_INVALID_ARGUMENT_IF(messageID.isEmpty())

        mMessagesToMarkRead[messageID] = messageID;

        mLastActivity = zsLib::now(); // register activity to cause connection to restart if not started

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::deletePushMessage(const char *inMessageID)
      {
        ZS_LOG_DEBUG(log("message needs to be deleted") + ZS_PARAM("message id", inMessageID))
        AutoRecursiveLock lock(*this);

        String messageID(inMessageID);
        ZS_THROW_INVALID_ARGUMENT_IF(messageID.isEmpty())

        mMessagesToRemove[messageID] = messageID;

        mLastActivity = zsLib::now(); // register activity to cause connection to restart if not started

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => friend RegisterQuery
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::notifyComplete(RegisterQuery &query)
      {
        ZS_LOG_DEBUG(log("notified reegister query complete") + ZS_PARAM("query", query.getID()))

        for (RegisterQueryList::iterator iter = mRegisterQueries.begin(); iter != mRegisterQueries.end(); ++iter)
        {
          RegisterQueryPtr existingQuery = (*iter);
          if (query.getID() != existingQuery->getID()) continue;

          mRegisterQueries.erase(iter);
          break;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => friend SendQuery
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::notifyComplete(SendQuery &query)
      {
        ZS_LOG_DEBUG(log("notified send query complete") + ZS_PARAM("query", query.getID()))

        AutoRecursiveLock lock(*this);

        const String &messageID = query.getMessageID();
        SendQueryMap::iterator found = mSendQueries.find(messageID);
        if (found != mSendQueries.end()) {
          mSendQueries.erase(found);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::PushMessagePtr ServicePushMailboxSession::getPushMessage(const String &messageID)
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfoWithFlag DeliveryInfoWithFlag;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfoWithFlagList DeliveryInfoWithFlagList;

        typedef std::map<PushStates, PushStatePeerDetailList> PushStateMap;

        ZS_LOG_DEBUG(log("get push message") + ZS_PARAM("message id", messageID))
        AutoRecursiveLock lock(*this);

        PushMessagePtr result(new PushMessage);

        String to;
        String from;
        String cc;
        String bcc;

        int messageIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

        bool hasMessage = mDB->getMessageDetails(
                                                 messageID.c_str(),
                                                 messageIndex,
                                                 to,
                                                 from,
                                                 cc,
                                                 bcc,
                                                 result->mMessageType,
                                                 result->mMimeType,
                                                 result->mPushType,
                                                 result->mPushInfos,
                                                 result->mSent,
                                                 result->mExpires,
                                                 result->mFullMessage
                                                 );

        if (!hasMessage) {
          ZS_LOG_WARNING(Detail, log("message was not found in database") + ZS_PARAM("message id", messageID))
          return PushMessagePtr();
        }

        result->mTo = convertFromDatabaseList(to);

        if (mPeerFiles) {
          IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();

          if (from == peerFilePublic->getPeerURI()) {
            ILocationPtr location = ILocation::getForLocal(Account::convert(mAccount));
            if (location) {
              result->mFrom = location->getPeer();
            }
          }
        }

        if (!result->mFrom) {
          result->mFrom = IPeer::create(Account::convert(mAccount), from);
        }

        result->mCC = convertFromDatabaseList(cc);
        result->mBCC = convertFromDatabaseList(bcc);

        DeliveryInfoWithFlagList deliveryInfos;
        mDB->getMessageDeliverStateForMessage(
                                              messageIndex,
                                              deliveryInfos
                                              );

        if (deliveryInfos.size() > 0) {
          // convert delivery infos from a flat table list to a state map
          for (DeliveryInfoWithFlagList::iterator iter = deliveryInfos.begin(); iter != deliveryInfos.end(); ++iter)
          {
            DeliveryInfoWithFlag &info = (*iter);

            PushStates state = IServicePushMailboxSession::toPushState(info.mFlag.c_str());
            if (PushState_None == state) {
              ZS_LOG_WARNING(Detail, log("state was not understood") + ZS_PARAM("message id", messageID) + ZS_PARAM("flag", info.mFlag))
            }

            PushStateDetailMap::iterator found = result->mPushStateDetails.find(state);

            if (found == result->mPushStateDetails.end()) {
              result->mPushStateDetails[state] = PushStatePeerDetailList();
              found = result->mPushStateDetails.find(state);
            }

            PushStatePeerDetailList &useList = (*found).second;

            if (info.mURI.isEmpty()) continue;

            PushStatePeerDetail detail;
            detail.mURI = info.mURI;
            detail.mErrorCode = info.mErrorCode;
            detail.mErrorReason = info.mErrorReason;

            useList.push_back(detail);
          }
        }

        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on timer"))

        if (timer == mNextFoldersUpdateTimer) {
          mLastActivity = zsLib::now();
          mRefreshFolders = true;

          mNextFoldersUpdateTimer->cancel();
          mNextFoldersUpdateTimer.reset();
        }

        if (timer == mRetryTimer) {
          mRetryTimer->cancel();
          mRetryTimer.reset();
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => ITCPMessagingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onTCPMessagingStateChanged(
                                                                 ITCPMessagingPtr messaging,
                                                                 ITCPMessagingDelegate::SessionStates state
                                                                 )
      {
        ZS_LOG_DEBUG(log("on TCP messaging state changed") + ITCPMessaging::toDebug(messaging) + ZS_PARAM("state", ITCPMessaging::toString(state)))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => ITransportStreamReaderDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onTransportStreamReaderReady(ITransportStreamReaderPtr reader)
      {
        ZS_LOG_DEBUG(log("on transport stream reader ready") + ZS_PARAM("id", reader->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => ITransportStreamWriterDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onTransportStreamWriterReady(ITransportStreamWriterPtr writer)
      {
        ZS_LOG_DEBUG(log("on transport stream writer ready") + ZS_PARAM("id", writer->getID()))

        AutoRecursiveLock lock(*this);
        if (writer != mWriter) {
          ZS_LOG_WARNING(Trace, log("notified about obsolete writer") + ZS_PARAM("id", writer->getID()))
          return;
        }

        if (mPendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("message upload is not required at this time"))
          return;
        }

        for (PendingDeliveryMap::iterator iter = mPendingDelivery.begin(); iter != mPendingDelivery.end(); ++iter)
        {
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*iter).second;

          if (!processedInfo.mData) {
            ZS_LOG_TRACE(log("uploading message contains no data to upload"))
            return;
          }

          if (processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
            ZS_LOG_TRACE(log("message upload is already complete"))
            return;
          }

          if (processedInfo.mTempFileName.hasData()) {
            ZS_LOG_TRACE(log("delivering the message via local file instead") + ZS_PARAM("file", processedInfo.mTempFileName))
            return;
          }

          size_t totalToSend = processedInfo.mData->SizeInBytes();
          totalToSend -= processedInfo.mSent;

          if (totalToSend > mDeliveryMaxChunkSize) {
            totalToSend = mDeliveryMaxChunkSize;
          }

          ITCPMessaging::ChannelHeaderPtr header(new ITCPMessaging::ChannelHeader);
          header->mChannelID = processedInfo.mMessage.mChannelID;

          writer->write(&(processedInfo.mData->BytePtr()[processedInfo.mSent]), totalToSend, header);

          processedInfo.mSent += totalToSend;

          if (processedInfo.mSent != totalToSend) {
            ZS_LOG_TRACE(log("will wait for next write ready to send next chunk of data"))
            return;
          }

          // all data is now sent thus need to monitor for final result
          processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), processedInfo.mPendingDeliveryMessageUpdateRequest, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IDNSDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onLookupCompleted(IDNSQueryPtr query)
      {
        ZS_LOG_DEBUG(log("on DNS lookup complete") + ZS_PARAM("id", query->getID()))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IBackgroundingDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBackgroundingGoingToBackground(
                                                                       IBackgroundingSubscriptionPtr subscription,
                                                                       IBackgroundingNotifierPtr notifier
                                                                       )
      {
        ZS_LOG_DEBUG(log("going to background"))

        AutoRecursiveLock lock(*this);

        get(mBackgroundingEnabled) = true;

        mBackgroundingNotifier = notifier;

        step();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBackgroundingGoingToBackgroundNow(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("going to background now"))

        AutoRecursiveLock lock(*this);

        mBackgroundingNotifier.reset();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBackgroundingReturningFromBackground(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("returning from background"))

        AutoRecursiveLock lock(*this);

        get(mBackgroundingEnabled) = false;
        mBackgroundingNotifier.reset();

        step();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBackgroundingApplicationWillQuit(IBackgroundingSubscriptionPtr subscription)
      {
        ZS_LOG_DEBUG(log("application will quit"))

        AutoRecursiveLock lock(*this);

        setError(IHTTP::HTTPStatusCode_ClientClosedRequest, "application is quitting");
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IReachabilityDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onReachabilityChanged(
                                                            IReachabilitySubscriptionPtr subscription,
                                                            InterfaceTypes interfaceTypes
                                                            )
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on reachability changed") + ZS_PARAM("reachability", IReachability::toString(interfaceTypes)))

        // as network reachability has changed, it's okay to recheck the connection immediately
        mDoNotRetryConnectionBefore = Time();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onWake()
      {
        AutoRecursiveLock lock(*this);
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServiceLockboxSessionForInternalDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onServiceLockboxSessionStateChanged(ServiceLockboxSessionPtr session)
      {
        ZS_LOG_DEBUG(log("lockbox reported state change"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServiceNamespaceGrantSessionWaitDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onServiceNamespaceGrantSessionForServicesWaitComplete(IServiceNamespaceGrantSessionPtr session)
      {
        ZS_LOG_DEBUG(log("namespace grant session wait is ready to be obtained (if needed)"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IServiceNamespaceGrantSessionQueryDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onServiceNamespaceGrantSessionForServicesQueryComplete(
                                                                                             IServiceNamespaceGrantSessionQueryPtr query,
                                                                                             ElementPtr namespaceGrantChallengeBundleEl
                                                                                             )
      {
        ZS_LOG_DEBUG(log("namespace grant session wait is ready to be obtained (if needed)"))

        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<ServersGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         ServersGetResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("servers get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mServersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("servers get result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mServersGetMonitor.reset();

        mPushMailboxServers = result->servers();

        if (mPushMailboxServers.size() < 1) {
          ZS_LOG_ERROR(Detail, log("no push-mailbox servers were returned from servers-get") + IMessageMonitor::toDebug(monitor))
          connectionFailure();
          return true;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              ServersGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("servers get error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mServersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("error result received on servers get monitor") + IMessageMonitor::toDebug(monitor))

        mServersGetMonitor.reset();
        connectionFailure();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<AccessResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         AccessResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("access result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mPeerURIFromAccessResult = result->peerURI();
        mUploadMessageURL = result->uploadMessageURL();
        mUploadMessageStringReplacementMessageID = result->uploadMessageStringReplacementMessageID();
        mUploadMessageStringReplacementMessageSize = result->uploadMessageStringReplacementMessageSize();

        mNamespaceGrantChallengeInfo = result->namespaceGrantChallengeInfo();

        if (mNamespaceGrantChallengeInfo.mID.hasData()) {
          // a namespace grant challenge was issue
          mGrantQuery = mGrantSession->query(mThisWeak.lock(), mNamespaceGrantChallengeInfo);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              AccessResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("access error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("access request failed") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<NamespaceGrantChallengeValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         NamespaceGrantChallengeValidateResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("namespace grant challenge validate result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mGrantMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mGrantMonitor->cancel();
        mGrantMonitor.reset();

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              NamespaceGrantChallengeValidateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("namespace grant challenge validate error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mGrantMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("namespace grant request failed") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<PeerValidateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         PeerValidateResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("peer validate result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PeerValidateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("peer validate error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        if (monitor != mPeerValidateMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("peer validate request failed") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<FoldersGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         FoldersGetResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("folders get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mFoldersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mFoldersGetMonitor.reset();

        const PushMessageFolderInfoList &folders = result->folders();

        for (PushMessageFolderInfoList::const_iterator iter = folders.begin(); iter != folders.end(); ++iter)
        {
          const PushMessageFolderInfo &info = (*iter);

          String name = info.mName;

          if (PushMessageFolderInfo::Disposition_Remove == info.mDisposition) {
            ZS_LOG_DEBUG(log("folder was removed") + ZS_PARAM("folder name", name))

            int folderIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
            if (mDB->getFolderIndex(name, folderIndex)) {
              ZS_THROW_INVALID_ASSUMPTION_IF(folderIndex < 0)

              mDB->removeAllMessagesFromFolder(folderIndex);
              mDB->removeAllVersionedMessagesFromFolder(folderIndex);
              mDB->removeFolder(folderIndex);
            }

            mRefreshMonitorFolderCreation = true;
            continue;
          }

          if (info.mRenamed) {
            ZS_LOG_DEBUG(log("folder was renamed (removing old folder)") + ZS_PARAM("folder name", info.mName) + ZS_PARAM("new name", info.mRenamed))
            int folderIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
            if (mDB->getFolderIndex(info.mRenamed, folderIndex)) {
              ZS_THROW_INVALID_ASSUMPTION_IF(folderIndex < 0)

              mDB->removeAllMessagesFromFolder(folderIndex);
              mDB->removeAllVersionedMessagesFromFolder(folderIndex);
              mDB->removeFolder(folderIndex);
            }

            if (mDB->getFolderIndex(info.mName, folderIndex)) {
              ZS_THROW_INVALID_ASSUMPTION_IF(folderIndex < 0)
              mDB->updateFolderName(folderIndex, info.mName);
            }

            name = info.mRenamed;
            mRefreshMonitorFolderCreation = true;
          }

          ZS_LOG_DEBUG(log("folder was updated/added") + info.toDebug())
          mDB->addOrUpdateFolder(name, info.mVersion, info.mUnread, info.mTotal);
        }

        mDB->setLastDownloadedVersionForFolders(result->version());

        if (mNextFoldersUpdateTimer) {
          mNextFoldersUpdateTimer->cancel();
          mNextFoldersUpdateTimer.reset();
        }

        Time updateNextTime = result->updateNext();
        Duration updateNextDuration;
        if (Time() != updateNextTime) {
          Time now = zsLib::now();
          if (now < updateNextTime) {
            updateNextDuration = updateNextTime - now;
          } else {
            updateNextDuration = Seconds(1);
          }

          if (updateNextDuration < Seconds(1)) {
            updateNextDuration = Seconds(1);
          }
        }

        if (Duration() != updateNextDuration) {
          mNextFoldersUpdateTimer = Timer::create(mThisWeak.lock(), updateNextDuration, false);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              FoldersGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("folders get error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mFoldersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mFoldersGetMonitor.reset();

        if (IHTTP::HTTPStatusCode_Conflict == result->errorCode()) {
          ZS_LOG_WARNING(Detail, log("folders get version conflict") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

          mRefreshFolders = true;
          mRefreshFoldersNeedingUpdate = true;
          mRefreshMonitorFolderCreation = true;
          mDB->setLastDownloadedVersionForFolders(String());
          mLastActivity = zsLib::now();

          mDB->flushFolderVersionedMessages();
          mDB->flushFolderMessages();
          mDB->flushFolders();

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_ERROR(Detail, log("folders get failed") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();
        return true;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<FolderUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         FolderUpdateResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("folder update result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (mFolderCreationUpdateMonitor == monitor) {
          mFolderCreationUpdateMonitor.reset();

          ZS_LOG_DEBUG(log("folder was updated"))
          mRefreshFolders = true;
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_WARNING(Detail, log("received notification on obsolete monitor") + IMessageMonitor::toDebug(monitor))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              FolderUpdateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("folder update error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (mFolderCreationUpdateMonitor == monitor) {
          mFolderCreationUpdateMonitor.reset();

          ZS_LOG_ERROR(Detail, log("failed to create folder") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))
          connectionFailure();
          return true;
        }

        ZS_LOG_WARNING(Detail, log("received notification on obsolete monitor") + IMessageMonitor::toDebug(monitor))
        return false;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<FolderGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         FolderGetResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("folder get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        MonitorList::iterator found = std::find(mFolderGetMonitors.begin(), mFolderGetMonitors.end(), monitor);
        if (found == mFolderGetMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        // no longer monitoring this request
        mFolderGetMonitors.erase(found);

        FolderGetRequestPtr request = FolderGetRequest::convert(monitor->getMonitoredMessage());

        const PushMessageFolderInfo &originalFolderInfo = request->folderInfo();

        FolderUpdateMap::iterator foundNeedingUpdate = mFoldersNeedingUpdate.find(originalFolderInfo.mName);
        if (foundNeedingUpdate == mFoldersNeedingUpdate.end()) {
          ZS_LOG_WARNING(Detail, log("received information about folder update but folder no longer requires update") + originalFolderInfo.toDebug())
          return true;
        }

        ProcessedFolderNeedingUpdateInfo &updateInfo = (*foundNeedingUpdate).second;

        if (!updateInfo.mSentRequest) {
          ZS_LOG_WARNING(Detail, log("received information about folder update but folder did not issue request") + originalFolderInfo.toDebug())
          return true;
        }

        if (originalFolderInfo.mVersion != updateInfo.mInfo.mDownloadedVersion) {
          ZS_LOG_WARNING(Detail, log("received information about folder update but folder version conflict") + originalFolderInfo.toDebug() + ZS_PARAM("expecting", updateInfo.mInfo.mDownloadedVersion))
          return true;
        }

        const PushMessageFolderInfo &folderInfo = result->folderInfo();
        const PushMessageInfoList &messages = result->messages();

        // start processing messages
        for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter) {
          const PushMessageInfo &message = (*iter);

          switch (message.mDisposition) {
            case PushMessageInfo::Disposition_NA:
            case PushMessageInfo::Disposition_Update: {
              mRefreshFoldersNeedingUpdate = true;

              ZS_LOG_TRACE(log("message updated in folder") + ZS_PARAM("folder name", originalFolderInfo.mName) + ZS_PARAM("message id", message.mID))

              mDB->addMessageToFolderIfNotPresent(updateInfo.mInfo.mIndex, message.mID);
              mDB->addOrUpdateMessage(message.mID, message.mVersion);
              break;
            }
            case PushMessageInfo::Disposition_Remove: {
              ZS_LOG_TRACE(log("message removed in folder") + ZS_PARAM("folder name", originalFolderInfo.mName) + ZS_PARAM("message id", message.mID))

              mDB->addRemovedFolderVersionedMessageEntryIfMessageNotRemoved(updateInfo.mInfo.mIndex, message.mID);
              mDB->removeMessageFromFolder(updateInfo.mInfo.mIndex, message.mID);
              break;
            }
          }
        }

        mDB->updateFolderDownloadInfo(updateInfo.mInfo.mIndex, folderInfo.mVersion, folderInfo.mUpdateNext);

        mFoldersNeedingUpdate.erase(foundNeedingUpdate);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              FolderGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("folder get error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        MonitorList::iterator found = std::find(mFolderGetMonitors.begin(), mFolderGetMonitors.end(), monitor);
        if (found == mFolderGetMonitors.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        // no longer monitoring this request
        mFolderGetMonitors.erase(found);

        bool hasConnectionFailure = false;

        FolderGetRequestPtr request = FolderGetRequest::convert(monitor->getMonitoredMessage());

        const PushMessageFolderInfo &originalFolderInfo = request->folderInfo();

        FolderUpdateMap::iterator foundNeedingUpdate = mFoldersNeedingUpdate.find(originalFolderInfo.mName);
        if (foundNeedingUpdate != mFoldersNeedingUpdate.end()) {
          ProcessedFolderNeedingUpdateInfo updateInfo = (*foundNeedingUpdate).second;

          if (updateInfo.mInfo.mDownloadedVersion != request->folderInfo().mVersion) {
            ZS_LOG_DEBUG(log("already detected conflict, will refetch information later"))
          } else {
            if (result->errorCode() == IHTTP::HTTPStatusCode_Conflict) {

              updateInfo.mInfo.mDownloadedVersion = String();

              ZS_LOG_WARNING(Detail, log("folder version download conflict detected") + originalFolderInfo.toDebug())

              // this folder conflicted, purge it and reload it
              mDB->removeAllMessagesFromFolder(updateInfo.mInfo.mIndex);
              mDB->removeAllVersionedMessagesFromFolder(updateInfo.mInfo.mIndex);
              mDB->resetFolderUniqueID(updateInfo.mInfo.mIndex);

              mDB->updateFolderDownloadInfo(updateInfo.mInfo.mIndex, String(), zsLib::now());
            } else {
              hasConnectionFailure = true;
            }
          }

          ZS_LOG_DEBUG(log("folder get required but folder get failed") + ZS_PARAM("folder name", updateInfo.mInfo.mFolderName))
          updateInfo.mSentRequest = false;
        }

        mRefreshFolders = true;

        ZS_LOG_ERROR(Detail, log("failed to get messages for a folder") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        if (hasConnectionFailure) {
          connectionFailure();
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<MessagesDataGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         MessagesDataGetResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("messages data get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mMessageDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMessageDataGetMonitor.reset();

        const PushMessageInfoList &messages = result->messages();

        for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          const PushMessageInfo &info = (*iter);

          MessageDataMap::iterator found = mMessagesNeedingData.find(info.mID);
          if (found == mMessagesNeedingData.end()) {
            ZS_LOG_WARNING(Detail, log("received information about a message that was not requested") + ZS_PARAM("message id", info.mID))
            continue;
          }

          ProcessedMessageNeedingDataInfo &processedInfo = (*found).second;

          processedInfo.mChannelID = info.mChannelID;

          // remember the mapping for the channel
          mMessagesNeedingDataChannels[info.mChannelID] = info.mID;
        }

        for (MessageDataMap::iterator iter_doNotUse = mMessagesNeedingData.begin(); iter_doNotUse != mMessagesNeedingData.end(); )
        {
          MessageDataMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedMessageNeedingDataInfo &processedInfo = (*current).second;
          if (!processedInfo.mSentRequest) {
            ZS_LOG_TRACE(log("this message was not part of the request") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))
            continue;
          }

          if (0 != processedInfo.mChannelID) {
            ZS_LOG_TRACE(log("this message received download channel information") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))
            continue;
          }

          // this message failed to download
          ZS_LOG_WARNING(Detail, log("message data failed to download") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))

          handleMessageGone(processedInfo.mInfo.mMessageID);

          mRefreshFolders = true;
          mRefreshFoldersNeedingUpdate = true;
          mRefreshMessagesNeedingData = true;

          mMessagesNeedingData.erase(current);
        }

        processChannelMessages();

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              MessagesDataGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("messages data get error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mMessageDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("failed to get message data") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();

        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<MessagesMetaDataGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         MessagesMetaDataGetResultPtr result
                                                                         )
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfo DeliveryInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfoList DeliveryInfoList;

        ZS_LOG_DEBUG(log("messages meta data get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor == mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
          mPendingDeliveryPrecheckMessageMetaDataGetMonitor.reset();

          const PushMessageInfoList &messages = result->messages();

          for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
          {
            const PushMessageInfo &info = (*iter);

            PendingDeliveryMap::iterator found = mPendingDelivery.find(info.mID);
            if (found == mPendingDelivery.end()) {
              ZS_LOG_WARNING(Detail, log("pre-checked message that is not pending delivery") + ZS_PARAM("message ID", info.mID))
              continue;
            }

            ProcessedPendingDeliveryMessageInfo &processedInfo = (*found).second;

            ZS_LOG_WARNING(Detail, log("no need to re-upload message as server already has message") + ZS_PARAM("message ID", info.mID) + ZS_PARAM("pending index", processedInfo.mInfo.mIndex))

            mDB->removePendingDeliveryMessage(processedInfo.mInfo.mIndex);

            SendQueryMap::iterator foundQuery = mSendQueries.find(info.mID);
            if (foundQuery != mSendQueries.end()) {
              SendQueryPtr query = (*foundQuery).second.lock();

              if (query) {
                query->notifyUploaded();
                query->notifyDeliveryInfoChanged(info.mFlags);
              } else {
                mSendQueries.erase(foundQuery);
              }
            }

            mPendingDelivery.erase(found);
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        if (monitor != mMessageMetaDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMessageMetaDataGetMonitor.reset();

        const PushMessageInfoList &messages = result->messages();

        for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          const PushMessageInfo &info = (*iter);

          MessageUpdateMap::iterator found = mMessagesNeedingUpdate.find(info.mID);
          if (found == mMessagesNeedingUpdate.end()) {
            ZS_LOG_WARNING(Detail, log("received information about a message that was not requested") + ZS_PARAM("message id", info.mID))
            continue;
          }

          ProcessedMessageNeedingUpdateInfo &processedInfo = (*found).second;

          // check if this message that was updated is related to any active send query
          SendQueryMap::iterator foundQuery = mSendQueries.find(info.mID);
          if (foundQuery != mSendQueries.end()) {
            SendQueryPtr query = (*foundQuery).second.lock();

            if (query) {
              query->notifyDeliveryInfoChanged(info.mFlags);
            } else {
              mSendQueries.erase(foundQuery);
            }
          }

          mDB->removeMessageDeliveryStatesForMessage(processedInfo.mInfo.mIndex);

          for (PushMessageInfo::FlagInfoMap::const_iterator flagIter = info.mFlags.begin(); flagIter != info.mFlags.end(); ++flagIter) {

            const PushMessageInfo::FlagInfo &flagInfo = (*flagIter).second;

            DeliveryInfoList deliveryInfos;

            for (PushMessageInfo::FlagInfo::URIInfoList::const_iterator uriIter = flagInfo.mFlagURIInfos.begin(); uriIter != flagInfo.mFlagURIInfos.end(); ++uriIter) {
              const PushMessageInfo::FlagInfo::URIInfo &uriInfo = *uriIter;

              DeliveryInfo deliveryInfo;
              deliveryInfo.mURI = uriInfo.mURI;
              deliveryInfo.mErrorCode = uriInfo.mErrorCode;
              deliveryInfo.mErrorReason = uriInfo.mErrorReason;

              deliveryInfos.push_back(deliveryInfo);
            }

            mDB->updateMessageDeliverStateForMessage(
                                                     processedInfo.mInfo.mIndex,
                                                     PushMessageInfo::FlagInfo::toString(flagInfo.mFlag),
                                                     deliveryInfos
                                                     );
          }

          PushInfoList pushInfos;
          copy(info.mPushInfos, pushInfos);

          mDB->updateMessage(
                             processedInfo.mInfo.mIndex,
                             info.mVersion,
                             info.mTo,
                             info.mFrom,
                             info.mCC,
                             info.mBCC,
                             info.mType,
                             info.mMimeType,
                             info.mEncoding,
                             info.mPushType,
                             pushInfos,
                             info.mSent,
                             info.mExpires,
                             info.mLength,
                             true
                             );

          mRefreshVersionedFolders = true;

          {
            String listID =  extractListID(info.mTo);
            if (listID.hasData()) mDB->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mFrom);
            if (listID.hasData()) mDB->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mCC);
            if (listID.hasData()) mDB->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mBCC);
            if (listID.hasData()) mDB->addOrUpdateListID(listID);
          }

          mRefreshListsNeedingDownload = true;
          mRefreshKeysFolderNeedsProcessing = true;

          // clear out the message since it's been processed
          mMessagesNeedingUpdate.erase(found);
        }

        for (MessageUpdateMap::iterator iter_doNotUse = mMessagesNeedingUpdate.begin(); iter_doNotUse != mMessagesNeedingUpdate.end(); )
        {
          MessageUpdateMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedMessageNeedingUpdateInfo &processedInfo = (*current).second;
          if (!processedInfo.mSentRequest) {
            ZS_LOG_TRACE(log("this message was not part of the request") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))
            continue;
          }

          // this message failed to download
          ZS_LOG_WARNING(Detail, log("message failed to download") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))

          handleMessageGone(processedInfo.mInfo.mMessageID);

          mRefreshFolders = true;
          mRefreshFoldersNeedingUpdate = true;
          mRefreshMessagesNeedingUpdate = true;

          mMessagesNeedingUpdate.erase(current);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              MessagesMetaDataGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("messages meta data get error result received") + IMessageMonitor::toDebug(monitor))

        ZS_LOG_ERROR(Detail, log("failed to get message metadata") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        AutoRecursiveLock lock(*this);
        if (monitor == mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
          connectionFailure();
          return true;
        }

        if (monitor != mMessageMetaDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        connectionFailure();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<MessageUpdateResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         MessageUpdateResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("message update result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        for (PendingDeliveryMap::iterator iter_doNotUse = mPendingDelivery.begin(); iter_doNotUse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (monitor == processedInfo.mPendingDeliveryMessageUpdateErrorMonitor) {
            ZS_LOG_TRACE(log("message upload error monitor completed") + IMessageMonitor::toDebug(monitor))
            processedInfo.mPendingDeliveryMessageUpdateErrorMonitor->cancel();
            processedInfo.mPendingDeliveryMessageUpdateErrorMonitor.reset();

            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
            return false;
          }

          if (monitor == processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor.reset();
            processedInfo.mPendingDeliveryMessageUpdateRequest.reset();

            ZS_THROW_BAD_STATE_IF(mPendingDelivery.size() < 1)

            ZS_LOG_DEBUG(log("message upload complete") + processedInfo.mMessage.toDebug() + ZS_PARAM("pending index", processedInfo.mInfo.mIndex))
            mDB->removePendingDeliveryMessage(processedInfo.mInfo.mIndex);

            SendQueryMap::iterator foundQuery = mSendQueries.find(processedInfo.mMessage.mID);
            if (foundQuery != mSendQueries.end()) {
              SendQueryPtr query = (*foundQuery).second.lock();
              if (query) {
                query->notifyUploaded();
              } else {
                mSendQueries.erase(foundQuery);
              }
            }

            mPendingDelivery.erase(current);
            
            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
            return true;
          }
        }

        for (MonitorList::iterator iter_doNotUse = mMarkingMonitors.begin(); iter_doNotUse != mMarkingMonitors.end(); )
        {
          MonitorList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          IMessageMonitorPtr markingMonitor = (*current);
          if (markingMonitor != monitor) continue;

          ZS_LOG_DEBUG(log("marking monitor is complete") + IMessageMonitor::toDebug(monitor))

          MessageUpdateRequestPtr request = MessageUpdateRequest::convert(monitor->getMonitoredMessage());

          handelUpdateMessageGone(request, false);

          if (PushMessageInfo::Disposition_Update == request->messageInfo().mDisposition) {
            MessageMap::iterator found = mMessagesToMarkRead.find(request->messageInfo().mID);
            if (found != mMessagesToMarkRead.end()) {
              ZS_LOG_DEBUG(log("message is now marked read") + ZS_PARAM("message id", request->messageInfo().mID))
              mMessagesToMarkRead.erase(found);
            }
          }

          mMarkingMonitors.erase(current);

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }
        
        ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              MessageUpdateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("message update error result received") + IMessageMonitor::toDebug(monitor))

        ZS_LOG_ERROR(Detail, log("failed to update message") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        AutoRecursiveLock lock(*this);
        for (PendingDeliveryMap::iterator iter_doNotUse = mPendingDelivery.begin(); iter_doNotUse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if ((monitor == processedInfo.mPendingDeliveryMessageUpdateErrorMonitor) ||
              (monitor == processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor)) {
            connectionFailure();
            return true;
          }
        }

        for (MonitorList::iterator iter_doNotUse = mMarkingMonitors.begin(); iter_doNotUse != mMarkingMonitors.end(); )
        {
          MonitorList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          IMessageMonitorPtr markingMonitor = (*current);
          if (markingMonitor != monitor) continue;

          ZS_LOG_DEBUG(log("marking monitor is complete") + IMessageMonitor::toDebug(monitor))

          mMarkingMonitors.erase(current);

          if (IHTTP::HTTPStatusCode_NotFound == result->errorCode()) {
            handelUpdateMessageGone(MessageUpdateRequest::convert(monitor->getMonitoredMessage()), IHTTP::HTTPStatusCode_NotFound == result->errorCode());
          } else {
            connectionFailure();
          }

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }
        
        ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<ListFetchResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         ListFetchResultPtr result
                                                                         )
      {
        typedef ListFetchResult::URIListMap URIListMap;
        typedef ListFetchResult::URIList URIList;

        ZS_LOG_DEBUG(log("list fetch result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mListFetchMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        URIListMap uriLists = result->listToURIs();

        for (URIListMap::const_iterator iter = uriLists.begin(); iter != uriLists.end(); ++iter)
        {
          const String &listID = (*iter).first;
          const URIList &uris = (*iter).second;

          ListDownloadMap::iterator found = mListsNeedingDownload.find(listID);
          if (found == mListsNeedingDownload.end()) {
            ZS_LOG_WARNING(Detail, log("list was returned that did not need downloading") + ZS_PARAM("list id", listID))
            continue;
          }

          ProcessedListsNeedingDownloadInfo &processedInfo = (*found).second;
          if (!processedInfo.mSentRequest) {
            ZS_LOG_WARNING(Detail, log("whose download was not requested") + ZS_PARAM("list id", listID))
            continue;
          }

          mDB->updateListURIs(processedInfo.mInfo.mIndex, uris);
          mDB->notifyListDownloaded(processedInfo.mInfo.mIndex);

          mListsNeedingDownload.erase(found);
        }

        for (ListDownloadMap::iterator iter_doNotUse = mListsNeedingDownload.begin(); iter_doNotUse != mListsNeedingDownload.end();)
        {
          ListDownloadMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedListsNeedingDownloadInfo &processedInfo = (*current).second;
          if (!processedInfo.mSentRequest) {
            ZS_LOG_TRACE(log("did not issue download request yet for this list") + ZS_PARAM("list id", processedInfo.mInfo.mListID))
            continue;
          }

          mDB->notifyListFailedToDownload(processedInfo.mInfo.mIndex);

          mListsNeedingDownload.erase(current);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              ListFetchResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("list fetch error result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);
        if (monitor != mListFetchMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("failed to get lists") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();

        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePushMailboxSession");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        IHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());
        IHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        IHelper::debugAppend(resultEl, "db", (bool)mDB);

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));

        IHelper::debugAppend(resultEl, "backgrounding subscription id", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);
        IHelper::debugAppend(resultEl, "backgrounding notifier", (bool)mBackgroundingNotifier);
        IHelper::debugAppend(resultEl, "backgrounding enabled", mBackgroundingEnabled);

        IHelper::debugAppend(resultEl, "reachability subscription", mReachabilitySubscription ? mReachabilitySubscription->getID() : 0);

        IHelper::debugAppend(resultEl, "lockbox subscription", mLockboxSubscription ? mLockboxSubscription->getID() : 0);

        IHelper::debugAppend(resultEl, "sent via object id", mSentViaObjectID);

        IHelper::debugAppend(resultEl, "tcp messaging id", mTCPMessaging ? mTCPMessaging->getID() : 0);
        IHelper::debugAppend(resultEl, "wire stream", mWireStream ? mWireStream->getID() : 0);

        IHelper::debugAppend(resultEl, "reader id", mReader ? mReader->getID() : 0);
        IHelper::debugAppend(resultEl, "writer id", mWriter ? mWriter->getID() : 0);

        IHelper::debugAppend(resultEl, "lockbox", mLockbox ? mLockbox->getID() : 0);
        IHelper::debugAppend(resultEl, mLockboxInfo.toDebug());
        IHelper::debugAppend(resultEl, IPeerFiles::toDebug(mPeerFiles));

        IHelper::debugAppend(resultEl, "grant session id", mGrantSession ? mGrantSession->getID() : 0);
        IHelper::debugAppend(resultEl, "grant query id", mGrantQuery ? mGrantQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "grant wait id", mGrantWait ? mGrantWait->getID() : 0);

        IHelper::debugAppend(resultEl, "obtained lock", mObtainedLock);

        IHelper::debugAppend(resultEl, "required connection", mRequiresConnection);
        IHelper::debugAppend(resultEl, "inactivty timeout (s)", mInactivityTimeout);
        IHelper::debugAppend(resultEl, "last activity", mLastActivity);

        IHelper::debugAppend(resultEl, "default last try duration (ms)", mDefaultLastRetryDuration);
        IHelper::debugAppend(resultEl, "last try duration (ms)", mLastRetryDuration);
        IHelper::debugAppend(resultEl, "do not retry connection before", mDoNotRetryConnectionBefore);
        IHelper::debugAppend(resultEl, "retry timer", mRetryTimer ? mRetryTimer->getID() : 0);

        IHelper::debugAppend(resultEl, "server list", mPushMailboxServers.size());
        IHelper::debugAppend(resultEl, "server lookup id", mServerLookup ? mServerLookup->getID() : 0);
        IHelper::debugAppend(resultEl, "server srv record", (bool)mServerSRV);
        IHelper::debugAppend(resultEl, "server ip", string(mServerIP));

        IHelper::debugAppend(resultEl, "peer uri from access result", mPeerURIFromAccessResult);

        IHelper::debugAppend(resultEl, mNamespaceGrantChallengeInfo.toDebug());

        IHelper::debugAppend(resultEl, "servers get monitor", mServersGetMonitor ? mServersGetMonitor->getID() : 0);
        IHelper::debugAppend(resultEl, "access monitor", mAccessMonitor ? mAccessMonitor->getID() : 0);
        IHelper::debugAppend(resultEl, "grant monitor", mGrantMonitor ? mGrantMonitor->getID() : 0);
        IHelper::debugAppend(resultEl, "peer validate monitor", mPeerValidateMonitor ? mPeerValidateMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "device token", mDeviceToken);
        IHelper::debugAppend(resultEl, "register queries", mRegisterQueries.size());

        IHelper::debugAppend(resultEl, "send queries", mSendQueries.size());

        IHelper::debugAppend(resultEl, "refresh folders", mRefreshFolders);
        IHelper::debugAppend(resultEl, "monitored folders", mMonitoredFolders.size());
        IHelper::debugAppend(resultEl, "notified monitored folders", mNotifiedMonitoredFolders.size());
        IHelper::debugAppend(resultEl, "folders get monitor", mFoldersGetMonitor ? mFoldersGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "next folder update timer", mNextFoldersUpdateTimer ? mNextFoldersUpdateTimer->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh folders needing update", mRefreshFoldersNeedingUpdate);
        IHelper::debugAppend(resultEl, "folders needing update", mFoldersNeedingUpdate.size());
        IHelper::debugAppend(resultEl, "folder get monitors", mFolderGetMonitors.size());

        IHelper::debugAppend(resultEl, "refresh monitored folder creation", mRefreshMonitorFolderCreation);
        IHelper::debugAppend(resultEl, "folder creation monitor", mFolderCreationUpdateMonitor ? mFolderCreationUpdateMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh messages needing update", mRefreshMessagesNeedingUpdate);
        IHelper::debugAppend(resultEl, "messages needing update", mMessagesNeedingUpdate.size());
        IHelper::debugAppend(resultEl, "messages metadata get monitor", mMessageMetaDataGetMonitor ? mMessageMetaDataGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh messages needing data", mRefreshMessagesNeedingData);
        IHelper::debugAppend(resultEl, "messages needing data", mMessagesNeedingData.size());
        IHelper::debugAppend(resultEl, "messages needing data channels", mMessagesNeedingDataChannels.size());
        IHelper::debugAppend(resultEl, "messages metadata get monitor", mMessageDataGetMonitor ? mMessageDataGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "pending channel data", mPendingChannelData.size());

        IHelper::debugAppend(resultEl, "refresh lists needing download", mRefreshListsNeedingDownload);
        IHelper::debugAppend(resultEl, "lists to download", mListsNeedingDownload.size());
        IHelper::debugAppend(resultEl, "list fetch monitor", mListFetchMonitor ? mListFetchMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh keys folder needs processing", mRefreshKeysFolderNeedsProcessing);

        IHelper::debugAppend(resultEl, "refresh messages needing decryption", mRefreshMessagesNeedingDecryption);

        IHelper::debugAppend(resultEl, "refresh pending delivery", mRefreshPendingDelivery);
        IHelper::debugAppend(resultEl, "max delivery chunk size", mDeliveryMaxChunkSize);
        IHelper::debugAppend(resultEl, "last channel", mLastChannel);
        IHelper::debugAppend(resultEl, "pending delivery pre-check required", mPendingDeliveryPrecheckRequired);
        IHelper::debugAppend(resultEl, "pending delivery", mPendingDelivery.size());
        IHelper::debugAppend(resultEl, "pending delivery pre-check messages metadata get monitor", mPendingDeliveryPrecheckMessageMetaDataGetMonitor ? mPendingDeliveryPrecheckMessageMetaDataGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh versioned folders", mRefreshVersionedFolders);

        IHelper::debugAppend(resultEl, "messages to mark read", mMessagesToMarkRead.size());
        IHelper::debugAppend(resultEl, "messages to remove", mMessagesToRemove.size());
        IHelper::debugAppend(resultEl, "marking monitors", mMarkingMonitors.size());

        IHelper::debugAppend(resultEl, "refresh needing expiry", mRefreshMessagesNeedingExpiry);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::step()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("step - already shutting down / shutdown"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(debug("step"))

        if (!stepBootstrapper()) goto post_step;
        if (!stepGrantLock()) goto post_step;

        if (!stepShouldConnect()) goto post_step;
        if (!stepDNS()) goto post_step;
        if (!stepLockboxAccess()) goto post_step;

        if (!stepConnect()) goto post_step;
        if (!stepRead()) goto post_step;

        if (!stepAccess()) goto post_step;

        if (!stepGrantLockClear()) goto post_step;
        if (!stepGrantChallenge()) goto post_step;
        if (!stepPeerValidate()) goto post_step;

        if (!stepFullyConnected()) goto post_step;

        if (!stepRefreshFolders()) goto post_step;
        if (!stepCheckFoldersNeedingUpdate()) goto post_step;
        if (!stepFolderGet()) goto post_step;

        if (!stepMakeMonitoredFolders()) goto post_step;

        if (!stepCheckMessagesNeedingUpdate()) goto post_step;
        if (!stepMessagesMetaDataGet()) goto post_step;

        if (!stepCheckMessagesNeedingData()) goto post_step;
        if (!stepMessagesDataGet()) goto post_step;

        if (!stepCheckListNeedingDownload()) goto post_step;
        if (!stepListFetch()) goto post_step;

        if (!stepProcessKeysFolder()) goto post_step;
        if (!stepDecryptMessages()) goto post_step;

        if (!stepPendingDelivery()) goto post_step;
        if (!stepPendingDeliveryUpdateRequest()) goto post_step;
        if (!stepDeliverViaURL()) goto post_step;

        if (!stepPrepareVersionedFolderMessages()) goto post_step;

        if (!stepMarkReadOrRemove()) goto post_step;

        if (!stepExpireMessages()) goto post_step;

        if (!stepBackgroundingReady()) goto post_step;

      post_step:
        postStep();

        ZS_LOG_TRACE(debug("step done"))
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepBootstrapper()
      {
        if (!mBootstrappedNetwork->isPreparationComplete()) {
          setState(SessionState_Pending);

          ZS_LOG_TRACE(log("waiting for preparation of lockbox bootstrapper to complete"))
          return false;
        }

        WORD errorCode = 0;
        String reason;

        if (mBootstrappedNetwork->wasSuccessful(&errorCode, &reason)) {
          ZS_LOG_TRACE(log("lockbox bootstrapper was successful"))
          return true;
        }

        ZS_LOG_ERROR(Detail, log("bootstrapped network failed for lockbox") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

        setError(errorCode, reason);
        cancel();
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepGrantLock()
      {
        if (mObtainedLock) {
          ZS_LOG_TRACE(log("already informed browser window ready thus no need to make sure grant wait lock is obtained"))
          return true;
        }

        if (mGrantWait) {
          ZS_LOG_TRACE(log("grant wait lock is already obtained"))
          return true;
        }

        // NOTE: While the lockbox service itself doesn't load a browser window
        // and thus does not need to obtain a grant wait directly, it can cause
        // a namespace grant challenge at the same time the rolodex causes a
        // grant challenge, so it's better to cause all namespace grants to
        // happen at once rather than asking the user to issue a grant
        // permission twice.

        mGrantWait = mGrantSession->obtainWaitToProceed(mThisWeak.lock());

        if (!mGrantWait) {
          ZS_LOG_TRACE(log("waiting to obtain grant wait lock"))
          return false;
        }

        ZS_LOG_DEBUG(log("obtained grant lock"))

        get(mObtainedLock) = true;
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepShouldConnect()
      {
        ZS_LOG_TRACE(log("step should connect"))

        Time now = zsLib::now();

        bool activity = (Time() != mLastActivity) && (mLastActivity + mInactivityTimeout > now);
        bool shouldConnect = ((activity) ||
                              (mRequiresConnection));

        if (mBackgroundingEnabled) {
          if (!mBackgroundingNotifier) {
            ZS_LOG_DEBUG(log("going to background thus should not be connected"))
            shouldConnect = false;
          }
        }

        if (!shouldConnect) {
          if (!mTCPMessaging) {
            setState(SessionState_Sleeping);
            ZS_LOG_TRACE(log("no need for connection and TCP messaging is already shutdown"))
            return false;
          }

          // attempt to shutdown the TCP messaging channel
          ZS_LOG_TRACE(log("telling TCP messaging to shutdown (due to inactivity)"))
          mTCPMessaging->shutdown();

          if (ITCPMessaging::SessionState_Shutdown != mTCPMessaging->getState()) {
            ZS_LOG_TRACE(log("waiting to shutdown TCP messaging"))
            setState(SessionState_GoingToSleep);
            return false;
          }

          ZS_LOG_DEBUG(log("TCP messaging is now shutdown"))

          mTCPMessaging.reset();
          setState(SessionState_Sleeping);

          connectionReset();
          return false;
        }

        ZS_LOG_TRACE(log("connection is required due to activity"))

        if (Time() != mDoNotRetryConnectionBefore) {
          if (now < mDoNotRetryConnectionBefore) {
            ZS_LOG_TRACE(log("waiting to retry TCP connection later"))
            return false;
          }

          ZS_LOG_DEBUG(log("okay to retry connection now"))
          mDoNotRetryConnectionBefore = Time();
        }

        if (!mTCPMessaging) {
          setState(SessionState_Connecting);
        } else {
          if (ITCPMessaging::SessionState_Connected != mTCPMessaging->getState()) {
            setState(SessionState_Connecting);
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepDNS()
      {
        if (mServersGetMonitor) {
          ZS_LOG_TRACE(log("waiting for services get result"))
          return false;
        }

        if (mTCPMessaging) {
          ZS_LOG_TRACE(log("already have a TCP connection thus no need to perform DNS"))
          return true;
        }

        if (mServerLookup) {
          if (!mServerLookup->isComplete()) {
            ZS_LOG_TRACE(log("waiting for DNS query to complete"))
            return false;
          }

          mServerSRV = mServerLookup->getSRV();

          mServerLookup->cancel();
          mServerLookup.reset();
        }

        if (!mServerIP.isEmpty()) {
          ZS_LOG_TRACE(log("already have a server IP"))
          return true;
        }

        if (mServerSRV) {
          if (!IDNS::extractNextIP(mServerSRV, mServerIP)) {
            mServerSRV = SRVResultPtr();
          }

          if (!mServerIP.isEmpty()) {
            ZS_LOG_DEBUG(log("not have a server IP") + ZS_PARAM("IP", string(mServerIP)))
            return true;
          }
        }

        String srv;

        while ((srv.isEmpty()) &&
               (mPushMailboxServers.size() > 0)) {
          Server server = mPushMailboxServers.front();
          mPushMailboxServers.pop_front();

          srv = getMultiplexedJsonTCPTransport(server);
          if (srv.hasData()) {
            mServerLookup = IDNS::lookupSRV(mThisWeak.lock(), srv, "push-mailbox", "tcp");
          }
        }

        if (mServerLookup) {
          ZS_LOG_DEBUG(log("waiting for DNS query to complete"))
          return true;
        }

        ServersGetRequestPtr request = ServersGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());
        request->type(OPENPEER_STACK_SERVER_TYPE_PUSH_MAILBOX);
        request->totalFinders(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_TOTAL_SERVERS_TO_GET));

        mServersGetMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<ServersGetResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mBootstrappedNetwork), "bootstrapped-servers", "servers-get", request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        ZS_LOG_DEBUG(log("attempting to get push mailbox servers"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepLockboxAccess()
      {
        ZS_LOG_TRACE(log("step - lockbox access"))

        WORD errorCode = 0;
        String reason;
        switch (mLockbox->getState(&errorCode, &reason)) {
          case IServiceLockboxSession::SessionState_Pending:
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration:
          case IServiceLockboxSession::SessionState_Ready:
          {
            break;
          }
          case IServiceLockboxSession::SessionState_Shutdown:
          {
            ZS_LOG_ERROR(Detail, log("lockbox session shutdown thus shutting down mailbox") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        if (!mLockboxSubscription) {
          ZS_LOG_DEBUG(log("now subscribing to lockbox state"))
          mLockboxSubscription = mLockbox->subscribe(mThisWeak.lock());
        }

        if (!mLockboxInfo.mAccessSecret.hasData()) {
          mLockboxInfo = mLockbox->getLockboxInfo();
          if (mLockboxInfo.mAccessSecret.hasData()) {
            ZS_LOG_DEBUG(log("obtained lockbox access secret"))
          }
        }

        if (mLockboxInfo.mAccessSecret.hasData()) {
          ZS_LOG_TRACE(log("now have lockbox acess secret"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting on lockbox to login"))
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepConnect()
      {
        ZS_LOG_TRACE(log("step connect"))

        if (mTCPMessaging) {
          WORD errorCode = 0;
          String reason;
          switch (mTCPMessaging->getState(&errorCode, &reason)) {
            case ITCPMessaging::SessionState_Pending:
            {
              ZS_LOG_TRACE(log("waiting for TCP messaging to connect"))
              return false;
            }
            case ITCPMessaging::SessionState_Connected:
            {
              ZS_LOG_TRACE(log("TCP messaging is connected"))

              mLastRetryDuration = mDefaultLastRetryDuration;
              mDoNotRetryConnectionBefore = Time();
              break;
            }
            case ITCPMessaging::SessionState_ShuttingDown:
            case ITCPMessaging::SessionState_Shutdown:
            {
              ZS_LOG_WARNING(Detail, log("TCP messaging unexepectedly shutdown") + ZS_PARAM("error", errorCode) + ZS_PARAM("reason", reason))

              mTCPMessaging.reset();

              connectionFailure();
              return false;
            }
          }

          return true;
        }

        ZS_LOG_DEBUG(log("issuing TCP connection request to server") + ZS_PARAM("IP", string(mServerIP)))

        if (mWireStream) {
          mWireStream->cancel();
          mWireStream.reset();
        }

        mWireStream = ITransportStream::create(mThisWeak.lock(), mThisWeak.lock());

#define CHANGE_WHEN_MLS_USED 1
#define CHANGE_WHEN_MLS_USED 2

        mReader = mWireStream->getReader();
        mWriter = mWireStream->getWriter();

        mWireStream->getReader()->notifyReaderReadyToRead();

        mTCPMessaging = ITCPMessaging::connect(mThisWeak.lock(), mWireStream, mWireStream, true, mServerIP);

        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepRead()
      {
        if (!mReader) {
          ZS_LOG_TRACE(log("step read - no reader present"))
          return false;
        }

        while (true) {
          ITransportStream::StreamHeaderPtr header;

          SecureByteBlockPtr buffer = mReader->read(&header);
          ITCPMessaging::ChannelHeaderPtr channelHeader = ITCPMessaging::ChannelHeader::convert(header);

          if (!buffer) {
            ZS_LOG_TRACE(log("no data read"))
            return true;
          }

          if (0 != channelHeader->mChannelID) {
            handleChannelMessage(channelHeader->mChannelID, buffer);
            continue;
          }

          const char *bufferStr = (CSTR)(buffer->BytePtr());

          if (0 == strcmp(bufferStr, "\n")) {
            ZS_LOG_TRACE(log("received new line ping"))
            continue;
          }

          DocumentPtr document = Document::createFromAutoDetect(bufferStr);
          message::MessagePtr message = Message::create(document, mThisWeak.lock());

          if (ZS_IS_LOGGING(Detail)) {
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("PUSH MAILBOX RECEIVED MESSAGE") + ZS_PARAM("json in", bufferStr))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
            ZS_LOG_BASIC(log("<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<<[oo]<"))
            ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          }

          if (!message) {
            ZS_LOG_WARNING(Detail, log("failed to create a message from the document"))
            continue;
          }

          if (IMessageMonitor::handleMessageReceived(message)) {
            ZS_LOG_DEBUG(log("message requester handled the message"))
            continue;
          }

          // scope: handle changed notifications
          {
            ChangedNotifyPtr notify = ChangedNotify::convert(message);
            if (notify) {
              handleChanged(notify);
              continue;
            }
          }

          // scope: handle list fetch requests
          {
            ListFetchRequestPtr request = ListFetchRequest::convert(message);
            if (request) {
              handleListFetch(request);
              continue;
            }
          }

          ZS_LOG_WARNING(Detail, log("message was not handled") + Message::toDebug(message))
        }
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepAccess()
      {
        if (mAccessMonitor) {
          if (!mAccessMonitor->isComplete()) {
            ZS_LOG_TRACE(log("waiting for access monitor to complete"))
            return false;
          }

          // access monitor is complete
          ZS_LOG_TRACE(log("access monitor is already complete"))
          return true;
        }

        ZS_LOG_DEBUG(log("issuing lockbox access request"))

        AccessRequestPtr request = AccessRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());
        request->lockboxInfo(mLockboxInfo);
        request->grantID(mGrantSession->getGrantID());

        mAccessMonitor = sendRequest(IMessageMonitorResultDelegate<AccessResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepGrantLockClear()
      {
        if (!mGrantWait) {
          ZS_LOG_TRACE(log("wait already cleared"))
          return true;
        }

        ZS_LOG_DEBUG(log("clearing grant wait"))

        mGrantWait->cancel();
        mGrantWait.reset();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepGrantChallenge()
      {
        if (mGrantMonitor) {
          ZS_LOG_TRACE(log("waiting for lockbox namespace grant challenge validate monitor to complete"))
          return false;
        }

        if (!mGrantQuery) {
          ZS_LOG_TRACE(log("no grant challenge query thus continuing..."))
          return true;
        }

        if (!mGrantQuery->isComplete()) {
          ZS_LOG_TRACE(log("waiting for the grant query to complete"))
          return false;
        }

        ElementPtr bundleEl = mGrantQuery->getNamespaceGrantChallengeBundle();
        if (!bundleEl) {
          ZS_LOG_ERROR(Detail, log("namespaces were no granted in challenge"))
          setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to access lockbox");
          cancel();
          return false;
        }

        const NamespaceInfoMap &namespaces = mNamespaceGrantChallengeInfo.mNamespaces;

        for (NamespaceInfoMap::const_iterator iter = namespaces.begin(); iter != namespaces.end(); ++iter)
        {
          const NamespaceInfo &namespaceInfo = (*iter).second;

          if (!mGrantSession->isNamespaceURLInNamespaceGrantChallengeBundle(bundleEl, namespaceInfo.mURL)) {
            ZS_LOG_WARNING(Detail, log("push mailbox was not granted required namespace") + ZS_PARAM("namespace", namespaceInfo.mURL))
            setError(IHTTP::HTTPStatusCode_Forbidden, "namespaces were not granted to push mailbox");
            cancel();
            return false;
          }
        }

        mGrantQuery->cancel();
        mGrantQuery.reset();

        ZS_LOG_DEBUG(log("all namespaces required were correctly granted, notify the push mailbox of the newly created access"))

        NamespaceGrantChallengeValidateRequestPtr request = NamespaceGrantChallengeValidateRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->namespaceGrantChallengeBundle(bundleEl);

        mGrantMonitor = sendRequest(IMessageMonitorResultDelegate<NamespaceGrantChallengeValidateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPeerValidate()
      {
        if (!mPeerFiles) {
          mPeerFiles = mLockbox->getPeerFiles();
          if (!mPeerFiles) {
            ZS_LOG_TRACE(log("waiting for lockbox to have peer files"))
            return false;
          }

          ZS_LOG_DEBUG(log("obtained peer files"))
        }

        IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();

        if (mPeerURIFromAccessResult == peerFilePublic->getPeerURI()) {
          ZS_LOG_TRACE(log("no need to issue peer validate request"))
          return true;
        }

        if (mPeerValidateMonitor) {
          if (!mPeerValidateMonitor->isComplete()) {
            ZS_LOG_TRACE(log("waiting for peer validation to complete"))
            return false;
          }

          ZS_LOG_DEBUG(log("peer validation completed"))

          mPeerValidateMonitor->cancel();
          mPeerValidateMonitor.reset();

          return true;
        }

        ZS_LOG_DEBUG(log("issuing peer validate request"))

        PeerValidateRequestPtr request = PeerValidateRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->lockboxInfo(mLockboxInfo);
        request->peerFiles(mPeerFiles);

        mPeerValidateMonitor = sendRequest(IMessageMonitorResultDelegate<PeerValidateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
        return false;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepFullyConnected()
      {
        ZS_LOG_TRACE(log("step fully connected"))
        setState(SessionState_Connected);

        mLastRetryDuration = mDefaultLastRetryDuration;

        if (mRequiresConnection) {
          mRequiresConnection = false;
          mLastActivity = zsLib::now();
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepRegisterQueries()
      {

        if (mRegisterQueries.size() < 1) {
          ZS_LOG_TRACE(log("step register queries - no register queries"))
          return true;
        }

        ZS_LOG_DEBUG(log("step register queries - issuing registrations"))

        PushSubscriptionInfoList subscriptions;
        RegisterQueryList queriesNeedingRequest;

        for (RegisterQueryList::iterator iter_doNotUse = mRegisterQueries.begin(); iter_doNotUse != mRegisterQueries.end(); )
        {
          RegisterQueryList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          RegisterQueryPtr query = (*current);

          if (query->isComplete()) {
            ZS_LOG_DEBUG(log("removing deleted query"))
            mRegisterQueries.erase(current);
            continue;
          }

          if (!query->needsRequest()) continue;

          subscriptions.push_back(query->getSubscriptionInfo());
          queriesNeedingRequest.push_back(query);
        }

        if (subscriptions.size() > 0) {
          RegisterPushRequestPtr request = RegisterPushRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());
#if defined(__APPLE__)
          request->pushServiceType(RegisterPushRequest::PushServiceType_APNS);
#elif defined(_ANDROID)
          request->pushServiceType(RegisterPushRequest::PushServiceType_GCM);
#endif //__APPLE__
          request->token(mDeviceToken);
          request->subscriptions(subscriptions);

          for (RegisterQueryList::iterator iter = queriesNeedingRequest.begin(); iter != queriesNeedingRequest.end(); ++iter)
          {
            RegisterQueryPtr query = (*iter);
            query->monitor(request);
          }

          sendRequest(request);
        }

        if (mRegisterQueries.size() < 1) {
          ZS_LOG_TRACE(log("all register device queries are complete"))
          return true;
        }

        ZS_LOG_TRACE(log("some register device queries are still pending"))
        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepRefreshFolders()
      {
        ZS_LOG_TRACE(log("step refresh folders"))

        if (mFoldersGetMonitor) {
          ZS_LOG_TRACE(log("waiting for folders update to complete"))
          return true;
        }

        if (!mRefreshFolders) {
          ZS_LOG_TRACE(log("refresh folders is not needed"))
          return true;
        }

        ZS_LOG_DEBUG(log("performing refresh of folders"))

        FoldersGetRequestPtr request = FoldersGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->version(mDB->getLastDownloadedVersionForFolders());

        mFoldersGetMonitor = sendRequest(IMessageMonitorResultDelegate<FoldersGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        mRefreshFolders = false;
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckFoldersNeedingUpdate()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderNeedingUpdateList FolderNeedingUpdateList;

        if (isFolderListUpdating()) {
          ZS_LOG_TRACE(log("will not update checked folders while updating folders list"))
          return true;
        }

        if (mFolderGetMonitors.size() > 0) {
          ZS_LOG_TRACE(log("cannot check for folders needing update while folder get requests are outstanding"))
          return true;
        }

        if (!mRefreshFoldersNeedingUpdate) {
          ZS_LOG_TRACE(log("no need to check if folders need updating at this time"))
          return true;
        }

        FolderNeedingUpdateList result;
        mDB->getFoldersNeedingUpdate(zsLib::now(), result);

        for (FolderNeedingUpdateList::const_iterator iter = result.begin(); iter != result.end(); ++iter)
        {
          const ProcessedFolderNeedingUpdateInfo::FolderNeedingUpdateInfo &info = (*iter);

          if (mMonitoredFolders.find(info.mFolderName) == mMonitoredFolders.end()) {
            ZS_LOG_TRACE(log("folder is not monitored so does not need an update") + ZS_PARAM("folder name", info.mFolderName))
            continue;
          }

          FolderUpdateMap::iterator existingFound = mFoldersNeedingUpdate.find(info.mFolderName);
          if (existingFound != mFoldersNeedingUpdate.end()) {
            ProcessedFolderNeedingUpdateInfo &existingProcessedInfo = (*existingFound).second;
            if (existingProcessedInfo.mInfo.mDownloadedVersion == info.mDownloadedVersion) {
              ZS_LOG_TRACE(log("already know about this folder needs to be updated thus skipping folder check") + ZS_PARAM("folder name", info.mFolderName))
              continue;
            }
          }

          ZS_LOG_DEBUG(log("will perform update check on folder") + ZS_PARAM("folder name", info.mFolderName))

          ProcessedFolderNeedingUpdateInfo processedInfo;
          processedInfo.mInfo = info;
          processedInfo.mSentRequest = false;

          mFoldersNeedingUpdate[info.mFolderName] = processedInfo;
        }

        mRefreshFoldersNeedingUpdate = false;

        ZS_LOG_DEBUG(log("will perform folder update check on folders") + ZS_PARAM("size", mFoldersNeedingUpdate.size()))
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepFolderGet()
      {
        if (isFolderListUpdating()) {
          ZS_LOG_TRACE(log("will not issue folder get while updating folders list"))
          return true;
        }

        for (FolderUpdateMap::iterator iter_doNotUse = mFoldersNeedingUpdate.begin(); iter_doNotUse != mFoldersNeedingUpdate.end(); )
        {
          FolderUpdateMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          String name = (*current).first;
          ProcessedFolderNeedingUpdateInfo &info = (*current).second;

          if (info.mSentRequest) {
            ZS_LOG_TRACE(log("folder already being updated") + ZS_PARAM("folder name", name))
            continue;
          }

          ZS_LOG_DEBUG(log("issuing folder get request") + ZS_PARAM("folder name", name))

          info.mSentRequest = true;

          FolderGetRequestPtr request = FolderGetRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          PushMessageFolderInfo pushInfo;
          pushInfo.mName = name;
          pushInfo.mVersion = info.mInfo.mDownloadedVersion;

          request->folderInfo(pushInfo);

          IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<FolderGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

          mFolderGetMonitors.push_back(monitor);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepMakeMonitoredFolders()
      {
        if ((isFolderListUpdating()) ||
            (areAnyFoldersUpdating())) {
          ZS_LOG_TRACE(log("will not attempt to create keys and sent folder if still updating folders"))
          return true;
        }

        if (mFolderCreationUpdateMonitor) {
          ZS_LOG_TRACE(log("folder is being created"))
          return true;
        }

        if (!mRefreshMonitorFolderCreation) {
          ZS_LOG_TRACE(log("do not need to update folders at this time"))
          return true;
        }

        for (FolderNameMap::iterator iter = mMonitoredFolders.begin(); iter != mMonitoredFolders.end(); ++iter)
        {
          const String &folderName = (*iter).first;
          int &index = (*iter).second;

          index = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

          // check with database to make sure folder is still value
          if (mDB->getFolderIndex(folderName, index)) {
            ZS_LOG_DEBUG(log("folder index found") + ZS_PARAM("folder name", folderName) + ZS_PARAM("folder index", index))
            continue;
          }

          ZS_LOG_DEBUG(log("creating keys folder on server") + ZS_PARAM("folder name", folderName))

          FolderUpdateRequestPtr request = FolderUpdateRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          PushMessageFolderInfo info;
          info.mDisposition = PushMessageFolderInfo::Disposition_Update;
          info.mName = folderName;

          request->folderInfo(info);

          mFolderCreationUpdateMonitor = sendRequest(IMessageMonitorResultDelegate<FolderGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
          break;
        }

        if (mFolderCreationUpdateMonitor) {
          ZS_LOG_TRACE(log("folder creation in progress"))
          return true;
        }

        mRefreshMonitorFolderCreation = false;

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckMessagesNeedingUpdate()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingUpdateList MessageNeedingUpdateList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingUpdateInfo MessageNeedingUpdateInfo;

        if ((isFolderListUpdating()) ||
            (areAnyFoldersUpdating())) {
          ZS_LOG_TRACE(log("will not download message while folders are updating"))
          return true;
        }

        if (mMessagesNeedingUpdate.size() > 0) {
          ZS_LOG_TRACE(log("already have messages needing update in pending queue thus no need to fetch again"))
          return true;
        }

        if (!mRefreshFoldersNeedingUpdate) {
          ZS_LOG_TRACE(log("no need to check for updates at this time"))
          return true;
        }

        MessageNeedingUpdateList updateList;
        mDB->getBatchOfMessagesNeedingUpdate(updateList);

        if (updateList.size() < 1) {
          ZS_LOG_DEBUG(log("no messages are requiring an update right now"))
          mRefreshFoldersNeedingUpdate = false;
          return true;
        }

        for (MessageNeedingUpdateList::iterator iter = updateList.begin(); iter != updateList.end(); ++iter)
        {
          MessageNeedingUpdateInfo &info = (*iter);

          MessageUpdateMap::iterator found = mMessagesNeedingUpdate.find(info.mMessageID);
          if (found != mMessagesNeedingUpdate.end()) {
            ZS_LOG_TRACE(log("message is already in queue to update") + ZS_PARAM("message id", info.mMessageID))
            continue;
          }

          ZS_LOG_DEBUG(log("message needs an update") + ZS_PARAM("message id", info.mMessageID))

          ProcessedMessageNeedingUpdateInfo processedInfo;
          processedInfo.mInfo = info;
          processedInfo.mSentRequest = false;

          mMessagesNeedingUpdate[info.mMessageID] = processedInfo;
        }

        ZS_LOG_DEBUG(log("processed batch of messages that needed upate") + ZS_PARAM("batch size", updateList.size()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepMessagesMetaDataGet()
      {
        if ((isFolderListUpdating()) ||
            (areAnyFoldersUpdating())) {
          ZS_LOG_TRACE(log("will not download message while folders are updating"))
          return true;
        }

        if (mMessageMetaDataGetMonitor) {
          ZS_LOG_TRACE(log("already have outstanding messages metadata get request"))
          return true;
        }

        MessageIDList fetchMessages;

        for (MessageUpdateMap::iterator iter_doNotUse = mMessagesNeedingUpdate.begin(); iter_doNotUse != mMessagesNeedingUpdate.end(); )
        {
          MessageUpdateMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          String messageID = (*current).first;
          ProcessedMessageNeedingUpdateInfo &info = (*current).second;

          info.mSentRequest = true;

          fetchMessages.push_back(messageID);
        }

        if (fetchMessages.size() < 1) {
          ZS_LOG_TRACE(log("no messages to update at this time"))
          return true;
        }

        ZS_LOG_DEBUG(log("issuing message metadata get request") + ZS_PARAM("total messages", fetchMessages.size()))

        MessagesMetaDataGetRequestPtr request = MessagesMetaDataGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->messageIDs(fetchMessages);

        mMessageMetaDataGetMonitor = sendRequest(IMessageMonitorResultDelegate<MessagesMetaDataGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckMessagesNeedingData()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingDataList MessageNeedingDataList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingDataInfo MessageNeedingDataInfo;

        if ((isFolderListUpdating()) ||
            (areAnyFoldersUpdating())) {
          ZS_LOG_TRACE(log("will not download message data while folders are updating"))
          return true;
        }

        if (mMessagesNeedingData.size() > 0) {
          ZS_LOG_TRACE(log("already have messages needing data in pending queue thus no need to fetch again"))
          return true;
        }

        if (!mRefreshMessagesNeedingData) {
          ZS_LOG_TRACE(log("no need to check for data at this time"))
          return true;
        }

        MessageNeedingDataList updateList;
        mDB->getBatchOfMessagesNeedingData(updateList);

        if (updateList.size() < 1) {
          ZS_LOG_DEBUG(log("no messages are requiring an data right now"))
          mRefreshMessagesNeedingData = true;
          return true;
        }

        for (MessageNeedingDataList::iterator iter = updateList.begin(); iter != updateList.end(); ++iter)
        {
          MessageNeedingDataInfo &info = (*iter);

          MessageDataMap::iterator found = mMessagesNeedingData.find(info.mMessageID);
          if (found != mMessagesNeedingData.end()) {
            ZS_LOG_TRACE(log("message is already in queue to get data") + ZS_PARAM("message id", info.mMessageID))
            continue;
          }

          ZS_LOG_DEBUG(log("message needs data") + ZS_PARAM("message id", info.mMessageID))

          ProcessedMessageNeedingDataInfo processedInfo;
          processedInfo.mInfo = info;
          processedInfo.mSentRequest = false;
          processedInfo.mChannelID = 0;
          processedInfo.mReceivedData = 0;

          mMessagesNeedingData[info.mMessageID] = processedInfo;
        }

        ZS_LOG_DEBUG(log("processed batch of messages that needed data") + ZS_PARAM("batch size", updateList.size()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepMessagesDataGet()
      {
        if ((isFolderListUpdating()) ||
            (areAnyFoldersUpdating())) {
          ZS_LOG_TRACE(log("will not download message data while folders are updating"))
          return true;
        }

        if (mMessageMetaDataGetMonitor) {
          ZS_LOG_TRACE(log("already have outstanding messages metadata get request"))
          return true;
        }

        MessageIDList fetchMessages;

        for (MessageUpdateMap::iterator iter_doNotUse = mMessagesNeedingUpdate.begin(); iter_doNotUse != mMessagesNeedingUpdate.end(); )
        {
          MessageUpdateMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          String messageID = (*current).first;
          ProcessedMessageNeedingUpdateInfo &info = (*current).second;

          info.mSentRequest = true;

          fetchMessages.push_back(messageID);
        }

        if (fetchMessages.size() < 1) {
          ZS_LOG_TRACE(log("no messages to update at this time"))
          return true;
        }

        ZS_LOG_DEBUG(log("issuing message metadata get request") + ZS_PARAM("total messages", fetchMessages.size()))

        MessagesDataGetRequestPtr request = MessagesDataGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->messageIDs(fetchMessages);

        mMessageMetaDataGetMonitor = sendRequest(IMessageMonitorResultDelegate<MessagesDataGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckListNeedingDownload()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::ListsNeedingDownloadInfo ListsNeedingDownloadInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::ListsNeedingDownloadList ListsNeedingDownloadList;

        if (mListFetchMonitor) {
          ZS_LOG_TRACE(log("list is currently being fetched"))
          return true;
        }

        if (mListsNeedingDownload.size() > 0) {
          ZS_LOG_TRACE(log("already have list that needs downloading"))
          return true;
        }

        if (!mRefreshListsNeedingDownload) {
          ZS_LOG_TRACE(log("do not need to refresh list needing download at this time"))
          return true;
        }

        ListsNeedingDownloadList downloadList;

        mDB->getBatchOfListsNeedingDownload(downloadList);

        if (downloadList.size() < 1) {
          ZS_LOG_DEBUG(log("all lists have downloaded"))
          mRefreshListsNeedingDownload = false;
          return true;
        }

        for (ListsNeedingDownloadList::iterator iter = downloadList.begin(); iter != downloadList.end(); ++iter)
        {
          const ListsNeedingDownloadInfo &info = (*iter);

          if (mListsNeedingDownload.end() != mListsNeedingDownload.find(info.mListID)) {
            ZS_LOG_WARNING(Debug, log("already will attempt to download this list") + ZS_PARAM("list ID", info.mListID))
            continue;
          }

          ZS_LOG_DEBUG(log("will download list") + ZS_PARAM("list id", info.mListID))

          ProcessedListsNeedingDownloadInfo processedInfo;
          processedInfo.mInfo = info;
          processedInfo.mSentRequest = false;

          mListsNeedingDownload[info.mListID] = processedInfo;
        }

        ZS_LOG_DEBUG(log("will download list information") + ZS_PARAM("total", mListsNeedingDownload.size()))

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepListFetch()
      {
        typedef ListFetchRequest::ListIDList ListIDList;

        if (mListFetchMonitor) {
          ZS_LOG_TRACE(log("list is currently being fetched"))
          return true;
        }

        if (mListsNeedingDownload.size() < 1) {
          ZS_LOG_TRACE(log("no lists to fetch at this time"))
          return true;
        }

        ListIDList fetchList;

        for (ListDownloadMap::iterator iter = mListsNeedingDownload.begin(); iter != mListsNeedingDownload.end(); ++iter)
        {
          ProcessedListsNeedingDownloadInfo &info = (*iter).second;
          if (info.mSentRequest) {
            ZS_LOG_TRACE(log("already requested fetch of this list") + ZS_PARAM("list id", info.mInfo.mListID))
            continue;
          }

          ZS_LOG_DEBUG(log("will issue list fetch") + ZS_PARAM("list id", info.mInfo.mListID))
          fetchList.push_back(info.mInfo.mListID);
        }

        if (fetchList.size() < 1) {
          ZS_LOG_TRACE(log("nothing to download at this time"))
          return true;
        }

        ZS_LOG_DEBUG(log("issuing list fetch request") + ZS_PARAM("lists to download", fetchList.size()))

        ListFetchRequestPtr request = ListFetchRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->listIDs(fetchList);

        mListFetchMonitor = sendRequest(IMessageMonitorResultDelegate<ListFetchResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepProcessKeysFolder()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingKeyingProcessingInfo MessagesNeedingKeyingProcessingInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingKeyingProcessingList MessagesNeedingKeyingProcessingList;

        typedef IServicePushMailboxDatabaseAbstractionDelegate::ReceivingKeyInfo ReceivingKeyInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::SendingKeyInfo SendingKeyInfo;

        if (!areMonitoredFoldersReady()) {
          ZS_LOG_TRACE(log("cannot process keys folder until keys and sent folder are ready"))
          return true;
        }

        if (!mRefreshKeysFolderNeedsProcessing) {
          ZS_LOG_TRACE(log("no need to process keys folder at this time"))
          return true;
        }

        FolderNameMap::iterator found = mMonitoredFolders.find(OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME);
        ZS_THROW_BAD_STATE_IF(found == mMonitoredFolders.end())

        int keysFolderIndex = (*found).second;
        ZS_THROW_BAD_STATE_IF(OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == keysFolderIndex)

        MessagesNeedingKeyingProcessingList messages;
        mDB->getBatchOfMessagesNeedingKeysProcessing(keysFolderIndex, OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE, OPENPEER_STACK_PUSH_MAILBOX_JSON_MIME_TYPE, messages);

        if (messages.size() < 1) {
          ZS_LOG_DEBUG(log("no keys in keying folder needing processing at this time"))
          mRefreshKeysFolderNeedsProcessing = false;
          return true;
        }

        for (MessagesNeedingKeyingProcessingList::iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          MessagesNeedingKeyingProcessingInfo &info = (*iter);

          // scope
          {
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE != info.mEncoding) {
              ZS_LOG_WARNING(Detail, log("failed to extract information from keying bundle") + ZS_PARAM("message index", info.mMessageIndex) + ZS_PARAM("encoding", "info.mEncoding") + ZS_PARAM("expecting", OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE))
              goto post_processing;
            }

            KeyingBundle bundle;
            if (!extractKeyingBundle(info.mData, bundle)) {
              ZS_LOG_WARNING(Detail, log("failed to extract information from keying bundle") + ZS_PARAM("message index", info.mMessageIndex))
              goto post_processing;
            }

            if (info.mFrom != bundle.mValidationPeer->getPeerURI()) {
              ZS_LOG_WARNING(Detail, log("can only accept keying material if the \"from\" is the same as the signing peer"))
              goto post_processing;
            }

            if (zsLib::now() > bundle.mExpires) {
              ZS_LOG_WARNING(Detail, log("this key has already expired (thus do not process)"))
              goto post_processing;
            }

            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_PKI == bundle.mType) {
              ReceivingKeyInfo receivingInfo;

              if (mDB->getReceivingKey(bundle.mReferencedKeyID, receivingInfo)) {
                ZS_LOG_DEBUG(log("found existing key thus no need to process again") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("uri", receivingInfo.mURI) + ZS_PARAM("passphrase", receivingInfo.mPassphrase) + ZS_PARAM("ephemeral private key", receivingInfo.mDHEphemerialPrivateKey) + ZS_PARAM("ephemeral public key", receivingInfo.mDHEphemerialPublicKey) + ZS_PARAM("expires", receivingInfo.mExpires))
                goto post_processing;
              }

              receivingInfo.mPassphrase = extractRSA(bundle.mSecret);
              if (receivingInfo.mPassphrase.isEmpty()) {
                ZS_LOG_WARNING(Detail, log("failed to extract RSA keying material") + ZS_PARAM("message index", info.mMessageIndex))
                goto post_processing;
              }

              receivingInfo.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
              receivingInfo.mKeyID = bundle.mReferencedKeyID;
              receivingInfo.mURI = info.mFrom;
              receivingInfo.mExpires = bundle.mExpires;

              mDB->addOrUpdateReceivingKey(receivingInfo);
              mDB->notifyMessageDecryptNowForKeys(receivingInfo.mKeyID);

              mRefreshMessagesNeedingDecryption = true;
            }

            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT == bundle.mType) {
              IDHPrivateKeyPtr privateKey;
              IDHPublicKeyPtr publicKey;

              if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_REQUEST_OFFER == bundle.mMode) {
                // L <-- (request offer) <-- R
                // L --> (offer request existing) --> R
                // L <-- (answer) <-- R (remote sending passphrase)

                ReceivingKeyInfo receivingInfo;

                if (mDB->getReceivingKey(bundle.mReferencedKeyID, receivingInfo)) {
                  ZS_LOG_DEBUG(log("found existing key") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("uri", receivingInfo.mURI) + ZS_PARAM("passphrase", receivingInfo.mPassphrase) + ZS_PARAM("ephemeral private key", receivingInfo.mDHEphemerialPrivateKey) + ZS_PARAM("ephemeral public key", receivingInfo.mDHEphemerialPublicKey) + ZS_PARAM("expires", receivingInfo.mExpires))

                  if (receivingInfo.mURI != bundle.mValidationPeer->getPeerURI()) {
                    ZS_LOG_WARNING(Detail, log("referenced key is not signed by from peer (thus do not respond)") + ZS_PARAM("existing key peer uri", receivingInfo.mURI) + IPeer::toDebug(bundle.mValidationPeer))
                    goto post_processing;
                  }

                  if (zsLib::now() > receivingInfo.mExpires) {
                    ZS_LOG_WARNING(Detail, log("referenced key is expired") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("expires", receivingInfo.mExpires))
                    goto post_processing;
                  }

                  if (!prepareDHKeys(receivingInfo.mKeyDomain, receivingInfo.mDHEphemerialPrivateKey, receivingInfo.mDHEphemerialPublicKey, privateKey, publicKey)) {
                    ZS_LOG_WARNING(Detail, log("referenced key cannot load DH keying material") + ZS_PARAM("key id", bundle.mReferencedKeyID))
                    goto post_processing;
                  }
                } else {
                  ZS_LOG_DEBUG(log("existing receiving key not found thus create one"))

                  if (!prepareNewDHKeys(NULL, receivingInfo.mKeyDomain, receivingInfo.mDHEphemerialPrivateKey, receivingInfo.mDHEphemerialPublicKey, privateKey, publicKey)) {
                    ZS_LOG_WARNING(Detail, log("failed to generate new DH ephemeral keys"))
                    goto post_processing;
                  }

                  receivingInfo.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
                  receivingInfo.mKeyID = bundle.mReferencedKeyID;
                  receivingInfo.mURI = info.mFrom;
                  receivingInfo.mExpires = bundle.mExpires;

                  mDB->addOrUpdateReceivingKey(receivingInfo);
                }

                // remote party has a sending passphrase and this peer needs for the passphrase for receiving
                sendOfferRequestExisting(info.mFrom, bundle.mReferencedKeyID, receivingInfo.mKeyDomain, publicKey, receivingInfo.mExpires);

              } else if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_OFFER_REQUEST_EXISTING == bundle.mMode) {
                // L <-- (offer request existing) <-- R
                // L --> (answer) --> R (local sending passphrase)

                // remote party needs a passphrase for receiving and this peer sends its sending passphrase
                bool isDHKey = false;
                String rawKeyID = extractRawSendingKeyID(bundle.mReferencedKeyID, isDHKey);

                SendingKeyInfo sendingInfo;
                sendingInfo.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
                if (!mDB->getSendingKey(rawKeyID, sendingInfo)) {
                  ZS_LOG_WARNING(Detail, log("sending key is not known on this device") + ZS_PARAM("key id", bundle.mReferencedKeyID))
                  goto post_processing;
                }

                if (!isDHKey) {
                  sendPKI(info.mFrom, bundle.mReferencedKeyID, sendingInfo.mRSAPassphrase, bundle.mValidationPeer, sendingInfo.mExpires);
                  goto post_processing;
                }

                // remote side needs DH keying material
                if (sendingInfo.mListSize > 1) {
                  if (sendingInfo.mListSize > sendingInfo.mTotalWithPassphrase) {
                    // need to figure out if a peer is ACKing receipt of a particular passphrase
                    String listID = internal::extractListID(sendingInfo.mURI);
                    int order = 0;
                    if (!mDB->getListOrder(listID, info.mFrom, order)) {
                      ZS_LOG_WARNING(Detail, log("the sender is not on the list thus the sender cannot receive the keying material") + ZS_PARAM("from", info.mFrom))
                      goto post_processing;
                    }

                    SecureByteBlockPtr ackBuffer;
                    if (sendingInfo.mAckDHPassphraseSet.hasData()) {
                      ackBuffer = IHelper::convertFromBase64(sendingInfo.mAckDHPassphraseSet);
                    } else {
                      size_t totalBytes = (sendingInfo.mListSize / 8) + (sendingInfo.mListSize % 8 != 0 ? 1 : 0);
                      ackBuffer = SecureByteBlockPtr(new SecureByteBlock(totalBytes));
                    }

                    size_t position = (order / 8);
                    size_t bit = (order % 8);
                    if (position >= ackBuffer->SizeInBytes()) {
                      ZS_LOG_ERROR(Detail, log("the sender is on the list but beyond the size of the buffer") + ZS_PARAM("key id", sendingInfo.mKeyID) + ZS_PARAM("from", info.mFrom) + ZS_PARAM("buffer size", ackBuffer->SizeInBytes()) + ZS_PARAM("order", order))
                      goto post_processing;
                    }

                    BYTE ackByte = ackBuffer->BytePtr()[position];
                    BYTE before = ackByte;

                    ackByte = ackByte | (1 << bit);

                    ackBuffer->BytePtr()[position] = ackByte;
                    if (before != ackByte) {
                      // byte has ordered in a new bit thus a new ack is present
                      ++sendingInfo.mTotalWithPassphrase;
                      ZS_LOG_DEBUG(log("remote party acked DH key") + ZS_PARAM("key id", sendingInfo.mKeyID) + ZS_PARAM("total", sendingInfo.mListSize) + ZS_PARAM("total acked", sendingInfo.mTotalWithPassphrase))

                      if (sendingInfo.mListSize > sendingInfo.mTotalWithPassphrase) {
                        sendingInfo.mAckDHPassphraseSet = IHelper::convertToBase64(*ackBuffer);
                      } else {
                        sendingInfo.mAckDHPassphraseSet = String();
                      }
                      mDB->addOrUpdateSendingKey(sendingInfo);
                    }
                  }
                } else {
                  if (0 == sendingInfo.mTotalWithPassphrase) {
                    sendingInfo.mTotalWithPassphrase = 1;
                    ZS_LOG_DEBUG(log("remote party acked DH key") + ZS_PARAM("key id", sendingInfo.mKeyID) + ZS_PARAM("total", sendingInfo.mListSize) + ZS_PARAM("total acked", sendingInfo.mTotalWithPassphrase))
                    mDB->addOrUpdateSendingKey(sendingInfo);
                  }
                }

                if (!prepareDHKeys(sendingInfo.mKeyDomain, sendingInfo.mDHEphemeralPrivateKey, sendingInfo.mDHEphemeralPrivateKey, privateKey, publicKey)) {
                  ZS_LOG_WARNING(Detail, log("unable to prepare DH keying material") + ZS_PARAM("key ID", sendingInfo.mKeyID))
                  goto post_processing;
                }

                sendAnswer(info.mFrom, bundle.mReferencedKeyID, sendingInfo.mDHPassphrase, bundle.mAgreement, sendingInfo.mKeyDomain, privateKey, publicKey, sendingInfo.mExpires);

              } else if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_ANSWER == bundle.mMode) {
                // remote party is answering and giving a passphrase which represents their sending passphrase (i.e. which is used as a local receiving key)
                ReceivingKeyInfo receivingInfo;
                receivingInfo.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

                if (!mDB->getReceivingKey(bundle.mReferencedKeyID, receivingInfo)) {
                  ZS_LOG_WARNING(Detail, log("never created an offer so this answer is not related") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("from", info.mFrom) + ZS_PARAM("uri", receivingInfo.mURI))
                  goto post_processing;
                }

                if (receivingInfo.mURI != info.mFrom) {
                  ZS_LOG_WARNING(Detail, log("from does not match the receiving key uri") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("from", info.mFrom) + ZS_PARAM("uri", receivingInfo.mURI))
                  goto post_processing;
                }

                if (receivingInfo.mPassphrase.hasData()) {
                  ZS_LOG_WARNING(Detail, log("already know about receiving passphrase thus will not update"))
                  goto post_processing;
                }

                receivingInfo.mPassphrase = getAnswerPassphrase(receivingInfo.mKeyDomain, receivingInfo.mDHEphemerialPrivateKey, receivingInfo.mDHEphemerialPublicKey, bundle.mAgreement, bundle.mSecret);

                if (receivingInfo.mPassphrase.isEmpty()) {
                  ZS_LOG_WARNING(Detail, log("cannot extract answer passphrase") + ZS_PARAM("keying id", bundle.mReferencedKeyID))
                  goto post_processing;
                }

                ZS_LOG_DEBUG(log("received answer passphrase") + ZS_PARAM("keying id", bundle.mReferencedKeyID) + ZS_PARAM("passphrase", receivingInfo.mPassphrase))

                mDB->addOrUpdateReceivingKey(receivingInfo);
                mDB->notifyMessageDecryptNowForKeys(receivingInfo.mKeyID);

                mRefreshMessagesNeedingDecryption = true;
              } else {
                ZS_LOG_WARNING(Detail, log("agreement mode is not supported") + ZS_PARAM("message index", info.mMessageIndex) + ZS_PARAM("agreement mode", bundle.mMode))
                goto post_processing;
              }
            }
          }

        post_processing:
          {
            mDB->notifyMessageKeyingProcessed(info.mMessageIndex);
          }
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepDecryptMessages()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingDecryptingInfo MessagesNeedingDecryptingInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingDecryptingList MessagesNeedingDecryptingList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::ReceivingKeyInfo ReceivingKeyInfo;

        if (!mRefreshMessagesNeedingDecryption) {
          ZS_LOG_TRACE(log("no messages should need decrypting at this time"))
          return true;
        }

        MessagesNeedingDecryptingList messages;
        mDB->getBatchOfMessagesNeedingDecrypting(OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE, messages);

        if (messages.size() < 1) {
          ZS_LOG_DEBUG(log("no messages are needing decryption at this time"))
          mRefreshMessagesNeedingDecryption = false;
          return true;
        }

        for (MessagesNeedingDecryptingList::iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          MessagesNeedingDecryptingInfo &info = (*iter);

          String keyID;

          {
            IHelper::SplitMap split;

            if (info.mEncoding.isEmpty()) {
              ZS_LOG_DEBUG(log("message was not encoded thus the encrypted data is the decrypted data") + ZS_PARAM("message index", info.mMessageIndex))
              mDB->notifyMessageDecrypted(info.mMessageIndex, info.mData, true);
              mRefreshVersionedFolders = true;
              continue;
            }

            IHelper::split(info.mEncoding, split, ':');
            if (split.size() != 5) {
              ZS_LOG_WARNING(Detail, log("encoding scheme is not known thus cannot decrypt") + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }
            if ((split[0] + ":") != OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME) {
              ZS_LOG_WARNING(Detail, log("encoding scheme is not \"" OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME "\" thus cannot decrypt") + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }

            String encodingNamespace = IHelper::convertToString(*IHelper::convertFromBase64(split[1]));
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE != encodingNamespace) {
              ZS_LOG_WARNING(Detail, log("encoding namespace is not \"" OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE "\" thus cannot decrypt") + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }

            keyID = split[2];

            ReceivingKeyInfo receivingKey;

            if (!mDB->getReceivingKey(keyID, receivingKey)) {
              ZS_LOG_WARNING(Detail, log("receiving key is not available key thus cannot decrypt yet") + ZS_PARAM("key id", keyID))
              goto decrypt_later;
            }

            if (receivingKey.mPassphrase.isEmpty()) {
              ZS_LOG_WARNING(Detail, log("receiving key does not contain passphrase yet thus cannot decrypt yet") + ZS_PARAM("key id", keyID))
              goto decrypt_later;
            }

            if (!info.mData) {
              ZS_LOG_WARNING(Detail, log("message cannot decrypt as it has no data") + ZS_PARAM("message index", info.mMessageIndex) + ZS_PARAM("key id", keyID))
              goto failed_to_decrypt;
            }

            SecureByteBlockPtr decryptedData;
            if (!decryptMessage(keyID, receivingKey.mPassphrase, split[3], split[4], *info.mData, decryptedData)) {
              ZS_LOG_WARNING(Detail, log("failed to decrypt message") + ZS_PARAM("message index", info.mMessageIndex) + ZS_PARAM("key id", keyID))
              goto failed_to_decrypt;
            }

            ZS_THROW_INVALID_ASSUMPTION_IF(!decryptedData)

            ZS_LOG_DEBUG(log("message is now decrypted") + ZS_PARAM("message index", info.mMessageIndex) + ZS_PARAM("key id", keyID))
            mDB->notifyMessageDecrypted(info.mMessageIndex, decryptedData, true);
            mRefreshVersionedFolders = true;
            continue;
          }

        failed_to_decrypt:
          {
            mDB->notifyMessageDecryptionFailure(info.mMessageIndex);
            continue;
          }
        decrypt_later:
          {
            mDB->notifyMessageDecryptLater(info.mMessageIndex, keyID);
            continue;
          }
        }

        ZS_LOG_DEBUG(log("finished attempting to decrypt messages") + ZS_PARAM("total", messages.size()))

        // decrypt again...
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPendingDelivery()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::PendingDeliveryMessageInfo PendingDeliveryMessageInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::PendingDeliveryMessageList PendingDeliveryMessageList;

        static PushStates checkStates[] =
        {
          PushState_Read,
          PushState_Answered,
          PushState_Flagged,
          PushState_Deleted,
          PushState_Draft,
          PushState_Recent,
          PushState_Delivered,
          PushState_Sent,
          PushState_Pushed,
          PushState_Error,

          PushState_None
        };

        if (mPendingDelivery.size() > 0) {
          ZS_LOG_TRACE(log("already have messages that are pending delivery"))
          return true;
        }

        if (mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
          ZS_LOG_TRACE(log("waiting for pending delivery pre-check to complete"))
          return true;
        }

        if (!mRefreshPendingDelivery) {
          ZS_LOG_TRACE(log("no need to check for messages needing delivery at this time"))
          return true;
        }


        PendingDeliveryMessageList pendingDelivery;

        mDB->getBatchOfMessagesToDeliver(pendingDelivery);

        if (pendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("no messages require delivery at this time"))
          mRefreshPendingDelivery = false;
          mPendingDeliveryPrecheckRequired = false;
          return true;
        }

        MessageIDList fetchMessages;

        for (PendingDeliveryMessageList::iterator iter = pendingDelivery.begin(); iter != pendingDelivery.end(); ++iter)
        {
          PendingDeliveryMessageInfo &info = (*iter);

          ProcessedPendingDeliveryMessageInfo processedInfo;
          processedInfo.mInfo = info;
          processedInfo.mSent = 0;

          String to;
          String from;
          String cc;
          String bcc;

          PushInfoList pushInfos;

          bool found = mDB->getMessageDetails(
                                              info.mMessageIndex,
                                              processedInfo.mMessage.mID,
                                              to,
                                              from,
                                              cc,
                                              bcc,
                                              processedInfo.mMessage.mType,
                                              processedInfo.mMessage.mMimeType,
                                              processedInfo.mMessage.mEncoding,
                                              processedInfo.mMessage.mPushType,
                                              pushInfos,
                                              processedInfo.mMessage.mSent,
                                              processedInfo.mMessage.mExpires,
                                              processedInfo.mData
                                              );

          if (!found) {
            ZS_LOG_ERROR(Detail, log("message pending delivery was not found") + ZS_PARAM("message index", info.mMessageIndex))
            mDB->removePendingDeliveryMessage(info.mIndex);
            continue;
          }

          copy(pushInfos, processedInfo.mMessage.mPushInfos);


          if (processedInfo.mData) {
            ZS_LOG_TRACE(log("allocating a sending channel ID"))
            processedInfo.mMessage.mChannelID = (++mLastChannel);
            if (mLastChannel >= 0x3FFFFFFF) {  // safe wrap around (should not happen)
              mLastChannel = 0;
            }
          }


          PushMessageInfo::FolderInfo folderInfo;
          folderInfo.mDisposition = PushMessageInfo::FolderInfo::Disposition_Update;
          folderInfo.mWhere = PushMessageInfo::FolderInfo::Where_Remote;
          folderInfo.mName = processedInfo.mInfo.mRemoteFolder;

          processedInfo.mMessage.mFolders.push_back(folderInfo);

          if (processedInfo.mInfo.mCopyToSent) {
            folderInfo.mDisposition = PushMessageInfo::FolderInfo::Disposition_Update;
            folderInfo.mWhere = PushMessageInfo::FolderInfo::Where_Local;
            folderInfo.mName = OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME;
            processedInfo.mMessage.mFolders.push_back(folderInfo);
          }

          fetchMessages.push_back(processedInfo.mMessage.mID);

          if (0 != processedInfo.mInfo.mSubscribeFlags) {
            PushMessageInfo::FlagInfoMap flags;

            for (int index = 0; checkStates[index] != PushState_None; ++index) {
              if (0 == (processedInfo.mInfo.mSubscribeFlags & checkStates[index])) continue;

              PushMessageInfo::FlagInfo flag;

              flag.mDisposition = IServicePushMailboxSession::canSubscribeState(checkStates[index]) ? PushMessageInfo::FlagInfo::Disposition_Subscribe : PushMessageInfo::FlagInfo::Disposition_Update;
              flag.mFlag = PushMessageInfo::FlagInfo::toFlag(IServicePushMailboxSession::toString(checkStates[index]));
              if (PushMessageInfo::FlagInfo::Flag_NA == flag.mFlag) {
                ZS_LOG_WARNING(Detail, log("flag was not understood") + ZS_PARAM("flag", IServicePushMailboxSession::toString(checkStates[index])))
                continue;
              }

              flags[flag.mFlag] = flag;
            }
          }

          mPendingDelivery[processedInfo.mMessage.mID] = processedInfo;
        }

        if (fetchMessages.size() < 1) {
          ZS_LOG_WARNING(Detail, log("all pending messages were filtered out"))
          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          return true;
        }

        ZS_LOG_DEBUG(log("issuing pending delivery pre-check message metadata get request") + ZS_PARAM("total messages", fetchMessages.size()))

        MessagesMetaDataGetRequestPtr request = MessagesMetaDataGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());

        request->messageIDs(fetchMessages);

        mPendingDeliveryPrecheckMessageMetaDataGetMonitor = sendRequest(IMessageMonitorResultDelegate<MessagesMetaDataGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPendingDeliveryUpdateRequest()
      {
        if (mPendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("no messages need uploading at this time"))
          return true;
        }

        bool issuedUpload = false;

        for (PendingDeliveryMap::iterator iter_doNotuse = mPendingDelivery.begin(); iter_doNotuse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (processedInfo.mPendingDeliveryMessageUpdateRequest) {
            ZS_LOG_TRACE(log("already issues pending delivery message update request"))
            continue;
          }

          ZS_THROW_BAD_STATE_IF(processedInfo.mPendingDeliveryMessageUpdateErrorMonitor)
          ZS_THROW_BAD_STATE_IF(processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor)
          
          ZS_LOG_DEBUG(log("issuing request to upload next message") + ZS_PARAM("message id", processedInfo.mMessage.mID))

          MessageUpdateRequestPtr request = MessageUpdateRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          request->messageInfo(processedInfo.mMessage);

          processedInfo.mPendingDeliveryMessageUpdateErrorMonitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));

          processedInfo.mPendingDeliveryMessageUpdateRequest = request;

          if (0 == processedInfo.mMessage.mChannelID) {
            // message contains no data thus can immediately expect final result
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
          } else {
            issuedUpload = true;
          }
        }

        if ((issuedUpload) &&
            (mWriter) &&
            (!mBackgroundingEnabled)) {
          services::ITransportStreamWriterDelegateProxy::create(mThisWeak.lock())->onTransportStreamWriterReady(mWriter);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepDeliverViaURL()
      {
        if (!mBackgroundingEnabled) {
          ZS_LOG_TRACE(log("not in backgrounding mode thus no need to deliver via local file and URL"))
          return true;
        }

        for (PendingDeliveryMap::iterator iter_doNotuse = mPendingDelivery.begin(); iter_doNotuse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          const String &messageID = (*current).first;
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (!processedInfo.mData) {
            ZS_LOG_TRACE(log("message does not have data to deliver"))
            continue;
          }

          if (processedInfo.mTempFileName.hasData()) {
            ZS_LOG_TRACE(log("already attempting to deliver via temporary file"))
            continue;
          }

          if (processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
            ZS_LOG_TRACE(log("already completed upload process for this message"))
            continue;
          }

          if (processedInfo.mSent >= processedInfo.mData->SizeInBytes()) {
            ZS_LOG_WARNING(Detail, log("already uploaded entire message") + ZS_PARAM("sent", processedInfo.mSent) + ZS_PARAM("total", processedInfo.mData->SizeInBytes()))
            continue;
          }

          size_t totalRemaining = processedInfo.mData->SizeInBytes() - processedInfo.mSent;

          SecureByteBlockPtr buffer = IHelper::convertToBuffer(&(processedInfo.mData->BytePtr()[processedInfo.mSent]), totalRemaining);

          processedInfo.mTempFileName = mDB->storeToTemporaryFile(*buffer);
          processedInfo.mDeliveryURL = mUploadMessageURL;

          // replace message ID
          {
            size_t found = processedInfo.mDeliveryURL.find(mUploadMessageStringReplacementMessageID);
            if (String::npos != found) {
              String replaceMessageID = urlencode(messageID);
              processedInfo.mDeliveryURL.replace(found, mUploadMessageStringReplacementMessageID.size(), replaceMessageID);
            }
          }

          // replace size
          {
            size_t found = processedInfo.mDeliveryURL.find(mUploadMessageStringReplacementMessageSize);
            if (String::npos != found) {
              String replaceMessageSize = string(totalRemaining);
              processedInfo.mDeliveryURL.replace(found, mUploadMessageStringReplacementMessageID.size(), replaceMessageSize);
            }
          }

          if (processedInfo.mTempFileName.hasData()) {
            ZS_LOG_DEBUG(log("going to deliver push message via URL") + ZS_PARAM("url", processedInfo.mDeliveryURL) + ZS_PARAM("file", processedInfo.mTempFileName))

            IServicePushMailboxSessionAsyncDatabaseDelegateProxy::create(mAsyncDB)->asyncNotifyPostFileDataToURL(processedInfo.mDeliveryURL, processedInfo.mTempFileName);
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), processedInfo.mPendingDeliveryMessageUpdateRequest, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));
          } else {
            ZS_LOG_DEBUG(log("unable to send via URL as file could not be created") + ZS_PARAM("url", processedInfo.mDeliveryURL))
          }
        }

        return true;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPrepareVersionedFolderMessages()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingNotificationInfo MessageNeedingNotificationInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingNotificationList MessageNeedingNotificationList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderIndex FolderIndex;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderIndexList FolderIndexList;

        typedef std::map<FolderIndex, FolderIndex> FoldersUpdatedMap;

        if (!mRefreshVersionedFolders) {
          ZS_LOG_TRACE(log("no need to refresh versioned folders"))
          return true;
        }

        MessageNeedingNotificationList needingNotification;
        mDB->getBatchOfMessagesNeedingNotification(needingNotification);

        if (needingNotification.size() < 1) {
          ZS_LOG_DEBUG(log("no messages need notification at this time"))
          mRefreshVersionedFolders = false;
          return true;
        }

        FoldersUpdatedMap updatedFolders;

        for (MessageNeedingNotificationList::iterator iter = needingNotification.begin(); iter != needingNotification.end(); ++iter) {
          MessageNeedingNotificationInfo &info = (*iter);

          {
            if (info.mDataLength > 0) {
              if (!info.mHasDecryptedData) {
                ZS_LOG_TRACE(log("cannot add to version table yet as data is not decrypted"))
                goto clear_notification;
              }
            }

            FolderIndexList folders;
            mDB->getFoldersWithMessage(info.mMessageID, folders);

            for (FolderIndexList::iterator folderIter = folders.begin(); folderIter != folders.end(); ++folderIter) {
              FolderIndex folderIndex = (*folderIter);

              mDB->addFolderVersionedMessage(folderIndex, info.mMessageID);
              updatedFolders[folderIndex] = folderIndex;
            }
          }

        clear_notification:
          mDB->clearMessageNeedsNotification(info.mIndex);
        }

        for (FoldersUpdatedMap::iterator iter = updatedFolders.begin(); iter != updatedFolders.end(); ++iter)
        {
          FolderIndex index = (*iter).second;
          String name = mDB->getFolderName(index);
          if (name.isEmpty()) continue;

          notifyChangedFolder(name);
        }

        // try to do some more notification
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepMarkReadOrRemove()
      {
        if (areAnyMessagesUploading()) {
          ZS_LOG_TRACE(log("cannot mark read and remove messages while uploading"))
          return true;
        }

        if (mMarkingMonitors.size() > 0) {
          ZS_LOG_TRACE(log("cannot mark read or removed while active mark read/removed is in progress"))
          return true;
        }

        if ((mMessagesToMarkRead.size() < 1) &&
            (mMessagesToRemove.size() < 1)) {
          ZS_LOG_TRACE(log("no messages to mark read or removed at this time"))
          return true;
        }

        ZS_LOG_DEBUG(log("marking read or removed") + ZS_PARAM("read", mMessagesToMarkRead.size()) + ZS_PARAM("remove", mMessagesToRemove.size()))

        for (MessageMap::iterator iter = mMessagesToMarkRead.begin(); iter != mMessagesToMarkRead.end(); ++iter)
        {
          String &messageID = (*iter).second;

          MessageUpdateRequestPtr request = MessageUpdateRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          PushMessageInfo info;
          info.mDisposition = PushMessageInfo::Disposition_Update;
          info.mID = messageID;

          PushMessageInfo::FlagInfoMap flags;
          PushMessageInfo::FlagInfo flag;
          flag.mDisposition = PushMessageInfo::FlagInfo::Disposition_Update;
          flag.mFlag = PushMessageInfo::FlagInfo::Flag_Read;
          flags[PushMessageInfo::FlagInfo::Flag_Read] = flag;

          request->messageInfo(info);

          IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));
          mMarkingMonitors.push_back(monitor);
        }

        for (MessageMap::iterator iter = mMessagesToRemove.begin(); iter != mMessagesToRemove.end(); ++iter)
        {
          String &messageID = (*iter).second;

          MessageUpdateRequestPtr request = MessageUpdateRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          PushMessageInfo info;
          info.mDisposition = PushMessageInfo::Disposition_Remove;
          info.mID = messageID;

          request->messageInfo(info);

          IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));
          mMarkingMonitors.push_back(monitor);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepExpireMessages()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingExpiryInfo MessagesNeedingExpiryInfo;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessagesNeedingExpiryList MessagesNeedingExpiryList;

        if (!mRefreshMessagesNeedingExpiry) {
          ZS_LOG_TRACE(log("no need to check for expiry at this time"))
          return true;
        }

        if (areAnyMessagesUploading()) {
          ZS_LOG_TRACE(log("will not attempt to expire messages while uploading"))
          return true;
        }

        if (areAnyMessagesUpdating()) {
          ZS_LOG_TRACE(log("will not attempt to expire messages while updating messages"))
          return true;
        }

        if (areAnyMessagesDownloadingData()) {
          ZS_LOG_TRACE(log("will not attempt to expire messages while downloading message data"))
          return true;
        }

        if (areAnyMessagesDecrypting()) {
          ZS_LOG_TRACE(log("will not attempt to expire messages while messages are decrypting"))
          return true;
        }

        if (areAnyMessagesMarkReadOrDeleting()) {
          ZS_LOG_TRACE(log("will not attempt to expire messages while messages are being marked read or deleted"))
          return true;
        }

        MessagesNeedingExpiryList needsExpiry;
        mDB->getBatchOfMessagesNeedingExpiry(
                                             zsLib::now(),
                                             needsExpiry
                                             );

        if (needsExpiry.size() < 1) {
          ZS_LOG_DEBUG(log("no messages needing expiry at this time"))
          mRefreshMessagesNeedingExpiry = false;
          return true;
        }

        ZS_LOG_DEBUG(log("expiring messages") + ZS_PARAM("total", needsExpiry.size()))

        for (MessagesNeedingExpiryList::iterator iter = needsExpiry.begin(); iter != needsExpiry.end(); ++iter) {
          MessagesNeedingExpiryInfo &info = (*iter);

          handleMessageGone(info.mMessageID);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepBackgroundingReady()
      {
        bool check = mBackgroundingEnabled || (isShuttingDown());

        if (!check) {
          ZS_LOG_TRACE(log("backgrounding not enabled and not shutting down"))
          return true;
        }

        if (areRegisterQueriesActive()) {
          ZS_LOG_TRACE(log("backgrounding not ready - register queries are still active"))
          return false;
        }

        if (isFolderListUpdating()) {
          ZS_LOG_TRACE(log("backgrounding not ready - folder list is updating"))
          return false;
        }

        if (areAnyFoldersUpdating()) {
          ZS_LOG_TRACE(log("backgrounding not ready - folders still updating"))
          return false;
        }

        if (areAnyMessagesUpdating()) {
          ZS_LOG_TRACE(log("backgrounding not ready - messages are updating"))
          return false;
        }

        if (areAnyMessagesDownloadingData()) {
          ZS_LOG_TRACE(log("backgrounding not ready - messages downloading data"))
          return false;
        }

        if (areAnyListsFetching()) {
          ZS_LOG_TRACE(log("backgrounding not ready - lists are being fetched"))
          return false;
        }

        if (areAnyMessagesUploading()) {
          ZS_LOG_TRACE(log("backgrounding not ready - messages are uploading"))
          return false;
        }

        if (areAnyMessagesMarkReadOrDeleting()) {
          ZS_LOG_TRACE(log("backgrounding not ready - mark read or delete message is still pending"))
          return false;
        }

        ZS_LOG_DEBUG(log("backgrounding is now ready"))

        if (mNextFoldersUpdateTimer) {
          mNextFoldersUpdateTimer->cancel();
          mNextFoldersUpdateTimer.reset();
        }

        mRequiresConnection = false;

        // now inactive
        mLastActivity = Time();

        if (mTCPMessaging) {
          ZS_LOG_DEBUG(log("waiting for TCP messaging to shutdown"))
          return false;
        }

        mBackgroundingNotifier.reset();

        if (isShuttingDown()) {
          ZS_LOG_DEBUG(log("now ready to complete shutdown process"))
          cancel(true);
          return false;
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return false;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::postStep()
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::setState(SessionStates state)
      {
        if (state == mCurrentState) return;

        if (SessionState_ShuttingDown == mCurrentState) {
          if (SessionState_Shutdown != state) {
            ZS_LOG_WARNING(Insane, log("cannot go into state while shutting down (probably okay)") + ZS_PARAM("new state", toString(state)))
            return;
          }
        }

        ZS_LOG_BASIC(debug("state changed") + ZS_PARAM("state", toString(state)) + ZS_PARAM("old state", toString(mCurrentState)))
        mCurrentState = state;

        ServicePushMailboxSessionPtr pThis = mThisWeak.lock();
        if (pThis) {
          ZS_LOG_DEBUG(debug("attempting to report state to delegate"))
          mSubscriptions.delegate()->onServicePushMailboxSessionStateChanged(pThis, mCurrentState);
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::setError(WORD errorCode, const char *inReason)
      {
        String reason(inReason ? String(inReason) : String());

        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }
        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, debug("error already set thus ignoring new error") + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("code", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::cancel(bool completed)
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("shutting down"))
        setState(SessionState_ShuttingDown);

        if (!completed) {
          if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();
        } else {
          mGracefulShutdownReference.reset();
        }

        if (mGracefulShutdownReference) {
          IWakeDelegateProxy::create(mGracefulShutdownReference)->onWake();
          return;
        }

        setState(SessionState_Shutdown);

        mGracefulShutdownReference.reset();

        connectionReset();

        mBackgroundingNotifier.reset();

        if (mBackgroundingSubscription) {
          mBackgroundingSubscription->cancel();
          mBackgroundingSubscription.reset();
        }

        if (mReachabilitySubscription) {
          mReachabilitySubscription->cancel();
          mReachabilitySubscription.reset();
        }

        if (mLockboxSubscription) {
          mLockboxSubscription->cancel();
          mLockboxSubscription.reset();
        }

        if (mRetryTimer) {
          mRetryTimer->cancel();
          mRetryTimer.reset();
        }

        if (mServerLookup) {
          mServerLookup->cancel();
          mServerLookup.reset();
        }

        if (mNextFoldersUpdateTimer) {
          mNextFoldersUpdateTimer->cancel();
          mNextFoldersUpdateTimer.reset();
        }

        // clean out any pending queries
        for (RegisterQueryList::iterator iter_doNotUse = mRegisterQueries.begin(); iter_doNotUse != mRegisterQueries.end(); )
        {
          RegisterQueryList::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          RegisterQueryPtr query = (*current);
          query->setError(IHTTP::HTTPStatusCode_Gone, "push mailbox service unexpectedly closed before query completed");
          query->cancel();
        }

        mRegisterQueries.clear();
        // scope: clean out send queries
        {
          for (SendQueryMap::iterator iter = mSendQueries.begin(); iter != mSendQueries.end(); ++iter)
          {
            SendQueryPtr query = (*iter).second.lock();
            if (!query) continue;

            query->cancel();
          }

          mSendQueries.clear();
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::connectionFailure()
      {
        Duration retryDuration = mLastRetryDuration;
        ZS_LOG_WARNING(Detail, log("connection failure") + ZS_PARAM("wait time (s)", retryDuration))

        mDoNotRetryConnectionBefore = zsLib::now() + retryDuration;
        ZS_LOG_WARNING(Detail, log("TCP messaging unexpectedly shutdown thus will retry again later") + ZS_PARAM("later", mDoNotRetryConnectionBefore) + ZS_PARAM("duration (s)", mLastRetryDuration))

        if (retryDuration < Seconds(1))
          retryDuration = Seconds(1);

        mLastRetryDuration = retryDuration + retryDuration;

        Duration maxDuration = Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_RETRY_CONNECTION_IN_SECONDS));
        if (mLastRetryDuration > maxDuration) {
          mLastRetryDuration = maxDuration;
        }

        connectionReset();

        mRetryTimer = Timer::create(mThisWeak.lock(), retryDuration, false);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //---------------------------------------------------------------------
      void ServicePushMailboxSession::connectionReset()
      {
        ZS_LOG_DEBUG(log("connection reset"))

        // any message monitored via the message monitor that was sent over the previous connection will not be able to complete
        UseMessageMonitorManager::notifyMessageSenderObjectGone(mSentViaObjectID);
        mSentViaObjectID = 0; // reset since no longer sending via this object ID

        if (mRetryTimer) {
          mRetryTimer->cancel();
          mRetryTimer.reset();
        }

        mServerIP = IPAddress();

        //.....................................................................
        // connection setup

        if (mAccessMonitor) {
          mAccessMonitor->cancel();
          mAccessMonitor.reset();
        }

        mPeerURIFromAccessResult.clear();

        if (mGrantQuery) {
          mGrantQuery->cancel();
          mGrantQuery.reset();
        }

        if (mGrantWait) {
          mGrantWait->cancel();
          mGrantWait.reset();

          get(mObtainedLock) = false;
        }

        if (mGrantMonitor) {
          mGrantMonitor->cancel();
          mGrantMonitor.reset();
        }

        if (mPeerValidateMonitor) {
          mPeerValidateMonitor->cancel();
          mPeerValidateMonitor.reset();
        }


        //.....................................................................
        // folder list updating

        mRefreshFolders = true;
        if (mFoldersGetMonitor) {
          mFoldersGetMonitor->cancel();
          mFoldersGetMonitor.reset();
        }


        //.....................................................................
        // folder updating

        mRefreshFoldersNeedingUpdate = mRefreshFoldersNeedingUpdate || (mFoldersNeedingUpdate.size() > 0);
        mFoldersNeedingUpdate.clear();

        for (MonitorList::iterator iter = mFolderGetMonitors.begin(); iter != mFolderGetMonitors.end(); ++iter) {
          IMessageMonitorPtr monitor = (*iter);

          monitor->cancel();
          monitor.reset();
        }


        //.....................................................................
        // keys and folder creation

        mRefreshMonitorFolderCreation = true;
        if (mFolderCreationUpdateMonitor) {
          mFolderCreationUpdateMonitor->cancel();
          mFolderCreationUpdateMonitor.reset();
        }


        //.....................................................................
        // messages needing update

        mRefreshMessagesNeedingUpdate = mRefreshMessagesNeedingUpdate || (mMessagesNeedingUpdate.size() > 0);
        mMessagesNeedingUpdate.clear();

        if (mMessageMetaDataGetMonitor) {
          mMessageMetaDataGetMonitor->cancel();
          mMessageMetaDataGetMonitor.reset();
        }


        //.....................................................................
        // messages needing data

        mRefreshMessagesNeedingData = mRefreshMessagesNeedingData || (mMessagesNeedingData.size() > 0);
        mMessagesNeedingData.clear();
        mMessagesNeedingDataChannels.clear();
        if (mMessageDataGetMonitor) {
          mMessageDataGetMonitor->cancel();
          mMessageDataGetMonitor.reset();
        }

        mPendingChannelData.clear();


        //.....................................................................
        // list fetching

        mRefreshListsNeedingDownload = true;
        mListsNeedingDownload.clear();

        if (mListFetchMonitor) {
          mListFetchMonitor->cancel();
          mListFetchMonitor.reset();
        }


        //.....................................................................
        // pending delivery

        mRefreshPendingDelivery = mRefreshPendingDelivery || (mPendingDelivery.size() > 0);
        mPendingDeliveryPrecheckRequired = mRefreshPendingDelivery;

        if (mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
          mPendingDeliveryPrecheckMessageMetaDataGetMonitor->cancel();
          mPendingDeliveryPrecheckMessageMetaDataGetMonitor.reset();
        }

        for (PendingDeliveryMap::iterator iter = mPendingDelivery.begin(); iter != mPendingDelivery.end(); ++iter) {
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*iter).second;

          if (processedInfo.mPendingDeliveryMessageUpdateErrorMonitor) {
            processedInfo.mPendingDeliveryMessageUpdateErrorMonitor->cancel();
            processedInfo.mPendingDeliveryMessageUpdateErrorMonitor.reset();
          }
          if (processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor->cancel();
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor.reset();
          }
        }

        mPendingDelivery.clear();


        //.....................................................................
        // mark read or delete message

        for (MonitorList::iterator iter = mMarkingMonitors.begin(); iter != mMarkingMonitors.end(); ++iter) {
          IMessageMonitorPtr monitor = (*iter);

          monitor->cancel();
        }
        mMarkingMonitors.clear();


        //.....................................................................
        // wire cleanup

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
          mTCPMessaging.reset();
        }

        if (mWireStream) {
          mWireStream->cancel();
          mWireStream.reset();
        }

        if (mReader) {
          mReader->cancel();
          mReader.reset();
        }

        if (mWriter) {
          mWriter->cancel();
          mWriter.reset();
        }
      }

      //---------------------------------------------------------------------
      void ServicePushMailboxSession::notifyChangedFolder(const String &folderName)
      {
        if (mMonitoredFolders.end() == mMonitoredFolders.find(folderName)) {
          ZS_LOG_TRACE(log("folder not monitored thus ignoring notification"))
          return;
        }

        mSubscriptions.delegate()->onServicePushMailboxSessionFolderChanged(mThisWeak.lock(), folderName);
        mNotifiedMonitoredFolders[folderName] = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
      }

      //---------------------------------------------------------------------
      bool ServicePushMailboxSession::send(MessagePtr message) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        AutoRecursiveLock lock(*this);
        if (!message) {
          ZS_LOG_ERROR(Detail, log("message to send was NULL"))
          return false;
        }

        if (isShutdown()) {
          ZS_LOG_WARNING(Detail, log("attempted to send a message but the location is shutdown"))
          return false;
        }

        if (!mWriter) {
          ZS_LOG_WARNING(Detail, log("requested to send a message but send stream is not ready"))
          return false;
        }

        DocumentPtr document = message->encode();

        SecureByteBlockPtr output = IHelper::writeAsJSON(document);

        if (ZS_IS_LOGGING(Detail)) {
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("MESSAGE INFO") + Message::toDebug(message))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log("PUSH MAILBOX SEND MESSAGE") + ZS_PARAM("json out", ((CSTR)(output->BytePtr()))))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
          ZS_LOG_BASIC(log(">>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>>[oo]>"))
          ZS_LOG_BASIC(log("-------------------------------------------------------------------------------------------"))
        }

        mWriter->write(output->BytePtr(), output->SizeInBytes());
        return true;
      }

      //---------------------------------------------------------------------
      IMessageMonitorPtr ServicePushMailboxSession::sendRequest(
                                                                IMessageMonitorDelegatePtr delegate,
                                                                MessagePtr requestMessage,
                                                                Duration timeout
                                                                )
      {
        IMessageMonitorPtr monitor = IMessageMonitor::monitor(delegate, requestMessage, timeout);
        if (!monitor) {
          ZS_LOG_WARNING(Detail, log("failed to create monitor"))
          return IMessageMonitorPtr();
        }

        sendRequest(requestMessage);
        return monitor;
      }

      //---------------------------------------------------------------------
      bool ServicePushMailboxSession::sendRequest(MessagePtr requestMessage)
      {
        bool result = send(requestMessage);
        if (!result) {
          // notify that the message requester failed to send the message...
          UseMessageMonitorManager::notifyMessageSendFailed(requestMessage);
          return false;
        }

        if (0 == mSentViaObjectID) {
          mSentViaObjectID = zsLib::createPUID();
        }

        UseMessageMonitorManager::trackSentViaObjectID(requestMessage, mSentViaObjectID);

        ZS_LOG_DEBUG(log("request successfully created"))
        return true;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleChanged(ChangedNotifyPtr notify)
      {
        ZS_LOG_DEBUG(log("received change notification"))

        String serverVersion = notify->version();

        if ((serverVersion.hasData()) &&
            (serverVersion != mDB->getLastDownloadedVersionForFolders())) {
          ZS_LOG_DEBUG(log("folders need updating") + ZS_PARAM("version", serverVersion))
          mRefreshFolders = true;
        }

        const PushMessageFolderInfoList &folders = notify->folders();
        const PushMessageInfoList &messages = notify->messages();

        for (PushMessageFolderInfoList::const_iterator iter = folders.begin(); iter != folders.end(); ++iter)
        {
          const PushMessageFolderInfo &info = (*iter);
          if (info.mDisposition == PushMessageFolderInfo::Disposition_Remove) {
            ZS_LOG_DEBUG(log("folder is removed") + ZS_PARAM("name", info.mName))
            mRefreshFolders = true;
            mRefreshMonitorFolderCreation = true;
            continue;
          }

          mRefreshFoldersNeedingUpdate = true;

          ZS_LOG_DEBUG(log("folder needs update") + ZS_PARAM("name", info.mName) + ZS_PARAM("version", info.mVersion))

          mDB->updateFolderServerVersionIfFolderExists(info.mName, info.mVersion);
        }

        for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          const PushMessageInfo &info = (*iter);
          if (info.mDisposition == PushMessageInfo::Disposition_Remove) {
            ZS_LOG_DEBUG(log("message is removed") + ZS_PARAM("message id", info.mID))

            handleMessageGone(info.mID);
            continue;
          }

          ZS_LOG_DEBUG(log("message needs meta data update") + ZS_PARAM("message id", info.mID) + ZS_PARAM("server version", info.mVersion))

          mDB->updateMessageServerVersionIfExists(info.mID, info.mVersion);
        }

        ZS_LOG_DEBUG(log("finished processing change notification"))
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleListFetch(ListFetchRequestPtr request)
      {
        typedef ListFetchRequest::ListIDList ListIDList;
        typedef ListFetchResult::URIListMap URIListMap;

        typedef IServicePushMailboxDatabaseAbstractionDelegate::URIList URIList;

        ListFetchResultPtr result = ListFetchResult::create(request);

        URIListMap listResults;

        const ListIDList &listIDs = request->listIDs();

        ZS_LOG_DEBUG(log("received request to fetch list(s)") + ZS_PARAM("total lists", listIDs.size()))

        for (ListIDList::const_iterator iter = listIDs.begin(); iter != listIDs.end(); ++iter)
        {
          const String &listID = (*iter);

          URIList uris;
          mDB->getListURIs(listID, uris);

          if (uris.size() > 0) {
            ZS_LOG_DEBUG(log("found list containing URIs") + ZS_PARAM("list id", listID) + ZS_PARAM("total", uris.size()))
            listResults[listID] = uris;
          } else {
            ZS_LOG_WARNING(Debug, log("did not find any URIs for list") + ZS_PARAM("list id", listID))
          }
        }

        result->listToURIs(listResults);

        send(result);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handelUpdateMessageGone(
                                                              MessageUpdateRequestPtr request,
                                                              bool notFoundError
                                                              )
      {
        if (!request) {
          ZS_LOG_WARNING(Detail, log("request was not provided"))
          return;
        }

        const PushMessageInfo &info = request->messageInfo();
        if (PushMessageInfo::Disposition_Remove == info.mDisposition) {
          handleMessageGone(info.mID);
          return;
        }

        if (!notFoundError) {
          ZS_LOG_TRACE(log("request was for update and message isn't gone so nothing to do") + info.toDebug())
          return;
        }

        ZS_LOG_WARNING(Detail, log("message was requested to update but the message is now gone"))

        handleMessageGone(info.mID);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleMessageGone(const String &messageID)
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderIndexList FolderIndexList;

        ZS_LOG_DEBUG(log("message is now gone") + ZS_PARAM("message id", messageID))

        // scope: removal message is gone
        {
          MessageMap::iterator found = mMessagesToRemove.find(messageID);
          if (found != mMessagesToRemove.end()) {
            mMessagesToRemove.erase(found);
          }
        }

        // scope: marking read message is gone
        {
          MessageMap::iterator found = mMessagesToMarkRead.find(messageID);
          if (found != mMessagesToMarkRead.end()) {
            mMessagesToMarkRead.erase(found);
          }
        }

        int index = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        if (!mDB->getMessageIndex(messageID, index)) {
          ZS_LOG_WARNING(Detail, log("message is not in local database"))
          return;
        }

        // scope: screen out pending delivery
        {
          PendingDeliveryMap::iterator found = mPendingDelivery.find(messageID);
          if (found != mPendingDelivery.end()) {
            ProcessedPendingDeliveryMessageInfo &processedInfo = (*found).second;

            if (processedInfo.mPendingDeliveryMessageUpdateRequest) {
              ZS_LOG_WARNING(Detail, log("cannot remove message that is pending delivery and being uploaded at this time") + ZS_PARAM("message id", messageID))
              return;
            }

            mPendingDelivery.erase(found);
          }
        }

        // scope: kill send query (if exists)
        {
          SendQueryMap::iterator found = mSendQueries.find(messageID);
          if (found != mSendQueries.end()) {
            SendQueryPtr query = (*found).second.lock();
            if (query) {
              query->notifyRemoved();
              query->cancel();
            }
            mSendQueries.erase(found);
          }
        }

        FolderIndexList folderIndexes;
        mDB->getFoldersWithMessage(messageID, folderIndexes);

        // notify that this message is now removed in the version table
        for (FolderIndexList::iterator iter = folderIndexes.begin(); iter != folderIndexes.end(); ++iter) {
          int folderIndex = (*iter);
          mDB->addRemovedFolderVersionedMessageEntryIfMessageNotRemoved(folderIndex, messageID);
          mDB->removeMessageFromFolder(folderIndex, messageID);
        }

        mDB->removeMessageDeliveryStatesForMessage(index);
        mDB->removePendingDeliveryMessageByMessageIndex(index);
        mDB->removeMessage(index);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleChannelMessage(
                                                           DWORD channel,
                                                           SecureByteBlockPtr buffer
                                                           )
      {
        mPendingChannelData.push_back(PendingChannelData(channel, buffer));

        if (mMessageDataGetMonitor) {
          // backlog the data because we need to wait for the monitor result to be processed first..
          return;
        }

        processChannelMessages();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::processChannelMessages()
      {
        while (mPendingChannelData.size() > 0) {
          PendingChannelData data = mPendingChannelData.front();
          mPendingChannelData.pop_front();

          SecureByteBlockPtr buffer = data.second;
          if (!buffer) {
            ZS_LOG_ERROR(Detail, log("channel had no message data") + ZS_PARAM("channel", data.first))
            continue;
          }

          ChannelToMessageMap::iterator foundChannel = mMessagesNeedingDataChannels.find(data.first);
          if (foundChannel == mMessagesNeedingDataChannels.end()) {
            ZS_LOG_WARNING(Detail, log("no channel number mapped for data") + ZS_PARAM("channel", data.first))
            continue;
          }

          String messageID = (*foundChannel).second;

          MessageDataMap::iterator foundData = mMessagesNeedingData.find(messageID);
          if (foundData == mMessagesNeedingData.end()) {
            ZS_LOG_WARNING(Detail, log("no message needing data for message id") + ZS_PARAM("message id", messageID))
            continue;
          }

          ProcessedMessageNeedingDataInfo &info = (*foundData).second;

          if (info.mReceivedData + buffer->SizeInBytes() > info.mInfo.mDataLength) {
            ZS_LOG_ERROR(Detail, log("data length does not match server length"))
            connectionFailure();
            continue;
          }

          if (!info.mBuffer) {
            info.mBuffer = SecureByteBlockPtr(new SecureByteBlock(info.mInfo.mDataLength));
          }

          ZS_LOG_DEBUG(log("receiving data from server") + ZS_PARAM("channel", data.first) + ZS_PARAM("message id", messageID) + ZS_PARAM("length", buffer->SizeInBytes()) + ZS_PARAM("total", info.mBuffer->SizeInBytes()))

          memcpy(&((info.mBuffer->BytePtr())[info.mReceivedData]), buffer->BytePtr(), buffer->SizeInBytes());

          info.mReceivedData += buffer->SizeInBytes();

          if (info.mReceivedData != info.mInfo.mDataLength) {
            ZS_LOG_DEBUG(log("more data still expected"))
            continue;
          }

          ZS_LOG_DEBUG(log("update database record with received for message"))

          mDB->updateMessageData(info.mInfo.mIndex, *info.mBuffer);

          mRefreshKeysFolderNeedsProcessing = true;
          mRefreshMessagesNeedingDecryption = true;

          mMessagesNeedingDataChannels.erase(foundChannel);
          mMessagesNeedingData.erase(foundData);

          IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        }
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::prepareMessageID(const String &inMessageID)
      {
        String messageID = inMessageID;
        if (messageID.isEmpty()) {
          messageID = IHelper::randomString(32);
        }

        if (std::string::npos == messageID.find('@')) {
          // must append the default domain
          messageID = messageID + "@" + mBootstrappedNetwork->getDomain();
        }

        return messageID;
      }
      
      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areRegisterQueriesActive() const
      {
        if (mRegisterQueries.size() > 0) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::isFolderListUpdating() const
      {
        if (mRefreshFolders) return true;
        if (mFoldersGetMonitor) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyFoldersUpdating() const
      {
        if (mRefreshFoldersNeedingUpdate) return true;
        if (mFoldersNeedingUpdate.size() > 0) return true;
        if (mFolderGetMonitors.size() > 0) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesUpdating() const
      {
        if (mRefreshMessagesNeedingUpdate) return true;
        if (mMessagesNeedingUpdate.size() > 0) return true;
        if (mMessageMetaDataGetMonitor) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesDownloadingData() const
      {
        if (mRefreshMessagesNeedingData) return true;
        if (mMessagesNeedingData.size() > 0) return true;
        if (mMessageDataGetMonitor) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyListsFetching() const
      {
        if (mRefreshListsNeedingDownload) return true;
        if (mListsNeedingDownload.size() > 0) return true;
        if (mListFetchMonitor) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areMonitoredFoldersReady() const
      {
        if (mRefreshMonitorFolderCreation) return false;
        if (mFolderCreationUpdateMonitor) return false;
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesDecrypting() const
      {
        if (mRefreshMessagesNeedingDecryption) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesUploading() const
      {
        if (mRefreshPendingDelivery) return true;
        if (mPendingDelivery.size() > 0) return true;
        if (mPendingDeliveryPrecheckMessageMetaDataGetMonitor) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyFoldersNeedingVersionedMessages() const
      {
        return mRefreshVersionedFolders;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesMarkReadOrDeleting() const
      {
        if (mMessagesToMarkRead.size() > 0) return true;
        if (mMessagesToRemove.size() > 0) return true;
        if (mMarkingMonitors.size() > 0) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesExpiring() const
      {
        return mRefreshMessagesNeedingExpiry;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::extractKeyingBundle(
                                                          SecureByteBlockPtr buffer,
                                                          KeyingBundle &outBundle
                                                          )
      {
        if (!buffer) {
          ZS_LOG_WARNING(Detail, log("extract failed as data buffer was null"))
          return false;
        }

        const char *bufferStr = (CSTR)(buffer->BytePtr());

        DocumentPtr document = Document::createFromAutoDetect(bufferStr);
        if (!document) {
          ZS_LOG_WARNING(Detail, log("failed to parse document") + ZS_PARAM("json", bufferStr))
          return false;
        }

        try {
          ElementPtr keyingBundleEl = document->findFirstChildElementChecked("keyingBundle");
          ElementPtr keyingEl = keyingBundleEl->findFirstChildElementChecked("keying");

          outBundle.mReferencedKeyID = IMessageHelper::getAttributeID(keyingEl);
          if (outBundle.mReferencedKeyID.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("missing referenced key ID in keying bundle"))
            return false;
          }

          outBundle.mExpires = IHelper::stringToTime(IMessageHelper::getElementText(keyingEl->findFirstChildElementChecked("expires")));

          outBundle.mType = IMessageHelper::getElementText(keyingEl->findFirstChildElementChecked("type"));
          if (outBundle.mType.isEmpty()) {
            ZS_LOG_WARNING(Detail, log("missing keying type in keying bundle"))
            return false;
          }

          outBundle.mMode = IMessageHelper::getElementText(keyingEl->findFirstChildElement("mode"));
          outBundle.mMode = IMessageHelper::getElementText(keyingEl->findFirstChildElement("agreement"));
          outBundle.mSecret = IMessageHelper::getElementText(keyingEl->findFirstChildElement("secret"));

          if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_PKI == outBundle.mType) {
            if (outBundle.mSecret.isEmpty()) {
              ZS_LOG_WARNING(Detail, log("missing keying secret in keying bundle"))
              return false;
            }
          }

          if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT == outBundle.mType) {
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_REQUEST_OFFER != outBundle.mMode) {
              if (outBundle.mAgreement.isEmpty()) {
                ZS_LOG_WARNING(Detail, log("missing keying agreement information in keying bundle"))
                return false;
              }
            }
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_ANSWER == outBundle.mMode) {
              if (outBundle.mSecret.isEmpty()) {
                ZS_LOG_WARNING(Detail, log("missing keying secret in keying bundle"))
                return false;
              }
            }
          }

          IPeerPtr peer = IPeer::getFromSignature(Account::convert(mAccount), keyingEl);
          if (!peer) {
            ZS_LOG_WARNING(Detail, log("cannot process keying material from unknown peer"))
            return false;
          }

          if (!peer->verifySignature(keyingEl)) {
            ZS_LOG_WARNING(Detail, log("peer signature did not validate"))
            return false;
          }

          outBundle.mValidationPeer = peer;

        } catch (CheckFailed &) {
          ZS_LOG_WARNING(Detail, log("expecting element which was missing in keying bundle"))
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::createKeyingBundle(const KeyingBundle &inBundle)
      {
        ElementPtr keyingBundleEl = Element::create("keyingBundle");
        ElementPtr keyingEl = IMessageHelper::createElementWithID("keying", inBundle.mReferencedKeyID);

        if (Time() != inBundle.mExpires) {
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", IHelper::timeToString(inBundle.mExpires)));
        }

        if (inBundle.mType.hasData()) {
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("type", inBundle.mType));
        }
        if (inBundle.mMode.hasData()) {
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("mode", inBundle.mMode));
        }
        if (inBundle.mAgreement.hasData()) {
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("agreement", inBundle.mAgreement));
        }
        if (inBundle.mSecret.hasData()) {
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("secret", inBundle.mSecret));
        }

        keyingBundleEl->adoptAsLastChild(keyingEl);

        if (!mPeerFiles) {
          ZS_LOG_WARNING(Detail, log("peer files missing"))
          return ElementPtr();
        }

        IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();

        peerFilePrivate->signElement(keyingEl);

        return keyingBundleEl;
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::extractRSA(const String &secret)
      {
        IHelper::SplitMap parts;
        IHelper::split(secret, parts, ':');
        if (parts.size() != 3) {
          ZS_LOG_WARNING(Detail, log("failed to extract RSA encrypted key") + ZS_PARAM("secret", secret))
          return String();
        }

        String salt = parts[0];
        String proof = parts[1];
        SecureByteBlockPtr encrytpedPassword = IHelper::convertFromBase64(parts[2]);

        if (!encrytpedPassword) {
          ZS_LOG_WARNING(Detail, log("failed to extract encrypted password") + ZS_PARAM("secret", secret))
          return String();
        }

        ZS_THROW_BAD_STATE_IF(!mPeerFiles)

        IPeerFilePrivatePtr peerFilePrivate = mPeerFiles->getPeerFilePrivate();

        if (!peerFilePrivate) {
          ZS_LOG_WARNING(Detail, log("failed to obtain peer file private"))
          return String();
        }

        SecureByteBlockPtr passphrase = peerFilePrivate->decrypt(*encrytpedPassword);
        if (!passphrase) {
          ZS_LOG_WARNING(Detail, log("failed to decrypt passphrase with RSA private key"))
          return String();
        }

        String resultPassphrase = IHelper::convertToString(*passphrase);

        String calculatedProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(resultPassphrase), "proof:" + salt, IHelper::HashAlgorthm_SHA1));
        if (calculatedProof != proof) {
          ZS_LOG_WARNING(Detail, log("proof for RSA key does not match") + ZS_PARAM("proof", proof) + ZS_PARAM("calculated proof", calculatedProof))
          return String();
        }

        return resultPassphrase;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::prepareDHKeyDomain(
                                                         int inKeyDomain,
                                                         IDHKeyDomainPtr &outKeyDomain,
                                                         SecureByteBlockPtr &outStaticPrivateKey,
                                                         SecureByteBlockPtr &outStaticPublicKey
                                                         )
      {
        String staticPrivateKeyStr;
        String staticPublicKeyStr;
        if (mDB->getKeyDomain(inKeyDomain, staticPrivateKeyStr, staticPublicKeyStr)) {
          outStaticPrivateKey = IHelper::convertFromBase64(staticPrivateKeyStr);
          outStaticPublicKey = IHelper::convertFromBase64(staticPublicKeyStr);
        }

        if ((outStaticPrivateKey) &&
            (outStaticPublicKey)) {
          ZS_LOG_TRACE(log("loaded existing static private/public key") + ZS_PARAM("key domain", inKeyDomain))
          return true;
        }

        IDHKeyDomainPtr keyDomain = IDHKeyDomain::loadPrecompiled((IDHKeyDomain::KeyDomainPrecompiledTypes)inKeyDomain);
        if (!keyDomain) {
          ZS_LOG_WARNING(Detail, log("failed to load existing key domain") + ZS_PARAM("key domain", inKeyDomain))
          return false;
        }

        outKeyDomain = keyDomain;

        IDHPublicKeyPtr publicKey;
        IDHPrivateKeyPtr privateKey = IDHPrivateKey::generate(keyDomain, publicKey);

        if ((!privateKey) ||
            (!publicKey)) {
          ZS_LOG_WARNING(Detail, log("failed to generate new DH static keys for key domain") + ZS_PARAM("key domain", inKeyDomain))
          return false;
        }

        outStaticPrivateKey = SecureByteBlockPtr(new SecureByteBlock);
        outStaticPublicKey = SecureByteBlockPtr(new SecureByteBlock);

        SecureByteBlock ephemeralPrivateKey;
        privateKey->save(&(*outStaticPrivateKey), &ephemeralPrivateKey);

        SecureByteBlock ephemeralPublicKey;
        publicKey->save(&(*outStaticPublicKey), &ephemeralPublicKey);

        ZS_THROW_INVALID_ASSUMPTION_IF(!outStaticPrivateKey)
        ZS_THROW_INVALID_ASSUMPTION_IF(!outStaticPublicKey)

        mDB->addKeyDomain(inKeyDomain, IHelper::convertToBase64(*outStaticPrivateKey), IHelper::convertToBase64(*outStaticPublicKey));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::prepareDHKeys(
                                                    int inKeyDomain,
                                                    const String &inEphemeralPrivateKey,
                                                    const String &inEphemeralPublicKey,
                                                    IDHPrivateKeyPtr &outPrivateKey,
                                                    IDHPublicKeyPtr &outPublicKey
                                                    )
      {
        if ((inEphemeralPrivateKey.isEmpty()) ||
            (inEphemeralPublicKey.isEmpty())) {
          ZS_LOG_WARNING(Detail, log("ephemeral keys missing values") + ZS_PARAM("ephemeral private key", inEphemeralPrivateKey) + ZS_PARAM("inEphemeralPublicKey", inEphemeralPublicKey))
          return false;
        }

        SecureByteBlockPtr ephemeralPrivateKey = IHelper::convertFromBase64(inEphemeralPrivateKey);
        SecureByteBlockPtr ephemeralPublicKey = IHelper::convertFromBase64(inEphemeralPublicKey);

        if ((!ephemeralPrivateKey) ||
            ((!ephemeralPublicKey))) {
          ZS_LOG_WARNING(Detail, log("ephemeral keys failed to decode") + ZS_PARAM("ephemeral private key", inEphemeralPrivateKey) + ZS_PARAM("inEphemeralPublicKey", inEphemeralPublicKey))
          return false;
        }

        IDHKeyDomainPtr keyDomain;
        SecureByteBlockPtr staticPrivateKey;
        SecureByteBlockPtr staticPublicKey;
        if (!prepareDHKeyDomain(inKeyDomain, keyDomain, staticPrivateKey, staticPublicKey)) {
          ZS_LOG_WARNING(Detail, log("failed to load static keys for key domain") + ZS_PARAM("key domain", inKeyDomain))
          return false;
        }

        ZS_THROW_INVALID_ASSUMPTION_IF(!keyDomain)
        ZS_THROW_INVALID_ASSUMPTION_IF(!staticPrivateKey)
        ZS_THROW_INVALID_ASSUMPTION_IF(!staticPublicKey)

        outPrivateKey = IDHPrivateKey::load(keyDomain, *staticPrivateKey, *ephemeralPrivateKey);
        outPublicKey = IDHPublicKey::load(*staticPublicKey, *ephemeralPublicKey);

        return (outPrivateKey) && (outPublicKey);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::prepareNewDHKeys(
                                                       const char *inKeyDomainStr,
                                                       int &outKeyDomain,
                                                       String &outEphemeralPrivateKey,
                                                       String &outEphemeralPublicKey,
                                                       IDHPrivateKeyPtr &outPrivateKey,
                                                       IDHPublicKeyPtr &outPublicKey
                                                       )
      {
        int inKeyDomain = (int)IDHKeyDomain::KeyDomainPrecompiledType_Unknown;
        if (NULL != inKeyDomainStr) {
          inKeyDomain = (int)IDHKeyDomain::fromNamespace(inKeyDomainStr);
        }

        if (((int)IDHKeyDomain::KeyDomainPrecompiledType_Unknown) == inKeyDomain) {
          inKeyDomain = (int)IDHKeyDomain::fromNamespace(services::ISettings::getString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DEFAULT_DH_KEY_DOMAIN));
        }

        outKeyDomain = inKeyDomain;

        SecureByteBlockPtr staticPrivateKey;
        SecureByteBlockPtr staticPublicKey;

        IDHKeyDomainPtr keyDomain;
        if (!prepareDHKeyDomain(inKeyDomain, keyDomain, staticPrivateKey, staticPublicKey)) {
          ZS_LOG_WARNING(Detail, log("failed to load key domain") + ZS_PARAM("domain", inKeyDomain))
          return false;
        }

        outPrivateKey = IDHPrivateKey::loadAndGenerateNewEphemeral(keyDomain, *staticPrivateKey, *staticPublicKey, outPublicKey);

        if (outPrivateKey) {
          SecureByteBlock staticPrivateKeyBuffer;
          SecureByteBlock ephemeralPrivateKeyBuffer;

          outPrivateKey->save(&staticPrivateKeyBuffer, &ephemeralPrivateKeyBuffer);

          outEphemeralPrivateKey = IHelper::convertToBase64(ephemeralPrivateKeyBuffer);
        }
        if (outPublicKey) {
          SecureByteBlock staticPublicKeyBuffer;
          SecureByteBlock ephemeralPublicKeyBuffer;

          outPublicKey->save(&staticPublicKeyBuffer, &ephemeralPublicKeyBuffer);

          outEphemeralPublicKey = IHelper::convertToBase64(ephemeralPublicKeyBuffer);
        }

        return (outEphemeralPrivateKey.hasData()) &&
               (outEphemeralPublicKey.hasData()) &&
               (outPrivateKey) &&
               (outPublicKey);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::extractDHFromAgreement(
                                                             const String &agreement,
                                                             int &outKeyDomain,
                                                             IDHPublicKeyPtr &outPublicKey
                                                             )
      {
        IHelper::SplitMap split;
        IHelper::split(agreement, split, ':');
        if (split.size() != 3) {
          ZS_LOG_WARNING(Detail, log("cannot split agrement properly") + ZS_PARAM("agreement", agreement))
          return false;
        }

        String domain = IHelper::convertToString(*IHelper::convertFromBase64(split[0]));

        IDHKeyDomain::KeyDomainPrecompiledTypes precompiledKeyDomain = IDHKeyDomain::fromNamespace(domain);
        if (IDHKeyDomain::KeyDomainPrecompiledType_Unknown == precompiledKeyDomain) {
          ZS_LOG_WARNING(Detail, log("cannot extract a known key domain") + ZS_PARAM("agreement", agreement) + ZS_PARAM("domain", domain))
          return false;
        }

        SecureByteBlockPtr staticPublicKey = IHelper::convertFromBase64(split[1]);
        SecureByteBlockPtr ephemeralPublicKey = IHelper::convertFromBase64(split[2]);

        outPublicKey = IDHPublicKey::load(*staticPublicKey, *ephemeralPublicKey);

        return (bool)outPublicKey;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::sendKeyingBundle(
                                                       const String &toURI,
                                                       const KeyingBundle &inBundle
                                                       )
      {
        ElementPtr keyingBundleEl = createKeyingBundle(inBundle);
        if (!keyingBundleEl) {
          ZS_LOG_WARNING(Detail, log("unable to create keying bundle"))
          return false;
        }

        DocumentPtr document = Document::create();
        document->adoptAsLastChild(keyingBundleEl);

        SecureByteBlockPtr output = IHelper::writeAsJSON(document);
        if (!output) {
          ZS_LOG_ERROR(Detail, log("failed to encode to json"))
          return false;
        }

        String messageID = prepareMessageID(String());

        if (!mPeerFiles) {
          ZS_LOG_WARNING(Detail, log("peer files missing"))
          return false;
        }

        IPeerFilePublicPtr peerFilePublic = mPeerFiles->getPeerFilePublic();

        PushInfoList empty;
        int index = mDB->insertMessage(
                                       messageID,
                                       toURI,
                                       peerFilePublic->getPeerURI(),
                                       NULL,
                                       NULL,
                                       OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE,
                                       OPENPEER_STACK_PUSH_MAILBOX_JSON_MIME_TYPE,
                                       OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE,
                                       NULL,
                                       empty,
                                       zsLib::now(),
                                       inBundle.mExpires,
                                       output,
                                       SecureByteBlockPtr(),
                                       false);

        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == index) {
          ZS_LOG_ERROR(Detail, log("failed to insert keying message into messages table") + ZS_PARAM("message id", messageID))
          return false;
        }

        mDB->insertPendingDeliveryMessage(index, OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME, false, 0);
        mRefreshPendingDelivery = true;

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::sendRequestOffer(
                                                       const String &toURI,
                                                       const String &keyID,
                                                       int inKeyDomain,
                                                       IDHPublicKeyPtr publicKey,
                                                       Time expires
                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!publicKey)

        KeyingBundle bundle;
        bundle.mReferencedKeyID = keyID;
        bundle.mType = OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT;
        bundle.mMode = OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_REQUEST_OFFER;
        String domain = IDHKeyDomain::toNamespace((IDHKeyDomain::KeyDomainPrecompiledTypes)inKeyDomain);

        SecureByteBlock staticPublicKey;
        SecureByteBlock ephemeralPublicKey;
        publicKey->save(&staticPublicKey, &ephemeralPublicKey);
        bundle.mAgreement = IHelper::convertToBase64(domain) + ":" + IHelper::convertToBase64(staticPublicKey) + ":" + IHelper::convertToBase64(ephemeralPublicKey);

        return sendKeyingBundle(toURI, bundle);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::sendOfferRequestExisting(
                                                               const String &toURI,
                                                               const String &keyID,
                                                               int inKeyDomain,
                                                               IDHPublicKeyPtr publicKey,
                                                               Time expires
                                                               )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!publicKey)

        KeyingBundle bundle;
        bundle.mReferencedKeyID = keyID;
        bundle.mType = OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT;
        bundle.mMode = OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_OFFER_REQUEST_EXISTING;
        String domain = IDHKeyDomain::toNamespace((IDHKeyDomain::KeyDomainPrecompiledTypes)inKeyDomain);

        SecureByteBlock staticPublicKey;
        SecureByteBlock ephemeralPublicKey;
        publicKey->save(&staticPublicKey, &ephemeralPublicKey);
        bundle.mAgreement = IHelper::convertToBase64(domain) + ":" + IHelper::convertToBase64(staticPublicKey) + ":" + IHelper::convertToBase64(ephemeralPublicKey);

        return sendKeyingBundle(toURI, bundle);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::sendPKI(
                                              const String &toURI,
                                              const String &keyID,
                                              const String &passphrase,
                                              IPeerPtr validationPeer,
                                              Time expires
                                              )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!validationPeer)

        KeyingBundle bundle;
        bundle.mReferencedKeyID = keyID;
        bundle.mType = OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_PKI;

        IPeerFilePublicPtr peerFilePublic = validationPeer->getPeerFilePublic();
        if (!peerFilePublic) {
          ZS_LOG_WARNING(Detail, log("peer file of remote peer is not known thus can't encrypt passphrase") + ZS_PARAM("to", toURI) + ZS_PARAM("validation peer", validationPeer->getPeerURI()))
          return false;
        }

        SecureByteBlockPtr encryptedPassphrase = peerFilePublic->encrypt(*IHelper::convertToBuffer(passphrase));

        String salt = IHelper::randomString(20);
        String proof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(passphrase), "proof:" + salt, IHelper::HashAlgorthm_SHA1));

        bundle.mSecret = salt + ":" + proof + ":" + IHelper::convertToBase64(*encryptedPassphrase);

        return sendKeyingBundle(toURI, bundle);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::sendAnswer(
                                                 const String &toURI,
                                                 const String &keyID,
                                                 const String &passphrase,
                                                 const String &remoteAgreement,
                                                 int inKeyDomain,
                                                 IDHPrivateKeyPtr privateKey,
                                                 IDHPublicKeyPtr publicKey,
                                                 Time expires
                                                 )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!privateKey)
        ZS_THROW_INVALID_ARGUMENT_IF(!publicKey)

        KeyingBundle bundle;
        bundle.mReferencedKeyID = keyID;
        bundle.mType = OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT;
        bundle.mMode = OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_ANSWER;
        String domain = IDHKeyDomain::toNamespace((IDHKeyDomain::KeyDomainPrecompiledTypes)inKeyDomain);

        SecureByteBlock staticPublicKey;
        SecureByteBlock ephemeralPublicKey;
        publicKey->save(&staticPublicKey, &ephemeralPublicKey);
        bundle.mAgreement = IHelper::convertToBase64(domain) + ":" + IHelper::convertToBase64(staticPublicKey) + ":" + IHelper::convertToBase64(ephemeralPublicKey);

        int remoteKeyDomain = (int)IDHKeyDomain::KeyDomainPrecompiledType_Unknown;
        IDHPublicKeyPtr remotePublicKey;
        if (!extractDHFromAgreement(remoteAgreement, remoteKeyDomain, remotePublicKey)) {
          ZS_LOG_WARNING(Detail, log("failed to extract remote DH agrement information") + ZS_PARAM("agreement", remoteAgreement))
          return false;
        }

        if (remoteKeyDomain != inKeyDomain) {
          ZS_LOG_WARNING(Detail, log("key domains do not match thus cannot answer") + ZS_PARAM("agreement", remoteAgreement) + ZS_PARAM("key domain", inKeyDomain) + ZS_PARAM("remote key domain", remoteKeyDomain))
          return false;
        }

        SecureByteBlockPtr agreementKey = privateKey->getSharedSecret(remotePublicKey);

        SecureByteBlockPtr key = IHelper::hmac(*IHelper::hmacKeyFromPassphrase(IHelper::convertToHex(*agreementKey)), "push-mailbox:", IHelper::HashAlgorthm_SHA256);

        bundle.mSecret = stack::IHelper::splitEncrypt(*key, *IHelper::convertToBuffer(passphrase));

        return sendKeyingBundle(toURI, bundle);
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::getAnswerPassphrase(
                                                            int inKeyDomain,
                                                            const String &ephemeralPrivateKey,
                                                            const String &ephemeralPublicKey,
                                                            const String &remoteAgreement,
                                                            const String &secret
                                                            )
      {
        IDHPrivateKeyPtr privateKey;
        IDHPublicKeyPtr publicKey;
        if (!prepareDHKeys(inKeyDomain, ephemeralPrivateKey, ephemeralPublicKey, privateKey, publicKey)) {
          ZS_LOG_WARNING(Detail, log("unable to prepare DH keying material") + ZS_PARAM("domain", inKeyDomain) + ZS_PARAM("private key", ephemeralPrivateKey) + ZS_PARAM("public key", ephemeralPublicKey))
          return String();
        }

        int remoteKeyDomain = (int)IDHKeyDomain::KeyDomainPrecompiledType_Unknown;
        IDHPublicKeyPtr remotePublicKey;
        if (!extractDHFromAgreement(remoteAgreement, remoteKeyDomain, remotePublicKey)) {
          ZS_LOG_WARNING(Detail, log("failed to extract remote DH agrement information") + ZS_PARAM("agreement", remoteAgreement))
          return String();
        }

        if (remoteKeyDomain != inKeyDomain) {
          ZS_LOG_WARNING(Detail, log("key domains do not match thus cannot extract answer") + ZS_PARAM("agreement", remoteAgreement) + ZS_PARAM("key domain", inKeyDomain) + ZS_PARAM("remote key domain", remoteKeyDomain))
          return String();
        }
        
        SecureByteBlockPtr agreementKey = privateKey->getSharedSecret(remotePublicKey);
        
        SecureByteBlockPtr key = IHelper::hmac(*IHelper::hmacKeyFromPassphrase(IHelper::convertToHex(*agreementKey)), "push-mailbox:", IHelper::HashAlgorthm_SHA256);
        
        SecureByteBlockPtr decryptedPassphrase = stack::IHelper::splitDecrypt(*key, secret);
        
        return IHelper::convertToString(*decryptedPassphrase);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::addToMasterList(
                                                      PeerOrIdentityListPtr &master,
                                                      const PeerOrIdentityListPtr &source
                                                      )
      {
        if (!master) master = PeerOrIdentityListPtr(new PeerOrIdentityList);

        if (!source) return;
        return addToMasterList(*master, *source);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::addToMasterList(
                                                      PeerOrIdentityList &ioMaster,
                                                      const PeerOrIdentityList &source
                                                      )
      {
        for (PeerOrIdentityList::const_iterator iter = source.begin(); iter != source.end(); ++iter) {
          ioMaster.push_back(*iter);
        }
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::convertToDatabaseList(
                                                              const PeerOrIdentityListPtr &source,
                                                              size_t &outListSize
                                                              )
      {
        outListSize = 0;
        if (!source) return String();
        return convertToDatabaseList(*source, outListSize);
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::ServicePushMailboxSession::convertToDatabaseList(
                                                                                         const PeerOrIdentityList &source,
                                                                                         size_t &outListSize
                                                                                         )
      {
        typedef std::map<String, String> SortedMap;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::URIList URIList;

        if (source.size() < 1) return String();

        if (source.size() < 2) {
          ZS_LOG_TRACE(log("list only contains 1 URI thus is not a list") + ZS_PARAM("uri", source.front()))
          outListSize = 1;
          return source.front();
        }

        SortedMap result;

        for (PeerOrIdentityList::const_iterator iter = source.begin(); iter != source.end(); ++iter)
        {
          const String &uri = (*iter);
          if (uri.isEmpty()) continue;

          result[uri] = uri;
          ++outListSize;
        }

        if (result.size() < 1) return String();
        if (result.size() < 2) {
          const String &outURI = result.begin()->first;
          ZS_LOG_TRACE(log("list only contains 1 URI thus is not a list") + ZS_PARAM("uri", outURI))
          return outURI;
        }

        String deviceID = services::ISettings::getString(OPENPEER_COMMON_SETTING_DEVICE_ID);

        SHA1 hasher;
        SecureByteBlock hashResult(hasher.DigestSize());

        hasher.Update((const BYTE *)(deviceID.c_str()), deviceID.length());

        URIList uris;

        for (SortedMap::const_iterator iter = result.begin(); iter != result.end(); ++iter)
        {
          const String &uri = (*iter).second;
          hasher.Update((const BYTE *)":", strlen(":"));
          hasher.Update((const BYTE *)(uri.c_str()), uri.length());

          uris.push_back(uri);
        }

        hasher.Final(hashResult);

        String listID = IHelper::convertToHex(hashResult);
        String listURI = String(OPENPEER_STACK_PUSH_MAILBOX_LIST_URI_PREFIX) + listID;

        if (mDB->hasListID(listID)) {
          ZS_LOG_TRACE(log("list already exists") + ZS_PARAM("list ID", listID))
          return listURI;
        }

        int index = mDB->addOrUpdateListID(listID);
        if (index < 0) {
          ZS_LOG_ERROR(Detail, log("failed to create list") + ZS_PARAM("list ID", listID))
          return String();
        }

        mDB->updateListURIs(index, uris);
        mDB->notifyListDownloaded(index);

        return listURI;
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::PeerOrIdentityListPtr ServicePushMailboxSession::convertFromDatabaseList(const String &uri)
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::URIList URIList;

        if (uri.isEmpty()) return PeerOrIdentityListPtr();

        PeerOrIdentityListPtr result(new PeerOrIdentityList);

        String listID = extractListID(uri);
        if (listID.isEmpty()) {
          result->push_back(uri);
          return result;
        }

        URIList uris;

        mDB->getListURIs(listID, uris);
        if (uris.size() < 1) {
          ZS_LOG_WARNING(Detail, log("list ID not found in database") + ZS_PARAM("list", uri))
          return PeerOrIdentityListPtr();
        }

        for (URIList::iterator iter = uris.begin(); iter != uris.end(); ++iter) {
          result->push_back(*iter);
        }

        return result;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::deliverNewSendingKey(
                                                           const String &uri,
                                                           PeerOrIdentityListPtr peerList,
                                                           String &outKeyID,
                                                           String &outInitialPassphrase,
                                                           Time &outExpires
                                                           )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerList)

        typedef IServicePushMailboxDatabaseAbstractionDelegate::SendingKeyInfo SendingKeyInfo;

        SendingKeyInfo info;

        // need to generate an active sending key
        info.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        info.mKeyID = IHelper::randomString(20);
        info.mURI = uri;
        info.mRSAPassphrase = IHelper::randomString(32*8/5);
        info.mDHPassphrase = IHelper::randomString(32*8/5);
        IDHPrivateKeyPtr privateKey;
        IDHPublicKeyPtr publicKey;
        bool result = prepareNewDHKeys(
                                       services::ISettings::getString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DEFAULT_DH_KEY_DOMAIN),
                                       info.mKeyDomain,
                                       info.mDHEphemeralPrivateKey,
                                       info.mDHEphemeralPublicKey,
                                       privateKey,
                                       publicKey
                                       );

        if (!result) {
          ZS_LOG_ERROR(Detail, log("failed to generate new sending key") + ZS_PARAM("uri", uri))
          return;
        }

        info.mActiveUntil = zsLib::now() + Seconds(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_ACTIVE_UNTIL_IN_SECONDS));
        info.mExpires = zsLib::now() + Seconds(services::ISettings::getUInt(OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_EXPIRES_AFTER_IN_SECONDS));

        outKeyID = info.mKeyID;
        outInitialPassphrase = info.mRSAPassphrase;
        outExpires = info.mExpires;

        mDB->addOrUpdateSendingKey(info);

        for (PeerOrIdentityList::iterator iter = peerList->begin(); iter != peerList->end(); ++iter) {
          const String &dest = (*iter);
          if (dest.isEmpty()) continue;

          IPeerPtr peer = IPeer::create(Account::convert(mAccount), dest);
          if (!peer) {
            ZS_LOG_WARNING(Detail, log("failed to create peer") + ZS_PARAM("peer", dest))
            continue;
          }

          bool sent = sendPKI(dest, info.mKeyID, info.mRSAPassphrase, peer, info.mExpires);
          if (!sent) {
            ZS_LOG_ERROR(Detail, log("failed to send PKI") + ZS_PARAM("peer", dest))
          }
        }

        sendRequestOffer(uri, info.mKeyID, info.mKeyDomain, publicKey, info.mExpires);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::encryptMessage(
                                                     const String &inPassphraseID,
                                                     const String &inPassphrase,
                                                     SecureByteBlock &inOriginalData,
                                                     String &outEncodingScheme,
                                                     SecureByteBlockPtr &outEncryptedMessage
                                                     )
      {
        String salt = IHelper::randomString(16*8/5);

        String proof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(inPassphrase), "proof:" + salt + ":" + IHelper::convertToHex(*IHelper::hash(inOriginalData))));

        outEncodingScheme = String(OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME) + IHelper::convertToBase64(OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE) + ":" + inPassphraseID + ":" + salt + ":" + proof;

        SecureByteBlockPtr key = IHelper::hmac(*IHelper::hmacKeyFromPassphrase(inPassphrase), "push-mailbox:" + inPassphraseID, IHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = IHelper::hash(salt, IHelper::HashAlgorthm_MD5);

        outEncryptedMessage = IHelper::encrypt(*key, *iv, inOriginalData);

        return (bool)outEncryptedMessage;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::decryptMessage(
                                                     const String &inPassphraseID,
                                                     const String &inPassphrase,
                                                     const String &inSalt,
                                                     const String &inProof,
                                                     SecureByteBlock &inOriginalData,
                                                     SecureByteBlockPtr &outDecryptedMessage
                                                     )
      {
        SecureByteBlockPtr key = IHelper::hmac(*IHelper::hmacKeyFromPassphrase(inPassphrase), "push-mailbox:" + inPassphraseID, IHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = IHelper::hash(inSalt, IHelper::HashAlgorthm_MD5);

        outDecryptedMessage = IHelper::decrypt(*key, *iv, inOriginalData);
        if (!outDecryptedMessage) {
          ZS_LOG_WARNING(Detail, log("message failed to decrypt"))
          return false;
        }

        String calculatedProof = IHelper::convertToHex(*IHelper::hmac(*IHelper::hmacKeyFromPassphrase(inPassphrase), "proof:" + inSalt + ":" + IHelper::convertToHex(*IHelper::hash(*outDecryptedMessage))));

        if (inProof != calculatedProof) {
          ZS_LOG_WARNING(Detail, log("proof failed to validate thus message failed to decrypt") + ZS_PARAM("proof", inProof) + ZS_PARAM("calculated proof", calculatedProof))
          outDecryptedMessage = SecureByteBlockPtr();
          return false;
        }


        return (bool)outDecryptedMessage;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSession
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IServicePushMailboxSession::toString(SessionStates state)
    {
      switch (state)
      {
        case SessionState_Pending:              return "Pending";
        case SessionState_Connecting:           return "Connecting";
        case SessionState_Connected:            return "Connected";
        case SessionState_GoingToSleep:         return "Going to sleep";
        case SessionState_Sleeping:             return "Sleeping";
        case SessionState_ShuttingDown:         return "Shutting down";
        case SessionState_Shutdown:             return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    const char *IServicePushMailboxSession::toString(PushStates state)
    {
      switch (state) {
        case PushState_None:      return "none";

        case PushState_Read:      return "read";
        case PushState_Answered:  return "answered";
        case PushState_Flagged:   return "flagged";
        case PushState_Deleted:   return "deleted";
        case PushState_Draft:     return "draft";
        case PushState_Recent:    return "recent";
        case PushState_Delivered: return "delivered";
        case PushState_Sent:      return "sent";
        case PushState_Pushed:    return "pushed";
        case PushState_Error:     return "errro";
      }

      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    IServicePushMailboxSession::PushStates IServicePushMailboxSession::toPushState(const char *state)
    {
      if (!state) return PushState_None;

      static PushStates states[] = {
        PushState_Read,
        PushState_Answered,
        PushState_Flagged,
        PushState_Deleted,
        PushState_Draft,
        PushState_Recent,
        PushState_Delivered,
        PushState_Sent,
        PushState_Pushed,
        PushState_Error,

        PushState_None
      };

      for (int index = 0; PushState_None != states[index]; ++index)
      {
        const char *compareStr = IServicePushMailboxSession::toString(states[index]);
        if (0 == strcmp(compareStr, state)) {
          return states[index];
        }
      }

      return PushState_None;
    }

    //-------------------------------------------------------------------------
    bool IServicePushMailboxSession::canSubscribeState(PushStates state)
    {
      switch (state) {
        case PushState_None:      return false;

        case PushState_Read:      return false;
        case PushState_Answered:  return false;
        case PushState_Flagged:   return false;
        case PushState_Deleted:   return false;
        case PushState_Draft:     return false;
        case PushState_Recent:    return false;
        case PushState_Delivered: return true;
        case PushState_Sent:      return true;
        case PushState_Pushed:    return true;
        case PushState_Error:     return true;
      }

      return "UNDEFINED";
    }
    
    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSession::toDebug(IServicePushMailboxSessionPtr session)
    {
      return internal::ServicePushMailboxSession::toDebug(session);
    }

    //-------------------------------------------------------------------------
    IServicePushMailboxSessionPtr IServicePushMailboxSession::create(
                                                                     IServicePushMailboxSessionDelegatePtr delegate,
                                                                     IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                                     IMessageQueuePtr databaseDelegateAsyncQueue,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IAccountPtr account,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
    {
      return internal::IServicePushMailboxSessionFactory::singleton().create(delegate, databaseDelegate, databaseDelegateAsyncQueue, servicePushMailbox, account, grantSession, lockboxSession);
    }


  }
}
