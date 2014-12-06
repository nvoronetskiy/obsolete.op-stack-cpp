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

#include "TestSetup.h"

#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Proxy.h>

#include <openpeer/stack/IStack.h>

#include "config.h"
#include "testing.h"

#include <list>

using zsLib::ULONG;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)


namespace openpeer { namespace stack { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_stack_test) } } }

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      //-----------------------------------------------------------------------
      TestSetup::TestSetup()
      {
      }

      //-----------------------------------------------------------------------
      TestSetup::~TestSetup()
      {
        mThisWeak.reset();

        // wait for shutdown
        {
          ULONG count = 0;
          do
          {
            count = 0;

            count += mThreadDelegate->getTotalUnprocessedMessages();
            count += mThreadStack->getTotalUnprocessedMessages();
            count += mThreadServices->getTotalUnprocessedMessages();
            count += mThreadKeyGeneration->getTotalUnprocessedMessages();
            if (0 != count)
              std::this_thread::yield();
          } while (count > 0);

        }

        TESTING_UNINSTALL_LOGGER()
        zsLib::proxyDump();
        TESTING_EQUAL(zsLib::proxyGetTotalConstructed(), 0);
      }

      //-----------------------------------------------------------------------
      TestSetupPtr TestSetup::create()
      {
        TestSetupPtr pThis(new TestSetup());
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      TestSetupPtr TestSetup::singleton()
      {
        static zsLib::SingletonLazySharedPtr<TestSetup> singleton(TestSetup::create());
        return singleton.singleton();
      }

      //-----------------------------------------------------------------------
      void TestSetup::init()
      {
        mThreadDelegate = MessageQueueThread::createBasic();
        mThreadStack = MessageQueueThread::createBasic();
        mThreadServices = MessageQueueThread::createBasic();
        mThreadKeyGeneration = MessageQueueThread::createBasic();

        IStack::setup(
                      mThreadDelegate,
                      mThreadStack,
                      mThreadServices,
                      mThreadKeyGeneration
                      );
      }

      //-----------------------------------------------------------------------
      size_t TestSetup::getTotalUnprocessedMessages() const
      {
        size_t count = 0;

        count += mThreadDelegate->getTotalUnprocessedMessages();
        count += mThreadStack->getTotalUnprocessedMessages();
        count += mThreadServices->getTotalUnprocessedMessages();
        count += mThreadKeyGeneration->getTotalUnprocessedMessages();

        return count;
      }



    }
  }
}

void doTestSetup()
{
  if (!OPENPEER_STACK_TEST_DO_SETUP_TEST) return;

  TESTING_INSTALL_LOGGER();

  TestSetupPtr setup = TestSetup::singleton();

  std::cout << "COMPLETE:     Setup complete.\n";

  TESTING_UNINSTALL_LOGGER()
}
