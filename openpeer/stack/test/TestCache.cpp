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

#include "TestCache.h"
#include "TestSetup.h"

#include <zsLib/MessageQueueThread.h>
#include <zsLib/Exception.h>
#include <zsLib/Proxy.h>

#include <openpeer/stack/IStack.h>
#include <openpeer/stack/ICache.h>
#include <openpeer/stack/ISettings.h>

#include "config.h"
#include "testing.h"

#include <list>

using zsLib::ULONG;

ZS_DECLARE_USING_PTR(openpeer::stack, IStack)
ZS_DECLARE_USING_PTR(openpeer::stack, ICache)
ZS_DECLARE_USING_PTR(openpeer::stack, ISettings)

ZS_DECLARE_USING_PTR(openpeer::stack::test, TestCache)
ZS_DECLARE_USING_PTR(openpeer::stack::test, TestSetup)


namespace openpeer { namespace stack { namespace test { ZS_DECLARE_SUBSYSTEM(openpeer_stack_test) } } }

namespace openpeer
{
  namespace stack
  {
    namespace test
    {
      //-----------------------------------------------------------------------
      TestCache::TestCache()
      {
      }

      //-----------------------------------------------------------------------
      TestCache::~TestCache()
      {
        mThisWeak.reset();
      }

      //-----------------------------------------------------------------------
      TestCachePtr TestCache::create()
      {
        TestCachePtr pThis(new TestCache());
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void TestCache::init()
      {
        ISettings::applyDefaults();
        ISettings::setString("openpeer/stack/cache-database-path", "/tmp");

      }

      //-----------------------------------------------------------------------
      void TestCache::testStore()
      {
        {
          String result = ICache::fetch("cookie1");
          TESTING_EQUAL(result, "")
        }
        {
          String result = ICache::fetch("cookie2");
          TESTING_EQUAL(result, "")
        }
        {
          String result = ICache::fetch("cookie'a'");
          TESTING_EQUAL(result, "")
        }

        {
          ICache::store("cookie1", Time(), "foobar");
          String result = ICache::fetch("cookie1");
          TESTING_EQUAL(result, "foobar")
        }

        {
          Time now = zsLib::now();

          ICache::store("cookie2", now + Seconds(3), "'3' seconds");
          {
            String result = ICache::fetch("cookie2");
            TESTING_EQUAL(result, "'3' seconds")
          }
          std::this_thread::sleep_for(zsLib::Seconds(1));
          {
            String result = ICache::fetch("cookie2");
            TESTING_EQUAL(result, "'3' seconds")
          }
          std::this_thread::sleep_for(zsLib::Seconds(4));
          {
            String result = ICache::fetch("cookie2");
            TESTING_EQUAL(result, "")
          }
        }

        {
          Time now = zsLib::now();

          {
            String result = ICache::fetch("long term");
            if (result.hasData()) {
              TESTING_EQUAL(result, "2 hours")
            }
          }
          ICache::store("long term", now + Seconds(60*60*2), "2 hours");
          {
            String result = ICache::fetch("long term");
            TESTING_EQUAL(result, "2 hours")
          }
          std::this_thread::sleep_for(zsLib::Seconds(1));
          {
            String result = ICache::fetch("long term");
            TESTING_EQUAL(result, "2 hours")
          }
          std::this_thread::sleep_for(zsLib::Seconds(4));
          {
            String result = ICache::fetch("long term");
            TESTING_EQUAL(result, "2 hours")
          }
        }

        {
          ICache::store("cookie'a'", Time(), "foo''bar");
          String result = ICache::fetch("cookie'a'");
          TESTING_EQUAL(result, "foo''bar")
        }

        {
          ICache::store("cookie3", Time(), "foobar");
          String result = ICache::fetch("cookie3");
          TESTING_EQUAL(result, "foobar")
          ICache::clear("cookie3");

          String result2 = ICache::fetch("cookie3");
          TESTING_EQUAL(result2, "")
        }

        {
          ICache::clear("cookie4");

          String result = ICache::fetch("cookie4");
          TESTING_EQUAL(result, "")
        }

        {
          ICache::store("cookie5", Time(), "coolness");
          String result = ICache::fetch("cookie5");
          TESTING_EQUAL(result, "coolness")
        }

        {
          ICache::store("cookie5", zsLib::now() + Seconds(5), "where's the beef?");
          String result = ICache::fetch("cookie5");
          TESTING_EQUAL(result, "where's the beef?")
        }

        {
          ICache::store("cookie5", Time(), "one hit wonder");
          String result = ICache::fetch("cookie5");
          TESTING_EQUAL(result, "one hit wonder")
        }
      }

    }
  }
}

void doTestCache()
{
  if (!OPENPEER_STACK_TEST_DO_CACHE_TEST) return;

  TESTING_INSTALL_LOGGER();

  TestSetupPtr setup = TestSetup::singleton();

  TestCachePtr testObject = TestCache::create();

  testObject->testStore();

  std::cout << "COMPLETE:     Cache complete.\n";

//  TESTING_CHECK(testObject->mNetworkDone)
//  TESTING_CHECK(testObject->mNetwork->isPreparationComplete())
//  TESTING_CHECK(testObject->mNetwork->wasSuccessful())


  TESTING_UNINSTALL_LOGGER()
}
