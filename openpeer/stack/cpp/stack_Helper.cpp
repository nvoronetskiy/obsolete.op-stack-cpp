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

#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>

#include <easySQLite/SqlField.h>
#include <easySQLite/SqlFieldSet.h>
#include <easySQLite/SqlValue.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

using namespace zsLib::XML;

namespace openpeer
{
  namespace stack
  {
    ZS_DECLARE_TYPEDEF_PTR(services::IHelper, UseServicesHelper)
    ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
    ZS_DECLARE_TYPEDEF_PTR(sql::FieldSet, SqlFieldSet)
    ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Helpers
      #pragma mark

      namespace sqlite_logger
      {
        ZS_IMPLEMENT_SUBSYSTEM(sqlite)
        ZS_DECLARE_SUBSYSTEM(sqlite)

        class SqliteLog {
        private:
          static Log::Params slog(const char *message)
          {
            return Log::Params(message, "sqlite");
          }

          static void sqliteLog(void *pArg, int iErrCode, const char *zMsg) {
            int primaryResultCode = (0xFF & iErrCode);
            int extendedResultCode = (0xFFFFFF00 & iErrCode);

            switch (primaryResultCode) {
              case SQLITE_OK:
              case SQLITE_NOTICE: {
                ZS_LOG_DEBUG(slog(zMsg) + ZS_PARAM("code", iErrCode) + ZS_PARAM("primary", primaryResultCode) + ZS_PARAM("extended", extendedResultCode))
                break;
              }
              case SQLITE_WARNING: {
                ZS_LOG_WARNING(Debug, slog(zMsg) + ZS_PARAM("code", iErrCode) + ZS_PARAM("primary", primaryResultCode) + ZS_PARAM("extended", extendedResultCode))
                break;
              }
              default: {
                ZS_LOG_ERROR(Debug, slog(zMsg) + ZS_PARAM("code", iErrCode) + ZS_PARAM("primary", primaryResultCode) + ZS_PARAM("extended", extendedResultCode))
                break;
              }
            }
          }

        public:
          SqliteLog()
          {
            sqlite3_config(SQLITE_CONFIG_LOG, sqliteLog, NULL);
          }

          static void prepare()
          {
            zsLib::Singleton<SqliteLog> singleton;
          }
        };
      }

      //-----------------------------------------------------------------------
      Log::Level getJavaScriptLogLevel();

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Helper
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr Helper::getSignatureInfo(
                                          ElementPtr signedEl,
                                          ElementPtr *outSignatureEl,
                                          String *outPeerURI,
                                          String *outKeyID,
                                          String *outKeyDomain,
                                          String *outService,
                                          String *outFullPublicKey,
                                          String *outFingerprint
                                          )
      {
        ElementPtr signatureEl;
        signedEl = UseServicesHelper::getSignatureInfo(signedEl, &signatureEl, outFullPublicKey, outFingerprint);

        if (outSignatureEl) {
          *outSignatureEl = signatureEl;
        }

        if (!signatureEl) {
          ZS_LOG_WARNING(Detail, log("could not find signature element"))
          return ElementPtr();
        }

        ElementPtr keyEl = signatureEl->findFirstChildElement("key");
        if (keyEl) {
          if (outPeerURI) {
            *outPeerURI = IMessageHelper::getElementTextAndDecode(keyEl->findFirstChildElement("uri"));
          }
          if (outKeyID) {
            *outKeyID = IMessageHelper::getAttributeID(keyEl);
          }
          if (outKeyDomain) {
            *outKeyDomain = IMessageHelper::getElementTextAndDecode(keyEl->findFirstChildElement("domain"));
          }
          if (outService) {
            *outService = IMessageHelper::getElementTextAndDecode(keyEl->findFirstChildElement("service"));
          }
          if (outFullPublicKey) {
            *outFullPublicKey = IMessageHelper::getElementTextAndDecode(keyEl->findFirstChildElement("x509Data"));
          }
          if (outFingerprint) {
            *outFingerprint = IMessageHelper::getElementTextAndDecode(keyEl->findFirstChildElement("fingerprint"));
          }
        }

        if (outSignatureEl) {
          *outSignatureEl = signatureEl;
        }
        return signedEl;
      }

