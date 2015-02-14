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
      }

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr MessageMonitorManager::convert(ForMessageMonitorPtr object)
      {
        return ZS_DYNAMIC_PTR_CAST(MessageMonitorManager, object);
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
      void MessageMonitorManager::monitorStart(MessageMonitorPtr inMonitor)
      {
        UseMessageMonitorPtr monitor = inMonitor;

        AutoRecursiveLock lock(mLock);

        PUID monitorID = monitor->getID();
        String requestID = monitor->getMonitoredMessageID();

        ZS_LOG_TRACE(log("monitoring request ID") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("request id", requestID))

        MonitorsMap::iterator found = mMonitors.find(requestID);
        if (found != mMonitors.end()) {
          ZS_LOG_TRACE(log("already found a monitor for the request ID") + ZS_PARAM("request id", requestID))

          MonitorMapPtr monitors = (*found).second;

          (*monitors)[monitorID] = monitor;
          return;
        }

        MonitorMapPtr monitors(new MonitorMap);
        (*monitors)[monitorID] = monitor;

        mMonitors[requestID] = monitors;
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
      void MessageMonitorManager::handleTimeoutAll(message::MessagePtr message)
      {
        AutoRecursiveLock lock(mLock);

        String id = message->messageID();
        MonitorsMap::iterator found = mMonitors.find(id);
        if (found == mMonitors.end()) {
          ZS_LOG_TRACE(log("no monitors watching this message") + ZS_PARAM("message id", id))
          return;
        }

        MonitorMapPtr monitors = (*found).second;

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

          monitor->handleTimeout();

          ZS_LOG_TRACE(log("monitor timed out") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("message id", id))
          monitors->erase(current);
        }

        ZS_LOG_TRACE(log("all monitors have timed out") + ZS_PARAM("message id", id))
        mMonitors.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager => ITimerDelegate
      #pragma mark

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

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageMonitorManagerFactory &IMessageMonitorManagerFactory::singleton()
      {
        return MessageMonitorManagerFactory::singleton();
      }

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr IMessageMonitorManagerFactory::createMessageMonitorManager()
      {
        if (this) {}
        return MessageMonitorManager::create();
      }

    }
  }
}
