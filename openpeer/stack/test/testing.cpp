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

#include "testing.h"
#include "config.h"

#include <zsLib/types.h>
#include <zsLib/helpers.h>
#include <openpeer/services/ILogger.h>

#include <iostream>

typedef openpeer::services::ILogger ILogger;

void doTestSetup();
void doTestCache();
void doTestLocationDB();
void doTestILocationDatabase();
void doTestP2PDatabaseMessage();
void doTestPushMailboxDB();
void doTestStack();
void doTestLockboxSession();
void doTestAccount();

namespace openpeer { namespace stack { namespace test { ZS_IMPLEMENT_SUBSYSTEM(openpeer_stack_test) } } }


namespace Testing
{
  std::atomic_uint &getGlobalPassedVar()
  {
    static std::atomic_uint value {};
    return value;
  }
  
  std::atomic_uint &getGlobalFailedVar()
  {
    static std::atomic_uint value {};
    return value;
  }
  
  void passed()
  {
    ++(getGlobalPassedVar());
  }
  void failed()
  {
    ++(getGlobalFailedVar());
  }
  
  void installLogger()
  {
    std::cout << "INSTALLING LOGGER...\n\n";
    ILogger::setLogLevel("zsLib", zsLib::Log::Trace);
    ILogger::setLogLevel("sqlite", zsLib::Log::Trace);
    ILogger::setLogLevel("openpeer_services", zsLib::Log::Trace);
    ILogger::setLogLevel("openpeer_stack", zsLib::Log::Trace);
    ILogger::setLogLevel("openpeer_stack_message", zsLib::Log::Trace);
    ILogger::setLogLevel(zsLib::Log::Trace);

    if (OPENPEER_STACK_TEST_USE_STDOUT_LOGGING) {
      ILogger::installStdOutLogger(false);
    }
    
    if (OPENPEER_STACK_TEST_USE_FIFO_LOGGING) {
      ILogger::installFileLogger(OPENPEER_STACK_TEST_FIFO_LOGGING_FILE, true);
    }
    
    if (OPENPEER_STACK_TEST_USE_TELNET_LOGGING) {
      ILogger::installTelnetLogger(OPENPEER_STACK_TEST_TELNET_LOGGING_PORT, 60, true);
    }
    
    std::cout << "INSTALLED LOGGER...\n\n";
  }
  
  void uninstallLogger()
  {
    std::cout << "REMOVING LOGGER...\n\n";
    
    if (OPENPEER_STACK_TEST_USE_STDOUT_LOGGING) {
      ILogger::uninstallStdOutLogger();
    }
    if (OPENPEER_STACK_TEST_USE_FIFO_LOGGING) {
      ILogger::uninstallFileLogger();
    }
    if (OPENPEER_STACK_TEST_USE_TELNET_LOGGING) {
      ILogger::uninstallTelnetLogger();
    }
    
    std::cout << "REMOVED LOGGER...\n\n";
  }
  
  void output()
  {
    std::cout << "PASSED:       [" << Testing::getGlobalPassedVar() << "]\n";
    if (0 != Testing::getGlobalFailedVar()) {
      std::cout << "***FAILED***: [" << Testing::getGlobalFailedVar() << "]\n";
    }
  }
  
  void runAllTests()
  {
    doTestSetup();
    doTestCache();
    doTestLocationDB();
    doTestILocationDatabase();
    doTestP2PDatabaseMessage();
    doTestPushMailboxDB();
    doTestStack();
//    doTestLockboxSession();
//    doTestAccount();
  }
}