      //-----------------------------------------------------------------------
      void Helper::convert(
                           const CandidateList &input,
                           IICESocket::CandidateList &output
                           )
      {
        for (CandidateList::const_iterator iter = input.begin(); iter != input.end(); ++iter)
        {
          output.push_back(*iter);
        }
      }

      //-----------------------------------------------------------------------
      void Helper::convert(
                           const IICESocket::CandidateList &input,
                           CandidateList &output
                           )
      {
        for (IICESocket::CandidateList::const_iterator iter = input.begin(); iter != input.end(); ++iter)
        {
          output.push_back(*iter);
        }
      }

      //-----------------------------------------------------------------------
      String Helper::splitEncrypt(
                                  SecureByteBlock &key,
                                  SecureByteBlock &value
                                  )
      {
        String salt = UseServicesHelper::randomString(UseServicesHelper::getHashDigestSize(UseServicesHelper::HashAlgorthm_MD5)*8/5);

        SecureByteBlockPtr iv = UseServicesHelper::hash(salt, UseServicesHelper::HashAlgorthm_MD5);
        String base64 = UseServicesHelper::convertToBase64(*UseServicesHelper::encrypt(key, *iv, value));

        SecureByteBlockPtr proofHash = UseServicesHelper::hash(value);
        String proof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(key, "proof:" + salt + ":" + UseServicesHelper::convertToHex(*UseServicesHelper::hash(value))));

        return salt + ":" + proof + ":" + base64;
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Helper::splitDecrypt(
                                              SecureByteBlock &key,
                                              const char *saltAndProofAndBase64EncodedData
                                              )
      {
        if (!saltAndProofAndBase64EncodedData) return SecureByteBlockPtr();

        UseServicesHelper::SplitMap split;
        UseServicesHelper::split(saltAndProofAndBase64EncodedData, split, ':');

        if (split.size() != 3) {
          ZS_LOG_WARNING(Detail, log("failed to decode data (does not have salt:proof:base64 pattern") + ZS_PARAM("value", saltAndProofAndBase64EncodedData))
          return SecureByteBlockPtr();
        }

        const String &salt = split[0];
        const String &proof = split[1];
        const String &value = split[2];

        SecureByteBlockPtr iv = UseServicesHelper::hash(salt, UseServicesHelper::HashAlgorthm_MD5);

        SecureByteBlockPtr dataToConvert = UseServicesHelper::convertFromBase64(value);
        if (UseServicesHelper::isEmpty(dataToConvert)) {
          ZS_LOG_WARNING(Detail, log("failed to decode data from base64") + ZS_PARAM("value", value))
          return SecureByteBlockPtr();
        }

        SecureByteBlockPtr decryptedData = UseServicesHelper::decrypt(key, *iv, *dataToConvert);

        SecureByteBlockPtr proofHash = UseServicesHelper::hash(*decryptedData);
        String calculatedProof = UseServicesHelper::convertToHex(*UseServicesHelper::hmac(key, "proof:" + salt + ":" + UseServicesHelper::convertToHex(*UseServicesHelper::hash(*decryptedData))));

        if (calculatedProof != proof) {
          ZS_LOG_WARNING(Detail, log("decryption proof failure") + ZS_PARAM("proof", proof) + ZS_PARAM("calculated", calculatedProof))
          return SecureByteBlockPtr();
        }

        return decryptedData;
      }

      //-------------------------------------------------------------------------
      String Helper::appendPath(
                                const char *inPath,
                                const char *fileName
                                )
      {
        String path(inPath);

        if (path.length() < 1) return String(fileName);

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

        return path + String(fileName);
      }

      //-----------------------------------------------------------------------
      zsLib::Log::Severity Helper::toSeverity(SqlDatabase::Trace::Severity severity)
      {
        switch (severity) {
          case SqlDatabase::Trace::Informational: return zsLib::Log::Informational;
          case SqlDatabase::Trace::Warning:       return zsLib::Log::Warning;
          case SqlDatabase::Trace::Error:         return zsLib::Log::Error;
          case SqlDatabase::Trace::Fatal:         return zsLib::Log::Fatal;
        }
        return zsLib::Log::Informational;
      }

