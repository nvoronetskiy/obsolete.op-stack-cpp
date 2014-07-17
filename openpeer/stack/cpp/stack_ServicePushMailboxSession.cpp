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

#include <openpeer/stack/message/bootstrapped-servers/ServersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/AccessRequest.h>
#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateRequest.h>
#include <openpeer/stack/message/push-mailbox/PeerValidateRequest.h>
#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
#include <openpeer/stack/message/push-mailbox/FoldersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/FolderGetRequest.h>
#include <openpeer/stack/message/push-mailbox/MessagesMetaDataGetRequest.h>
#include <openpeer/stack/message/push-mailbox/MessagesDataGetRequest.h>
#include <openpeer/stack/message/push-mailbox/ListFetchRequest.h>
#include <openpeer/stack/message/push-mailbox/ChangedNotify.h>

#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>
#include <openpeer/services/ITCPMessaging.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Stringize.h>

#define OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME ".sent"


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_USING_PTR(openpeer::services, IBackgrounding)

      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, AccessRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, NamespaceGrantChallengeValidateRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, PeerValidateRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, FoldersGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, FolderGetRequest)
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
        size_t listLength = strlen("list:");
        if (0 != strncmp("list:", uri, listLength)) return String();

        return uri.substr(listLength);
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
                                                           ServiceNamespaceGrantSessionPtr grantSession,
                                                           ServiceLockboxSessionPtr lockboxSession
                                                           ) :
        zsLib::MessageQueueAssociator(queue),

        SharedRecursiveLock(SharedRecursiveLock::create()),

        mDB(databaseDelegate),

        mCurrentState(SessionState_Pending),

        mBootstrappedNetwork(network),

        mSentViaObjectID(mID),

        mLockbox(lockboxSession),
        mGrantSession(grantSession),

        mRequiresConnection(true),
        mInactivityTimeout(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT))),
        mLastActivity(zsLib::now()),

        mDefaultLastRetryDuration(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS))),
        mLastRetryDuration(mDefaultLastRetryDuration),

        mRefreshFolders(true),

        mRefreshFoldersNeedingUpdate(true),

        mRefreshMessagesNeedingUpdate(true),

        mRefreshMessagesNeedingData(true),

        mRefreshListsNeedingDownload(true)
      {
        ZS_LOG_BASIC(log("created"))

        mDefaultSubscription = mSubscriptions.subscribe(delegate, IStackForInternal::queueDelegate());
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

        mMonitoredFolders[OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME] = OPENPEER_STACK_PUSH_MAILBOX_SENT_FOLDER_NAME;

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
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!databaseDelegate) // must not be NULL

        ServicePushMailboxSessionPtr pThis(new ServicePushMailboxSession(IStackForInternal::queueStack(), BootstrappedNetwork::convert(servicePushMailbox), delegate, databaseDelegate, ServiceNamespaceGrantSession::convert(grantSession), ServiceLockboxSession::convert(lockboxSession)));
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
        ZS_LOG_DETAIL(log("subscribing to socket state"))

        AutoRecursiveLock lock(*this);
        if (!originalDelegate) return mDefaultSubscription;

        IServicePushMailboxSessionSubscriptionPtr subscription = mSubscriptions.subscribe(originalDelegate);

        IServicePushMailboxSessionDelegatePtr delegate = mSubscriptions.delegate(subscription, true);

        if (delegate) {
          ServicePushMailboxSessionPtr pThis = mThisWeak.lock();

          if (SessionState_Pending != mCurrentState) {
            delegate->onServicePushMailboxSessionStateChanged(pThis, mCurrentState);
          }

          for (FolderNameMap::const_iterator iter = mMonitoredFolders.begin(); iter != mMonitoredFolders.end(); ++iter)
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
                                                                                    unsigned int priority
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

        mMonitoredFolders[folderName] = folderName;

        // need to recheck which folders need an update
        mRefreshFoldersNeedingUpdate = true;

        // since we just started monitoring, indicate that the folder has changed
        mSubscriptions.delegate()->onServicePushMailboxSessionFolderChanged(mThisWeak.lock(), folderName);

        // wake up and refresh the folders list to start the monitor process on this folder
        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::getFolderMessageUpdates(
                                                              const char *inFolder,
                                                              String inLastVersionDownloaded,
                                                              String &outUpdatedToVersion,
                                                              PushMessageListPtr &outMessagesAdded,
                                                              MessageIDListPtr &outMessagesRemoved
                                                              )
      {
        outUpdatedToVersion = String();
        outMessagesAdded = PushMessageListPtr();
        outMessagesRemoved = MessageIDListPtr();

        return false;
      }

      //-----------------------------------------------------------------------
      IServicePushMailboxSendQueryPtr ServicePushMailboxSession::sendMessage(
                                                                             IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                             const PeerOrIdentityList &to,
                                                                             const PeerOrIdentityList &cc,
                                                                             const PeerOrIdentityList &bcc,
                                                                             const PushMessage &message,
                                                                             bool copyToSentFolder
                                                                             )
      {
        return IServicePushMailboxSendQueryPtr();
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
      void ServicePushMailboxSession::markPushMessageRead(const char *messageID)
      {
        AutoRecursiveLock lock(*this);
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::deletePushMessage(const char *messageID)
      {
        AutoRecursiveLock lock(*this);
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
        ZS_LOG_DEBUG(log("notified query complete") + ZS_PARAM("query", query.getID()))

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
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
            mDB->removeFolder(name);
            continue;
          }

          if (info.mRenamed) {
            ZS_LOG_DEBUG(log("folder was renamed (removing old folder)") + ZS_PARAM("folder name", info.mName) + ZS_PARAM("new name", info.mRenamed))
            mDB->removeFolder(info.mName);
            name = info.mRenamed;
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
          mDB->setLastDownloadedVersionForFolders(String());
          mLastActivity = zsLib::now();

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

        FolderGetMonitorList::iterator found = std::find(mFolderGetMonitors.begin(), mFolderGetMonitors.end(), monitor);
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
          ZS_LOG_WARNING(Detail, log("received information about folder update but folder versions conflict") + originalFolderInfo.toDebug() + ZS_PARAM("expecting", updateInfo.mInfo.mDownloadedVersion))
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

              mDB->addRemovedVersionedMessageEntryIfMessageNotRemoved(updateInfo.mInfo.mIndex, message.mID);
              mDB->removeMessageFromFolder(updateInfo.mInfo.mIndex, message.mID);
              break;
            }
          }
        }

        mDB->updateFolderDownloadInfo(updateInfo.mInfo.mIndex, folderInfo.mVersion, folderInfo.mUpdateNext);

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

        FolderGetMonitorList::iterator found = std::find(mFolderGetMonitors.begin(), mFolderGetMonitors.end(), monitor);
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

          mDB->notifyMessageFailedToDownloadData(processedInfo.mInfo.mIndex);

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
        if (monitor != mMessageMetaDataGetMonitor) {
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
                             info.mPushInfo.mType,
                             info.mPushInfo.mValues,
                             info.mPushInfo.mCustom,
                             info.mTime,
                             info.mExpires,
                             info.mLength
                             );

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

          mDB->notifyMessageFailedToUpdate(processedInfo.mInfo.mIndex);

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

        AutoRecursiveLock lock(*this);
        if (monitor != mMessageMetaDataGetMonitor) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("failed to get message metadata") + IMessageMonitor::toDebug(monitor) + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

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

        AutoRecursiveLock lock(*this);
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

        IHelper::debugAppend(resultEl, "refresh folders", mRefreshFolders);
        IHelper::debugAppend(resultEl, "monitored folders", mMonitoredFolders.size());
        IHelper::debugAppend(resultEl, "folders get monitor", mFoldersGetMonitor ? mFoldersGetMonitor->getID() : 0);

        IHelper::debugAppend(resultEl, "next folder update timer", mNextFoldersUpdateTimer ? mNextFoldersUpdateTimer->getID() : 0);

        IHelper::debugAppend(resultEl, "refresh folders needing update", mRefreshFoldersNeedingUpdate);
        IHelper::debugAppend(resultEl, "folders needing update", mFoldersNeedingUpdate.size());
        IHelper::debugAppend(resultEl, "folder get monitors", mFolderGetMonitors.size());

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

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
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

        if (!stepCheckMessagesNeedingUpdate()) goto post_step;
        if (!stepMessagesMetaDataGet()) goto post_step;

        if (!stepCheckMessagesNeedingData()) goto post_step;
        if (!stepMessagesDataGet()) goto post_step;

        if (!stepCheckListNeedingDownload()) goto post_step;
        if (!stepListFetch()) goto post_step;

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

        bool shouldConnect = ((mLastActivity + mInactivityTimeout > now) ||
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

        mWireStream = ITransportStream::create(ITransportStreamWriterDelegatePtr(), mThisWeak.lock());

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
        ZS_LOG_TRACE(log("step register queries"))

        if (mRegisterQueries.size() < 1) return true;

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
        if (mFoldersGetMonitor) {
          ZS_LOG_TRACE(log("will not update checked folders while folders get is outstanding"))
          return true;
        }

        typedef IServicePushMailboxDatabaseAbstractionDelegate::FolderNeedingUpdateList FolderNeedingUpdateList;

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
        if (mFoldersGetMonitor) {
          ZS_LOG_TRACE(log("will not update checked folders while folders get is outstanding"))
          return true;
        }

        for (FolderUpdateMap::iterator iter_doNotUse = mFoldersNeedingUpdate.begin(); iter_doNotUse != mFoldersNeedingUpdate.end(); )
        {
          FolderUpdateMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          String name = (*current).first;
          ProcessedFolderNeedingUpdateInfo &info = (*current).second;

          if (info.mSentRequest) {
            ZS_LOG_TRACE(log("folder already being updated (or completed)") + ZS_PARAM("folder name", name))
            if (mFolderGetMonitors.size() < 1) { // all monitors are complete thus can purge this folder from the list
              ZS_LOG_DEBUG(log("purging folder needing update that is already completed") + ZS_PARAM("folder name", name))
              mFoldersNeedingUpdate.erase(current);
            }
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
      bool ServicePushMailboxSession::stepCheckMessagesNeedingUpdate()
      {
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingUpdateList MessageNeedingUpdateList;
        typedef IServicePushMailboxDatabaseAbstractionDelegate::MessageNeedingUpdateInfo MessageNeedingUpdateInfo;

        if ((mFoldersGetMonitor) ||
            (mFolderGetMonitors.size() > 0)) {
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
          mRefreshFoldersNeedingUpdate = true;
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
        if ((mFoldersGetMonitor) ||
            (mFolderGetMonitors.size() > 0)) {
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

        if ((mFoldersGetMonitor) ||
            (mFolderGetMonitors.size() > 0)) {
          ZS_LOG_TRACE(log("will not download message while folders are updating"))
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
        if ((mFoldersGetMonitor) ||
            (mFolderGetMonitors.size() > 0)) {
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

        mDB->getBatchOfListNeedingDownload(downloadList);

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
      bool ServicePushMailboxSession::stepBackgroundingReady()
      {
        if (!mBackgroundingEnabled) {
          ZS_LOG_TRACE(log("backgrounding not enabled"))
          return true;
        }

        bool backgroundingReady = (mRegisterQueries.size() < 1) &&
                                  (!mFoldersGetMonitor);

#define WARNING_ADD_CHECKS_TO_SEE_IF_BACKGROUNDING_READY 1
#define WARNING_ADD_CHECKS_TO_SEE_IF_BACKGROUNDING_READY 2

        if (!backgroundingReady) {
          ZS_LOG_TRACE(log("backgrounding is not ready yet") + toDebug())
          return true;
        }

        ZS_LOG_DEBUG(log("backgrounding is now ready"))

        mBackgroundingNotifier.reset();

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
      void ServicePushMailboxSession::cancel()
      {
        if (isShutdown()) {
          ZS_LOG_DEBUG(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("shutting down"))
        setState(SessionState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        if (mGracefulShutdownReference) {

#define WARNING_MAKE_SURE_ALL_MESSAGES_ARE_UPLOADED 1
#define WARNING_MAKE_SURE_ALL_MESSAGES_ARE_UPLOADED 2

#define WARNING_MAKE_SURE_ALL_MESSAGES_ARE_DOWNLOADED 1
#define WARNING_MAKE_SURE_ALL_MESSAGES_ARE_DOWNLOADED 2

          if (mTCPMessaging) {
            mTCPMessaging->shutdown();

            if (ITCPMessaging::SessionState_Shutdown != mTCPMessaging->getState()) {
              ZS_LOG_DEBUG(log("waiting for TCP messaging to shutdown"))
              return;
            }
          }
        }

        setState(SessionState_Shutdown);

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
        }

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

        if (mAccessMonitor) {
          mAccessMonitor->cancel();
          mAccessMonitor.reset();
        }

        mPeerURIFromAccessResult.clear();

        if (mGrantQuery) {
          mGrantQuery->cancel();
          mGrantQuery.reset();
        }

        if (mGrantMonitor) {
          mGrantMonitor->cancel();
          mGrantMonitor.reset();
        }

        if (mPeerValidateMonitor) {
          mPeerValidateMonitor->cancel();
          mPeerValidateMonitor.reset();
        }

        if (mFoldersGetMonitor) {
          mFoldersGetMonitor->cancel();
          mFoldersGetMonitor.reset();
        }

        for (FolderGetMonitorList::iterator iter = mFolderGetMonitors.begin(); iter != mFolderGetMonitors.end(); ++iter) {
          IMessageMonitorPtr monitor = (*iter);

          monitor->cancel();
          monitor.reset();
        }

        mRefreshFoldersNeedingUpdate = mRefreshFoldersNeedingUpdate || (mFoldersNeedingUpdate.size() > 0);
        mFoldersNeedingUpdate.clear();

        if (mMessageMetaDataGetMonitor) {
          mMessageMetaDataGetMonitor->cancel();
          mMessageMetaDataGetMonitor.reset();
        }

        mRefreshMessagesNeedingUpdate = mRefreshMessagesNeedingUpdate || (mMessagesNeedingUpdate.size() > 0);
        mMessagesNeedingUpdate.clear();

        if (mMessageDataGetMonitor) {
          mMessageDataGetMonitor->cancel();
          mMessageDataGetMonitor.reset();
        }

        mRefreshMessagesNeedingData = mRefreshMessagesNeedingData || (mMessagesNeedingData.size() > 0);
        mMessagesNeedingData.clear();
        mMessagesNeedingDataChannels.clear();
        mPendingChannelData.clear();

        if (mListFetchMonitor) {
          mListFetchMonitor->cancel();
          mListFetchMonitor.reset();
        }

        mRefreshListsNeedingDownload = true;
        mListsNeedingDownload.clear();

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
          mTCPMessaging.reset();
        }

#define WARNING_CLEAR_OUT_SESSION_STATE 1
#define WARNING_CLEAR_OUT_SESSION_STATE 2
        
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
            continue;
          }

          mRefreshFoldersNeedingUpdate = true;

          ZS_LOG_DEBUG(log("folder needs update") + ZS_PARAM("name", info.mName) + ZS_PARAM("version", info.mVersion))

          mDB->updateFolderIfExits(info.mName, info.mVersion);
        }

        for (PushMessageInfoList::const_iterator iter = messages.begin(); iter != messages.end(); ++iter)
        {
          const PushMessageInfo &info = (*iter);
          if (info.mDisposition == PushMessageInfo::Disposition_Remove) {
            ZS_LOG_DEBUG(log("message is removed") + ZS_PARAM("message id", info.mID))

            mDB->notifyMessageRemovedFromServer(info.mID);
            continue;
          }

          ZS_LOG_DEBUG(log("message needs meta data update") + ZS_PARAM("message id", info.mID) + ZS_PARAM("server version", info.mVersion))

          mDB->updateMessageIfExists(info.mID, info.mVersion);
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

          ZS_LOG_DEBUG(log("all database record with received for message"))

          mDB->updateMessageData(info.mInfo.mIndex, info.mBuffer->BytePtr(), info.mBuffer->SizeInBytes());

          mMessagesNeedingDataChannels.erase(foundChannel);
          mMessagesNeedingData.erase(foundData);
        }
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
    ElementPtr IServicePushMailboxSession::toDebug(IServicePushMailboxSessionPtr session)
    {
      return internal::ServicePushMailboxSession::toDebug(session);
    }

    //-------------------------------------------------------------------------
    IServicePushMailboxSessionPtr IServicePushMailboxSession::create(
                                                                     IServicePushMailboxSessionDelegatePtr delegate,
                                                                     IServicePushMailboxDatabaseAbstractionDelegatePtr databaseDelegate,
                                                                     IServicePushMailboxPtr servicePushMailbox,
                                                                     IServiceNamespaceGrantSessionPtr grantSession,
                                                                     IServiceLockboxSessionPtr lockboxSession
                                                                     )
    {
      return internal::IServicePushMailboxSessionFactory::singleton().create(delegate, databaseDelegate, servicePushMailbox, grantSession, lockboxSession);
    }


  }
}
