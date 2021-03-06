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

#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/internal/types.h>

#define OPENPEER_STACK_PEER_FILE_PRIVATE_KEY_EXPIRY_IN_HOURS (24*365*2)
#define OPENPEER_STACK_PEER_FILE_SIGNATURE_ALGORITHM "https://meta.openpeer.org/2012/12/14/jsonsig#rsa-sha1"
#define OPENPEER_STACK_PEER_FILE_CIPHER "https://meta.openpeer.org/2013/07/21/jsonmsg#rsa-sha1-aes-cfb-32-16-16-sha256-md5"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IPeerFilePublicForPeerFilePrivate;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePrivateForPeerFiles
      #pragma mark

      interaction IPeerFilePrivateForPeerFiles
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerFilePrivateForPeerFiles, ForPeerFiles)

        static bool generate(
                             PeerFilesPtr peerFiles,
                             PeerFilePrivatePtr &outPeerFilePrivate,
                             PeerFilePublicPtr &outPeerFilePublic,
                             const char *password,
                             ElementPtr signedSalt
                             );

        static bool generate(
                             PeerFilesPtr peerFiles,
                             PeerFilePrivatePtr &outPeerFilePrivate,
                             PeerFilePublicPtr &outPeerFilePublic,
                             IRSAPrivateKeyPtr rsaPrivateKey,
                             IRSAPublicKeyPtr rsaPublicKey,
                             const char *password,
                             ElementPtr signedSalt
                             );

        static bool loadFromElement(
                                    PeerFilesPtr peerFiles,
                                    PeerFilePrivatePtr &outPeerFilePrivate,
                                    PeerFilePublicPtr &outPeerFilePublic,
                                    const char *password,
                                    ElementPtr peerFileRootElement
                                    );

        virtual ElementPtr saveToElement() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFilePrivate
      #pragma mark

      class PeerFilePrivate : public Noop,
                              public IPeerFilePrivate,
                              public IPeerFilePrivateForPeerFiles
      {
      public:
        friend interaction IPeerFilePrivateFactory;

        ZS_DECLARE_TYPEDEF_PTR(IPeerFilePublicForPeerFilePrivate, UsePeerFilePublic)

      protected:
        PeerFilePrivate(PeerFilesPtr peerFiles);
        
        PeerFilePrivate(Noop) : Noop(true) {};

        void init();

      public:
        ~PeerFilePrivate();

        static PeerFilePrivatePtr convert(IPeerFilePrivatePtr peerFilePrivate);
        static PeerFilePrivatePtr convert(ForPeerFilesPtr peerFilePrivate);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerFilePrivate => IPeerFilePrivate
        #pragma mark

        static ElementPtr toDebug(IPeerFilePrivatePtr peerFilePrivate);

        virtual PUID getID() const {return mID;}

        virtual IPeerFilesPtr getAssociatedPeerFiles() const;
        virtual IPeerFilePublicPtr getAssociatedPeerFilePublic() const;

        virtual ElementPtr saveToElement() const;

        virtual SecureByteBlockPtr getPassword() const;

        virtual IRSAPrivateKeyPtr getPrivateKey() const;

        virtual ElementPtr getPrivateData() const;

        virtual String getSecretProof() const;
        virtual SecureByteBlockPtr getSalt() const;

        virtual void signElement(
                                 ElementPtr elementToSign,
                                 SignatureTypes signatureType = SignatureType_PeerURI,
                                 IRSAPublicKeyPtr publicKey = IRSAPublicKeyPtr()
                                 ) const;

        virtual SecureByteBlockPtr decrypt(const SecureByteBlock &buffer) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerFilePrivate => IPeerFilePrivateForPeerFiles
        #pragma mark

        static bool generate(
                             PeerFilesPtr peerFiles,
                             PeerFilePrivatePtr &outPeerFilePrivate,
                             PeerFilePublicPtr &outPeerFilePublic,
                             IRSAPrivateKeyPtr rsaPrivateKey,
                             IRSAPublicKeyPtr rsaPublicKey,
                             const char *password,
                             ElementPtr signedSaltBundleEl
                             );

        static bool loadFromElement(
                                    PeerFilesPtr peerFiles,
                                    PeerFilePrivatePtr &outPeerFilePrivate,
                                    PeerFilePublicPtr &outPeerFilePublic,
                                    const char *password,
                                    ElementPtr peerFileRootElement
                                    );

        // (duplicate) virtual ElementPtr saveToElement() const;

      private:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerFilePrivate => (internal)
        #pragma mark

        Log::Params log(const char *message) const;

        SecureByteBlockPtr getKey(
                                  const char *phrase,
                                  const char *saltInBase64
                                  );
        SecureByteBlockPtr getIV(
                                 const char *phrase,
                                 const char *saltInBase64
                                 );

        String encrypt(
                       const char *phrase,
                       const char *saltAsBase64,
                       SecureByteBlock &buffer
                       );
        String encrypt(
                       const char *phrase,
                       const char *saltAsBase64,
                       const char *value
                       );

        String decryptString(
                             const char *phrase,
                             const String &saltAsBase64,
                             const String &encryptedStringInBase64
                             );
        SecureByteBlockPtr decryptBuffer(
                                         const char *phrase,
                                         const String &saltAsBase64,
                                         const String &encryptedStringInBase64
                                         );

        bool generateDocuments(
                               IRSAPublicKeyPtr publicKey,
                               ElementPtr signedSaltBundleEl,
                               DocumentPtr &outPublicPeerDocument
                               );

        bool loadFromElement(
                             ElementPtr peerFileRootElement,
                             DocumentPtr &outPublicPeerDocument
                             );

        bool verifySignatures(UsePeerFilePublicPtr peerFilePublic);

        ElementPtr findSection(const char *sectionID) const;

      protected:
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PeerFilePrivate => (data)
        #pragma mark

        PUID mID;

        PeerFilePrivateWeakPtr mThisWeak;
        PeerFilesWeakPtr mOuter;

        DocumentPtr mDocument;
        DocumentPtr mPrivateDataDoc;
        SecureByteBlockPtr mPassword;

        String mPeerURI;

        IRSAPrivateKeyPtr mPrivateKey;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePrivateFactory
      #pragma mark

      interaction IPeerFilePrivateFactory
      {
        static IPeerFilePrivateFactory &singleton();

        virtual bool generate(
                              PeerFilesPtr peerFiles,
                              PeerFilePrivatePtr &outPeerFilePrivate,
                              PeerFilePublicPtr &outPeerFilePublic,
                              const char *password,
                              ElementPtr signedSaltBundleEl
                              );

        virtual bool generate(
                              PeerFilesPtr peerFiles,
                              PeerFilePrivatePtr &outPeerFilePrivate,
                              PeerFilePublicPtr &outPeerFilePublic,
                              IRSAPrivateKeyPtr rsaPrivateKey,
                              IRSAPublicKeyPtr rsaPublicKey,
                              const char *password,
                              ElementPtr signedSaltBundleEl
                              );

        virtual bool loadFromElement(
                                     PeerFilesPtr peerFiles,
                                     PeerFilePrivatePtr &outPeerFilePrivate,
                                     PeerFilePublicPtr &outPeerFilePublic,
                                     const char *password,
                                     ElementPtr peerFileRootElement
                                     );
      };
      
    }
  }
}
