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

#include <openpeer/stack/internal/stack_Cache.h>

#include <openpeer/stack/message/Message.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/services/ISettings.h>
#include <openpeer/services/IHelper.h>

#include <easySQLite/SqlCommon.h>
#include <easySQLite/SqlField.h>
#include <easySQLite/SqlTable.h>
#include <easySQLite/SqlRecord.h>

#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>
#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Log.h>


#define OPENPEER_STACK_CACHE_DATABASE_VERSION 1

#define OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_COOKIE "cookie"
#define OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_EXPIRES "expires"
#define OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_VALUE "value"

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using namespace zsLib::XML;
      using services::IHelper;

      ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseSettings)
      ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
      ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
      ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
      ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (table definitions)
      #pragma mark

      //-----------------------------------------------------------------------
      SqlField TableDefinition_Version[] =
      {
        SqlField(sql::FIELD_KEY),
        SqlField("version", sql::type_int, sql::flag_not_null),
        SqlField(sql::DEFINITION_END)
      };

      //-----------------------------------------------------------------------
      SqlField TableDefinition_Cache[] =
      {
        SqlField(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_COOKIE, sql::type_text, sql::flag_primary_key | sql::flag_not_null),
        SqlField(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_EXPIRES, sql::type_int, sql::flag_not_null),
        SqlField(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_VALUE, sql::type_text, sql::flag_not_null),
        SqlField(sql::DEFINITION_END)
      };

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
        outRawBuffer = SecureByteBlockPtr();

        String str = ICache::fetch(cookieNamePath);
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
        std::unique_ptr<char[]> output = generator->write(el);

        String result = (const char *)output.get();

        ICache::store(cookieNamePath, expires, result);
      }

      //-----------------------------------------------------------------------
      String ICacheForServices::fetch(const char *cookieNamePath)
      {
        return ICache::fetch(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      void ICacheForServices::store(
                                    const char *cookieNamePath,
                                    Time expires,
                                    const char *str
                                    )
      {
        ICache::store(cookieNamePath, expires, str);
      }

      //-----------------------------------------------------------------------
      void ICacheForServices::clear(const char *cookieNamePath)
      {
        ICache::clear(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      Log::Params ICacheForServices::slog(const char *message)
      {
        return Log::Params(message, "stack::ICacheForServices");
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
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      Cache::~Cache()
      {
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      CachePtr Cache::convert(ICachePtr cache)
      {
        return ZS_DYNAMIC_PTR_CAST(Cache, cache);
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
      CachePtr Cache::singleton()
      {
        static SingletonLazySharedPtr<Cache> singleton(Cache::create());
        CachePtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, internal::Cache::slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      void Cache::setup(ICacheDelegatePtr delegate)
      {
        AutoRecursiveLock lock(mLock);
        mDelegate = delegate;

        ZS_LOG_DEBUG(log("setup called") + ZS_PARAM("has delegate", (bool)delegate))

        services::ICache::setup(delegate ? mThisWeak.lock() : services::ICacheDelegatePtr());
      }

      //-----------------------------------------------------------------------
      String Cache::fetch(const char *cookieNamePath) const
      {
        if (!cookieNamePath) return String();

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;

          prepareDB();
          String result;
          if (fetchFromDatabase(cookieNamePath, result)) return result;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot fetch cookie)") + ZS_PARAM("cookie name", cookieNamePath))
          return String();
        }

        return delegate->fetch(cookieNamePath);
      }

      //-----------------------------------------------------------------------
      void Cache::store(
                        const char *cookieNamePath,
                        Time expires,
                        const char *str
                        )
      {
        if (!cookieNamePath) return;
        if (!str) {
          clear(cookieNamePath);
          return;
        }
        if (!(*str)) {
          clear(cookieNamePath);
          return;
        }

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;

          prepareDB();
          if (storeInDatabase(cookieNamePath, expires, str)) return;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot store cookie)") + ZS_PARAM("cookie name", cookieNamePath) + ZS_PARAM("expires", expires) + ZS_PARAM("value", str))
          return;
        }

        delegate->store(cookieNamePath, expires, str);
      }

      //-----------------------------------------------------------------------
      void Cache::clear(const char *cookieNamePath)
      {
        if (!cookieNamePath) return;

        ICacheDelegatePtr delegate;

        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;

          prepareDB();
          if (clearFromDatabase(cookieNamePath)) return;
        }

        if (!delegate) {
          ZS_LOG_WARNING(Debug, log("no cache installed (thus cannot clear cookie)") + ZS_PARAM("cookie name", cookieNamePath))
          return;
        }

        delegate->clear(cookieNamePath);
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
      Log::Params Cache::slog(const char *message)
      {
        return Log::Params(message, "stack::Cache");
      }

      //-----------------------------------------------------------------------
      void Cache::prepareDB() const
      {
        if (mDelegate) return;
        if (mDB) return;

        String path = UseSettings::getString(OPENPEER_STACK_SETTING_CACHE_DATABASE_PATH);

        if (path.length() < 1) {
          ZS_LOG_FATAL(Basic, log("missing setting") + ZS_PARAM("name", OPENPEER_STACK_SETTING_CACHE_DATABASE_PATH))
          return;
        }

        char endChar = path[path.length()-1];
        if (('/' != endChar) &&
            ('\\' != endChar)) {

          endChar = '/';

          String::size_type found1 = path.find('/');
          String::size_type found2 = path.find('\\');

          if (String::npos == found1) {
            if (String::npos != found2) endChar = '\\';
          } else {
            if ((String::npos != found2) &&
                (found2 < found1)) endChar = '\\';
          }

          path += endChar;
        }

        String fileName = UseSettings::getString(OPENPEER_STACK_SETTING_CACHE_DATABASE_FILENAME);

        if (fileName.length() < 1) {
          ZS_LOG_FATAL(Basic, log("missing setting") + ZS_PARAM("name", OPENPEER_STACK_SETTING_CACHE_DATABASE_FILENAME))
          return;
        }

        path += fileName;

        try {
          mDB = SQLDatabasePtr(new SQLDatabase);

          mDB->open(path);

          SqlTable versionTable(mDB->getHandle(), "version", TableDefinition_Version);

          if (!versionTable.exists()) {

            versionTable.create();

            SqlRecord record(versionTable.fields());
            record.setInteger("version", OPENPEER_STACK_CACHE_DATABASE_VERSION);
            versionTable.addRecord(&record);
          }

          versionTable.open();

          SqlRecord *versionRecord = versionTable.getTopRecord();

          auto databaseVersion = versionRecord->getValue("version")->asInteger();

          bool flushAllTables = false;

          if (OPENPEER_STACK_CACHE_DATABASE_VERSION != databaseVersion) {
            ZS_LOG_WARNING(Detail, log("version mismatch thus flushing cache") + ZS_PARAM("db version", databaseVersion) + ZS_PARAM("current version", OPENPEER_STACK_CACHE_DATABASE_VERSION))

            flushAllTables = true;

            versionRecord->setInteger("version", OPENPEER_STACK_CACHE_DATABASE_VERSION);
            versionTable.updateRecord(versionRecord);
          }

          SqlTable cacheTable(mDB->getHandle(), "cache", TableDefinition_Cache);

          if (cacheTable.exists()) {
            if (flushAllTables) {
              ZS_LOG_DETAIL(log("cache database is being upgraded"))
              cacheTable.remove();
              cacheTable.create();
              ZS_LOG_DETAIL(log("cache database is upgraded"))
            }
          } else {
            ZS_LOG_DETAIL(log("creating cache table"))
            cacheTable.create();
          }

          ZS_LOG_DEBUG(log("cleaning out expired records"))

          cacheTable.deleteRecords("expires = 0");

          Time now = zsLib::now();
          auto sinceEpoch = zsLib::timeSinceEpoch<Seconds>(now);

          cacheTable.deleteRecords("expires < " + string(sinceEpoch.count()));

        } catch (SQLException &e) {
          ZS_LOG_ERROR(Detail, log("database preparation failure") + ZS_PARAM("message", e.msg()))
          mDB.reset();
        }
      }

      //-----------------------------------------------------------------------
      bool Cache::fetchFromDatabase(
                                    const char *cookieNamePath,
                                    String &outValue
                                    ) const
      {
        if (!mDB) return false;

        String cookieName(cookieNamePath);

        try {
          SqlTable cacheTable(mDB->getHandle(), "cache", TableDefinition_Cache);

          if (!cacheTable.open("cookie = \'" + SqlEscape(cookieName) + "\'")) {
            ZS_LOG_WARNING(Detail, log("unable to lookup cookie database"))
            return false;
          }

          SqlRecord *cookieRecord = cacheTable.getTopRecord();
          if (!cookieRecord) {
            ZS_LOG_TRACE(log("no database cache entry for cookie") + ZS_PARAM("cookie name", cookieName))
            outValue.clear();
            return true;
          }

          auto expires = cookieRecord->getValue(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_EXPIRES)->asInteger();
          if (0 != expires) {
            Time expiryTime = zsLib::timeSinceEpoch<Seconds>(Seconds(expires));
            if (zsLib::now() > expiryTime) {
              ZS_LOG_TRACE(log("cookie already expired") + ZS_PARAM("cookie name", cookieName))
              outValue.clear();
              return true;
            }
          }

          outValue = cookieRecord->getValue(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_VALUE)->asString();

          ZS_LOG_TRACE(log("cookie fetched") + ZS_PARAM("cookie name", cookieName) + ZS_PARAM("value", outValue))

        } catch(SQLException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      bool Cache::storeInDatabase(
                                  const char *cookieNamePath,
                                  Time expires,
                                  const char *str
                                  )
      {
        if (!mDB) return false;

        String cookieName(cookieNamePath);
        String data(str);

        try {
          SqlTable cacheTable(mDB->getHandle(), "cache", TableDefinition_Cache);

          if (!cacheTable.open("cookie = \'" + SqlEscape(cookieName) + "\'")) {
            ZS_LOG_WARNING(Detail, log("unable to lookup cookie database"))
            return false;
          }

          SqlRecord *cookieRecord = cacheTable.getTopRecord();
          if (!cookieRecord) {
            ZS_LOG_TRACE(log("adding new database cache entry for cookie") + ZS_PARAM("cookie name", cookieName) + ZS_PARAM("expires", expires) + ZS_PARAM("value", str))

            SqlRecord record(cacheTable.fields());
            record.setString(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_COOKIE, cookieName);
            record.setInteger(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_EXPIRES, Time() == expires ? 0 : zsLib::timeSinceEpoch<Seconds>(expires).count());
            record.setString(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_VALUE, data);
            cacheTable.addRecord(&record);
            return true;
          }

          ZS_LOG_TRACE(log("updating database cache entry for cookie") + ZS_PARAM("cookie name", cookieName) + ZS_PARAM("expires", expires) + ZS_PARAM("value", str))

          cookieRecord->setInteger(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_EXPIRES, Time() == expires ? 0 : zsLib::timeSinceEpoch<Seconds>(expires).count());
          cookieRecord->setString(OPENPEER_STACK_CACHE_TABLE_CACHE_FIELD_VALUE, data);

          cacheTable.updateRecord(cookieRecord);

        } catch(SQLException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
          return false;
        }
        
        return true;
      }

      //-----------------------------------------------------------------------
      bool Cache::clearFromDatabase(const char *cookieNamePath)
      {
        if (!mDB) return false;

        try {
          SqlTable cacheTable(mDB->getHandle(), "cache", TableDefinition_Cache);

          if (!cacheTable.deleteRecords("cookie = \'" + SqlEscape(String(cookieNamePath)) + "\'")) {
            ZS_LOG_WARNING(Detail, log("unable to delete from cookie database"))
            return false;
          }

          ZS_LOG_TRACE(log("cookie removed") + ZS_PARAM("cookie name", cookieNamePath))

        } catch(SQLException &e) {
          ZS_LOG_ERROR(Detail, log("database failure") + ZS_PARAM("message", e.msg()))
          return false;
        }

        return true;
      }

      //-----------------------------------------------------------------------
      String Cache::SqlEscape(const String &input)
      {
        String result(input);
        result.replaceAll("\'", "\'\'");
        return result;
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
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->setup(delegate);
    }

    //-------------------------------------------------------------------------
    String ICache::fetch(const char *cookieNamePath)
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return String();
      return singleton->fetch(cookieNamePath);
    }

    //-------------------------------------------------------------------------
    void ICache::store(
                       const char *cookieNamePath,
                       Time expires,
                       const char *str
                       )
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->store(cookieNamePath, expires, str);
    }

    //-------------------------------------------------------------------------
    void ICache::clear(const char *cookieNamePath)
    {
      internal::CachePtr singleton = internal::Cache::singleton();
      if (!singleton) return;
      singleton->clear(cookieNamePath);
    }

  }
}
