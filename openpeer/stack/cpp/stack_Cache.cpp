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

#include <openpeer/stack/internal/stack_Cache.h>

#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Log.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using namespace zsLib::XML;
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ICacheForServices
      #pragma mark

      //-----------------------------------------------------------------------
      MessagePtr ICacheForServices::getFromCache(
                                                 const char *cookieNamePath,
                                                 message::MessagePtr originalMessage,
                                                 SecureByteBlockPtr &outRawBuffer,
                                                 IMessageSourcePtr source
                                                 )
      {
        ICachePtr singleton = ICache::singleton();

        outRawBuffer = SecureByteBlockPtr();

        String str = singleton->fetch(cookieNamePath);
        if (str.isEmpty()) return MessagePtr();

        DocumentPtr doc = Document::createFromParsedJSON(str);
        if (!doc) return MessagePtr();

        ElementPtr rootEl = doc->getFirstChildElement();
        if (!rootEl) return MessagePtr();

        String messageID = originalMessage->messageID();
        String appID = originalMessage->appID();

        if (messageID.hasData()) {
          rootEl->setAttribute("id", messageID);
        }
        if (appID.hasData()) {
          rootEl->setAttribute("appid", appID);
        }

        Time tick = zsLib::now();
        IMessageHelper::setAttributeTimestamp(rootEl, tick);

        MessagePtr message = Message::create(rootEl, source);
        if (!message) return MessagePtr();

        outRawBuffer = IHelper::convertToBuffer(str);
        return message;
      }

      //-----------------------------------------------------------------------
      void ICacheForServices::storeMessage(
                                           const char *cookieNamePath,
                                           Time expires,
                                           message::MessagePtr message
                                           )
      {
        if (!message) return;

        ElementPtr el = message->creationElement();
        if (!el) return;

        GeneratorPtr generator = Generator::createJSONGenerator();
        boost::shared_array<char> output = generator->write(el);

        String result = (const char *)output.get();

        ICachePtr singleton = ICache::singleton();

        singleton->store(cookieNamePath, expires, result);
      }

      //-----------------------------------------------------------------------
      String ICacheForServices::fetch(const char *cookieNamePath)
      {
        return ICache::singleton()->fetch(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      void ICacheForServices::store(
                                    const char *cookieNamePath,
                                    Time expires,
                                    const char *str
                                    )
      {
        ICache::singleton()->store(cookieNamePath, expires, str);
      }

      //-----------------------------------------------------------------------
      void ICacheForServices::clear(const char *cookieNamePath)
      {
        ICache::singleton()->clear(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache
      #pragma mark

      //-----------------------------------------------------------------------
      Cache::Cache()
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      Cache::~Cache()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::convert(ICachePtr cache)
      {
        return dynamic_pointer_cast<Cache>(cache);
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::create()
      {
        CachePtr pThis(new Cache());
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => ICache
      #pragma mark

      //-----------------------------------------------------------------------
      void Cache::setup(ICacheDelegatePtr delegate)
      {
        singleton()->actualSetup(delegate);
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::singleton()
      {
        static CachePtr singleton = Cache::create();
        return singleton;
      }

      //-----------------------------------------------------------------------
      String Cache::fetch(const char *cookieNamePath) const
      {
        if (!cookieNamePath) return String();

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (delegate) return delegate->fetch(cookieNamePath);
        return String();
      }

      //-----------------------------------------------------------------------
      void Cache::store(
                        const char *cookieNamePath,
                        Time expires,
                        const char *str
                        )
      {
        if (!cookieNamePath) return;
        if (!str) clear(cookieNamePath);
        if (!(*str)) clear(cookieNamePath);

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (delegate) return delegate->store(cookieNamePath, expires, str);
      }

      //-----------------------------------------------------------------------
      void Cache::clear(const char *cookieNamePath)
      {
        if (!cookieNamePath) return;

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (delegate) return delegate->clear(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => services::ICacheDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Cache => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Cache::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::Cache");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      void Cache::actualSetup(ICacheDelegatePtr delegate)
      {
        AutoRecursiveLock lock(mLock);
        mDelegate = delegate;

        ZS_LOG_DEBUG(log("setup called") + ZS_PARAM("has delegate", (bool)delegate))

        services::ICache::setup(delegate ? mThisWeak.lock() : services::ICacheDelegatePtr());
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ICache
    #pragma mark

    //-------------------------------------------------------------------------
    void ICache::setup(ICacheDelegatePtr delegate)
    {
      internal::Cache::setup(delegate);
    }

    //-------------------------------------------------------------------------
    ICachePtr ICache::singleton()
    {
      return internal::Cache::singleton();
    }

  }
}
