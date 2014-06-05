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

#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>

#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/bootstrapped-servers/ServersGetRequest.h>
#include <openpeer/stack/message/push-mailbox/AccessRequest.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>
#include <openpeer/services/ITCPMessaging.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>

#include <zsLib/Stringize.h>

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

      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetRequest)
      ZS_DECLARE_USING_PTR(message::push_mailbox, AccessRequest)

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
        mLockbox(lockboxSession),
        mGrantSession(grantSession),

        mInactivityTimeout(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT))),
        mLastActivity(zsLib::now()),

        mDefaultLastRetryDuration(Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS))),
        mLastRetryDuration(mDefaultLastRetryDuration)
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

        IServicePushMailboxSessionDelegatePtr delegate = mSubscriptions.delegate(subscription);

        if (delegate) {
          ServicePushMailboxSessionPtr pThis = mThisWeak.lock();

          if (SessionState_Pending != mCurrentState) {
            delegate->onServicePushMailboxSessionStateChanged(pThis, mCurrentState);
          }
#define WARNING_NOTIFY_ABOUT_FOLDERS_THAT_ARE_MONITORED_AND_HAVE_ALREADY_NOTIFIED 1
#define WARNING_NOTIFY_ABOUT_FOLDERS_THAT_ARE_MONITORED_AND_HAVE_ALREADY_NOTIFIED 2
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
                                                                                    const char *deviceToken,
                                                                                    const char *mappedType,
                                                                                    bool unreadBadge,
                                                                                    const char *sound,
                                                                                    const char *action,
                                                                                    const char *launchImage,
                                                                                    unsigned int priority
                                                                                    )
      {
        return IServicePushMailboxRegisterQueryPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::monitorFolder(const char *folderName)
      {
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
        mLastActivity = zsLib::now();

#define WARNING_MAY_NEED_TO_FORCE_FOLDERS_VERSION_RECHECK 1
#define WARNING_MAY_NEED_TO_FORCE_FOLDERS_VERSION_RECHECK 2

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::markPushMessageRead(const char *messageID)
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::deletePushMessage(const char *messageID)
      {
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
        ZS_LOG_DEBUG(log("servers get result received") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))

        AutoRecursiveLock lock(*this);
        if (monitor != mServersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("servers get result received on obsolete monitor") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))
          return false;
        }

        mServersGetMonitor.reset();

        mPushMailboxServers = result->servers();

        if (mPushMailboxServers.size() < 1) {
          ZS_LOG_ERROR(Detail, log("no push-mailbox servers were returned from servers-get") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))
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
        ZS_LOG_ERROR(Debug, log("servers get error result received") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))

        AutoRecursiveLock lock(*this);
        if (monitor != mServersGetMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("error result received on servers get monitor") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))

        mServersGetMonitor.reset();
        connectionFailure();

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
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
        ZS_LOG_DEBUG(log("access result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))
          return false;
        }

        if (result->peerValidate()) {
          mPeerChallengeID = result->peerChallengeID();
        } else {
          mPeerChallengeID.clear();
        }

        mNamespaceGrantChallengeInfo = result->namespaceGrantChallengeInfo();

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
        ZS_LOG_ERROR(Debug, log("access error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        if (monitor != mAccessMonitor) {
          ZS_LOG_WARNING(Detail, log("error result received on obsolete monitor") + ZS_PARAM("monitor", IMessageMonitor::toDebug(monitor)))
          return false;
        }

        ZS_LOG_ERROR(Detail, log("access request failed") + ZS_PARAM("error", result->errorCode()) + ZS_PARAM("reason", result->errorReason()))

        connectionFailure();

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
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
        ZS_LOG_DEBUG(log("namespace grant challenge validate result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              NamespaceGrantChallengeValidateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("namespace grant challenge validate error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("peer validate result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              PeerValidateResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("peer validate error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("folders get result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              FoldersGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("folders get error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("folder update result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

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
        ZS_LOG_ERROR(Debug, log("folder update error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

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
        ZS_LOG_DEBUG(log("folder get result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              FolderGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("folder get error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("messages data get result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              MessagesDataGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("messages data get error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("messages meta data get result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              MessagesMetaDataGetResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("messages meta data get error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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
        ZS_LOG_DEBUG(log("message update result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

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
        ZS_LOG_ERROR(Debug, log("message update error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

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
        ZS_LOG_DEBUG(log("list fetch result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              ListFetchResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("list fetch error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession => IMessageMonitorResultDelegate<RegisterPushResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorResultReceived(
                                                                         IMessageMonitorPtr monitor,
                                                                         RegisterPushResultPtr result
                                                                         )
      {
        ZS_LOG_DEBUG(log("list fetch result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
      }

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::handleMessageMonitorErrorResultReceived(
                                                                              IMessageMonitorPtr monitor,
                                                                              RegisterPushResultPtr ignore,
                                                                              message::MessageResultPtr result
                                                                              )
      {
        ZS_LOG_ERROR(Debug, log("list fetch error result received") + ZS_PARAM("message ID", monitor->getMonitoredMessageID()))

        AutoRecursiveLock lock(*this);
        return false;
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

        IHelper::debugAppend(resultEl, "subscriptions", mSubscriptions.size());
        IHelper::debugAppend(resultEl, "default subscription", (bool)mDefaultSubscription);

        IHelper::debugAppend(resultEl, "db", (bool)mDB);

        IHelper::debugAppend(resultEl, "state", toString(mCurrentState));

        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));
        IHelper::debugAppend(resultEl, "lockbox", mLockbox ? mLockbox->getID() : 0);

        IHelper::debugAppend(resultEl, "grant session", mGrantSession ? mGrantSession->getID() : 0);
        IHelper::debugAppend(resultEl, "grant query", mGrantQuery ? mGrantQuery->getID() : 0);
        IHelper::debugAppend(resultEl, "grant wait", mGrantWait ? mGrantWait->getID() : 0);

        IHelper::debugAppend(resultEl, "obtained lock", mObtainedLock);

        IHelper::debugAppend(resultEl, "inactivty timeout (s)", mInactivityTimeout);
        IHelper::debugAppend(resultEl, "last activity", mLastActivity);

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
        if (!stepLockboxAccess()) goto post_step;
        if (!stepConnect()) goto post_step;
        if (!stepAccess()) goto post_step;

        if (!stepGrantLockClear()) goto post_step;
        if (!stepGrantChallenge()) goto post_step;

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

        bool shouldConnect = (mLastActivity + mInactivityTimeout > now);

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

        mServersGetMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<ServersGetResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mBootstrappedNetwork), "bootstrapped-servers", "servers-get", request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SERVERS_GET_TIMEOUT_IN_SECONDS)));

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

        if (!mLockboxInfo.mAccessSecret.hasData()) {
          mLockboxInfo = mLockbox->getLockboxInfo();
          if (mLockboxInfo.mAccessSecret.hasData()) {
            ZS_LOG_DEBUG(log("obtained lockbox access secret"))
          }
        }

        if (!mPeerFiles) {
          mPeerFiles = mLockbox->getPeerFiles();
          if (mPeerFiles) {
            ZS_LOG_DEBUG(log("obtained peer files"))
          }
        }

        if ((mLockboxInfo.mAccessSecret.hasData()) &&
            (mPeerFiles)) {
          ZS_LOG_TRACE(log("now have lockbox acess secret and peeer files"))
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


        AccessRequestPtr request = AccessRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());
        request->lockboxInfo(mLockboxInfo);
        request->peerFiles(mPeerFiles);
        request->grantID(mGrantSession->getGrantID());

        mAccessMonitor = sendRequest(IMessageMonitorResultDelegate<AccessResult>::convert(mThisWeak.lock()), request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_ACCESS_TIMEOUT_IN_SECONDS)));
//        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-access", request);


//        if (mLockboxAccessMonitor) {
//          ZS_LOG_TRACE(log("waiting for lockbox access monitor to complete"))
//          return false;
//        }
//
//        if (mLockboxInfo.mAccessToken.hasData()) {
//          ZS_LOG_TRACE(log("already have a lockbox access key"))
//          return true;
//        }
//
//        setState(SessionState_Pending);
//
//        LockboxAccessRequestPtr request = LockboxAccessRequest::create();
//        request->domain(mBootstrappedNetwork->getDomain());
//
//        if (mLoginIdentity) {
//          IdentityInfo identityInfo = mLoginIdentity->getIdentityInfo();
//          request->identityInfo(identityInfo);
//
//          LockboxInfo lockboxInfo = mLoginIdentity->getLockboxInfo();
//          mLockboxInfo.mergeFrom(lockboxInfo);
//
//          if (!IHelper::isValidDomain(mLockboxInfo.mDomain)) {
//            ZS_LOG_DEBUG(log("domain from identity is invalid, reseting to default domain") + ZS_PARAM("domain", mLockboxInfo.mDomain))
//
//            mLockboxInfo.mDomain = mBootstrappedNetwork->getDomain();
//
//            // account/keying information must also be incorrect if domain is not valid
//            mLockboxInfo.mKey.reset();
//          }
//
//          if (mBootstrappedNetwork->getDomain() != mLockboxInfo.mDomain) {
//            ZS_LOG_DEBUG(log("default bootstrapper is not to be used for this lockbox as an altenative lockbox must be used thus preparing replacement bootstrapper"))
//
//            mBootstrappedNetwork = BootstrappedNetwork::convert(IBootstrappedNetwork::prepare(mLockboxInfo.mDomain, mThisWeak.lock()));
//            return false;
//          }
//
//          if (mForceNewAccount) {
//            ZS_LOG_DEBUG(log("forcing a new lockbox account to be created for the identity"))
//            get(mForceNewAccount) = false;
//            mLockboxInfo.mKey.reset();
//          }
//
//          if (!mLockboxInfo.mKey) {
//            mLockboxInfo.mAccountID.clear();
//            mLockboxInfo.mKey.reset();
//            mLockboxInfo.mHash.clear();
//            mLockboxInfo.mResetFlag = true;
//
//            mLockboxInfo.mKey = IHelper::random(32);
//
//            ZS_LOG_DEBUG(log("created new lockbox key") + ZS_PARAM("key", IHelper::convertToBase64(*mLockboxInfo.mKey)))
//          }
//
//          ZS_THROW_BAD_STATE_IF(!mLockboxInfo.mKey)
//
//          ZS_LOG_DEBUG(log("creating lockbox key hash") + ZS_PARAM("key", IHelper::convertToBase64(*mLockboxInfo.mKey)))
//          mLockboxInfo.mHash = IHelper::convertToHex(*IHelper::hash(*mLockboxInfo.mKey));
//        }
//
//        mLockboxInfo.mDomain = mBootstrappedNetwork->getDomain();
//
//        request->grantID(mGrantSession->getGrantID());
//        request->lockboxInfo(mLockboxInfo);
//
//        NamespaceInfoMap namespaces;
//        getNamespaces(namespaces);
//        request->namespaceURLs(namespaces);
//
//        mLockboxAccessMonitor = IMessageMonitor::monitor(IMessageMonitorResultDelegate<LockboxAccessResult>::convert(mThisWeak.lock()), request, Seconds(OPENPEER_STACK_SERVICE_LOCKBOX_TIMEOUT_IN_SECONDS));
//        mBootstrappedNetwork->sendServiceMessage("identity-lockbox", "lockbox-access", request);

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

        setState(SessionState_ShuttingDown);

        if (mTCPMessaging) {
          mTCPMessaging->shutdown();
        }

        mGracefulShutdownReference.reset();

        if (mRetryTimer) {
          mRetryTimer->cancel();
          mRetryTimer.reset();
        }

        if (mServersGetMonitor) {
          mServersGetMonitor->cancel();
          mServersGetMonitor.reset();
        }

        if (mAccessMonitor) {
          mAccessMonitor->cancel();
          mAccessMonitor.reset();
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

        if (mRetryTimer) {
          mRetryTimer->cancel();
          mRetryTimer.reset();
        }

        mRetryTimer = Timer::create(mThisWeak.lock(), retryDuration, false);

        mServerIP = IPAddress();

        if (mAccessMonitor) {
          mAccessMonitor->cancel();
          mAccessMonitor.reset();
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
                                                                ) const
      {
        IMessageMonitorPtr monitor = IMessageMonitor::monitor(delegate, requestMessage, timeout);
        if (!monitor) {
          ZS_LOG_WARNING(Detail, log("failed to create monitor"))
          return IMessageMonitorPtr();
        }

        bool result = send(requestMessage);
        if (!result) {
          // notify that the message requester failed to send the message...
          UseMessageMonitorManager::notifyMessageSendFailed(requestMessage);
          return monitor;
        }

        ZS_LOG_DEBUG(log("request successfully created"))
        return monitor;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::handleChanged(ChangedNotifyPtr notify)
      {
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
