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

#pragma once

#include <openpeer/stack/internal/types.h>

#include <map>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IMessageMonitorForMessageMonitorManager;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerForMessageMonitor
      #pragma mark

      interaction IMessageMonitorManagerForMessageMonitor
      {
        typedef IMessageMonitorManagerForMessageMonitor ForMessageMonitor;
        typedef shared_ptr<ForMessageMonitor> ForMessageMonitorPtr;
        typedef weak_ptr<ForMessageMonitor> ForMessageMonitorWeakPtr;

        static ForMessageMonitorPtr singleton();

        virtual void monitorStart(MessageMonitorPtr requester) = 0;

        virtual void monitorEnd(MessageMonitor &monitor) = 0;

        virtual bool handleMessage(message::MessagePtr message) = 0;

        virtual RecursiveLock &getLock() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark MessageMonitorManager
      #pragma mark

      class MessageMonitorManager : public Noop,
                                    public IMessageMonitorManagerForMessageMonitor
      {
      public:
        friend interaction IMessageMonitorManagerFactory;
        friend interaction IMessageMonitorManagerForMessageMonitor;

        typedef IMessageMonitorForMessageMonitorManager UseMessageMonitor;
        typedef shared_ptr<UseMessageMonitor> UseMessageMonitorPtr;
        typedef weak_ptr<UseMessageMonitor> UseMessageMonitorWeakPtr;

      protected:
        MessageMonitorManager();
        
        MessageMonitorManager(Noop) : Noop(true) {};

        static MessageMonitorManagerPtr create();

      public:
        ~MessageMonitorManager();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitorManager => IMessageMonitorManagerForMessageMonitor
        #pragma mark

        static ForMessageMonitorPtr singleton();

        virtual void monitorStart(MessageMonitorPtr requester);

        virtual void monitorEnd(MessageMonitor &monitor);

        virtual bool handleMessage(message::MessagePtr message);

        // (duplicate) virtual RecursiveLock &getLock() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitorManager => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        virtual RecursiveLock &getLock() const {return mLock;}

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark MessageMonitorManager => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        MessageMonitorManagerWeakPtr mThisWeak;

        typedef PUID MonitorID;
        typedef std::map<MonitorID, UseMessageMonitorWeakPtr> MonitorMap;
        typedef boost::shared_ptr<MonitorMap> MonitorMapPtr;

        typedef String MessageID;
        typedef std::map<MessageID, MonitorMapPtr> MonitorsMap;

        MonitorsMap mMonitors;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerFactory
      #pragma mark

      interaction IMessageMonitorManagerFactory
      {
        static IMessageMonitorManagerFactory &singleton();

        virtual MessageMonitorManagerPtr createMessageMonitorManager();
      };

    }
  }
}
