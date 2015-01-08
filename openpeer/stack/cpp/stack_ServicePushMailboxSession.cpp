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

#include <openpeer/stack/IHelper.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>
#include <openpeer/stack/IPeerFilePrivate.h>

#include <openpeer/services/IDHKeyDomain.h>
#include <openpeer/services/IDHPrivateKey.h>
#include <openpeer/services/IDHPublicKey.h>
#include <openpeer/services/IDecryptor.h>
#include <openpeer/services/IEncryptor.h>
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

#include <sys/stat.h>

#define OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME ".sent"
#define OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME "keys"

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
    ZS_DECLARE_TYPEDEF_PTR(message::IMessageHelper, UseMessageHelper)
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    ZS_DECLARE_TYPEDEF_PTR(stack::IHelper, UseStackHelper)

    namespace internal
    {
      using CryptoPP::SHA1;
      using zsLib::Numeric;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_TYPEDEF_PTR(IStackForInternal, UseStack)

      ZS_DECLARE_TYPEDEF_PTR(openpeer::services::ISettings, UseSettings)

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

      typedef IServicePushMailboxSessionDatabaseAbstraction::index index;

      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::SendingKeyRecord, SendingKeyRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::MessageDeliveryStateRecord, MessageDeliveryStateRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::MessageDeliveryStateRecordList, MessageDeliveryStateRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::FolderMessageRecord, FolderMessageRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageRecord, FolderVersionedMessageRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IMessageTable, IMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::MessageRecordList, MessageRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ListRecordList, ListRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::ReceivingKeyRecord, ReceivingKeyRecord)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::PendingDeliveryMessageRecordList, PendingDeliveryMessageRecordList)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IFolderMessageTable, IFolderMessageTable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::IListURITable, IListURITable)
      ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::KeyDomainRecord, KeyDomainRecord)


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
        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(inKeyID, split, '-');
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
        return UseServicesHelper::randomString(OPENPEER_STACK_PUSH_MAILBOX_SENDING_KEY_LENGTH);
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
                                                           IServicePushMailboxSessionTransferDelegatePtr transferDelegate,
                                                           AccountPtr account,
                                                           ServiceNamespaceGrantSessionPtr grantSession,
                                                           ServiceLockboxSessionPtr lockboxSession
                                                           ) :
        zsLib::MessageQueueAssociator(queue),

        SharedRecursiveLock(SharedRecursiveLock::create()),

        mTransferDelegate(IServicePushMailboxSessionTransferDelegateProxy::create(UseStack::queueDelegate(), transferDelegate)),

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
        mMaxMessageDownloadToMemorySize(UseSettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_TO_MEMORY_SIZE_IN_BYTES)),

        mRefreshListsNeedingDownload(true),

        mRefreshKeysFolderNeedsProcessing(true),

        mRefreshMessagesNeedingDecryption(true),

        mRefreshPendingDelivery(true),
        mDeliveryMaxChunkSize(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_CHUNK_SIZE_IN_BYTES)),
        mLastChannel(0),
        mPendingDeliveryPrecheckRequired(true),

        mRefreshVersionedFolders(true),

        mRefreshMessagesNeedingExpiry(true)
      {
        ZS_LOG_BASIC(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate, UseStack::queueDelegate());

#define WARNING_TODO_CREATE_DB 1
#define WARNING_TODO_CREATE_DB 2
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
        return ZS_DYNAMIC_PTR_CAST(ServicePushMailboxSession, session);
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
                                                                     IServicePushMailboxSessionTransferDelegatePtr transferDelegate,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IAccountPtr account,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!transferDelegate) // must not be NULL

        ServicePushMailboxSessionPtr pThis(new ServicePushMailboxSession(UseStack::queueStack(), BootstrappedNetwork::convert(servicePushMailbox), delegate, transferDelegate, Account::convert(account), ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(lockboxSession)));
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
                                                                                    const RegisterDeviceInfo &registerDeviceInfo
                                                                                    )
      {
        AutoRecursiveLock lock(*this);

        if (mDeviceToken.hasData()) {
          ZS_THROW_INVALID_ARGUMENT_IF(registerDeviceInfo.mDeviceToken != mDeviceToken)  // this should never change within a single run
        }

        PushSubscriptionInfo info;
        info.mFolder = registerDeviceInfo.mFolder;
        info.mExpires = registerDeviceInfo.mExpires;
        info.mMapped = registerDeviceInfo.mMappedType;
        info.mUnreadBadge = registerDeviceInfo.mUnreadBadge;
        info.mSound = registerDeviceInfo.mSound;
        info.mAction = registerDeviceInfo.mAction;
        info.mLaunchImage = registerDeviceInfo.mLaunchImage;
        info.mPriority = registerDeviceInfo.mPriority;
        info.mValueNames = registerDeviceInfo.mValueNames;

        mDeviceToken = registerDeviceInfo.mDeviceToken;

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
        ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageRecord, FolderVersionedMessageRecord)
        ZS_DECLARE_TYPEDEF_PTR(IServicePushMailboxSessionDatabaseAbstraction::FolderVersionedMessageRecordList, FolderVersionedMessageRecordList)

        typedef std::map<MessageID, MessageID> RemovedMap;
        typedef std::map<MessageID, PushMessagePtr> AddedMap;

        outUpdatedToVersion = String();
        outMessagesAdded = PushMessageListPtr();
        outMessagesRemoved = MessageIDListPtr();

        AutoRecursiveLock lock(*this);

        if (!mDB) {
          ZS_LOG_WARNING(Detail, log("database not ready"))
          return false;
        }

        FolderRecordPtr folderRecord = mDB->folderTable()->getIndexAndUniqueID(inFolder);

        if (!folderRecord) {
          ZS_LOG_WARNING(Detail, log("no information about the folder exists") + ZS_PARAM("folder", inFolder))
          return false;
        }

        index versionIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;

        if (inLastVersionDownloaded.hasData()) {
          UseServicesHelper::SplitMap split;
          UseServicesHelper::split(inLastVersionDownloaded, split, '-');
          if (split.size() != 2) {
            ZS_LOG_WARNING(Detail, log("version string is not legal") + ZS_PARAM("version", inLastVersionDownloaded))
            return false;
          }

          if (split[0] != folderRecord->mUniqueID) {
            ZS_LOG_WARNING(Detail, log("version conflict detected") + ZS_PARAM("input version", split[0]) + ZS_PARAM("current version", folderRecord->mUniqueID))
            return false;
          }

          try {
            versionIndex = Numeric<decltype(versionIndex)>(split[1]);
          } catch (Numeric<decltype(versionIndex)>::ValueOutOfRange &) {
            ZS_LOG_WARNING(Detail, log("version index is not legal") + ZS_PARAM("version index", split[1]))
            return false;
          }
        }

        FolderVersionedMessageRecordListPtr afterIndex = mDB->folderVersionedMessageTable()->getBatchAfterIndex(versionIndex, folderRecord->mIndex);
        afterIndex = (afterIndex ? afterIndex : FolderVersionedMessageRecordListPtr(new FolderVersionedMessageRecordList));

        AddedMap allAdded;

        AddedMap added;
        RemovedMap removed;

        auto lastVersionFound = versionIndex;

        for (FolderVersionedMessageRecordList::iterator iter = afterIndex->begin(); iter != afterIndex->end(); ++iter)
        {
          FolderVersionedMessageRecord &info = (*iter);
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
          outUpdatedToVersion = folderRecord->mUniqueID + "-" + string(lastVersionFound);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::getLatestDownloadVersionAvailableForFolder(const char *inFolder)
      {
        ZS_LOG_DEBUG(log("get latest download version available for folder") + ZS_PARAM("folder", inFolder))

        AutoRecursiveLock lock(*this);

        if (!mDB) {
          ZS_LOG_WARNING(Detail, log("database not ready"))
          return String();
        }

        FolderRecordPtr folderRecord = mDB->folderTable()->getIndexAndUniqueID(inFolder);

        if (!folderRecord) {
          ZS_LOG_WARNING(Detail, log("no information about the folder exists") + ZS_PARAM("folder", inFolder))
          return String();
        }

        index versionIndex = mDB->folderVersionedMessageTable()->getLastIndexForFolder(folderRecord->mIndex);
        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == versionIndex) {
          ZS_LOG_WARNING(Detail, log("no versioned messages exist for the folder") + ZS_PARAM("folder", inFolder))
          return String();
        }

        return folderRecord->mUniqueID + "-" + string(versionIndex);
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSendQueryPtr ServicePushMailboxSession::sendMessage(
                                                                             IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                             const PushMessage &message,
                                                                             const char *remoteFolder,
                                                                             bool copyToSentFolder
                                                                             )
      {
        typedef std::list<size_t> SizeList;
        typedef std::list<PeerOrIdentityListPtr> PeerPtrList;

        AutoRecursiveLock lock(*this);

        if (!mDB) {
          ZS_LOG_WARNING(Detail, log("database not ready"))
          return IServicePushMailboxSendQueryPtr();
        }

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

        SizeList sizes;
        sizes.push_back(sizeTo);
        sizes.push_back(sizeCC);
        sizes.push_back(sizeBCC);

        PeerPtrList peerLists;
        peerLists.push_back(message.mTo);
        peerLists.push_back(message.mCC);
        peerLists.push_back(message.mBCC);

        mSendQueries[messageID] = query;

        SendingKeyRecordPtr sendingKeyRecord = mDB->sendingKeyTable()->getActive(allURI, zsLib::now());

        // figure out which sending key to use
        if (sendingKeyRecord) {
          ZS_LOG_TRACE(log("obtained active sending key for URI") + ZS_PARAM("uri", allURI))
        } else {
          deliverNewSendingKey(allURI, allPeers, sendingKeyRecord->mKeyID, sendingKeyRecord->mRSAPassphrase, sendingKeyRecord->mExpires);
          if (sendingKeyRecord->mKeyID.isEmpty()) {
            ZS_LOG_ERROR(Detail, log("failed to deliver encryption key") + ZS_PARAM("uri", allURI))
          }
          sendingKeyRecord->mListSize = sizeAll;
          sendingKeyRecord->mTotalWithDHPassphrase = 0;
        }

        String keyID = (sendingKeyRecord->mListSize == sendingKeyRecord->mTotalWithDHPassphrase ? makeDHSendingKeyID(sendingKeyRecord->mKeyID) : makeRSASendingKeyID(sendingKeyRecord->mKeyID));
        String passphrase = (sendingKeyRecord->mListSize == sendingKeyRecord->mTotalWithDHPassphrase ? sendingKeyRecord->mDHPassphrase : sendingKeyRecord->mRSAPassphrase);

        String decryptedFileName;
        String encoding;
        SecureByteBlockPtr encryptedMessage;
        if (message.mFullMessage) {
          if (!encryptMessage(OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE, keyID, passphrase, *message.mFullMessage, encoding, encryptedMessage)) {
            ZS_LOG_ERROR(Detail, log("failed to encrypt emssage"))
            encoding = String();
            encryptedMessage = UseServicesHelper::clone(message.mFullMessage);
          }
        } else {
          decryptedFileName = message.mFullMessageFileName;
          if (decryptedFileName.hasData()) {
            String salt = UseServicesHelper::randomString(16*8/5);
            encoding = String(OPENPEER_STACK_PUSH_MAILBOX_TEMP_KEYING_URI_SCHEME) + UseServicesHelper::convertToBase64(OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE) + ":" + keyID + "=" + passphrase + ":" + salt;
          }
        }

        SecureByteBlockPtr encryptedMetaData;
        if (message.mMetaData) {
          String metaData = UseServicesHelper::toString(message.mMetaData);
          if (!encryptMessage(OPENPEER_STACK_PUSH_MAILBOX_KEYING_METADATA_TYPE, keyID, passphrase, *UseServicesHelper::convertToBuffer(UseServicesHelper::toString(message.mMetaData)), encoding, encryptedMetaData)) {
            ZS_LOG_ERROR(Detail, log("failed to encrypt emssage"))
            encoding = String();
            encryptedMessage = UseServicesHelper::convertToBuffer(UseServicesHelper::toString(message.mMetaData));
          }
        }

        MessageRecord messageRecord;
        messageRecord.mMessageID = messageID;
        messageRecord.mTo = to;
        messageRecord.mFrom = peerFilePublic->getPeerURI();
        messageRecord.mCC = cc;
        messageRecord.mBCC = bcc;
        messageRecord.mType = message.mMessageType;
        messageRecord.mMimeType = message.mMimeType;
        messageRecord.mEncoding = encoding;
        messageRecord.mPushType = message.mPushType;
        messageRecord.mPushInfos = message.mPushInfos;
        messageRecord.mSent = message.mSent;
        messageRecord.mExpires = message.mExpires;
        messageRecord.mEncryptedData = encryptedMessage;
        messageRecord.mDecryptedFileName = decryptedFileName;
        messageRecord.mDecryptedData = message.mFullMessage;
        messageRecord.mNeedsNotification = true;

        auto messageIndex = mDB->messageTable()->insert(messageRecord);

        mRefreshVersionedFolders = true;

        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == messageIndex) {
          ZS_LOG_ERROR(Detail, log("failed to insert message into the database"))
          query->cancel();
          return query;
        }

        decltype(PendingDeliveryMessageRecord::mSubscribeFlags) flags = 0;
        for (PushStateDetailMap::const_iterator iter = message.mPushStateDetails.begin(); iter != message.mPushStateDetails.end(); ++iter)
        {
          PushStates flag = (*iter).first;
          flags = flags | ((int)(flag));
        }

        PendingDeliveryMessageRecord pendingRecord;
        pendingRecord.mIndexMessage = messageIndex;
        pendingRecord.mRemoteFolder = remoteFolder;
        pendingRecord.mCopyToSent = copyToSentFolder;
        pendingRecord.mSubscribeFlags = flags;

        mDB->pendingDeliveryMessageTable()->insert(pendingRecord);
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
        ZS_LOG_DEBUG(log("get push message") + ZS_PARAM("message id", messageID))

        AutoRecursiveLock lock(*this);

        PushMessagePtr result(new PushMessage);

        MessageRecordPtr message = mDB->messageTable()->getByMessageID(messageID);

        if (!message) {
          ZS_LOG_WARNING(Detail, log("message was not found in database") + ZS_PARAM("message id", messageID))
          return PushMessagePtr();
        }

        result->mMessageID = message->mMessageID;
        String to = message->mTo;
        String from = message->mFrom;
        String cc = message->mCC;
        String bcc = message->mBCC;
        result->mMessageType = message->mType;
        result->mMimeType = message->mMimeType;

        result->mFullMessage = message->mDecryptedData;
        result->mFullMessageFileName = message->mDecryptedFileName;
        result->mMetaData = message->mDecryptedMetaData;

        result->mPushType = message->mPushType;
        result->mPushInfos = message->mPushInfos;

        result->mSent = message->mSent;
        result->mExpires = message->mExpires;


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

        MessageDeliveryStateRecordListPtr deliveryStateRecords = mDB->messageDeliveryStateTable()->getForMessage(message->mIndex);
        deliveryStateRecords = (deliveryStateRecords ? deliveryStateRecords : MessageDeliveryStateRecordListPtr(new MessageDeliveryStateRecordList));

        if (deliveryStateRecords->size() > 0) {
          // convert delivery infos from a flat table list to a state map
          for (auto iter = deliveryStateRecords->begin(); iter != deliveryStateRecords->end(); ++iter)
          {
            MessageDeliveryStateRecord &info = (*iter);

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
      #pragma mark ServicePushMailboxSession => IServicePushMailboxSessionAsyncDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::onNotifierComplete(
                                                         const char *messageID,
                                                         bool wasSuccessful
                                                         )
      {
        AutoRecursiveLock lock(*this);

        // scope: check pending delivery
        {
          PendingDeliveryMap::iterator found = mPendingDelivery.find(messageID);
          if (found != mPendingDelivery.end()) {
            ZS_LOG_DEBUG(log("notified upload complete for pending message") + ZS_PARAM("message ID", messageID))

            ProcessedPendingDeliveryMessageInfo &processedInfo = (*found).second;

            if (!processedInfo.mPendingDeliveryMessageUpdateRequest) {
              ZS_LOG_WARNING(Detail, log("notifieed complete but no update request was sent (or request already completed)") + ZS_PARAM("message ID", messageID))
              return;
            }

            if (processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
              ZS_LOG_WARNING(Detail, log("notifieed complete but already waiting for completion") + ZS_PARAM("message ID", messageID))
              return;
            }

            // now that the upload has finished the response from the upload request should complete
            processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), processedInfo.mPendingDeliveryMessageUpdateRequest, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
            return;
          }
        }

        // scope: check pending download
        {
          MessageDataMap::iterator found = mMessagesNeedingData.find(messageID);
          if (found != mMessagesNeedingData.end()) {
            ZS_LOG_DEBUG(log("notified download complete for message needing data") + ZS_PARAM("message ID", messageID) + ZS_PARAM("was successful", wasSuccessful))

            ProcessedMessageNeedingDataInfo &processedInfo = (*found).second;

            mDB->messageTable()->notifyDownload(processedInfo.mInfo.mIndex, wasSuccessful);

            if (wasSuccessful) {
              mRefreshFolders = true;
              mRefreshFoldersNeedingUpdate = true;
              mRefreshMessagesNeedingData = true;
            } else {
              ZS_LOG_ERROR(Detail, log("notified download complete failed") + ZS_PARAM("message ID", messageID))
            }

            mMessagesNeedingData.erase(found);
            IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
          }
        }

        ZS_LOG_TRACE(log("notified complete but message was not found anywhere needing notification") + ZS_PARAM("message ID", messageID) + ZS_PARAM("was successful", wasSuccessful))
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

          if (processedInfo.mEncryptedFileName.hasData()) {
            ZS_LOG_TRACE(log("delivering the message via local file instead") + ZS_PARAM("file", processedInfo.mEncryptedFileName))
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

        mBackgroundingEnabled = true;

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

        mBackgroundingEnabled = false;
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
        mDownloadMessageURL = result->uploadMessageURL();
        mStringReplacementMessageID = result->stringReplacementMessageID();
        mStringReplacementMessageSize = result->stringReplacementMessageSize();

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

            auto folderIndex = mDB->folderTable()->getIndex(name);
            if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN != folderIndex) {
              mDB->folderMessageTable()->removeAllFromFolder(folderIndex);
              mDB->folderVersionedMessageTable()->removeAllRelatedToFolder(folderIndex);
              mDB->folderTable()->removeByIndex(folderIndex);
            }

            mRefreshMonitorFolderCreation = true;
            continue;
          }

          FolderRecord folderRecord;

          if (info.mRenamed) {
            ZS_LOG_DEBUG(log("folder was renamed (removing old folder)") + ZS_PARAM("folder name", info.mName) + ZS_PARAM("new name", info.mRenamed))
            auto folderIndex = mDB->folderTable()->getIndex(info.mRenamed);
            if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN != folderIndex) {
              mDB->folderMessageTable()->removeAllFromFolder(folderIndex);
              mDB->folderVersionedMessageTable()->removeAllRelatedToFolder(folderIndex);
              mDB->folderTable()->removeByIndex(folderIndex);
            }

            folderIndex = mDB->folderTable()->getIndex(info.mName);
            if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN != folderIndex) {
              mDB->folderTable()->updateFolderName(folderIndex, info.mName);
            }

            folderRecord.mIndex = folderIndex;
            name = info.mRenamed;
            mRefreshMonitorFolderCreation = true;
          }

          ZS_LOG_DEBUG(log("folder was updated/added") + info.toDebug())

          folderRecord.mFolderName = name;
          folderRecord.mServerVersion = info.mVersion;
          folderRecord.mTotalUnreadMessages = info.mUnread;
          folderRecord.mTotalMessages = info.mTotal;
          folderRecord.mUpdateNext = info.mUpdateNext;
          mDB->folderTable()->addOrUpdate(folderRecord);
        }

        mDB->settingsTable()->setLastDownloadedVersionForFolders(result->version());

        if (mNextFoldersUpdateTimer) {
          mNextFoldersUpdateTimer->cancel();
          mNextFoldersUpdateTimer.reset();
        }

        Time updateNextTime = result->updateNext();
        Seconds updateNextDuration {};
        if (Time() != updateNextTime) {
          Time now = zsLib::now();
          if (now < updateNextTime) {
            updateNextDuration = zsLib::toSeconds(updateNextTime - now);
          } else {
            updateNextDuration = Seconds(1);
          }

          if (updateNextDuration < Seconds(1)) {
            updateNextDuration = Seconds(1);
          }
        }

        if (Seconds() != updateNextDuration) {
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
          mDB->settingsTable()->setLastDownloadedVersionForFolders(String());
          mLastActivity = zsLib::now();

          mDB->folderVersionedMessageTable()->flushAll();
          mDB->folderMessageTable()->flushAll();
          mDB->folderTable()->flushAll();

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

              FolderMessageRecord folderMessageRecord;
              folderMessageRecord.mIndexFolderRecord = updateInfo.mInfo.mIndex;
              folderMessageRecord.mMessageID = message.mID;

              mDB->folderMessageTable()->addToFolderIfNotPresent(folderMessageRecord);
              mDB->messageTable()->addOrUpdate(message.mID, message.mVersion);
              break;
            }
            case PushMessageInfo::Disposition_Remove: {
              ZS_LOG_TRACE(log("message removed in folder") + ZS_PARAM("folder name", originalFolderInfo.mName) + ZS_PARAM("message id", message.mID))

              FolderVersionedMessageRecord folderVersionedMessageRecord;
              folderVersionedMessageRecord.mIndexFolderRecord = updateInfo.mInfo.mIndex;
              folderVersionedMessageRecord.mMessageID = message.mID;
              folderVersionedMessageRecord.mRemovedFlag = true;

              mDB->folderVersionedMessageTable()->addRemovedEntryIfNotAlreadyRemoved(folderVersionedMessageRecord);

              FolderMessageRecord folderMessageRecord;
              folderMessageRecord.mIndexFolderRecord = updateInfo.mInfo.mIndex;
              folderMessageRecord.mMessageID = message.mID;

              mDB->folderMessageTable()->removeFromFolder(folderMessageRecord);
              break;
            }
          }
        }

        mDB->folderTable()->updateDownloadInfo(updateInfo.mInfo.mIndex, folderInfo.mVersion, folderInfo.mUpdateNext);

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
              mDB->folderMessageTable()->removeAllFromFolder(updateInfo.mInfo.mIndex);
              mDB->folderVersionedMessageTable()->removeAllRelatedToFolder(updateInfo.mInfo.mIndex);
              mDB->folderTable()->resetUniqueID(updateInfo.mInfo.mIndex);

              mDB->folderTable()->updateDownloadInfo(updateInfo.mInfo.mIndex, String(), zsLib::now());
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

          if (processedInfo.mInfo.mEncryptedFileName.hasData()) {
            ZS_LOG_TRACE(log("downloading via URL and not via channel") + ZS_PARAM("message id", processedInfo.mInfo.mMessageID))
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
//        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfo DeliveryInfo;
//        typedef IServicePushMailboxDatabaseAbstractionDelegate::DeliveryInfoList DeliveryInfoList;

        ZS_LOG_DEBUG(log("messages meta data get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        const PushMessageInfoList &messages = result->messages();

        bool handled = false;

        for (PendingDeliveryMap::iterator iter_doNotUse = mPendingDelivery.begin(); iter_doNotUse != mPendingDelivery.end();)
        {
          PendingDeliveryMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          const String &messageID = (*current).first;
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor != monitor) continue;

          processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor.reset();

          handled = true;

          for (PushMessageInfoList::const_iterator messIter = messages.begin(); messIter != messages.end(); ++messIter)
          {
            const PushMessageInfo &pushInfo = (*messIter);

            if (pushInfo.mID != messageID) continue;

            if (0 != pushInfo.mRemaining) {
              ZS_LOG_DEBUG(log("message is partially delivered") + ZS_PARAM("message ID", messageID))

              processedInfo.mSent = pushInfo.mLength - pushInfo.mRemaining;
              IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
              goto next_iter_pending_delivery;
            }

            ZS_LOG_DEBUG(log("message is already delivered") + ZS_PARAM("message ID", messageID))

            mDB->pendingDeliveryMessageTable()->remove(processedInfo.mInfo.mIndex);

            SendQueryMap::iterator foundQuery = mSendQueries.find(pushInfo.mID);
            if (foundQuery != mSendQueries.end()) {
              SendQueryPtr query = (*foundQuery).second.lock();

              if (query) {
                query->notifyUploaded();
                query->notifyDeliveryInfoChanged(pushInfo.mFlags);
              } else {
                mSendQueries.erase(foundQuery);
              }
            }

            mPendingDelivery.erase(current);
            goto next_iter_pending_delivery;
          }

        next_iter_pending_delivery:
          {
          }
        }

        if (handled) return true;

        if (monitor != mMessageMetaDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        mMessageMetaDataGetMonitor.reset();

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

          mDB->pendingDeliveryMessageTable()->removeByMessageIndex(processedInfo.mInfo.mIndexMessageRecord);

          MessageDeliveryStateRecordList deliveryStates;

          for (auto flagIter = info.mFlags.begin(); flagIter != info.mFlags.end(); ++flagIter) {

            const PushMessageInfo::FlagInfo &flagInfo = (*flagIter).second;

            for (auto uriIter = flagInfo.mFlagURIInfos.begin(); uriIter != flagInfo.mFlagURIInfos.end(); ++uriIter) {
              const PushMessageInfo::FlagInfo::URIInfo &uriInfo = *uriIter;

              MessageDeliveryStateRecord deliveryState;
              deliveryState.mFlag = PushMessageInfo::FlagInfo::toString(flagInfo.mFlag);
              deliveryState.mURI = uriInfo.mURI;
              deliveryState.mErrorCode = uriInfo.mErrorCode;
              deliveryState.mErrorReason = uriInfo.mErrorReason;

              deliveryStates.push_back(deliveryState);
            }
          }

          mDB->messageDeliveryStateTable()->updateForMessage(
                                                             processedInfo.mInfo.mIndexMessageRecord,
                                                             deliveryStates
                                                             );

          PushInfoList pushInfos;
          copy(info.mPushInfos, pushInfos);

          MessageRecord message;
          message.mIndex = processedInfo.mInfo.mIndexMessageRecord;
          message.mDownloadedVersion = info.mVersion;
          message.mServerVersion = info.mVersion;
          message.mTo = info.mTo;
          message.mFrom = info.mFrom;
          message.mCC = info.mCC;
          message.mBCC = info.mBCC;
          message.mType = info.mType;
          message.mMimeType = info.mMimeType;
          message.mEncoding = info.mEncoding;
          message.mPushType = info.mPushType;
          message.mPushInfos = pushInfos;
          message.mSent = info.mSent;
          message.mExpires = info.mExpires;
          if (info.mMetaData.hasData()) {
            message.mEncryptedMetaData = UseServicesHelper::convertFromBase64(info.mMetaData);
          }
          message.mEncryptedDataLength = info.mLength;
          message.mNeedsNotification = true;

          mDB->messageTable()->update(message);

          mRefreshVersionedFolders = true;
          mRefreshMessagesNeedingData = true;

          {
            String listID =  extractListID(info.mTo);
            if (listID.hasData()) mDB->listTable()->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mFrom);
            if (listID.hasData()) mDB->listTable()->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mCC);
            if (listID.hasData()) mDB->listTable()->addOrUpdateListID(listID);
          }
          {
            String listID =  extractListID(info.mBCC);
            if (listID.hasData()) mDB->listTable()->addOrUpdateListID(listID);
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
        ZS_LOG_ERROR(Detail, log("failed to get message metadata") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        AutoRecursiveLock lock(*this);
        for (PendingDeliveryMap::iterator iter_doNotUse = mPendingDelivery.begin(); iter_doNotUse != mPendingDelivery.end();)
        {
          PendingDeliveryMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor == monitor) {
            connectionFailure();
            return true;
          }
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
            mDB->pendingDeliveryMessageTable()->remove(processedInfo.mInfo.mIndex);

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

          handleUpdateMessageGone(request, false);

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
            handleUpdateMessageGone(MessageUpdateRequest::convert(monitor->getMonitoredMessage()), IHTTP::HTTPStatusCode_NotFound == result->errorCode());
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

          mDB->listURITable()->update(processedInfo.mInfo.mIndex, uris);
          mDB->listTable()->notifyDownloaded(processedInfo.mInfo.mIndex);

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

          mDB->listTable()->notifyFailedToDownload(processedInfo.mInfo.mIndex);

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
        UseServicesHelper::debugAppend(objectEl, "id", mID);
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

        UseServicesHelper::debugAppend(resultEl, "id", mID);
        UseServicesHelper::debugAppend(resultEl, "graceful shutdown reference", (bool)mGracefulShutdownReference);

        UseServicesHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());
        UseServicesHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        UseServicesHelper::debugAppend(resultEl, "transfer delegate", (bool)mTransferDelegate);

        UseServicesHelper::debugAppend(resultEl, "db", mDB ? mDB->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        UseServicesHelper::debugAppend(resultEl, "error code", mLastError);
        UseServicesHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        UseServicesHelper::debugAppend(resultEl, "account id", mAccount ? mAccount->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));

        UseServicesHelper::debugAppend(resultEl, "backgrounding subscription id", mBackgroundingSubscription ? mBackgroundingSubscription->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "backgrounding notifier", (bool)mBackgroundingNotifier);
        UseServicesHelper::debugAppend(resultEl, "backgrounding enabled", mBackgroundingEnabled);

        UseServicesHelper::debugAppend(resultEl, "reachability subscription", mReachabilitySubscription ? mReachabilitySubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "lockbox subscription", mLockboxSubscription ? mLockboxSubscription->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "sent via object id", mSentViaObjectID);

        UseServicesHelper::debugAppend(resultEl, "tcp messaging id", mTCPMessaging ? mTCPMessaging->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "wire stream", mWireStream ? mWireStream->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "reader id", mReader ? mReader->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "writer id", mWriter ? mWriter->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "lockbox", mLockbox ? mLockbox->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, mLockboxInfo.toDebug());
        UseServicesHelper::debugAppend(resultEl, IPeerFiles::toDebug(mPeerFiles));

        UseServicesHelper::debugAppend(resultEl, "grant session id", mGrantSession ? mGrantSession->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant query id", mGrantQuery ? mGrantQuery->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant wait id", mGrantWait ? mGrantWait->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "obtained lock", mObtainedLock);

        UseServicesHelper::debugAppend(resultEl, "required connection", mRequiresConnection);
        UseServicesHelper::debugAppend(resultEl, "inactivty timeout (s)", mInactivityTimeout);
        UseServicesHelper::debugAppend(resultEl, "last activity", mLastActivity);

        UseServicesHelper::debugAppend(resultEl, "default last try duration (ms)", mDefaultLastRetryDuration);
        UseServicesHelper::debugAppend(resultEl, "last try duration (ms)", mLastRetryDuration);
        UseServicesHelper::debugAppend(resultEl, "do not retry connection before", mDoNotRetryConnectionBefore);
        UseServicesHelper::debugAppend(resultEl, "retry timer", mRetryTimer ? mRetryTimer->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "server list", mPushMailboxServers.size());
        UseServicesHelper::debugAppend(resultEl, "server lookup id", mServerLookup ? mServerLookup->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "server srv record", (bool)mServerSRV);
        UseServicesHelper::debugAppend(resultEl, "server ip", string(mServerIP));

        UseServicesHelper::debugAppend(resultEl, "peer uri from access result", mPeerURIFromAccessResult);
        UseServicesHelper::debugAppend(resultEl, "upload message url", mUploadMessageURL);
        UseServicesHelper::debugAppend(resultEl, "upload message url", mDownloadMessageURL);
        UseServicesHelper::debugAppend(resultEl, "string replacement message id", mStringReplacementMessageID);
        UseServicesHelper::debugAppend(resultEl, "string replacement message size", mStringReplacementMessageSize);

        UseServicesHelper::debugAppend(resultEl, mNamespaceGrantChallengeInfo.toDebug());

        UseServicesHelper::debugAppend(resultEl, "servers get monitor", mServersGetMonitor ? mServersGetMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "access monitor", mAccessMonitor ? mAccessMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "grant monitor", mGrantMonitor ? mGrantMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "peer validate monitor", mPeerValidateMonitor ? mPeerValidateMonitor->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "device token", mDeviceToken);
        UseServicesHelper::debugAppend(resultEl, "register queries", mRegisterQueries.size());

        UseServicesHelper::debugAppend(resultEl, "send queries", mSendQueries.size());

        UseServicesHelper::debugAppend(resultEl, "refresh folders", mRefreshFolders);
        UseServicesHelper::debugAppend(resultEl, "monitored folders", mMonitoredFolders.size());
        UseServicesHelper::debugAppend(resultEl, "notified monitored folders", mNotifiedMonitoredFolders.size());
        UseServicesHelper::debugAppend(resultEl, "folders get monitor", mFoldersGetMonitor ? mFoldersGetMonitor->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "next folder update timer", mNextFoldersUpdateTimer ? mNextFoldersUpdateTimer->getID() : 0);


        UseServicesHelper::debugAppend(resultEl, "refresh folders needing update", mRefreshFoldersNeedingUpdate);
        UseServicesHelper::debugAppend(resultEl, "folders needing update", mFoldersNeedingUpdate.size());
        UseServicesHelper::debugAppend(resultEl, "folder get monitors", mFolderGetMonitors.size());


        UseServicesHelper::debugAppend(resultEl, "refresh monitored folder creation", mRefreshMonitorFolderCreation);
        UseServicesHelper::debugAppend(resultEl, "folder creation monitor", mFolderCreationUpdateMonitor ? mFolderCreationUpdateMonitor->getID() : 0);


        UseServicesHelper::debugAppend(resultEl, "refresh messages needing update", mRefreshMessagesNeedingUpdate);
        UseServicesHelper::debugAppend(resultEl, "messages needing update", mMessagesNeedingUpdate.size());
        UseServicesHelper::debugAppend(resultEl, "messages metadata get monitor", mMessageMetaDataGetMonitor ? mMessageMetaDataGetMonitor->getID() : 0);


        UseServicesHelper::debugAppend(resultEl, "refresh messages needing data", mRefreshMessagesNeedingData);
        UseServicesHelper::debugAppend(resultEl, "messages needing data", mMessagesNeedingData.size());
        UseServicesHelper::debugAppend(resultEl, "messages needing data channels", mMessagesNeedingDataChannels.size());
        UseServicesHelper::debugAppend(resultEl, "messages metadata get monitor", mMessageDataGetMonitor ? mMessageDataGetMonitor->getID() : 0);
        UseServicesHelper::debugAppend(resultEl, "max message download to memory size", mMaxMessageDownloadToMemorySize);

        UseServicesHelper::debugAppend(resultEl, "pending channel data", mPendingChannelData.size());


        UseServicesHelper::debugAppend(resultEl, "refresh lists needing download", mRefreshListsNeedingDownload);
        UseServicesHelper::debugAppend(resultEl, "lists to download", mListsNeedingDownload.size());
        UseServicesHelper::debugAppend(resultEl, "list fetch monitor", mListFetchMonitor ? mListFetchMonitor->getID() : 0);

        UseServicesHelper::debugAppend(resultEl, "refresh keys folder needs processing", mRefreshKeysFolderNeedsProcessing);

        UseServicesHelper::debugAppend(resultEl, "refresh messages needing decryption", mRefreshMessagesNeedingDecryption);
        UseServicesHelper::debugAppend(resultEl, "messages needing decryption", mMessagesNeedingDecryption.size());


        UseServicesHelper::debugAppend(resultEl, "refresh pending delivery", mRefreshPendingDelivery);
        UseServicesHelper::debugAppend(resultEl, "max delivery chunk size", mDeliveryMaxChunkSize);
        UseServicesHelper::debugAppend(resultEl, "last channel", mLastChannel);
        UseServicesHelper::debugAppend(resultEl, "pending delivery pre-check required", mPendingDeliveryPrecheckRequired);
        UseServicesHelper::debugAppend(resultEl, "pending delivery", mPendingDelivery.size());

        UseServicesHelper::debugAppend(resultEl, "refresh versioned folders", mRefreshVersionedFolders);


        UseServicesHelper::debugAppend(resultEl, "messages to mark read", mMessagesToMarkRead.size());
        UseServicesHelper::debugAppend(resultEl, "messages to remove", mMessagesToRemove.size());
        UseServicesHelper::debugAppend(resultEl, "marking monitors", mMarkingMonitors.size());

        UseServicesHelper::debugAppend(resultEl, "refresh needing expiry", mRefreshMessagesNeedingExpiry);

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
        if (!stepLockboxAccess()) goto post_step;

        if (!stepShouldConnect()) goto post_step;
        if (!stepDNS()) goto post_step;

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
        if (!stepDecryptMessagesAsync()) goto post_step;

        if (!stepPendingDelivery()) goto post_step;
        if (!stepPendingDeliveryEncryptMessages()) goto post_step;
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

        mObtainedLock = true;
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

        if (!mLockboxInfo.mToken.hasData()) {
          mLockboxInfo = mLockbox->getLockboxInfo();
          if (mLockboxInfo.mToken.hasData()) {
            ZS_LOG_DEBUG(log("obtained lockbox token"))
          }
        }

        if (!mDB) {
          String userHash = UseServicesHelper::convertToHex(*UseServicesHelper::hash(mLockboxInfo.mDomain + ":" + mLockboxInfo.mAccountID));

          mDB = IServicePushMailboxSessionDatabaseAbstraction::create(userHash, UseSettings::getString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_TEMPORARY_PATH), UseSettings::getString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DATABASE_PATH));

          if (!mDB) {
            ZS_LOG_ERROR(Detail, log("unable to create database"))
            cancel();
            return false;
          }
        }

        if (mLockboxInfo.mToken.hasData()) {
          ZS_LOG_TRACE(log("now have lockbox acess secret"))
          return true;
        }

        ZS_LOG_TRACE(log("waiting on lockbox to login"))
        return false;
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

        request->version(mDB->settingsTable()->getLastDownloadedVersionForFolders());

        mFoldersGetMonitor = sendRequest(IMessageMonitorResultDelegate<FoldersGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        mRefreshFolders = false;
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckFoldersNeedingUpdate()
      {
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

        FolderRecordListPtr result = mDB->folderTable()->getNeedingUpdate(zsLib::now());
        result = (result ? result : FolderRecordListPtr(new FolderRecordList));

        for (auto iter = result->begin(); iter != result->end(); ++iter)
        {
          const FolderRecord &info = (*iter);

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

          index = mDB->folderTable()->getIndex(folderName);

          // check with database to make sure folder is still value
          if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == index) {
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
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateInfo, MessageNeedingUpdateInfo)
        ZS_DECLARE_TYPEDEF_PTR(IMessageTable::MessageNeedingUpdateList, MessageNeedingUpdateList)

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

        MessageNeedingUpdateListPtr updateList = mDB->messageTable()->getBatchNeedingUpdate();
        updateList = (updateList ? updateList : MessageNeedingUpdateListPtr(new MessageNeedingUpdateList));

        if (updateList->size() < 1) {
          ZS_LOG_DEBUG(log("no messages are requiring an update right now"))
          mRefreshFoldersNeedingUpdate = false;
          return true;
        }

        for (MessageNeedingUpdateList::iterator iter = updateList->begin(); iter != updateList->end(); ++iter)
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

        ZS_LOG_DEBUG(log("processed batch of messages that needed upate") + ZS_PARAM("batch size", updateList->size()))
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

        MessageRecordListPtr updateList = mDB->messageTable()->getBatchNeedingData(zsLib::now());
        updateList = (updateList ? updateList : MessageRecordListPtr(new MessageRecordList));

        if (updateList->size() < 1) {
          ZS_LOG_DEBUG(log("no messages are requiring an data right now"))
          mRefreshMessagesNeedingData = false;
          return true;
        }

        for (auto iter = updateList->begin(); iter != updateList->end(); ++iter)
        {
          MessageRecord &info = (*iter);

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

        ZS_LOG_DEBUG(log("processed batch of messages that needed data") + ZS_PARAM("batch size", updateList->size()))
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

        for (MessageDataMap::iterator iter_doNotUse = mMessagesNeedingData.begin(); iter_doNotUse != mMessagesNeedingData.end(); )
        {
          MessageDataMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          const String &messageID = (*current).first;
          ProcessedMessageNeedingDataInfo &processedInfo = (*current).second;

          if (processedInfo.mSentRequest) {
            ZS_LOG_TRACE(log("already sent request for message data get") + ZS_PARAM("message ID", messageID))
            continue;
          }

          if ((processedInfo.mInfo.mEncryptedDataLength > mMaxMessageDownloadToMemorySize) ||
              (processedInfo.mInfo.mEncryptedFileName.hasData())) { // check if this message cannot be processed in memory

            if (processedInfo.mInfo.mEncryptedFileName.isEmpty()) {
              processedInfo.mInfo.mEncryptedFileName = mDB->storage()->getStorageFileName();
              FILE *file = fopen(processedInfo.mInfo.mEncryptedFileName, "wb");
              if (NULL == file) {
                ZS_LOG_ERROR(Detail, log("failed to create storage file") + ZS_PARAM("file", processedInfo.mInfo.mEncryptedFileName) + ZS_PARAM("message ID", messageID))
                processedInfo.mInfo.mEncryptedFileName.clear();
                continue;
              }
              fclose(file);

              mDB->messageTable()->updateEncryptionFileName(processedInfo.mInfo.mIndex, processedInfo.mInfo.mEncryptedFileName);
            }

            if (processedInfo.mReceivedData < 1) {
              struct stat st;
              st.st_size = 0;
              int result = stat(processedInfo.mInfo.mEncryptedFileName, &st);
              if (result < 0) {
                int error = errno;
                ZS_LOG_ERROR(Detail, log("failed to get file size") + ZS_PARAM("file", processedInfo.mInfo.mEncryptedFileName) + ZS_PARAM("message ID", messageID) + ZS_PARAM("error", error) + ZS_PARAM("error string", strerror(error)))
                // by clearing out the file it will attempt to create a new one
                processedInfo.mInfo.mEncryptedFileName.clear();
                mDB->messageTable()->updateEncryptionFileName(processedInfo.mInfo.mIndex, processedInfo.mInfo.mEncryptedFileName);

                IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
                continue;
              }

              processedInfo.mReceivedData = st.st_size;
            }

            processedInfo.mSentRequest = true;
            
            if (processedInfo.mReceivedData >= processedInfo.mInfo.mEncryptedDataLength) {
              ZS_LOG_TRACE(log("all data has been received") + ZS_PARAM("message ID", messageID))
              continue;
            }

            if (processedInfo.mDeliveryURL.hasData()) {
              ZS_LOG_TRACE(log("already attempting to delivery message via URL") + ZS_PARAM("message ID", messageID))
              continue;
            }

            // need to issue a request to download data via URL

            auto totalRemaining = processedInfo.mInfo.mEncryptedDataLength - processedInfo.mReceivedData;
            processedInfo.mDeliveryURL = mDownloadMessageURL;

            // replace message ID
            {
              size_t found = processedInfo.mDeliveryURL.find(mStringReplacementMessageID);
              if (String::npos != found) {
                String replaceMessageID = urlencode(messageID);
                processedInfo.mDeliveryURL.replace(found, mStringReplacementMessageID.size(), replaceMessageID);
              }
            }

            // replace size
            {
              size_t found = processedInfo.mDeliveryURL.find(mStringReplacementMessageSize);
              if (String::npos != found) {
                String replaceMessageSize = string(totalRemaining);
                processedInfo.mDeliveryURL.replace(found, mStringReplacementMessageID.size(), replaceMessageSize);
              }
            }

            ZS_LOG_DEBUG(log("going to fetch push message via URL") + ZS_PARAM("url", processedInfo.mDeliveryURL) + ZS_PARAM("file", processedInfo.mInfo.mEncryptedFileName))

            mTransferDelegate->onServicePushMailboxSessionTransferDownloadDataFromURL(mThisWeak.lock(), processedInfo.mDeliveryURL, processedInfo.mInfo.mEncryptedFileName, processedInfo.mInfo.mEncryptedDataLength, totalRemaining, AsyncNotifier::create(getAssociatedMessageQueue(), mThisWeak.lock(), messageID));
            continue;
          }


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

        mMessageDataGetMonitor = sendRequest(IMessageMonitorResultDelegate<MessagesDataGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepCheckListNeedingDownload()
      {
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

        ListRecordListPtr downloadList = mDB->listTable()->getBatchNeedingDownload(zsLib::now());
        downloadList = (downloadList ? downloadList : ListRecordListPtr(new ListRecordList));

        if (downloadList->size() < 1) {
          ZS_LOG_DEBUG(log("all lists have downloaded"))
          mRefreshListsNeedingDownload = false;
          return true;
        }

        for (auto iter = downloadList->begin(); iter != downloadList->end(); ++iter)
        {
          const ListRecord &info = (*iter);

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

        MessageRecordListPtr messages = mDB->messageTable()->getBatchNeedingKeysProcessing(keysFolderIndex, OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE, OPENPEER_STACK_PUSH_MAILBOX_JSON_MIME_TYPE);
        messages = (messages ? messages : MessageRecordListPtr(new MessageRecordList));

        if (messages->size() < 1) {
          ZS_LOG_DEBUG(log("no keys in keying folder needing processing at this time"))
          mRefreshKeysFolderNeedsProcessing = false;
          return true;
        }

        for (auto iter = messages->begin(); iter != messages->end(); ++iter)
        {
          MessageRecord &info = (*iter);

          // scope
          {
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE != info.mEncoding) {
              ZS_LOG_WARNING(Detail, log("failed to extract information from keying bundle") + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("encoding", "info.mEncoding") + ZS_PARAM("expecting", OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE))
              goto post_processing;
            }

            KeyingBundle bundle;
            if (!extractKeyingBundle(info.mEncryptedData, bundle)) {
              ZS_LOG_WARNING(Detail, log("failed to extract information from keying bundle") + ZS_PARAM("message index", info.mIndex))
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
              ReceivingKeyRecordPtr receivingKeyRecord = mDB->receivingKeyTable()->getByKeyID(bundle.mReferencedKeyID);

              if (receivingKeyRecord) {
                ZS_LOG_DEBUG(log("found existing key thus no need to process again") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("uri", receivingKeyRecord->mURI) + ZS_PARAM("passphrase", receivingKeyRecord->mPassphrase) + ZS_PARAM("ephemeral private key", receivingKeyRecord->mDHEphemeralPrivateKey) + ZS_PARAM("ephemeral public key", receivingKeyRecord->mDHEphemeralPublicKey) + ZS_PARAM("expires", receivingKeyRecord->mExpires))
                goto post_processing;
              }

              receivingKeyRecord = ReceivingKeyRecordPtr(new ReceivingKeyRecord);

              receivingKeyRecord->mPassphrase = extractRSA(bundle.mSecret);
              if (receivingKeyRecord->mPassphrase.isEmpty()) {
                ZS_LOG_WARNING(Detail, log("failed to extract RSA keying material") + ZS_PARAM("message index", info.mIndex))
                goto post_processing;
              }

              receivingKeyRecord->mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
              receivingKeyRecord->mKeyID = bundle.mReferencedKeyID;
              receivingKeyRecord->mURI = info.mFrom;
              receivingKeyRecord->mExpires = bundle.mExpires;

              mDB->receivingKeyTable()->addOrUpdate(*receivingKeyRecord);
              mDB->messageTable()->notifyDecryptNowForKeys(receivingKeyRecord->mKeyID);

              mRefreshMessagesNeedingDecryption = true;
            }

            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE_AGREEMENT == bundle.mType) {
              IDHPrivateKeyPtr privateKey;
              IDHPublicKeyPtr publicKey;

              if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_REQUEST_OFFER == bundle.mMode) {
                // L <-- (request offer) <-- R
                // L --> (offer request existing) --> R
                // L <-- (answer) <-- R (remote sending passphrase)

                ReceivingKeyRecordPtr receivingKeyRecord = mDB->receivingKeyTable()->getByKeyID(bundle.mReferencedKeyID);

                if (receivingKeyRecord) {
                  ZS_LOG_DEBUG(log("found existing key") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("uri", receivingKeyRecord->mURI) + ZS_PARAM("passphrase", receivingKeyRecord->mPassphrase) + ZS_PARAM("ephemeral private key", receivingKeyRecord->mDHEphemeralPrivateKey) + ZS_PARAM("ephemeral public key", receivingKeyRecord->mDHEphemeralPublicKey) + ZS_PARAM("expires", receivingKeyRecord->mExpires))

                  if (receivingKeyRecord->mURI != bundle.mValidationPeer->getPeerURI()) {
                    ZS_LOG_WARNING(Detail, log("referenced key is not signed by from peer (thus do not respond)") + ZS_PARAM("existing key peer uri", receivingKeyRecord->mURI) + IPeer::toDebug(bundle.mValidationPeer))
                    goto post_processing;
                  }

                  if (zsLib::now() > receivingKeyRecord->mExpires) {
                    ZS_LOG_WARNING(Detail, log("referenced key is expired") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("expires", receivingKeyRecord->mExpires))
                    goto post_processing;
                  }

                  if (!prepareDHKeys(receivingKeyRecord->mKeyDomain, receivingKeyRecord->mDHEphemeralPrivateKey, receivingKeyRecord->mDHEphemeralPublicKey, privateKey, publicKey)) {
                    ZS_LOG_WARNING(Detail, log("referenced key cannot load DH keying material") + ZS_PARAM("key id", bundle.mReferencedKeyID))
                    goto post_processing;
                  }
                } else {
                  receivingKeyRecord = ReceivingKeyRecordPtr(new ReceivingKeyRecord);

                  ZS_LOG_DEBUG(log("existing receiving key not found thus create one"))

                  if (!prepareNewDHKeys(NULL, receivingKeyRecord->mKeyDomain, receivingKeyRecord->mDHEphemeralPrivateKey, receivingKeyRecord->mDHEphemeralPublicKey, privateKey, publicKey)) {
                    ZS_LOG_WARNING(Detail, log("failed to generate new DH ephemeral keys"))
                    goto post_processing;
                  }

                  receivingKeyRecord->mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
                  receivingKeyRecord->mKeyID = bundle.mReferencedKeyID;
                  receivingKeyRecord->mURI = info.mFrom;
                  receivingKeyRecord->mExpires = bundle.mExpires;

                  mDB->receivingKeyTable()->addOrUpdate(*receivingKeyRecord);
                }

                // remote party has a sending passphrase and this peer needs for the passphrase for receiving
                sendOfferRequestExisting(info.mFrom, bundle.mReferencedKeyID, receivingKeyRecord->mKeyDomain, publicKey, receivingKeyRecord->mExpires);

              } else if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_OFFER_REQUEST_EXISTING == bundle.mMode) {
                // L <-- (offer request existing) <-- R
                // L --> (answer) --> R (local sending passphrase)

                // remote party needs a passphrase for receiving and this peer sends its sending passphrase
                bool isDHKey = false;
                String rawKeyID = extractRawSendingKeyID(bundle.mReferencedKeyID, isDHKey);

                SendingKeyRecordPtr sendingKeyRecord = mDB->sendingKeyTable()->getByKeyID(rawKeyID);
                if (!sendingKeyRecord) {
                  ZS_LOG_WARNING(Detail, log("sending key is not known on this device") + ZS_PARAM("key id", bundle.mReferencedKeyID))
                  goto post_processing;
                }

                if (!isDHKey) {
                  sendPKI(info.mFrom, bundle.mReferencedKeyID, sendingKeyRecord->mRSAPassphrase, bundle.mValidationPeer, sendingKeyRecord->mExpires);
                  goto post_processing;
                }

                // remote side needs DH keying material
                if (sendingKeyRecord->mListSize > 1) {
                  if (sendingKeyRecord->mListSize > sendingKeyRecord->mTotalWithDHPassphrase) {
                    // need to figure out if a peer is ACKing receipt of a particular passphrase
                    String listID = internal::extractListID(sendingKeyRecord->mURI);
                    int order = mDB->listURITable()->getOrder(listID, info.mFrom);
                    if (OPENPEER_STACK_PUSH_MAILBOX_ORDER_UNKNOWN == order) {
                      ZS_LOG_WARNING(Detail, log("the sender is not on the list thus the sender cannot receive the keying material") + ZS_PARAM("from", info.mFrom))
                      goto post_processing;
                    }

                    SecureByteBlockPtr ackBuffer;
                    if (sendingKeyRecord->mAckDHPassphraseSet.hasData()) {
                      ackBuffer = UseServicesHelper::convertFromBase64(sendingKeyRecord->mAckDHPassphraseSet);
                    } else {
                      size_t totalBytes = (sendingKeyRecord->mListSize / 8) + (sendingKeyRecord->mListSize % 8 != 0 ? 1 : 0);
                      ackBuffer = SecureByteBlockPtr(new SecureByteBlock(totalBytes));
                    }

                    size_t position = (order / 8);
                    size_t bit = (order % 8);
                    if (position >= ackBuffer->SizeInBytes()) {
                      ZS_LOG_ERROR(Detail, log("the sender is on the list but beyond the size of the buffer") + ZS_PARAM("key id", sendingKeyRecord->mKeyID) + ZS_PARAM("from", info.mFrom) + ZS_PARAM("buffer size", ackBuffer->SizeInBytes()) + ZS_PARAM("order", order))
                      goto post_processing;
                    }

                    BYTE ackByte = ackBuffer->BytePtr()[position];
                    BYTE before = ackByte;

                    ackByte = ackByte | (1 << bit);

                    ackBuffer->BytePtr()[position] = ackByte;
                    if (before != ackByte) {
                      // byte has ordered in a new bit thus a new ack is present
                      ++sendingKeyRecord->mTotalWithDHPassphrase;
                      ZS_LOG_DEBUG(log("remote party acked DH key") + ZS_PARAM("key id", sendingKeyRecord->mKeyID) + ZS_PARAM("total", sendingKeyRecord->mListSize) + ZS_PARAM("total acked", sendingKeyRecord->mTotalWithDHPassphrase))

                      if (sendingKeyRecord->mListSize > sendingKeyRecord->mTotalWithDHPassphrase) {
                        sendingKeyRecord->mAckDHPassphraseSet = UseServicesHelper::convertToBase64(*ackBuffer);
                      } else {
                        sendingKeyRecord->mAckDHPassphraseSet = String();
                      }

                      mDB->sendingKeyTable()->addOrUpdate(*sendingKeyRecord);
                    }
                  }
                } else {
                  if (0 == sendingKeyRecord->mTotalWithDHPassphrase) {
                    sendingKeyRecord->mTotalWithDHPassphrase = 1;
                    ZS_LOG_DEBUG(log("remote party acked DH key") + ZS_PARAM("key id", sendingKeyRecord->mKeyID) + ZS_PARAM("total", sendingKeyRecord->mListSize) + ZS_PARAM("total acked", sendingKeyRecord->mTotalWithDHPassphrase))
                    mDB->sendingKeyTable()->addOrUpdate(*sendingKeyRecord);
                  }
                }

                if (!prepareDHKeys(sendingKeyRecord->mKeyDomain, sendingKeyRecord->mDHEphemeralPrivateKey, sendingKeyRecord->mDHEphemeralPrivateKey, privateKey, publicKey)) {
                  ZS_LOG_WARNING(Detail, log("unable to prepare DH keying material") + ZS_PARAM("key ID", sendingKeyRecord->mKeyID))
                  goto post_processing;
                }

                sendAnswer(info.mFrom, bundle.mReferencedKeyID, sendingKeyRecord->mDHPassphrase, bundle.mAgreement, sendingKeyRecord->mKeyDomain, privateKey, publicKey, sendingKeyRecord->mExpires);

              } else if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_MODE_ANSWER == bundle.mMode) {
                // remote party is answering and giving a passphrase which represents their sending passphrase (i.e. which is used as a local receiving key)
                ReceivingKeyRecordPtr receivingKeyRecord = mDB->receivingKeyTable()->getByKeyID(bundle.mReferencedKeyID);

                if (!receivingKeyRecord) {
                  ZS_LOG_WARNING(Detail, log("never created an offer so this answer is not related") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("from", info.mFrom))
                  goto post_processing;
                }

                if (receivingKeyRecord->mURI != info.mFrom) {
                  ZS_LOG_WARNING(Detail, log("from does not match the receiving key uri") + ZS_PARAM("key id", bundle.mReferencedKeyID) + ZS_PARAM("from", info.mFrom) + ZS_PARAM("uri", receivingKeyRecord->mURI))
                  goto post_processing;
                }

                if (receivingKeyRecord->mPassphrase.hasData()) {
                  ZS_LOG_WARNING(Detail, log("already know about receiving passphrase thus will not update"))
                  goto post_processing;
                }

                receivingKeyRecord->mPassphrase = getAnswerPassphrase(receivingKeyRecord->mKeyDomain, receivingKeyRecord->mDHEphemeralPrivateKey, receivingKeyRecord->mDHEphemeralPublicKey, bundle.mAgreement, bundle.mSecret);

                if (receivingKeyRecord->mPassphrase.isEmpty()) {
                  ZS_LOG_WARNING(Detail, log("cannot extract answer passphrase") + ZS_PARAM("keying id", bundle.mReferencedKeyID))
                  goto post_processing;
                }

                ZS_LOG_DEBUG(log("received answer passphrase") + ZS_PARAM("keying id", bundle.mReferencedKeyID) + ZS_PARAM("passphrase", receivingKeyRecord->mPassphrase))

                mDB->receivingKeyTable()->addOrUpdate(*receivingKeyRecord);
                mDB->messageTable()->notifyDecryptNowForKeys(receivingKeyRecord->mKeyID);

                mRefreshMessagesNeedingDecryption = true;
              } else {
                ZS_LOG_WARNING(Detail, log("agreement mode is not supported") + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("agreement mode", bundle.mMode))
                goto post_processing;
              }
            }
          }

        post_processing:
          {
            mDB->messageTable()->notifyKeyingProcessed(info.mIndex);
          }
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepDecryptMessages()
      {
        if (!mRefreshMessagesNeedingDecryption) {
          ZS_LOG_TRACE(log("no messages should need decrypting at this time"))
          return true;
        }

        if (mMessagesNeedingDecryption.size() > 0) {
          ZS_LOG_TRACE(log("messages are still pending decrypting"))
          return true;
        }

        MessageRecordListPtr messages = mDB->messageTable()->getBatchNeedingDecrypting(OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE);
        messages = (messages ? messages : MessageRecordListPtr(new MessageRecordList));

        if (messages->size() < 1) {
          ZS_LOG_DEBUG(log("no messages are needing decryption at this time"))
          mRefreshMessagesNeedingDecryption = false;
          return true;
        }

        for (auto iter = messages->begin(); iter != messages->end(); ++iter)
        {
          MessageRecord &info = (*iter);

          String keyID;

          {
            UseServicesHelper::SplitMap split;

            if (info.mEncoding.isEmpty()) {
              ZS_LOG_DEBUG(log("message was not encoded thus the encrypted data is the decrypted data") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex))
              if (info.mEncryptedFileName.hasData()) {
                mDB->messageTable()->notifyDecrypted(info.mIndex, String(), info.mEncryptedFileName, true); // encrypted and decrypted point to same location
              } else {
                mDB->messageTable()->notifyDecrypted(info.mIndex, info.mEncryptedData, true);
              }

              mRefreshVersionedFolders = true;
              continue;
            }

            UseServicesHelper::split(info.mEncoding, split, ':');
            if (split.size() != 5) {
              ZS_LOG_WARNING(Detail, log("encoding scheme is not known thus cannot decrypt") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }
            if ((split[0] + ":") != OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME) {
              ZS_LOG_WARNING(Detail, log("encoding scheme is not \"" OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME "\" thus cannot decrypt") + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }

            String encodingNamespace = UseServicesHelper::convertToString(*UseServicesHelper::convertFromBase64(split[1]));
            if (OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE != encodingNamespace) {
              ZS_LOG_WARNING(Detail, log("encoding namespace is not \"" OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE "\" thus cannot decrypt") + ZS_PARAM("encoding", info.mEncoding))
              goto failed_to_decrypt;
            }

            keyID = split[2];

            ReceivingKeyRecordPtr receivingKeyRecord = mDB->receivingKeyTable()->getByKeyID(keyID);

            if (!receivingKeyRecord) {
              ZS_LOG_WARNING(Detail, log("receiving key is not available key thus cannot decrypt yet") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("key id", keyID))
              goto decrypt_later;
            }

            if (receivingKeyRecord->mPassphrase.isEmpty()) {
              ZS_LOG_WARNING(Detail, log("receiving key does not contain passphrase yet thus cannot decrypt yet") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("key id", keyID))
              goto decrypt_later;
            }

            if ((info.mEncryptedMetaData) &&
                (!info.mDecryptedMetaData)) {

              SecureByteBlockPtr decryptedData;
              if (!decryptMessage(OPENPEER_STACK_PUSH_MAILBOX_KEYING_METADATA_TYPE, keyID, receivingKeyRecord->mPassphrase, split[3], split[4], *info.mEncryptedMetaData, decryptedData)) {
                ZS_LOG_WARNING(Detail, log("failed to decrypt message meta data") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))
                goto failed_to_decrypt;
              }

              ZS_THROW_INVALID_ASSUMPTION_IF(!decryptedData)

              ZS_LOG_DEBUG(log("message meta data is now decrypted") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))

              mDB->messageTable()->notifyDecryptedMetaData(info.mIndex, decryptedData);
            }

            if (info.mEncryptedFileName.isEmpty()) {
              if (!info.mEncryptedData) {
                ZS_LOG_WARNING(Detail, log("message cannot decrypt as it has no data") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))

                mDB->messageTable()->notifyDecrypted(info.mIndex, SecureByteBlockPtr(), true);
                mRefreshVersionedFolders = true;
                continue;
              }

              SecureByteBlockPtr decryptedData;
              if (!decryptMessage(OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE, keyID, receivingKeyRecord->mPassphrase, split[3], split[4], *info.mEncryptedData, decryptedData)) {
                ZS_LOG_WARNING(Detail, log("failed to decrypt message") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))
                goto failed_to_decrypt;
              }

              ZS_THROW_INVALID_ASSUMPTION_IF(!decryptedData)

              ZS_LOG_DEBUG(log("message is now decrypted") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))
              mDB->messageTable()->notifyDecrypted(info.mIndex, decryptedData, true);
              mRefreshVersionedFolders = true;
              continue;
            }

            ZS_LOG_DEBUG(log("will decrypt file asynchronously") + ZS_PARAM("message ID", info.mMessageID))

            // this needs more processing since it's a file
            ProcessedMessagesNeedingDecryptingInfo processedInfo;

            processedInfo.mDecryptedFileName = mDB->storage()->getStorageFileName();
            processedInfo.mInfo = info;
            processedInfo.mPassphraseID = keyID;
            processedInfo.mPassphrase = receivingKeyRecord->mPassphrase;
            processedInfo.mSalt = split[3];
            processedInfo.mProof = split[4];

            UseDecryptorPtr decryptor = prepareDecryptor(keyID, receivingKeyRecord->mPassphrase, split[3]);

            processedInfo.mDecryptor = AsyncDecrypt::create(getAssociatedMessageQueue(), mThisWeak.lock(), decryptor, info.mEncryptedFileName, processedInfo.mDecryptedFileName);
            if (processedInfo.mDecryptor) {
              ZS_LOG_ERROR(Detail, log("failed to create asycn decryptor") + ZS_PARAM("message ID", info.mMessageID) + ZS_PARAM("message index", info.mIndex) + ZS_PARAM("key id", keyID))
              goto failed_to_decrypt;
            }

            mMessagesNeedingDecryption[info.mMessageID] = processedInfo;
            continue;
          }

        failed_to_decrypt:
          {
            mDB->messageTable()->notifyDecryptionFailure(info.mIndex);
            continue;
          }
        decrypt_later:
          {
            mDB->messageTable()->notifyDecryptLater(info.mIndex, keyID);
            continue;
          }
        }

        ZS_LOG_DEBUG(log("finished attempting to decrypt messages") + ZS_PARAM("total", messages->size()))

        // decrypt again...
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepDecryptMessagesAsync()
      {
        if (mMessagesNeedingDecryption.size() < 1) {
          ZS_LOG_TRACE(log("nothing decrypting asynchronously"))
          return true;
        }


        for (MessageDecryptionMap::iterator iter_doNotUse = mMessagesNeedingDecryption.begin(); iter_doNotUse != mMessagesNeedingDecryption.end();)
        {
          MessageDecryptionMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          const String &messageID = (*current).first;
          ProcessedMessagesNeedingDecryptingInfo &processedInfo = (*current).second;

          // scope: check decryption
          {
            if (!processedInfo.mDecryptor->isComplete()) {
              ZS_LOG_TRACE(log("message is still decrypting") + ZS_PARAM("message ID", messageID))
              continue;
            }

            if (!processedInfo.mDecryptor->wasSuccessful()) {
              ZS_LOG_WARNING(Detail, log("failed to decrypt message") + ZS_PARAM("message ID", messageID))
              goto failed_to_decrypt;
            }

            if (!validateDecryption(processedInfo.mPassphraseID, processedInfo.mPassphrase, processedInfo.mSalt, processedInfo.mDecryptor->getHash(), processedInfo.mProof)) {
              ZS_LOG_WARNING(Detail, log("decryption proof validation failed") + ZS_PARAM("message ID", messageID))
              goto failed_to_decrypt;
            }

            ZS_LOG_DEBUG(log("message is now decrypted") + ZS_PARAM("message ID", processedInfo.mInfo.mMessageID) + ZS_PARAM("message index", processedInfo.mInfo.mIndex) + ZS_PARAM("key id", processedInfo.mPassphraseID))
            mDB->messageTable()->notifyDecrypted(processedInfo.mInfo.mIndex, String(), processedInfo.mDecryptedFileName, true);

            // remove the file from the system since the encrypted version is no longer needed
            remove(processedInfo.mInfo.mEncryptedFileName);

            mRefreshVersionedFolders = true;

            mMessagesNeedingDecryption.erase(current);
            continue;
          }

        failed_to_decrypt:
          {
            mDB->messageTable()->notifyDecryptionFailure(processedInfo.mInfo.mIndex);
            mMessagesNeedingDecryption.erase(current);
            continue;
          }
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPendingDelivery()
      {
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

        if (!mRefreshPendingDelivery) {
          ZS_LOG_TRACE(log("no need to check for messages needing delivery at this time"))
          return true;
        }


        PendingDeliveryMessageRecordListPtr pendingDelivery = mDB->pendingDeliveryMessageTable()->getBatchToDeliver();
        pendingDelivery = (pendingDelivery ? pendingDelivery : PendingDeliveryMessageRecordListPtr(new PendingDeliveryMessageRecordList));

        if (pendingDelivery->size() < 1) {
          ZS_LOG_TRACE(log("no messages require delivery at this time"))
          mRefreshPendingDelivery = false;
          mPendingDeliveryPrecheckRequired = false;
          return true;
        }

        MessageIDList fetchMessages;

        for (auto iter = pendingDelivery->begin(); iter != pendingDelivery->end(); ++iter)
        {
          PendingDeliveryMessageRecord &info = (*iter);

          ProcessedPendingDeliveryMessageInfo processedInfo;
          processedInfo.mInfo = info;

          MessageRecordPtr message = mDB->messageTable()->getByIndex(info.mIndexMessage);

          if (!message) {
            ZS_LOG_ERROR(Detail, log("message pending delivery was not found") + ZS_PARAM("message index", info.mIndexMessage))
            mDB->pendingDeliveryMessageTable()->remove(info.mIndex);
            continue;
          }

          processedInfo.mMessage.mID = message->mMessageID;
          processedInfo.mMessage.mTo = message->mTo;
          processedInfo.mMessage.mFrom = message->mFrom;
          processedInfo.mMessage.mCC = message->mCC;
          processedInfo.mMessage.mBCC = message->mBCC;
          processedInfo.mMessage.mType = message->mType;
          processedInfo.mMessage.mMimeType = message->mMimeType;
          processedInfo.mMessage.mEncoding = message->mEncoding;
          if (message->mEncryptedMetaData) {
            processedInfo.mMessage.mMetaData = UseServicesHelper::convertToBase64(*(message->mEncryptedMetaData));
          }
          processedInfo.mMessage.mPushType = message->mPushType;
          copy(message->mPushInfos, processedInfo.mMessage.mPushInfos);
          processedInfo.mMessage.mSent = message->mSent;
          processedInfo.mMessage.mExpires = message->mExpires;
          processedInfo.mEncryptedFileName = message->mEncryptedFileName;
          processedInfo.mDecryptedFileName = message->mDecryptedFileName;

          if (processedInfo.mDecryptedFileName.hasData()) {
            if (processedInfo.mEncryptedFileName.isEmpty()) {
              processedInfo.mEncryptedFileName = mDB->storage()->getStorageFileName();

              UseEncryptorPtr encryptor =  prepareEncryptor(processedInfo.mMessage.mEncoding);

              if (!encryptor) {
                ZS_LOG_ERROR(Detail, log("failed to create encryptor for decrypted file") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
                handleMessageGone(processedInfo.mMessage.mID);
                continue;
              }

              processedInfo.mEncryptor = AsyncEncrypt::create(getAssociatedMessageQueue(), mThisWeak.lock(), encryptor, processedInfo.mDecryptedFileName, processedInfo.mEncryptedFileName);
            } else {
              ZS_LOG_TRACE(log("file was previously encrypted"))
            }
          }

          if (processedInfo.mData) {
            ZS_LOG_TRACE(log("allocating a sending channel ID"))
            processedInfo.mMessage.mChannelID = pickNextChannel();
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

          fetchMessages.push_back(processedInfo.mMessage.mID);
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

        IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<MessagesMetaDataGetResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS)));

        for (MessageIDList::iterator iter = fetchMessages.begin(); iter != fetchMessages.end(); ++iter)
        {
          const String &messageID = (*iter);

          PendingDeliveryMap::iterator found = mPendingDelivery.find(messageID);
          ZS_THROW_BAD_STATE_IF(found == mPendingDelivery.end())

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*found).second;

          processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor = monitor;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPendingDeliveryEncryptMessages()
      {
        if (mPendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("no messages need uploading at this time"))
          return true;
        }

        for (PendingDeliveryMap::iterator iter_doNotuse = mPendingDelivery.begin(); iter_doNotuse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (!processedInfo.mEncryptor) {
            ZS_LOG_INSANE(log("message is not encrypting") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (!processedInfo.mEncryptor->isComplete()) {
            ZS_LOG_TRACE(log("message is still pending encryption") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (!processedInfo.mEncryptor->wasSuccessful()) {
            ZS_LOG_ERROR(Detail, log("message failed to encrypt") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("decrypted file name", processedInfo.mDecryptedFileName) + ZS_PARAM("encrypted file name", processedInfo.mEncryptedFileName))

            handleMessageGone(processedInfo.mMessage.mID);
            continue;
          }

          processedInfo.mMessage.mEncoding = finalizeEncodingScheme(processedInfo.mMessage.mEncoding, processedInfo.mEncryptor->getHash());

          if (processedInfo.mMessage.mEncoding.isEmpty()) {
            ZS_LOG_ERROR(Detail, log("message failed to encrypt") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("decrypted file name", processedInfo.mDecryptedFileName) + ZS_PARAM("encrypted file name", processedInfo.mEncryptedFileName))

            handleMessageGone(processedInfo.mMessage.mID);
            continue;
          }

          processedInfo.mInfo.mEncryptedDataLength = processedInfo.mEncryptor->getOutputSize();
          processedInfo.mEncryptor.reset();

          mDB->messageTable()->updateEncodingAndEncryptedDataLength(processedInfo.mInfo.mIndexMessage, processedInfo.mMessage.mEncoding, processedInfo.mInfo.mEncryptedDataLength);
        }

        for (PendingDeliveryMap::iterator iter_doNotuse = mPendingDelivery.begin(); iter_doNotuse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if ((processedInfo.mEncryptedFileName.isEmpty()) ||
              (0 != processedInfo.mInfo.mEncryptedDataLength) ||
              (processedInfo.mEncryptor)) {
            ZS_LOG_INSANE(log("message does not need post encryption processing") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          struct stat st;
          st.st_size = 0;
          int result = stat(processedInfo.mEncryptedFileName, &st);

          if (result < 0) {
            int error = errno;
            ZS_LOG_ERROR(Detail, log("could not open encrypted file") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("encrypted file name", processedInfo.mEncryptedFileName) + ZS_PARAM("error", error) + ZS_PARAM("error string", strerror(error)))
            remove(processedInfo.mEncryptedFileName);
            handleMessageGone(processedInfo.mMessage.mID);
            continue;
          }

          processedInfo.mInfo.mEncryptedDataLength = st.st_size;

          if (0 == st.st_size) {
            // message contains no data thus no need to send it encrypted
            processedInfo.mDecryptedFileName.clear();
            processedInfo.mEncryptedFileName.clear();
            processedInfo.mData.reset();
            processedInfo.mMessage.mEncoding.clear();

            mDB->messageTable()->updateEncodingAndFileNames(processedInfo.mInfo.mIndexMessage, String(), String(), String(), 0);
            continue;
          }
        }

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

          if (processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
            ZS_LOG_TRACE(log("waiting for the pending delivery pre-check to complete") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (processedInfo.mPendingDeliveryMessageUpdateRequest) {
            ZS_LOG_TRACE(log("already issues pending delivery message update request") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          ZS_THROW_BAD_STATE_IF(processedInfo.mPendingDeliveryMessageUpdateErrorMonitor)
          ZS_THROW_BAD_STATE_IF(processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor)
          
          ZS_LOG_DEBUG(log("issuing request to upload next message") + ZS_PARAM("message id", processedInfo.mMessage.mID))

          MessageUpdateRequestPtr request = MessageUpdateRequest::create();
          request->domain(mBootstrappedNetwork->getDomain());

          request->messageInfo(processedInfo.mMessage);

          processedInfo.mPendingDeliveryMessageUpdateErrorMonitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));

          processedInfo.mPendingDeliveryMessageUpdateRequest = request;

          if ((0 == processedInfo.mMessage.mChannelID) &&
              (processedInfo.mEncryptedFileName.isEmpty())) {
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
        if (mPendingDelivery.size() < 1) {
          ZS_LOG_TRACE(log("no messages need uploading at this time"))
          return true;
        }

        for (PendingDeliveryMap::iterator iter_doNotuse = mPendingDelivery.begin(); iter_doNotuse != mPendingDelivery.end(); )
        {
          PendingDeliveryMap::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          const String &messageID = (*current).first;
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*current).second;

          if (processedInfo.mDeliveryURL.hasData()) {
            ZS_LOG_TRACE(log("already attempting to deliver via temporary file") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
            ZS_LOG_TRACE(log("waiting for the pending delivery pre-check to complete") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (processedInfo.mPendingDeliveryMessageUpdateUploadCompleteMonitor) {
            ZS_LOG_TRACE(log("already completed upload process for this message") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
            continue;
          }

          if (processedInfo.mData) {
            if (processedInfo.mSent >= processedInfo.mData->SizeInBytes()) {
              ZS_LOG_WARNING(Detail, log("already uploaded entire message") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("sent", processedInfo.mSent) + ZS_PARAM("total", processedInfo.mData->SizeInBytes()))
              continue;
            }
          } else if (processedInfo.mSent >= processedInfo.mInfo.mEncryptedDataLength) {
            ZS_LOG_WARNING(Detail, log("already uploaded entire message") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("sent", processedInfo.mSent) + ZS_PARAM("total", processedInfo.mData->SizeInBytes()))
            continue;
          }

          if (processedInfo.mEncryptor) {
            ZS_LOG_WARNING(Detail, log("message is in the process of being encrypted still") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("sent", processedInfo.mSent) + ZS_PARAM("total", processedInfo.mData->SizeInBytes()))
            continue;
          }

          if ((!processedInfo.mData) &&
              (processedInfo.mEncryptedFileName.isEmpty())) {
            ZS_LOG_TRACE(log("message does not have data to deliver"))
            continue;
          }

          if (processedInfo.mEncryptedFileName.isEmpty()) {
            ZS_THROW_INVALID_ASSUMPTION_IF(!processedInfo.mData)

            if (!mBackgroundingEnabled) {
              ZS_LOG_TRACE(log("no need to upload via URL when not backgrounding") + ZS_PARAM("message ID", processedInfo.mMessage.mID))
              continue;
            }

            processedInfo.mEncryptedFileName = mDB->storage()->storeToTemporaryFile(*processedInfo.mData);

            if (processedInfo.mEncryptedFileName.isEmpty()) {
              ZS_LOG_ERROR(Detail, log("failed to store encrypted data to disk") + ZS_PARAM("message ID", processedInfo.mMessage.mID) + ZS_PARAM("size", processedInfo.mData->SizeInBytes()))
              continue;
            }

            processedInfo.mInfo.mEncryptedDataLength = processedInfo.mData->SizeInBytes();

            mDB->messageTable()->updateEncryptionStorage(processedInfo.mInfo.mIndexMessage, processedInfo.mEncryptedFileName, processedInfo.mInfo.mEncryptedDataLength);

            processedInfo.mData.reset();
          }

          auto totalRemaining = processedInfo.mInfo.mEncryptedDataLength - processedInfo.mSent;
          processedInfo.mDeliveryURL = mUploadMessageURL;

          // replace message ID
          {
            size_t found = processedInfo.mDeliveryURL.find(mStringReplacementMessageID);
            if (String::npos != found) {
              String replaceMessageID = urlencode(messageID);
              processedInfo.mDeliveryURL.replace(found, mStringReplacementMessageID.size(), replaceMessageID);
            }
          }

          // replace size
          {
            size_t found = processedInfo.mDeliveryURL.find(mStringReplacementMessageSize);
            if (String::npos != found) {
              String replaceMessageSize = string(totalRemaining);
              processedInfo.mDeliveryURL.replace(found, mStringReplacementMessageID.size(), replaceMessageSize);
            }
          }

          ZS_LOG_DEBUG(log("going to deliver push message via URL") + ZS_PARAM("url", processedInfo.mDeliveryURL) + ZS_PARAM("file", processedInfo.mEncryptedFileName))
          mTransferDelegate->onServicePushMailboxSessionTransferUploadFileDataToURL(mThisWeak.lock(), processedInfo.mDeliveryURL, processedInfo.mEncryptedFileName, processedInfo.mMessage.mLength, totalRemaining, AsyncNotifier::create(getAssociatedMessageQueue(), mThisWeak.lock(), processedInfo.mMessage.mID));
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepPrepareVersionedFolderMessages()
      {
        ZS_DECLARE_TYPEDEF_PTR(IFolderMessageTable::IndexList, IndexList)

        typedef std::map<FolderIndex, FolderIndex> FoldersUpdatedMap;

        if (!mRefreshVersionedFolders) {
          ZS_LOG_TRACE(log("no need to refresh versioned folders"))
          return true;
        }

        MessageRecordListPtr needingNotification = mDB->messageTable()->getBatchNeedingNotification();
        needingNotification = (needingNotification ? needingNotification : MessageRecordListPtr(new MessageRecordList));

        if (needingNotification->size() < 1) {
          ZS_LOG_DEBUG(log("no messages need notification at this time"))
          mRefreshVersionedFolders = false;
          return true;
        }

        FoldersUpdatedMap updatedFolders;

        for (auto iter = needingNotification->begin(); iter != needingNotification->end(); ++iter) {
          MessageRecord &info = (*iter);

          {
            if (info.mEncryptedDataLength > 0) {
              if (!info.mHasDecryptedData) {
                ZS_LOG_TRACE(log("cannot add to version table yet as data is not decrypted"))
                goto clear_notification;
              }
            }

            IndexListPtr folders = mDB->folderMessageTable()->getWithMessageID(info.mMessageID);
            folders = (folders ? folders : IndexListPtr(new IndexList));

            for (auto folderIter = folders->begin(); folderIter != folders->end(); ++folderIter) {
              FolderIndex folderIndex = (*folderIter);

              FolderVersionedMessageRecord folderVersionedMessageRecord;
              folderVersionedMessageRecord.mMessageID = info.mMessageID;
              folderVersionedMessageRecord.mIndexFolderRecord = folderIndex;

              mDB->folderVersionedMessageTable()->add(folderVersionedMessageRecord);
              updatedFolders[folderIndex] = folderIndex;
            }
          }

        clear_notification:
          mDB->messageTable()->clearNeedsNotification(info.mIndex);
        }

        for (FoldersUpdatedMap::iterator iter = updatedFolders.begin(); iter != updatedFolders.end(); ++iter)
        {
          FolderIndex index = (*iter).second;
          String name = mDB->folderTable()->getName(index);
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

          IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));
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

          IMessageMonitorPtr monitor = sendRequest(IMessageMonitorResultDelegate<MessageUpdateResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS)));
          mMarkingMonitors.push_back(monitor);
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::stepExpireMessages()
      {
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

        MessageRecordListPtr needsExpiry = mDB->messageTable()->getBatchNeedingExpiry(zsLib::now());
        needsExpiry = (needsExpiry ? needsExpiry : MessageRecordListPtr(new MessageRecordList));

        if (needsExpiry->size() < 1) {
          ZS_LOG_DEBUG(log("no messages needing expiry at this time"))
          mRefreshMessagesNeedingExpiry = false;
          return true;
        }

        ZS_LOG_DEBUG(log("expiring messages") + ZS_PARAM("total", needsExpiry->size()))

        for (auto iter = needsExpiry->begin(); iter != needsExpiry->end(); ++iter) {
          MessageRecord &info = (*iter);

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

        mLastError = errorCode;
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

        // clean out decrypting messages
        for (MessageDecryptionMap::iterator iter = mMessagesNeedingDecryption.begin(); iter != mMessagesNeedingDecryption.end(); ++iter)
        {
          ProcessedMessagesNeedingDecryptingInfo &processedInfo = (*iter).second;
          if (!processedInfo.mDecryptor) continue;

          processedInfo.mDecryptor->cancel();
          processedInfo.mDecryptor.reset();
        }
        mMessagesNeedingDecryption.clear();

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
        auto retryDuration = mLastRetryDuration;
        ZS_LOG_WARNING(Detail, log("connection failure") + ZS_PARAM("wait time (s)", retryDuration))

        mDoNotRetryConnectionBefore = zsLib::now() + retryDuration;
        ZS_LOG_WARNING(Detail, log("TCP messaging unexpectedly shutdown thus will retry again later") + ZS_PARAM("later", mDoNotRetryConnectionBefore) + ZS_PARAM("duration (s)", mLastRetryDuration))

        if (retryDuration < Seconds(1))
          retryDuration = Seconds(1);

        mLastRetryDuration = retryDuration + retryDuration;

        auto maxDuration = Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_RETRY_CONNECTION_IN_SECONDS));
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

          mObtainedLock = false;
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

        for (PendingDeliveryMap::iterator iter = mPendingDelivery.begin(); iter != mPendingDelivery.end(); ++iter) {
          ProcessedPendingDeliveryMessageInfo &processedInfo = (*iter).second;

          if (processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor) {
            processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor->cancel();
            processedInfo.mPendingDeliveryPrecheckMessageMetaDataGetMonitor.reset();
          }

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

        SecureByteBlockPtr output = UseServicesHelper::writeAsJSON(document);

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
                                                                Seconds timeout
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
            (serverVersion != mDB->settingsTable()->getLastDownloadedVersionForFolders())) {
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

          mDB->folderTable()->updateServerVersionIfFolderExists(info.mName, info.mVersion);
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

          mDB->messageTable()->updateServerVersionIfExists(info.mID, info.mVersion);
        }

        ZS_LOG_DEBUG(log("finished processing change notification"))
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleListFetch(ListFetchRequestPtr request)
      {
        ZS_DECLARE_TYPEDEF_PTR(IListURITable::URIList, URIList)

        typedef ListFetchRequest::ListIDList ListIDList;
        typedef ListFetchResult::URIListMap URIListMap;

        ListFetchResultPtr result = ListFetchResult::create(request);

        URIListMap listResults;

        const ListIDList &listIDs = request->listIDs();

        ZS_LOG_DEBUG(log("received request to fetch list(s)") + ZS_PARAM("total lists", listIDs.size()))

        for (ListIDList::const_iterator iter = listIDs.begin(); iter != listIDs.end(); ++iter)
        {
          const String &listID = (*iter);

          URIListPtr uris = mDB->listURITable()->getURIs(listID);
          uris = (uris ? uris : URIListPtr(new URIList));

          if (uris->size() > 0) {
            ZS_LOG_DEBUG(log("found list containing URIs") + ZS_PARAM("list id", listID) + ZS_PARAM("total", uris->size()))
            listResults[listID] = *uris;
          } else {
            ZS_LOG_WARNING(Debug, log("did not find any URIs for list") + ZS_PARAM("list id", listID))
          }
        }

        result->listToURIs(listResults);

        send(result);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleUpdateMessageGone(
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
        ZS_DECLARE_TYPEDEF_PTR(IFolderMessageTable::IndexList, IndexList)

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

        auto indexMessage = mDB->messageTable()->getIndex(messageID);
        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == indexMessage) {
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

        IndexListPtr folderIndexes = mDB->folderMessageTable()->getWithMessageID(messageID);

        // notify that this message is now removed in the version table
        for (auto iter = folderIndexes->begin(); iter != folderIndexes->end(); ++iter) {
          index folderIndex = (*iter);

          FolderVersionedMessageRecord folderVersionedMessageRecord;
          folderVersionedMessageRecord.mIndexFolderRecord = folderIndex;
          folderVersionedMessageRecord.mMessageID = messageID;
          folderVersionedMessageRecord.mRemovedFlag = true;

          mDB->folderVersionedMessageTable()->addRemovedEntryIfNotAlreadyRemoved(folderVersionedMessageRecord);

          FolderMessageRecord folderMessageRecord;
          folderMessageRecord.mIndexFolderRecord = folderIndex;
          folderMessageRecord.mMessageID = messageID;

          mDB->folderMessageTable()->removeFromFolder(folderMessageRecord);
        }

        mDB->messageDeliveryStateTable()->removeForMessage(indexMessage);
        mDB->pendingDeliveryMessageTable()->removeByMessageIndex(indexMessage);
        mDB->messageTable()->remove(indexMessage);
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

          if (info.mReceivedData + buffer->SizeInBytes() > info.mInfo.mEncryptedDataLength) {
            ZS_LOG_ERROR(Detail, log("data length does not match server length"))
            connectionFailure();
            continue;
          }

          if (!info.mBuffer) {
            info.mBuffer = SecureByteBlockPtr(new SecureByteBlock(static_cast<SecureByteBlock::size_type>(info.mInfo.mEncryptedDataLength)));
          }

          ZS_LOG_DEBUG(log("receiving data from server") + ZS_PARAM("channel", data.first) + ZS_PARAM("message id", messageID) + ZS_PARAM("length", buffer->SizeInBytes()) + ZS_PARAM("total", info.mBuffer->SizeInBytes()))

          memcpy(&((info.mBuffer->BytePtr())[info.mReceivedData]), buffer->BytePtr(), buffer->SizeInBytes());

          info.mReceivedData += buffer->SizeInBytes();

          if (info.mReceivedData != info.mInfo.mEncryptedDataLength) {
            ZS_LOG_DEBUG(log("more data still expected"))
            continue;
          }

          ZS_LOG_DEBUG(log("update database record with received for message"))

          mDB->messageTable()->updateEncryptedData(info.mInfo.mIndex, *info.mBuffer);

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
          messageID = UseServicesHelper::randomString(32);
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
        if (mMessagesNeedingDecryption.size() > 0) return true;
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::areAnyMessagesUploading() const
      {
        if (mRefreshPendingDelivery) return true;
        if (mPendingDelivery.size() > 0) return true;
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

          outBundle.mExpires = UseServicesHelper::stringToTime(IMessageHelper::getElementText(keyingEl->findFirstChildElementChecked("expires")));

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
          keyingEl->adoptAsLastChild(IMessageHelper::createElementWithNumber("expires", UseServicesHelper::timeToString(inBundle.mExpires)));
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
        UseServicesHelper::SplitMap parts;
        UseServicesHelper::split(secret, parts, ':');
        if (parts.size() != 3) {
          ZS_LOG_WARNING(Detail, log("failed to extract RSA encrypted key") + ZS_PARAM("secret", secret))
          return String();
        }

        String salt = parts[0];
        String proof = parts[1];
        SecureByteBlockPtr encrytpedPassword = UseServicesHelper::convertFromBase64(parts[2]);

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

        String resultPassphrase = UseServicesHelper::convertToString(*passphrase);

        String calculatedProof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(resultPassphrase), "proof:" + salt, UseServicesHelper::HashAlgorthm_SHA1));
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
        //String staticPrivateKeyStr;
        //String staticPublicKeyStr;

        KeyDomainRecordPtr keyDomainRecord = mDB->keyDomainTable()->getByKeyDomain(inKeyDomain);

        if (keyDomainRecord) {
          outStaticPrivateKey = UseServicesHelper::convertFromBase64(keyDomainRecord->mDHStaticPrivateKey);
          outStaticPublicKey = UseServicesHelper::convertFromBase64(keyDomainRecord->mDHStaticPublicKey);
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

        keyDomainRecord = KeyDomainRecordPtr(new KeyDomainRecord);
        keyDomainRecord->mKeyDomain = inKeyDomain;
        keyDomainRecord->mDHStaticPrivateKey = UseServicesHelper::convertToBase64(*outStaticPrivateKey);
        keyDomainRecord->mDHStaticPublicKey = UseServicesHelper::convertToBase64(*outStaticPublicKey);

        mDB->keyDomainTable()->add(*keyDomainRecord);

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

        SecureByteBlockPtr ephemeralPrivateKey = UseServicesHelper::convertFromBase64(inEphemeralPrivateKey);
        SecureByteBlockPtr ephemeralPublicKey = UseServicesHelper::convertFromBase64(inEphemeralPublicKey);

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

          outEphemeralPrivateKey = UseServicesHelper::convertToBase64(ephemeralPrivateKeyBuffer);
        }
        if (outPublicKey) {
          SecureByteBlock staticPublicKeyBuffer;
          SecureByteBlock ephemeralPublicKeyBuffer;

          outPublicKey->save(&staticPublicKeyBuffer, &ephemeralPublicKeyBuffer);

          outEphemeralPublicKey = UseServicesHelper::convertToBase64(ephemeralPublicKeyBuffer);
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
        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(agreement, split, ':');
        if (split.size() != 3) {
          ZS_LOG_WARNING(Detail, log("cannot split agrement properly") + ZS_PARAM("agreement", agreement))
          return false;
        }

        String domain = UseServicesHelper::convertToString(*UseServicesHelper::convertFromBase64(split[0]));

        IDHKeyDomain::KeyDomainPrecompiledTypes precompiledKeyDomain = IDHKeyDomain::fromNamespace(domain);
        if (IDHKeyDomain::KeyDomainPrecompiledType_Unknown == precompiledKeyDomain) {
          ZS_LOG_WARNING(Detail, log("cannot extract a known key domain") + ZS_PARAM("agreement", agreement) + ZS_PARAM("domain", domain))
          return false;
        }

        SecureByteBlockPtr staticPublicKey = UseServicesHelper::convertFromBase64(split[1]);
        SecureByteBlockPtr ephemeralPublicKey = UseServicesHelper::convertFromBase64(split[2]);

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

        SecureByteBlockPtr output = UseServicesHelper::writeAsJSON(document);
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

        MessageRecord messageRecord;
        messageRecord.mMessageID = messageID;
        messageRecord.mTo = toURI;
        messageRecord.mFrom = peerFilePublic->getPeerURI();
        messageRecord.mType = OPENPEER_STACK_PUSH_MAILBOX_KEYING_TYPE;
        messageRecord.mMimeType = OPENPEER_STACK_PUSH_MAILBOX_JSON_MIME_TYPE;
        messageRecord.mEncoding = OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE;
        messageRecord.mSent = zsLib::now();
        messageRecord.mExpires = inBundle.mExpires;
        messageRecord.mEncryptedData = output;
        messageRecord.mNeedsNotification = false;

        auto indexMessage = mDB->messageTable()->insert(messageRecord);

        if (OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN == indexMessage) {
          ZS_LOG_ERROR(Detail, log("failed to insert keying message into messages table") + ZS_PARAM("message id", messageID))
          return false;
        }

        PendingDeliveryMessageRecord pendingDeliveryMessageRecord;
        pendingDeliveryMessageRecord.mIndexMessage = indexMessage;
        pendingDeliveryMessageRecord.mRemoteFolder = OPENPEER_STACK_PUSH_MAILBOX_KEYS_FOLDER_NAME;
        pendingDeliveryMessageRecord.mCopyToSent = false;
        pendingDeliveryMessageRecord.mSubscribeFlags = 0;

        mDB->pendingDeliveryMessageTable()->insert(pendingDeliveryMessageRecord);
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
        bundle.mAgreement = UseServicesHelper::convertToBase64(domain) + ":" + UseServicesHelper::convertToBase64(staticPublicKey) + ":" + UseServicesHelper::convertToBase64(ephemeralPublicKey);

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
        bundle.mAgreement = UseServicesHelper::convertToBase64(domain) + ":" + UseServicesHelper::convertToBase64(staticPublicKey) + ":" + UseServicesHelper::convertToBase64(ephemeralPublicKey);

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

        SecureByteBlockPtr encryptedPassphrase = peerFilePublic->encrypt(*UseServicesHelper::convertToBuffer(passphrase));

        String salt = UseServicesHelper::randomString(20);
        String proof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(passphrase), "proof:" + salt, UseServicesHelper::HashAlgorthm_SHA1));

        bundle.mSecret = salt + ":" + proof + ":" + UseServicesHelper::convertToBase64(*encryptedPassphrase);

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
        bundle.mAgreement = UseServicesHelper::convertToBase64(domain) + ":" + UseServicesHelper::convertToBase64(staticPublicKey) + ":" + UseServicesHelper::convertToBase64(ephemeralPublicKey);

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

        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(UseServicesHelper::convertToHex(*agreementKey)), "push-mailbox:", UseServicesHelper::HashAlgorthm_SHA256);

        bundle.mSecret = stack::IHelper::splitEncrypt(*key, *UseServicesHelper::convertToBuffer(passphrase));

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
        
        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(UseServicesHelper::convertToHex(*agreementKey)), "push-mailbox:", UseServicesHelper::HashAlgorthm_SHA256);
        
        SecureByteBlockPtr decryptedPassphrase = stack::IHelper::splitDecrypt(*key, secret);
        
        return UseServicesHelper::convertToString(*decryptedPassphrase);
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
        ZS_DECLARE_TYPEDEF_PTR(IListURITable::URIList, URIList)
        typedef std::map<String, String> SortedMap;

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

        String listID = UseServicesHelper::convertToHex(hashResult);
        String listURI = String(OPENPEER_STACK_PUSH_MAILBOX_LIST_URI_PREFIX) + listID;

        if (mDB->listTable()->hasListID(listID)) {
          ZS_LOG_TRACE(log("list already exists") + ZS_PARAM("list ID", listID))
          return listURI;
        }

        auto indexListRecord = mDB->listTable()->addOrUpdateListID(listID);
        if (indexListRecord < 0) {
          ZS_LOG_ERROR(Detail, log("failed to create list") + ZS_PARAM("list ID", listID))
          return String();
        }

        mDB->listURITable()->update(indexListRecord, uris);
        mDB->listTable()->notifyDownloaded(indexListRecord);

        return listURI;
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::PeerOrIdentityListPtr ServicePushMailboxSession::convertFromDatabaseList(const String &uri)
      {
        ZS_DECLARE_TYPEDEF_PTR(IListURITable::URIList, URIList)

        if (uri.isEmpty()) return PeerOrIdentityListPtr();

        PeerOrIdentityListPtr result(new PeerOrIdentityList);

        String listID = extractListID(uri);
        if (listID.isEmpty()) {
          result->push_back(uri);
          return result;
        }

        URIListPtr uris = mDB->listURITable()->getURIs(listID);
        uris = (uris ? uris : URIListPtr(new URIList));

        if (uris->size() < 1) {
          ZS_LOG_WARNING(Detail, log("list ID not found in database") + ZS_PARAM("list", uri))
          return PeerOrIdentityListPtr();
        }

        for (URIList::iterator iter = uris->begin(); iter != uris->end(); ++iter) {
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

        SendingKeyRecord info;

        // need to generate an active sending key
        info.mIndex = OPENPEER_STACK_PUSH_MAILBOX_INDEX_UNKNOWN;
        info.mKeyID = UseServicesHelper::randomString(20);
        info.mURI = uri;
        info.mRSAPassphrase = UseServicesHelper::randomString(32*8/5);
        info.mDHPassphrase = UseServicesHelper::randomString(32*8/5);
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

        info.mActiveUntil = zsLib::now() + Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SENDING_KEY_ACTIVE_UNTIL_IN_SECONDS));
        info.mExpires = zsLib::now() + Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SENDING_KEY_EXPIRES_AFTER_IN_SECONDS));

        outKeyID = info.mKeyID;
        outInitialPassphrase = info.mRSAPassphrase;
        outExpires = info.mExpires;

        mDB->sendingKeyTable()->addOrUpdate(info);

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
      ServicePushMailboxSession::UseEncryptorPtr ServicePushMailboxSession::prepareEncryptor(const String &tempEncodingScheme)
      {
        UseServicesHelper::SplitMap split;
        UseServicesHelper::SplitMap splitKey;

        UseServicesHelper::split(tempEncodingScheme, split, ':');

        if (split.size() < 4) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme))
          return UseEncryptorPtr();
        }

        if (split[0] + ":" != OPENPEER_STACK_PUSH_MAILBOX_TEMP_KEYING_URI_SCHEME) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme) + ZS_PARAM("expecting", OPENPEER_STACK_PUSH_MAILBOX_TEMP_KEYING_URI_SCHEME))
          return UseEncryptorPtr();
        }

        String salt = split[3];
        String keying = split[2];

        UseServicesHelper::split(keying, splitKey, '=');
        if (splitKey.size() < 2) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme))
          return UseEncryptorPtr();
        }

        String passphraseID = splitKey[0];
        String passphrase = splitKey[1];

        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(passphrase), String("push-mailbox:") + OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE + ":" + passphraseID, UseServicesHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = UseServicesHelper::hash(salt, UseServicesHelper::HashAlgorthm_MD5);

        return UseEncryptor::create(*key, *iv);
      }

      //-----------------------------------------------------------------------
      String ServicePushMailboxSession::finalizeEncodingScheme(
                                                               const String &tempEncodingScheme,
                                                               const String &sourceHexHash
                                                               )
      {
        UseServicesHelper::SplitMap split;
        UseServicesHelper::SplitMap splitKey;

        UseServicesHelper::split(tempEncodingScheme, split, ':');

        if (split.size() < 4) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme))
          return String();
        }

        if (split[0] + ":" != OPENPEER_STACK_PUSH_MAILBOX_TEMP_KEYING_URI_SCHEME) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme) + ZS_PARAM("expecting", OPENPEER_STACK_PUSH_MAILBOX_TEMP_KEYING_URI_SCHEME))
          return String();
        }

        String salt = split[3];
        String keying = split[2];

        UseServicesHelper::split(keying, splitKey, '=');
        if (splitKey.size() < 2) {
          ZS_LOG_ERROR(Detail, log("temp encoding scheme is not understood") + ZS_PARAM("encoding", tempEncodingScheme))
          return String();
        }

        String passphraseID = splitKey[0];
        String passphrase = splitKey[1];

        String proof;
        String calculatedProof = String(OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE) + "=" + UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(passphrase), String("proof:") + OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE + ":" + salt + ":" + sourceHexHash));

        if (split.size() > 4) {
          proof = split[4];
        }

        if (proof.isEmpty()) {
          proof = calculatedProof;
        } else {
          proof += "," + calculatedProof;
        }

        return String(OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME) + UseServicesHelper::convertToBase64(OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE) + ":" + passphraseID + ":" + salt + ":" + proof;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::encryptMessage(
                                                     const char *inDataType,
                                                     const String &inPassphraseID,
                                                     const String &inPassphrase,
                                                     SecureByteBlock &inOriginalData,
                                                     String &ioEncodingScheme,
                                                     SecureByteBlockPtr &outEncryptedMessage
                                                     )
      {
        String scheme = OPENPEER_STACK_PUSH_MAILBOX_KEYING_URI_SCHEME;
        String salt;
        String proof;
        String keyID = inPassphraseID;
        String dataType(inDataType);

        if (ioEncodingScheme.hasData()) {
          UseServicesHelper::SplitMap values;
          UseServicesHelper::split(ioEncodingScheme, values, ':');

          //key:encoding_type:passphrase_id:salt:proof
          if (values.size() > 0) {
            scheme = values[0] + ":";
          }
          if (values.size() > 2) {
            keyID = values[2];
          }
          if (values.size() > 3) {
            salt = values[3];
          }
          if (values.size() > 4) {
            proof = values[4];
          }
        }

        if (salt.isEmpty()) {
          salt = UseServicesHelper::randomString(16*8/5);
        }

        String calculatedProof = dataType + "=" + UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "proof:" + dataType + ":" + salt + ":" + UseServicesHelper::convertToHex(*UseServicesHelper::hash(inOriginalData))));

        if (proof.isEmpty()) {
          proof = calculatedProof;
        } else {
          proof += "," + calculatedProof;
        }

        ioEncodingScheme = scheme + UseServicesHelper::convertToBase64(OPENPEER_STACK_PUSH_MAILBOX_KEYING_ENCODING_TYPE) + ":" + keyID + ":" + salt + ":" + proof;

        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "push-mailbox:" + dataType + ":" + inPassphraseID, UseServicesHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = UseServicesHelper::hash(salt, UseServicesHelper::HashAlgorthm_MD5);

        outEncryptedMessage = UseServicesHelper::encrypt(*key, *iv, inOriginalData);

        return (bool)outEncryptedMessage;
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::UseDecryptorPtr ServicePushMailboxSession::prepareDecryptor(
                                                                                             const String &inPassphraseID,
                                                                                             const String &inPassphrase,
                                                                                             const String &inSalt
                                                                                             )
      {
        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "push-mailbox:" + inPassphraseID, UseServicesHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = UseServicesHelper::hash(inSalt, UseServicesHelper::HashAlgorthm_MD5);

        return UseDecryptor::create(*key, *iv);
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::validateDecryption(
                                                         const String &inPassphraseID,
                                                         const String &inPassphrase,
                                                         const String &inSalt,
                                                         const String &inDataHash,
                                                         const String &inProof
                                                         )
      {
        String calculatedProof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "proof:" + inSalt + ":" + inDataHash));
        if (validateProof(OPENPEER_STACK_PUSH_MAILBOX_KEYING_DATA_TYPE, calculatedProof, inProof)) {
          ZS_LOG_WARNING(Detail, log("proof failed to validate thus message failed to decrypt properly") + ZS_PARAM("proof", inProof) + ZS_PARAM("calculated proof", calculatedProof))
          return false;
        }
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::validateProof(
                                                    const char *inDataType,
                                                    const String &inCalculatedProofHash,
                                                    const String &inProof
                                                    )
      {
        String dataType(inDataType);

        UseServicesHelper::SplitMap values;
        UseServicesHelper::split(inProof, values, ',');

        for (auto iter = values.begin(); iter != values.end(); ++iter) {
          const String &value = (*iter).second;
          UseServicesHelper::SplitMap keying;
          UseServicesHelper::split(value, keying, '=');
          if (keying.size() < 2) {
            ZS_LOG_WARNING(Detail, log("proof information was not understood") + ZS_PARAMIZE(inProof) + ZS_PARAMIZE(value))
            continue;
          }

          String type = keying[0];
          String hash = keying[1];

          if (type != dataType) continue;

          if (hash != inCalculatedProofHash) {
            ZS_LOG_WARNING(Detail, log("hash proof failure") + ZS_PARAMIZE(inDataType) + ZS_PARAMIZE(inCalculatedProofHash) + ZS_PARAMIZE(inProof) + ZS_PARAMIZE(type) + ZS_PARAMIZE(hash))
            return false;
          }

          return true;
        }

        ZS_LOG_WARNING(Detail, log("hash proof not found") + ZS_PARAMIZE(inDataType) + ZS_PARAMIZE(inCalculatedProofHash) + ZS_PARAMIZE(inProof))
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::decryptMessage(
                                                     const char *inDataType,
                                                     const String &inPassphraseID,
                                                     const String &inPassphrase,
                                                     const String &inSalt,
                                                     const String &inProof,
                                                     SecureByteBlock &inOriginalData,
                                                     SecureByteBlockPtr &outDecryptedMessage
                                                     )
      {
        SecureByteBlockPtr key = UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "push-mailbox:" + inPassphraseID, UseServicesHelper::HashAlgorthm_SHA256);
        SecureByteBlockPtr iv = UseServicesHelper::hash(inSalt, UseServicesHelper::HashAlgorthm_MD5);

        outDecryptedMessage = UseServicesHelper::decrypt(*key, *iv, inOriginalData);
        if (!outDecryptedMessage) {
          ZS_LOG_WARNING(Detail, log("message failed to decrypt"))
          return false;
        }

        String calculatedProof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(*UseServicesHelper::hmacKeyFromPassphrase(inPassphrase), "proof:" + inSalt + ":" + UseServicesHelper::convertToHex(*UseServicesHelper::hash(*outDecryptedMessage))));

        if (validateProof(inDataType, calculatedProof, inProof)) {
          ZS_LOG_WARNING(Detail, log("proof failed to validate thus message failed to decrypt") + ZS_PARAM("proof", inProof) + ZS_PARAM("calculated proof", calculatedProof))
          outDecryptedMessage = SecureByteBlockPtr();
          return false;
        }


        return (bool)outDecryptedMessage;
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::ChannelID ServicePushMailboxSession::pickNextChannel()
      {
        (++mLastChannel);
        if (mLastChannel >= 0x3FFFFFFF) {  // safe wrap around (should not happen)
          mLastChannel = 1;
        }
        return mLastChannel;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePushMailboxSessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServicePushMailboxSessionFactory &IServicePushMailboxSessionFactory::singleton()
      {
        return ServicePushMailboxSessionFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSessionPtr IServicePushMailboxSessionFactory::create(
                                                                             IServicePushMailboxSessionDelegatePtr delegate,
                                                                             IServicePushMailboxSessionTransferDelegatePtr transferDelegate,
                                                                             IServicePushMailboxPtr servicePushMailbox,
                                                                             IAccountPtr account,
                                                                             IServiceNamespaceGrantSessionPtr grantSession,
                                                                             IServiceLockboxSessionPtr lockboxSession
                                                                             )
      {
        if (this) {}
        return ServicePushMailboxSession::create(delegate, transferDelegate, servicePushMailbox, account, grantSession, lockboxSession);
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
    const char *IServicePushMailboxSessionTypes::toString(SessionStates state)
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
    const char *IServicePushMailboxSessionTypes::toString(PushStates state)
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
    IServicePushMailboxSessionTypes::PushStates IServicePushMailboxSessionTypes::toPushState(const char *state)
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
    bool IServicePushMailboxSessionTypes::canSubscribeState(PushStates state)
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
                                                                     IServicePushMailboxSessionTransferDelegatePtr transferDelegate,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IAccountPtr account,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
    {
      return internal::IServicePushMailboxSessionFactory::singleton().create(delegate, transferDelegate, servicePushMailbox, account, grantSession, lockboxSession);
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePushMailboxSession::PushInfo
    #pragma mark

    //-------------------------------------------------------------------------
    IServicePushMailboxSession::PushInfoPtr IServicePushMailboxSession::PushInfo::create(ElementPtr pushInfoEl)
    {
      if (!pushInfoEl) return PushInfoPtr();

      PushInfoPtr result(new PushInfo);
      result->mServiceType = UseMessageHelper::getElementTextAndDecode(pushInfoEl->findFirstChildElement(Definitions::Names::serviceType()));
      result->mValues = pushInfoEl->findFirstChildElement(Definitions::Names::values());
      result->mCustom = pushInfoEl->findFirstChildElement(Definitions::Names::custom());

      if (result->mValues) result->mValues = result->mValues->clone()->toElement();
      if (result->mCustom) result->mCustom = result->mCustom->clone()->toElement();

      return result;
    }

    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSessionTypes::PushInfo::createElement() const
    {
      ElementPtr pushInfoEl = Element::create(Definitions::Names::pushRoot());

      if (mServiceType.hasData()) {
        pushInfoEl->adoptAsLastChild(UseMessageHelper::createElementWithTextAndJSONEncode(Definitions::Names::serviceType(), mServiceType));
      }

      if (mValues) {
        if (Definitions::Names::values() == mValues->getValue()) {
          pushInfoEl->adoptAsLastChild(mValues->clone()->toElement());
        } else {
          ElementPtr valuesEl = Element::create(Definitions::Names::values());
          valuesEl->adoptAsFirstChild(mValues->clone()->toElement());
          pushInfoEl->adoptAsLastChild(valuesEl);
        }
      }

      if (mCustom) {
        if (PushInfo::Definitions::Names::custom() == mCustom->getValue()) {
          pushInfoEl->adoptAsLastChild(mCustom->clone()->toElement());
        } else {
          ElementPtr customEl = Element::create(Definitions::Names::custom());
          customEl->adoptAsFirstChild(mCustom->clone()->toElement());
          pushInfoEl->adoptAsLastChild(customEl);
        }
      }

      return pushInfoEl;
    }

    //-------------------------------------------------------------------------
    bool IServicePushMailboxSessionTypes::PushInfo::hasData() const
    {
      return ((mServiceType.hasData()) ||
              (mValues) ||
              (mCustom));
    }

    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSessionTypes::PushInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("IServicePushMailboxSessionTypes::PushInfo");

      UseServicesHelper::debugAppend(resultEl, "service type", mServiceType);
      UseServicesHelper::debugAppend(resultEl, "values", (bool)mValues);
      UseServicesHelper::debugAppend(resultEl, "custom", (bool)mCustom);

      return resultEl;
    }

    //-------------------------------------------------------------------------
    IServicePushMailboxSessionTypes::PushInfoListPtr IServicePushMailboxSessionTypes::PushInfoList::create(ElementPtr pushesEl)
    {
      if (!pushesEl) return PushInfoListPtr();

      PushInfoListPtr result(new PushInfoList);
      ElementPtr pushEl = pushesEl->findFirstChildElement(PushInfo::Definitions::Names::pushRoot());
      while (pushEl) {
        PushInfoPtr pushInfo = PushInfo::create(pushEl);
        if (pushInfo) result->push_back(*pushInfo);

        pushEl = pushEl->findNextSiblingElement(PushInfo::Definitions::Names::pushRoot());
      }
      return result;
    }

    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSessionTypes::PushInfoList::createElement() const
    {
      ElementPtr resultEl = Element::create(Definitions::Names::pushes());

      for (auto iter = begin(); iter != end(); ++iter)
      {
        const PushInfo &info = (*iter);
        if (!info.hasData()) continue;

        resultEl->adoptAsLastChild(info.createElement());
      }

      return resultEl;
    }

    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSessionTypes::PushInfoList::toDebug() const
    {
      ElementPtr resultEl = Element::create("IServicePushMailboxSessionTypes::PushInfoList");

      for (auto iter = begin(); iter != end(); ++iter)
      {
        const PushInfo &info = (*iter);
        if (!info.hasData()) continue;

        UseServicesHelper::debugAppend(resultEl, info.toDebug());
      }
      return resultEl;
    }

    //-------------------------------------------------------------------------
    ElementPtr IServicePushMailboxSessionTypes::PushMessage::toDebug() const
    {
      ElementPtr resultEl = Element::create("IServicePushMailboxSessionTypes::PushMessage");

      UseServicesHelper::debugAppend(resultEl, "message id", mMessageID);

      UseServicesHelper::debugAppend(resultEl, "message type", mMessageType);
      UseServicesHelper::debugAppend(resultEl, "mime type", mMimeType);
      UseServicesHelper::debugAppend(resultEl, "full message", (bool)mFullMessage);
      UseServicesHelper::debugAppend(resultEl, "full message filename", mFullMessageFileName);

      UseServicesHelper::debugAppend(resultEl, "meta data", mMetaData ? UseServicesHelper::toString(mMetaData) : String());

      UseServicesHelper::debugAppend(resultEl, "push type", mPushType);
      UseServicesHelper::debugAppend(resultEl, mPushInfos.toDebug());
      UseServicesHelper::debugAppend(resultEl, "sent", mSent);
      UseServicesHelper::debugAppend(resultEl, "expires", mExpires);

      UseServicesHelper::debugAppend(resultEl, "from", IPeer::toDebug(mFrom));

      UseServicesHelper::debugAppend(resultEl, "to", mTo ? mTo->size() : 0);
      UseServicesHelper::debugAppend(resultEl, "cc", mCC ? mCC->size() : 0);
      UseServicesHelper::debugAppend(resultEl, "bcc", mBCC ? mBCC->size() : 0);

      UseServicesHelper::debugAppend(resultEl, "push state details", mPushStateDetails.size());

      return resultEl;
    }

  }
}