      //-----------------------------------------------------------------------
      bool Helper::mkdir(
                         const char *inPath,
                         mode_t modes
                         )
      {
        String path(inPath);
        if (path.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("path is empty") + ZS_PARAMIZE(path))
        }

        if (::mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
          auto error = errno;
          if (EEXIST != error) {
            ZS_LOG_WARNING(Detail, log("unable to create storage path") + ZS_PARAMIZE(path))
            return false;
          }
          ZS_LOG_TRACE(log("path already exists") + ZS_PARAMIZE(path))
        } else {
          ZS_LOG_TRACE(log("path created") + ZS_PARAMIZE(path))
        }
        return true;
      }

      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Helper => (friends)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Level Helper::getJavaScriptLogLevel()
      {
        return internal::getJavaScriptLogLevel();
      }

      //-----------------------------------------------------------------------
      void Helper::enableSqliteErrorLogging()
      {
      }

      //-----------------------------------------------------------------------
      ElementPtr Helper::toDebug(
                                 const SqlTable *table,
                                 const SqlRecord *record
                                 )
      {
        if (!table) return ElementPtr();

        ElementPtr resultEl = Element::create(table->name().c_str());
        
        if (!record) {
          UseServicesHelper::debugAppend(resultEl, "record", false);
          return resultEl;
        }
        
        const SqlFieldSet *fields = table->fields();

        for (int loop = 0; loop < fields->count(); ++loop) {
          const SqlField *field = fields->getByIndex(loop);
          if (field->isIgnored()) continue;
          
          const SqlValue *value = record->getValue(loop);
          if (value->isIgnored()) continue;
          
          if (value->isNull()) {
            UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), "(null)");
            continue;
          }
          
          switch (field->getType()) {
            case sql::type_undefined: UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asString()); break;
            case sql::type_int:       UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asInteger()); break;
            case sql::type_text:      UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asString()); break;
            case sql::type_float:     UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asDouble()); break;
            case sql::type_bool:      UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asBool()); break;
            case sql::type_time:      UseServicesHelper::debugAppend(resultEl, field->getName().c_str(), value->asInteger()); break;
          }
        }

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Helper => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params Helper::log(const char *message)
      {
        return Log::Params(message, "stack::Helper");
      }

    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IHelper
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IHelper::getSignatureInfo(
                                         ElementPtr signedEl,
                                         ElementPtr *outSignatureEl,
                                         String *outPeerURI,
                                         String *outKeyID,
                                         String *outKeyDomain,
                                         String *outService,
                                         String *outFullPublicKey,
                                         String *outFingerprint
                                         )
    {
      return internal::Helper::getSignatureInfo(signedEl, outSignatureEl, outPeerURI, outKeyID, outKeyDomain, outService, outFullPublicKey, outFingerprint);
    }

    //-------------------------------------------------------------------------
    void IHelper::convert(
                          const CandidateList &input,
                          IICESocket::CandidateList &output
                          )
    {
      return internal::Helper::convert(input, output);
    }

    //-------------------------------------------------------------------------
    void IHelper::convert(
                          const IICESocket::CandidateList &input,
                          CandidateList &output
                          )
    {
      return internal::Helper::convert(input, output);
    }
    
    //-------------------------------------------------------------------------
    String IHelper::splitEncrypt(
                                 SecureByteBlock &key,
                                 SecureByteBlock &value
                                 )
    {
      return internal::Helper::splitEncrypt(key, value);
    }

    //-------------------------------------------------------------------------
    SecureByteBlockPtr IHelper::splitDecrypt(
                                           SecureByteBlock &key,
                                           const char *hexIVAndBase64EncodedData
                                           )
    {
      return internal::Helper::splitDecrypt(key, hexIVAndBase64EncodedData);
    }

    //-------------------------------------------------------------------------
    String IHelper::appendPath(
                             const char *path,
                             const char *fileName
                             )
    {
      return internal::Helper::appendPath(path, fileName);
    }

    //-------------------------------------------------------------------------
    zsLib::Log::Severity IHelper::toSeverity(SqlDatabase::Trace::Severity severity)
    {
      return internal::Helper::toSeverity(severity);
    }

    //-------------------------------------------------------------------------
    bool IHelper::mkdir(
                        const char *path,
                        mode_t modes
                        )
    {
      return internal::Helper::mkdir(path, modes);
    }
  }
}
