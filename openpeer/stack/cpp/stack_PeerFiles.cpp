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

#include <openpeer/stack/internal/stack_PeerFiles.h>
#include <openpeer/stack/internal/stack_PeerFilePublic.h>
#include <openpeer/stack/internal/stack_PeerFilePrivate.h>
#include <openpeer/stack/internal/stack_Helper.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/helpers.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using services::IHelper;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFiles
      #pragma mark

      //-----------------------------------------------------------------------
      PeerFiles::PeerFiles() :
        mID(zsLib::createPUID())
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      PeerFiles::~PeerFiles()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr PeerFiles::convert(IPeerFilesPtr object)
      {
        return boost::dynamic_pointer_cast<PeerFiles>(object);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFiles => IPeerFiles
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr PeerFiles::toDebug(IPeerFilesPtr peerFiles)
      {
        if (!peerFiles) return ElementPtr();
        PeerFilesPtr pThis = PeerFiles::convert(peerFiles);
        return pThis->toDebug();
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr PeerFiles::generate(
                                       const char *password,
                                       ElementPtr signedSaltBundleEl
                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!password)
        ZS_THROW_INVALID_ARGUMENT_IF(!signedSaltBundleEl)

        PeerFilesPtr pThis(new PeerFiles);
        pThis->mThisWeak = pThis;

        PeerFilePrivatePtr peerFilePrivate;
        PeerFilePublicPtr peerFilePublic;

        if (!IPeerFilePrivateForPeerFiles::generate(pThis, peerFilePrivate, peerFilePublic, password, signedSaltBundleEl)) {
          ZS_LOG_DEBUG(pThis->log("failed to generate private peer file"))
          return PeerFilesPtr();
        }
        pThis->mPrivate = peerFilePrivate;
        pThis->mPublic = peerFilePublic;
        return pThis;
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr PeerFiles::generate(
                                       IRSAPrivateKeyPtr privateKey,
                                       IRSAPublicKeyPtr publicKey,
                                       const char *password,
                                       ElementPtr signedSaltBundleEl
                                       )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!password)
        ZS_THROW_INVALID_ARGUMENT_IF(!signedSaltBundleEl)

        PeerFilesPtr pThis(new PeerFiles);
        pThis->mThisWeak = pThis;

        PeerFilePrivatePtr peerFilePrivate;
        PeerFilePublicPtr peerFilePublic;

        if (!IPeerFilePrivateForPeerFiles::generate(pThis, peerFilePrivate, peerFilePublic, privateKey, publicKey, password, signedSaltBundleEl)) {
          ZS_LOG_DEBUG(pThis->log("failed to generate private peer file"))
          return PeerFilesPtr();
        }
        pThis->mPrivate = peerFilePrivate;
        pThis->mPublic = peerFilePublic;
        return pThis;
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr PeerFiles::loadFromElement(
                                              const char *password,
                                              ElementPtr privatePeerRootElement
                                              )
      {
        if (!password) return PeerFilesPtr();
        if (!privatePeerRootElement)return PeerFilesPtr();

        PeerFilesPtr pThis(new PeerFiles);
        pThis->mThisWeak = pThis;

        PeerFilePrivatePtr peerFilePrivate;
        PeerFilePublicPtr peerFilePublic;

        if (!IPeerFilePrivateForPeerFiles::loadFromElement(pThis, peerFilePrivate, peerFilePublic, password, privatePeerRootElement)) {
          ZS_LOG_DEBUG(pThis->log("failed to load private peer file"))
          return PeerFilesPtr();
        }
        pThis->mPrivate = peerFilePrivate;
        pThis->mPublic = peerFilePublic;
        return pThis;
      }

      //-----------------------------------------------------------------------
      ElementPtr PeerFiles::saveToPrivatePeerElement() const
      {
        if (!mPrivate) return ElementPtr();
        return mPrivate->saveToElement();
      }

      //-----------------------------------------------------------------------
      IPeerFilePublicPtr PeerFiles::getPeerFilePublic() const
      {
        return PeerFilePublic::convert(mPublic);
      }

      //-----------------------------------------------------------------------
      IPeerFilePrivatePtr PeerFiles::getPeerFilePrivate() const
      {
        return PeerFilePrivate::convert(mPrivate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark PeerFiles => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params PeerFiles::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("PeerFiles");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      ElementPtr PeerFiles::toDebug() const
      {
        ElementPtr resultEl = Element::create("PeerFiles");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UsePeerFilePublic::toDebug(mPublic));

        return resultEl;
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPeerFiles
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IPeerFiles::toDebug(IPeerFilesPtr peerFiles)
    {
      return internal::PeerFiles::toDebug(peerFiles);
    }

    //-------------------------------------------------------------------------
    IPeerFilesPtr IPeerFiles::generate(
                                       const char *password,
                                       ElementPtr signedSalt
                                       )
    {
      return internal::IPeerFilesFactory::singleton().generate(password, signedSalt);
    }

    //-------------------------------------------------------------------------
    IPeerFilesPtr IPeerFiles::generate(
                                       IRSAPrivateKeyPtr privateKey,
                                       IRSAPublicKeyPtr publicKey,
                                       const char *password,
                                       ElementPtr signedSalt
                                       )
    {
      return internal::IPeerFilesFactory::singleton().generate(privateKey, publicKey, password, signedSalt);
    }

    //-------------------------------------------------------------------------
    IPeerFilesPtr IPeerFiles::loadFromElement(
                                              const char *password,
                                              ElementPtr privatePeerRootElement
                                              )
    {
      return internal::IPeerFilesFactory::singleton().loadFromElement(password, privatePeerRootElement);
    }
  }
}
