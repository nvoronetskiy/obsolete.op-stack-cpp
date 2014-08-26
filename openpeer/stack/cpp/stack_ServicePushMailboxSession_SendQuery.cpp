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

#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
//#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
//
//#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
//#include <openpeer/stack/internal/stack_Helper.h>
//#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_Stack.h>
//
//#include <openpeer/stack/message/bootstrapped-servers/ServersGetRequest.h>
//#include <openpeer/stack/message/push-mailbox/RegisterPushRequest.h>
//#include <openpeer/stack/message/push-mailbox/NamespaceGrantChallengeValidateRequest.h>
//#include <openpeer/stack/message/push-mailbox/PeerValidateRequest.h>
//
#include <openpeer/services/IHelper.h>
//#include <openpeer/services/ISettings.h>
//#include <openpeer/services/ITCPMessaging.h>
//
//#include <zsLib/Log.h>
#include <zsLib/XML.h>
//#include <zsLib/helpers.h>
//
//#include <zsLib/Stringize.h>

#include <cryptopp/sha.h>


namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using CryptoPP::SHA1;

      typedef IStackForInternal UseStack;
//
      using services::IHelper;
//
//      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;
//
//      ZS_DECLARE_USING_PTR(openpeer::services, IBackgrounding)
//
//      ZS_DECLARE_USING_PTR(message::bootstrapped_servers, ServersGetRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, AccessRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, NamespaceGrantChallengeValidateRequest)
//      ZS_DECLARE_USING_PTR(message::push_mailbox, PeerValidateRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark (helpers)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQuery::SendQuery(
                                                      IMessageQueuePtr queue,
                                                      const SharedRecursiveLock &lock,
                                                      ServicePushMailboxSessionPtr outer,
                                                      IServicePushMailboxSendQueryDelegatePtr delegate,
                                                      const String &messageID
                                                      ) :
        MessageQueueAssociator(queue),
        SharedRecursiveLock(lock),
        mOuter(outer),
        mDelegate(IServicePushMailboxSendQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate))
      {
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::init()
      {
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQuery::~SendQuery()
      {
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => friend ServicePushMailboxSession
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::SendQueryPtr ServicePushMailboxSession::SendQuery::create(
                                                                                           IMessageQueuePtr queue,
                                                                                           const SharedRecursiveLock &lock,
                                                                                           ServicePushMailboxSessionPtr outer,
                                                                                           IServicePushMailboxSendQueryDelegatePtr delegate,
                                                                                           const String &messageID
                                                                                           )
      {
        SendQueryPtr pThis(new SendQuery(queue, lock, outer, delegate, messageID));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::notifyUploaded()
      {
        ZS_LOG_DEBUG(log("message was uploaded to server") + ZS_PARAM("message id", mMessageID))

        AutoRecursiveLock lock(*this);

        mUploaded = true;

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate already gone"))
          return;
        }

        try {
          mDelegate->onPushMailboxSendQueryMessageUploaded(mThisWeak.lock());
        } catch (IServicePushMailboxSendQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          mDelegate.reset();
        }
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::notifyRemoved()
      {
        ZS_LOG_WARNING(Detail, log("message was removed") + ZS_PARAM("message id", mMessageID))

        AutoRecursiveLock lock(*this);

        if (mDelegate) {
          try {
            mDelegate->onPushMailboxSendQueryPushStatesChanged(mThisWeak.lock());
          } catch (IServicePushMailboxSendQueryDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
            mDelegate.reset();
          }
        }

        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::notifyDeliveryInfoChanged(const PushMessageInfo::FlagInfoMap &flags)
      {
        ZS_LOG_DEBUG(log("message delivery information changed") + ZS_PARAM("message id", mMessageID))

        AutoRecursiveLock lock(*this);

        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("delegate already gone"))
          return;
        }

        SHA1 hasher;
        SecureByteBlock hashResult(hasher.DigestSize());

        for (PushMessageInfo::FlagInfoMap::const_iterator iter = flags.begin(); iter != flags.end(); ++iter)
        {
          const PushMessageInfo::FlagInfo &info = (*iter).second;

          hasher.Update((const BYTE *) ":", strlen(":"));

          hasher.Update((const BYTE *) (&info.mDisposition), sizeof(info.mDisposition));
          hasher.Update((const BYTE *) (&info.mFlag), sizeof(info.mFlag));

          size_t total = info.mFlagURIInfos.size();
          hasher.Update((const BYTE *) (&total), sizeof(total));

          for (PushMessageInfo::FlagInfo::URIInfoList::const_iterator uriIter = info.mFlagURIInfos.begin(); uriIter != info.mFlagURIInfos.end(); ++uriIter)
          {
            const PushMessageInfo::FlagInfo::URIInfo &uriInfo = (*uriIter);

            hasher.Update((const BYTE *) ":", strlen(":"));
            hasher.Update((const BYTE *) (&uriInfo.mErrorCode), sizeof(uriInfo.mErrorCode));
            hasher.Update((const BYTE *) uriInfo.mURI.c_str(), uriInfo.mURI.length());
            hasher.Update((const BYTE *) ":", strlen(":"));
            hasher.Update((const BYTE *) uriInfo.mErrorReason.c_str(), uriInfo.mErrorReason.length());
          }
        }
        hasher.Final(hashResult);

        String deliveryHash = IHelper::convertToHex(hashResult);
        if (flags.size() < 1) {
          deliveryHash = String();
        }

        if (deliveryHash == mDeliveryHash) {
          ZS_LOG_TRACE(log("hash did not change thus no need to notify about delivery state change"))
          return;
        }

        mDeliveryHash = deliveryHash;

        ZS_LOG_DEBUG(log("notifying about delivery state change") + ZS_PARAM("message id", mMessageID) + ZS_PARAM("hash", mDeliveryHash))

        try {
          mDelegate->onPushMailboxSendQueryPushStatesChanged(mThisWeak.lock());
        } catch (IServicePushMailboxSendQueryDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
          mDelegate.reset();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => IServicePushMailboxSendQuery
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePushMailboxSession::SendQuery::isUploaded() const
      {
        AutoRecursiveLock lock(*this);
        return mUploaded;
      }

      //-----------------------------------------------------------------------
      ServicePushMailboxSession::PushMessagePtr ServicePushMailboxSession::SendQuery::getPushMessage()
      {
        AutoRecursiveLock lock(*this);
        if (mComplete) {
          ZS_LOG_WARNING(Detail, log("send query was previously cancelled (thus cannot return send updates)"))
          return PushMessagePtr();
        }

        ServicePushMailboxSessionPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Detail, log("push messaging service is gone (thus cannot return send udpates)"))
          return PushMessagePtr();
        }

        return outer->getPushMessage(mMessageID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePushMailboxSession::SendQuery => (interal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::SendQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePushMailboxSession::SendQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePushMailboxSession::SendQuery::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePushMailboxSession::SendQuery::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePushMailboxSession::SendQuery");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "complete", (bool)mComplete);
        IHelper::debugAppend(resultEl, "uploaded", (bool)mUploaded);

        IHelper::debugAppend(resultEl, "message id", mMessageID);
        IHelper::debugAppend(resultEl, "delivery hash", mDeliveryHash);

        return resultEl;
      }
      
      //-----------------------------------------------------------------------
      void ServicePushMailboxSession::SendQuery::cancel()
      {
        mComplete = true;

        //SendQueryPtr pThis = mThisWeak.lock();
        //if ((pThis) &&
        //    (mDelegate)) {
        //}

        ServicePushMailboxSessionPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyComplete(*this);
          mOuter.reset();
        }

        mDelegate.reset();
      }

      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }


  }
}
