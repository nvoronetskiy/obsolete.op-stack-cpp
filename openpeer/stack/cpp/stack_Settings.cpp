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

#include <openpeer/stack/internal/stack_Settings.h>
#include <openpeer/stack/internal/stack.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/XML.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    ZS_DECLARE_TYPEDEF_PTR(services::ISettings, UseServicesSettings)

    namespace internal
    {
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ISettingsForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void ISettingsForStack::verifyRequiredSettingsOnce() throw (ISettings::InvalidUsage)
      {
        SettingsPtr singleton = Settings::singleton();
        if (!singleton) return;
        singleton->verifyRequiredSettingsOnce();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Settings
      #pragma mark

      //-----------------------------------------------------------------------
      Settings::Settings()
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      Settings::~Settings()
      {
        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      SettingsPtr Settings::convert(ISettingsPtr settings)
      {
        return ZS_DYNAMIC_PTR_CAST(Settings, settings);
      }

      //-----------------------------------------------------------------------
      SettingsPtr Settings::create()
      {
        SettingsPtr pThis(new Settings());
        pThis->mThisWeak = pThis;
        return pThis;
      }

      //-----------------------------------------------------------------------
      SettingsPtr Settings::singleton()
      {
        static SingletonLazySharedPtr<Settings> singleton(Settings::create());
        SettingsPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Settings => ISettings
      #pragma mark

      //-----------------------------------------------------------------------
      void Settings::setup(ISettingsDelegatePtr delegate)
      {
        {
          AutoRecursiveLock lock(mLock);
          mDelegate = delegate;

          ZS_LOG_DEBUG(log("setup called") + ZS_PARAM("has delegate", (bool)delegate))
        }

        services::ISettings::setup(delegate ? mThisWeak.lock() : services::ISettingsDelegatePtr());
      }

      //-----------------------------------------------------------------------
      void Settings::applyDefaults()
      {
        services::ISettings::applyDefaults();

        setString(OPENPEER_STACK_SETTING_STACK_STACK_THREAD_PRIORITY, "normal");
        setString(OPENPEER_STACK_SETTING_STACK_KEY_GENERATION_THREAD_PRIORITY, "low");

        setBool(OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_OVER_INSECURE_HTTP, false);
        setBool(OPENPEER_STACK_SETTING_BOOTSTRAPPER_SERVICE_FORCE_WELL_KNOWN_USING_POST, false);
        setBool(OPENPEER_STACK_SETTING_BOOTSTREPPER_SKIP_SIGNATURE_VALIDATION, false);

        setBool(OPENPEER_STACK_SETTING_ACCOUNT_SHUTDOWN_ON_ICE_SOCKET_FAILURE, false);
        setBool(OPENPEER_STACK_SETTING_ACCOUNT_PEER_LOCATION_DEBUG_FORCE_MESSAGES_OVER_RELAY, false);

        setString(OPENPEER_STACK_SETTING_ACCOUNT_DEFAULT_KEY_DOMAIN, "https://meta.openpeer.org/dh/modp/4096");

        setUInt(OPENPEER_STACK_SETTING_BACKGROUNDING_ACCOUNT_PHASE, 3);
        setUInt(OPENPEER_STACK_SETTING_BACKGROUNDING_PUSH_MAILBOX_PHASE, 3);

        setUInt(OPENPEER_STACK_SETTING_FINDER_TOTAL_SERVERS_TO_GET, 2);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_REQUEST_TIMEOUT_IN_SECONDS, 60);
        setUInt(OPENPEER_STACK_SETTING_FINDER_MAX_CLIENT_SESSION_KEEP_ALIVE_IN_SECONDS, 0);
        setUInt(OPENPEER_STACK_SETTING_FINDER_CONNECTION_MUST_SEND_PING_IF_NO_SEND_ACTIVITY_IN_SECONDS, 25);

        setUInt(OPENPEER_STACK_SETTING_SERVICE_PEER_LOOKUP_PEER_FILES_GET_TIMEOUT_IN_SECONDS, 60);

        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_TOTAL_SERVERS_TO_GET, 2);

        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_INACTIVITY_TIMEOUT, 600);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_RETRY_CONNECTION_IN_SECONDS, 2);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_RETRY_CONNECTION_IN_SECONDS, 4*(60*60));

        setString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DEFAULT_DH_KEY_DOMAIN, "https://meta.openpeer.org/dh/modp/4096");

        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SENDING_KEY_ACTIVE_UNTIL_IN_SECONDS, 60*60*24*30);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_SENDING_KEY_EXPIRES_AFTER_IN_SECONDS, 60*60*24*150);

        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_TIME_IN_SECONDS, 60*60*2);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_UPLOAD_CHUNK_SIZE_IN_BYTES, 10*1024);
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_TO_MEMORY_SIZE_IN_BYTES, 64*1024);
        setUInt(OPENPEER_STACK_SETTING_PUBLICATION_MOVE_DOCUMENT_TO_CACHE_TIME, 120);

        setString(OPENPEER_STACK_SETTING_SERVERMESSAGING_DEFAULT_KEY_DOMAIN, "https://meta.openpeer.org/dh/modp/4096");

        setString(OPENPEER_STACK_SETTING_CACHE_DATABASE_FILENAME, "cache.db");

        setString(OPENPEER_STACK_SETTING_PUSH_MAILBOX_DATABASE_FILE_POSTFIX, "_push_mailbox.db");
        setUInt(OPENPEER_STACK_SETTING_PUSH_MAILBOX_MAX_MESSAGE_DOWNLOAD_SIZE_IN_BYTES, 0);

        {
          AutoRecursiveLock lock(mLock);
          mAppliedDefaults = true;
        }
      }

      //-----------------------------------------------------------------------
      void Settings::verifyRequiredSettings() throw (InvalidUsage)
      {
        applyDefaultsIfNoDelegatePresent();

        // check any required settings here:
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_APPLICATION_NAME);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_APPLICATION_IMAGE_URL);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_APPLICATION_URL);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_APPLICATION_AUTHORIZATION_ID);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_USER_AGENT);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_DEVICE_ID);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_OS);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_SYSTEM);
        UseServicesSettings::verifySettingExists(OPENPEER_COMMON_SETTING_INSTANCE_ID);

        UseServicesSettings::verifyRequiredSettings();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Settings => ISettingsForStack
      #pragma mark

      //-----------------------------------------------------------------------
      void Settings::verifyRequiredSettingsOnce() throw (InvalidUsage)
      {
        {
          AutoRecursiveLock lock(mLock);
          if (mVerifiedOnce) return;
        }

        verifyRequiredSettings();

        {
          AutoRecursiveLock lock(mLock);
          mVerifiedOnce = true;
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Settings => ISettingsDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      String Settings::getString(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getString(key);
        return delegate->getString(key);
      }

      //-----------------------------------------------------------------------
      LONG Settings::getInt(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getInt(key);
        return delegate->getInt(key);
      }

      //-----------------------------------------------------------------------
      ULONG Settings::getUInt(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getUInt(key);
        return delegate->getUInt(key);
      }

      //-----------------------------------------------------------------------
      bool Settings::getBool(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getBool(key);
        return delegate->getBool(key);
      }

      //-----------------------------------------------------------------------
      float Settings::getFloat(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getFloat(key);
        return delegate->getFloat(key);
      }

      //-----------------------------------------------------------------------
      double Settings::getDouble(const char *key) const
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) return UseServicesSettings::getDouble(key);
        return delegate->getDouble(key);
      }

      //-----------------------------------------------------------------------
      void Settings::setString(
                               const char *key,
                               const char *value
                               )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setString(key, value);
          return;
        }
        delegate->setString(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::setInt(
                            const char *key,
                            LONG value
                            )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setInt(key, value);
          return;
        }
        delegate->setInt(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::setUInt(
                             const char *key,
                             ULONG value
                             )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setUInt(key, value);
          return;
        }
        delegate->setUInt(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::setBool(
                             const char *key,
                             bool value
                             )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setBool(key, value);
          return;
        }
        delegate->setBool(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::setFloat(
                              const char *key,
                              float value
                              )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setFloat(key, value);
          return;
        }
        delegate->setFloat(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::setDouble(
                               const char *key,
                               double value
                               )
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::setDouble(key, value);
          return;
        }
        delegate->setDouble(key, value);
      }

      //-----------------------------------------------------------------------
      void Settings::clear(const char *key)
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;
        }

        if (!delegate) {
          UseServicesSettings::clear(key);
          return;
        }
        delegate->clear(key);
      }

      //-----------------------------------------------------------------------
      void Settings::clearAll()
      {
        ISettingsDelegatePtr delegate;
        {
          AutoRecursiveLock lock(mLock);
          delegate = mDelegate;

          mVerifiedOnce = false;
        }

        if (!delegate) {
          UseServicesSettings::clearAll();
          return;
        }
        delegate->clearAll();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Settings => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Settings::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("stack::Settings");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params Settings::slog(const char *message)
      {
        return Log::Params(message, "stack::Settings");
      }

      //-----------------------------------------------------------------------
      void Settings::applyDefaultsIfNoDelegatePresent()
      {
        {
          AutoRecursiveLock lock(mLock);
          if (mDelegate) return;

          if (mAppliedDefaults) return;
        }

        ZS_LOG_WARNING(Detail, log("To prevent issues with missing settings, the default settings are being applied. Recommend installing a settings delegate to fetch settings required from a externally."))

        applyDefaults();
      }
      
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark ISettings
    #pragma mark

    //-------------------------------------------------------------------------
    void ISettings::setup(ISettingsDelegatePtr delegate)
    {
      internal::SettingsPtr singleton = internal::Settings::singleton();
      if (!singleton) return;
      singleton->setup(delegate);
    }

    //-------------------------------------------------------------------------
    void ISettings::setString(
                              const char *key,
                              const char *value
                              )
    {
      return UseServicesSettings::setString(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::setInt(
                           const char *key,
                           LONG value
                           )
    {
      return UseServicesSettings::setInt(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::setUInt(
                            const char *key,
                            ULONG value
                            )
    {
      return UseServicesSettings::setUInt(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::setBool(
                            const char *key,
                            bool value
                            )
    {
      return UseServicesSettings::setBool(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::setFloat(
                             const char *key,
                             float value
                             )
    {
      return UseServicesSettings::setFloat(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::setDouble(
                              const char *key,
                              double value
                              )
    {
      return UseServicesSettings::setDouble(key, value);
    }

    //-------------------------------------------------------------------------
    void ISettings::clear(const char *key)
    {
      return UseServicesSettings::clear(key);
    }
    
    //-------------------------------------------------------------------------
    bool ISettings::apply(const char *jsonSettings)
    {
      return UseServicesSettings::apply(jsonSettings);
    }

    //-------------------------------------------------------------------------
    void ISettings::applyDefaults()
    {
      internal::SettingsPtr singleton = internal::Settings::singleton();
      if (!singleton) return;
      singleton->applyDefaults();
    }

    //-------------------------------------------------------------------------
    void ISettings::clearAll()
    {
      UseServicesSettings::clearAll();
    }

    //-------------------------------------------------------------------------
    void ISettings::verifySettingExists(const char *key) throw (InvalidUsage)
    {
      UseServicesSettings::verifySettingExists(key);
    }

    //-------------------------------------------------------------------------
    void ISettings::verifyRequiredSettings() throw (InvalidUsage)
    {
      internal::SettingsPtr singleton = internal::Settings::singleton();
      if (!singleton) return;
      singleton->verifyRequiredSettings();
    }
  }
}
