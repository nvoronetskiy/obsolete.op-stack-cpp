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

#include <openpeer/stack/message/Message.h>

#include <openpeer/services/IHelper.h>

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
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForMessageMonitor
      #pragma mark

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr IMessageMonitorManagerForMessageMonitor::singleton()
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
      MessageMonitorManager::MessageMonitorManager()
      {
        ZS_LOG_DEBUG(log("created"))
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
        mMonitors.clear();

        ZS_LOG_DEBUG(log("destoyed"))
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
        static MessageMonitorManagerPtr global = IMessageMonitorManagerFactory::singleton().createMessageMonitorManager();
        return global;
      }

      //-----------------------------------------------------------------------
      void MessageMonitorManager::monitorStart(MessageMonitorPtr monitor)
      {
        AutoRecursiveLock lock(mLock);

        PUID monitorID = monitor->forMessageMonitorManager().getID();
        String requestID = monitor->forMessageMonitorManager().getMonitoredMessageID();

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
      void MessageMonitorManager::monitorEnd(MessageMonitor &monitor)
      {
        AutoRecursiveLock lock(mLock);

        PUID monitorID = monitor.forMessageMonitorManager().getID();
        String requestID = monitor.forMessageMonitorManager().getMonitoredMessageID();

        ZS_LOG_TRACE(log("remove monitoring of request ID") + ZS_PARAM("monitor id", monitorID) + ZS_PARAM("request id", requestID))

        MonitorsMap::iterator found = mMonitors.find(requestID);
        ZS_THROW_INVALID_ASSUMPTION_IF(found == mMonitors.end())

        MonitorMapPtr monitors = (*found).second;
        MonitorMap::iterator foundMonitor = monitors->find(monitorID);

        ZS_THROW_INVALID_ASSUMPTION_IF(foundMonitor == monitors->end())

        monitors->erase(foundMonitor);

        if (monitors->size() > 0) {
          ZS_LOG_TRACE(log("still more monitors active - continue to monitor request ID"))
          return;
        }

        ZS_LOG_TRACE(log("still more monitors active - continue to monitor request ID"))
        mMonitors.erase(found);
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
        ElementPtr objectEl = Element::create("MessageMonitorManager");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      bool MessageMonitorManager::handleMessage(message::MessagePtr message)
      {
        AutoRecursiveLock lock(mLock);

        String id = message->messageID();
        MonitorsMap::iterator found = mMonitors.find(id);
        if (found == mMonitors.end()) return false;

        MonitorMapPtr monitors = (*found).second;

        bool handled = false;

        for (MonitorMap::iterator iter = monitors->begin(); iter != monitors->end(); )
        {
          MonitorMap::iterator current = iter; ++iter;

          MessageMonitorPtr monitor = (*current).second.lock();
          if (!monitor) {
            monitors->erase(current);
            continue;
          }

          handled = handled || monitor->forMessageMonitorManager().handleMessage(message);
        }

        return handled;
      }
    }
  }
}
