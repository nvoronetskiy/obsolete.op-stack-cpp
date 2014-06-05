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

#include <openpeer/stack/internal/stack_MessageMonitor.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/message/Message.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>

#define OPENPEER_STACK_DEFAULT_MESSAGE_MONITOR_TRACK_MESSAGE_ID_TO_OBJECT_ID_TIME_IN_SECONDS (60*2)


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;
      using services::IHelper;

      ZS_DECLARE_TYPEDEF_PTR(IMessageMonitorManagerForMessageMonitor::ForMessageMonitor, ForMessageMonitor)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForAccountFinder
      #pragma mark

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForAccountFinder::notifyMessageSendFailed(message::MessagePtr message)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSendFailed(message);
      }

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForAccountFinder::notifyMessageSenderObjectGone(PUID objectID)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSenderObjectGone(objectID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForAccountPeerLocation
      #pragma mark

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForAccountPeerLocation::notifyMessageSendFailed(message::MessagePtr message)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSendFailed(message);
      }

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForAccountPeerLocation::notifyMessageSenderObjectGone(PUID objectID)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSenderObjectGone(objectID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForPushMailbox
      #pragma mark

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForPushMailbox::notifyMessageSendFailed(message::MessagePtr message)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSendFailed(message);
      }

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForPushMailbox::notifyMessageSenderObjectGone(PUID objectID)
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->notifyMessageSenderObjectGone(objectID);
      }

      //-----------------------------------------------------------------------
      void IMessageMonitorManagerForPushMailbox::trackSentViaObjectID(
                                                                      message::MessagePtr message,
                                                                      SentViaObjectID sentViaObjectID
                                                                      )
      {
        MessageMonitorManagerPtr manager = MessageMonitorManager::singleton();
        if (!manager) return;
        manager->trackSentViaObjectID(message, sentViaObjectID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForMessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      ForMessageMonitorPtr IMessageMonitorManagerForMessageMonitor::singleton()
      {
        return MessageMonitorManager::singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager
      #pragma mark

      //-----------------------------------------------------------------------
      MessageMonitorManager::MessageMonitorManager() :
        MessageQueueAssociator(UseStack::queueStack()),
        SharedRecursiveLock(SharedRecursiveLock::create())
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr MessageMonitorManager::create()
      {
        MessageMonitorManagerPtr pThis(new MessageMonitorManager);
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      MessageMonitorManager::~MessageMonitorManager()
      {
        if(isNoop()) return;
        
        AutoRecursiveLock lock(mLock);
        mThisWeak.reset();

        ZS_LOG_DETAIL(log("destoyed"))
        mMonitors.clear();

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }
      }

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr MessageMonitorManager::convert(ForMessageMonitorPtr object)
      {
        return dynamic_pointer_cast<MessageMonitorManager>(object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => friend MessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr MessageMonitorManager::singleton()
      {
        static SingletonLazySharedPtr<MessageMonitorManager> singleton(IMessageMonitorManagerFactory::singleton().createMessageMonitorManager());
        MessageMonitorManagerPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      MessageMonitorManager::SentViaObjectID MessageMonitorManager::monitorStart(MessageMonitorPtr inMonitor)
      {
        UseMessageMonitorPtr monitor = inMonitor;

        AutoRecursiveLock lock(mLock);

        PUID monitorID = monitor->getID();
        String requestID = monitor->getMonitoredMessageID();

        SentViaObjectID objectID = 0;

        SentViaObjectMap::iterator foundSentVia = mSentViaObjectIDs.find(requestID);
        if (foundSentVia != mSentViaObjectIDs.end()) {
          objectID = (*foundSentVia).second;
        }

        ZS_LOG_TRACE(log("monitoring request ID") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("request id", requestID) + ZS_PARAM("sent via object id", objectID))

        MonitorsMap::iterator found = mMonitors.find(requestID);
        if (found != mMonitors.end()) {
          ZS_LOG_TRACE(log("already found a monitor for the request ID") + ZS_PARAM("request id", requestID))

          MonitorMapPtr monitors = (*found).second;

          (*monitors)[monitorID] = monitor;
          return objectID;
        }

        MonitorMapPtr monitors(new MonitorMap);
        (*monitors)[monitorID] = monitor;

        mMonitors[requestID] = monitors;
        return objectID;
      }

      //-----------------------------------------------------------------------
      void MessageMonitorManager::monitorEnd(MessageMonitor &inMonitor)
      {
        UseMessageMonitor &monitor = inMonitor;

        AutoRecursiveLock lock(mLock);

        PUID monitorID = monitor.getID();
        String requestID = monitor.getMonitoredMessageID();

        ZS_LOG_TRACE(log("remove monitoring of request ID") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", requestID))

        MonitorsMap::iterator found = mMonitors.find(requestID);
        if (found == mMonitors.end()) {
          ZS_LOG_TRACE(log("already removed all monitors for this message id"))
          return;
        }

        MonitorMapPtr monitors = (*found).second;
        MonitorMap::iterator foundMonitor = monitors->find(monitorID);

        if (foundMonitor == monitors->end()) {
          ZS_LOG_TRACE(log("already removed monitor for this message"))
          return;
        }

        monitors->erase(foundMonitor);

        if (monitors->size() > 0) {
          ZS_LOG_TRACE(log("still more monitors active - continue to monitor request ID") + ZS_PARAM("size", monitors->size()))
          return;
        }

        ZS_LOG_TRACE(log("no more monitors active (stop monitoring this message id)"))
        mMonitors.erase(found);
      }

      //-----------------------------------------------------------------------
      bool MessageMonitorManager::handleMessage(message::MessagePtr message)
      {
        AutoRecursiveLock lock(mLock);

        String id = message->messageID();
        MonitorsMap::iterator found = mMonitors.find(id);
        if (found == mMonitors.end()) {
          ZS_LOG_TRACE(log("no monitors watching this message") + ZS_PARAM("message id", id))
          return false;
        }

        MonitorMapPtr monitors = (*found).second;

        bool handled = false;

        for (MonitorMap::iterator iter = monitors->begin(); iter != monitors->end(); )
        {
          MonitorMap::iterator current = iter; ++iter;

          MonitorID monitorID = (*current).first;
          UseMessageMonitorPtr monitor = (*current).second.lock();
          if (!monitor) {
            ZS_LOG_WARNING(Debug, log("monitor is gone") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", id))
            monitors->erase(current);
            continue;
          }

          bool monitorDidHandle = monitor->handleMessage(message);
          handled = handled || monitorDidHandle;

          if (!monitorDidHandle) {
            ZS_LOG_TRACE(log("monitor did not handle request") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", id))
            continue;
          }

          ZS_LOG_TRACE(log("monitor handled request") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", id))
          monitors->erase(current);
        }

        if (monitors->size() < 1) {
          ZS_LOG_TRACE(log("all monitors have completed monitoring this message") + ZS_PARAM("message id", id))
          mMonitors.erase(found);
        }
        
        return handled;
      }

      //-----------------------------------------------------------------------
      void MessageMonitorManager::trackSentViaObjectID(
                                                       message::MessagePtr message,
                                                       SentViaObjectID sentViaObjectID
                                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!message)

        if (0 == sentViaObjectID) return;

        AutoRecursiveLock lock(*this);

        String messageID = message->messageID();

        mSentViaObjectIDs[messageID] = sentViaObjectID;

        Time expires = zsLib::now() + Seconds(OPENPEER_STACK_DEFAULT_MESSAGE_MONITOR_TRACK_MESSAGE_ID_TO_OBJECT_ID_TIME_IN_SECONDS);

        mExpiredSentViaObjectIDs.push_back(MessageExpiryPair(messageID, expires));

        ZS_LOG_TRACE(log("tracking mapping of message ID to object ID (for future monitoring of same message ID)") + ZS_PARAM("message id", messageID) + ZS_PARAM("sent via sender id", sentViaObjectID) + ZS_PARAM("until", expires))

        if (mTimer) return;

        mTimer = Timer::create(mThisWeak.lock(), Seconds(30));
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => IMessageMonitorManagerForAccountFinder
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitorManager::notifyMessageSendFailed(message::MessagePtr message)
      {
        ZS_LOG_WARNING(Detail, log("notify message send failure") + ZS_PARAM("message", message->messageID()))

        AutoRecursiveLock lock(*this);

        mPendingFailures.push_back(message);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      void MessageMonitorManager::notifyMessageSenderObjectGone(PUID objectID)
      {
        ZS_LOG_DEBUG(log("notify sender object gone") + ZS_PARAM("sender object id", objectID))

        AutoRecursiveLock lock(*this);

        mPendingGone.push_back(objectID);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => IMessageMonitorManagerForMessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitorManager::onWake()
      {
        ZS_LOG_TRACE(log("on wake"))

        AutoRecursiveLock lock(*this);

        // scope: handle message send failures
        {
          for (PendingMessageSendFailureMessageList::iterator iterPending = mPendingFailures.begin(); iterPending != mPendingFailures.end(); ++iterPending)
          {
            MessagePtr message = (*iterPending);

            String id = message->messageID();
            MonitorsMap::iterator found = mMonitors.find(id);
            if (found == mMonitors.end()) {
              ZS_LOG_TRACE(log("no monitors watching this message") + ZS_PARAM("message id", id))
              continue;
            }

            MonitorMapPtr monitors = (*found).second;

            for (MonitorMap::iterator iter = monitors->begin(); iter != monitors->end(); )
            {
              MonitorMap::iterator current = iter; ++iter;

              MonitorID monitorID = (*current).first;
              UseMessageMonitorPtr monitor = (*current).second.lock();
              if (!monitor) {
                ZS_LOG_WARNING(Debug, log("monitor is already gone") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", id))
                monitors->erase(current);
                continue;
              }

              monitor->notifySendMessageFailure(message);
            }
          }
          ZS_LOG_TRACE(log("clearing out pending failures") + ZS_PARAM("size", mPendingFailures.size()))
          mPendingFailures.clear();
        }

        // scope: handle sender object gone
        {
          for (PendingSenderObjectGoneList::iterator iterPending = mPendingGone.begin(); iterPending != mPendingGone.end(); ++iterPending)
          {
            SentViaObjectID id = (*iterPending);

            for (MonitorsMap::iterator iterMonitors = mMonitors.begin(); iterMonitors != mMonitors.end(); )
            {
              MonitorsMap::iterator currentMonitors = iterMonitors; ++iterMonitors;

              MonitorMapPtr monitors = (*currentMonitors).second;

              for (MonitorMap::iterator iter = monitors->begin(); iter != monitors->end(); )
              {
                MonitorMap::iterator current = iter; ++iter;

                MonitorID monitorID = (*current).first;
                UseMessageMonitorPtr monitor = (*current).second.lock();
                if (!monitor) {
                  ZS_LOG_WARNING(Debug, log("monitor is already gone") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("gone object id", id))
                  monitors->erase(current);
                  continue;
                }

                monitor->notifySenderObjectGone(id);
              }
            }
          }
          ZS_LOG_TRACE(log("clearing out pending sender object gone notifications") + ZS_PARAM("size", mPendingGone.size()))
          mPendingGone.clear();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void MessageMonitorManager::onTimer(TimerPtr timer)
      {
        ZS_LOG_TRACE(log("on timer"))

        AutoRecursiveLock lock(*this);

        Time tick = zsLib::now();

        while (mExpiredSentViaObjectIDs.size() > 0)
        {
          MessageExpiryPair &info = mExpiredSentViaObjectIDs.front();

          if (tick < info.second) {
            ZS_LOG_TRACE(log("message has not expired yet (thus stop processing more)") + ZS_PARAM("expires", info.second) + ZS_PARAM("now", tick))
            return;
          }

          const String &mesageID = info.first;

          SentViaObjectMap::iterator found = mSentViaObjectIDs.find(mesageID);
          if (found != mSentViaObjectIDs.end()) {
            SentViaObjectID objectID = (*found).second;
            ZS_LOG_DEBUG(log("message ID to sent via object ID mapping is no longer tracking") + ZS_PARAM("message ID", mesageID) + ZS_PARAM("object ID", objectID))
            mSentViaObjectIDs.erase(found);
          } else {
            ZS_LOG_WARNING(Detail, log("message ID to sent via object ID mapping was already removed (sent to multiple senders?)") + ZS_PARAM("message ID", mesageID))
          }

          mExpiredSentViaObjectIDs.pop_front();
        }

        if (mTimer) {
          mTimer->cancel();
          mTimer.reset();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params MessageMonitorManager::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::MessageMonitorManager");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params MessageMonitorManager::slog(const char *message)
      {
        return Log::Params(message, "stack::MessageMonitorManager");
      }
    }
  }
}
