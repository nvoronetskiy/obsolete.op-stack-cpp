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

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

using namespace zsLib::XML;

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Forwards
      #pragma mark

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
        signedEl = services::IHelper::getSignatureInfo(signedEl, &signatureEl, outFullPublicKey, outFingerprint);

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
        String salt = services::IHelper::randomString(services::IHelper::getHashDigestSize(services::IHelper::HashAlgorthm_MD5)*8/5);

        SecureByteBlockPtr iv = services::IHelper::hash(salt, services::IHelper::HashAlgorthm_MD5);
        String base64 = services::IHelper::convertToBase64(*services::IHelper::encrypt(key, *iv, value));

        SecureByteBlockPtr proofHash = services::IHelper::hash(value);
        String proof = services::IHelper::convertToHex(*services::IHelper::hmac(key, "proof:" + salt + ":" + services::IHelper::convertToHex(*services::IHelper::hash(value))));

        return salt + ":" + proof + ":" + base64;
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Helper::splitDecrypt(
                                              SecureByteBlock &key,
                                              const char *saltAndProofAndBase64EncodedData
                                              )
      {
        if (!saltAndProofAndBase64EncodedData) return SecureByteBlockPtr();

        services::IHelper::SplitMap split;
        services::IHelper::split(saltAndProofAndBase64EncodedData, split, ':');

        if (split.size() != 3) {
          ZS_LOG_WARNING(Detail, log("failed to decode data (does not have salt:proof:base64 pattern") + ZS_PARAM("value", saltAndProofAndBase64EncodedData))
          return SecureByteBlockPtr();
        }

        const String &salt = split[0];
        const String &proof = split[1];
        const String &value = split[2];

        SecureByteBlockPtr iv = services::IHelper::hash(salt, services::IHelper::HashAlgorthm_MD5);

        SecureByteBlockPtr dataToConvert = services::IHelper::convertFromBase64(value);
        if (services::IHelper::isEmpty(dataToConvert)) {
          ZS_LOG_WARNING(Detail, log("failed to decode data from base64") + ZS_PARAM("value", value))
          return SecureByteBlockPtr();
        }

        SecureByteBlockPtr decryptedData = services::IHelper::decrypt(key, *iv, *dataToConvert);

        SecureByteBlockPtr proofHash = services::IHelper::hash(*decryptedData);
        String calculatedProof = services::IHelper::convertToHex(*services::IHelper::hmac(key, "proof:" + salt + ":" + services::IHelper::convertToHex(*services::IHelper::hash(*decryptedData))));

        if (calculatedProof != proof) {
          ZS_LOG_WARNING(Detail, log("decryption proof failure") + ZS_PARAM("proof", proof) + ZS_PARAM("calculated", calculatedProof))
          return SecureByteBlockPtr();
        }

        return decryptedData;
      }

      //-----------------------------------------------------------------------
      Log::Level Helper::getJavaScriptLogLevel()
      {
        return internal::getJavaScriptLogLevel();
      }

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

  }
}
