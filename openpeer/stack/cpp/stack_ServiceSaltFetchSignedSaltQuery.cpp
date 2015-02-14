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

#include <openpeer/stack/internal/stack_ServiceSaltFetchSignedSaltQuery.h>
#include <openpeer/stack/message/peer-salt/SignedSaltGetRequest.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>

#define OPENPEER_STACK_SERVICE_SALT_FETCH_SIGNED_SALT_QUERY_GET_TIMEOUT_IN_SECONDS (60*2)

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      //      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      ZS_DECLARE_USING_PTR(message::peer_salt, SignedSaltGetRequest)

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceSaltFetchSignedSaltQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ServiceSaltFetchSignedSaltQuery::ServiceSaltFetchSignedSaltQuery(
                                                                       IMessageQueuePtr queue,
                                                                       IServiceSaltFetchSignedSaltQueryDelegatePtr delegate,
                                                                       IServiceSaltPtr serviceSalt,
                                                                       UINT totalToFetch
                                                                       ) :
        zsLib::MessageQueueAssociator(queue),
        mDelegate(IServiceSaltFetchSignedSaltQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mBootstrappedNetwork(BootstrappedNetwork::convert(serviceSalt)),
        mLastError(0),
        mTotalToFetch(totalToFetch)
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      ServiceSaltFetchSignedSaltQuery::~ServiceSaltFetchSignedSaltQuery()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServiceSaltFetchSignedSaltQuery::init()
      {
        AutoRecursiveLock lock(getLock());

        UseBootstrappedNetwork::prepare(mBootstrappedNetwork->getDomain(), mThisWeak.lock());
      }

      //-----------------------------------------------------------------------
      ServiceSaltFetchSignedSaltQueryPtr ServiceSaltFetchSignedSaltQuery::convert(IServiceSaltFetchSignedSaltQueryPtr query)
      {
        return ZS_DYNAMIC_PTR_CAST(ServiceSaltFetchSignedSaltQuery, query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceSaltFetchSignedSaltQuery => IServiceSaltFetchSignedSaltQuery
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServiceSaltFetchSignedSaltQuery::toDebug(IServiceSaltFetchSignedSaltQueryPtr query)
      {
        if (!query) return ElementPtr();
        return ServiceSaltFetchSignedSaltQuery::convert(query)->toDebug();
      }

      //-----------------------------------------------------------------------
      ServiceSaltFetchSignedSaltQueryPtr ServiceSaltFetchSignedSaltQuery::fetchSignedSalt(
                                                                                          IServiceSaltFetchSignedSaltQueryDelegatePtr delegate,
                                                                                          IServiceSaltPtr serviceSalt,
                                                                                          UINT totalToFetch
                                                                                          )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!delegate)
        ZS_THROW_INVALID_ARGUMENT_IF(!serviceSalt)
        ZS_THROW_INVALID_ARGUMENT_IF(totalToFetch < 1)

        ServiceSaltFetchSignedSaltQueryPtr pThis(new ServiceSaltFetchSignedSaltQuery(UseStack::queueStack(), delegate, serviceSalt, totalToFetch));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServiceSaltPtr ServiceSaltFetchSignedSaltQuery::getService() const
      {
        AutoRecursiveLock lock(getLock());
        return BootstrappedNetwork::convert(mBootstrappedNetwork);
      }

      //-----------------------------------------------------------------------
      bool ServiceSaltFetchSignedSaltQuery::isComplete() const
      {
        AutoRecursiveLock lock(getLock());
        return (0 == mTotalToFetch);
      }

      //-----------------------------------------------------------------------
      bool ServiceSaltFetchSignedSaltQuery::wasSuccessful(
                                                          WORD *outErrorCode,
                                                          String *outErrorReason
                                                          ) const
      {
        AutoRecursiveLock lock(getLock());
        if (0 != mTotalToFetch) return false;

        if (outErrorCode) {
          *outErrorCode = mLastError;
        }
        if (outErrorReason) {
          *outErrorReason = mLastErrorReason;
        }

        return (0 == mLastError);
      }

      //-----------------------------------------------------------------------
      size_t ServiceSaltFetchSignedSaltQuery::getTotalSignedSaltsAvailable() const
      {
        AutoRecursiveLock lock(getLock());
        return mSaltBundles.size();
      }

      //-----------------------------------------------------------------------
      ElementPtr ServiceSaltFetchSignedSaltQuery::getNextSignedSalt()
      {
        AutoRecursiveLock lock(getLock());
        ElementPtr result = mSaltBundles.front();
        mSaltBundles.pop_front();
        return result;
      }

      //-----------------------------------------------------------------------
      void ServiceSaltFetchSignedSaltQuery::cancel()
      {
        AutoRecursiveLock lock(getLock());

        if (0 == mTotalToFetch) {
          ZS_LOG_DEBUG(log("already cancelled"))
          return;
        }

        ZS_LOG_DEBUG(log("cancel called"))

        mTotalToFetch = 0;

        if (mSaltMonitor) {
          mSaltMonitor->cancel();
          mSaltMonitor.reset();
        }

        ServiceSaltFetchSignedSaltQueryPtr pThis = mThisWeak.lock();
        if ((pThis) &&
            (mDelegate)) {
          try {
            mDelegate->onServiceSaltFetchSignedSaltCompleted(pThis);
          } catch(IServiceCertificatesValidateQueryDelegateProxy::Exceptions::DelegateGone &) {
          }
        }

        mDelegate.reset();

        mBootstrappedNetwork.reset();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceSaltFetchSignedSaltQuery => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServiceSaltFetchSignedSaltQuery::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceSaltFetchSignedSaltQuery => IMessageMonitorResultDelegate<SignedSaltGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServiceSaltFetchSignedSaltQuery::handleMessageMonitorResultReceived(
                                                                               IMessageMonitorPtr monitor,
                                                                               SignedSaltGetResultPtr response
                                                                               )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mSaltMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        ZS_LOG_DEBUG(log("salt get finished successfully"))
        mSaltBundles = response->saltBundles();

        if (mSaltBundles.size() < mTotalToFetch) {
          ZS_LOG_WARNING(Detail, log("failed to obtain the number of salts requested"))

          mLastError = IHTTP::HTTPStatusCode_NoContent;
          mLastErrorReason = IHTTP::toString(IHTTP::toStatusCode(mLastError));
          mSaltBundles.clear();
        }

        for (SaltBundleList::iterator iter = mSaltBundles.begin(); iter != mSaltBundles.end(); ++iter)
        {
          ElementPtr bundleEl = (*iter);
          if (!mBootstrappedNetwork->isValidSignature(bundleEl)) {
            ZS_LOG_WARNING(Detail, log("bundle returned was improperly signed"))

            mLastError = IHTTP::HTTPStatusCode_NoCert;
            mLastErrorReason = IHTTP::toString(IHTTP::toStatusCode(mLastError));
            mSaltBundles.clear();
            break;
          }
          ZS_LOG_DEBUG(log("salt bundle passed validation"))
        }

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServiceSaltFetchSignedSaltQuery::handleMessageMonitorErrorResultReceived(
                                                                                    IMessageMonitorPtr monitor,
                                                                                    SignedSaltGetResultPtr response, // will always be NULL
                                                                                    message::MessageResultPtr result
                                                                                    )
      {
        AutoRecursiveLock lock(getLock());
        if (monitor != mSaltMonitor) {
          ZS_LOG_WARNING(Detail, log("monitor notified for obsolete request"))
          return false;
        }

        mLastError = result->errorCode();
        mLastErrorReason = result->errorReason();

        ZS_LOG_DEBUG(debug("salt get failed"))

        cancel();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServiceSaltFetchSignedSaltQuery => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &ServiceSaltFetchSignedSaltQuery::getLock() const
      {
        return mLock;
      }

      //-----------------------------------------------------------------------
      Log::Params ServiceSaltFetchSignedSaltQuery::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServiceSaltFetchSignedSaltQuery");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServiceSaltFetchSignedSaltQuery::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServiceSaltFetchSignedSaltQuery::toDebug() const
      {
        AutoRecursiveLock lock(getLock());

        ElementPtr resultEl = Element::create("ServiceSaltFetchSignedSaltQuery");

        IHelper::debugAppend(resultEl, "id", mID);
        IHelper::debugAppend(resultEl, UseBootstrappedNetwork::toDebug(mBootstrappedNetwork));
        IHelper::debugAppend(resultEl, IMessageMonitor::toDebug(mSaltMonitor));
        IHelper::debugAppend(resultEl, "salt bundles", mSaltBundles.size());
        IHelper::debugAppend(resultEl, "total to fetch", mTotalToFetch);
        IHelper::debugAppend(resultEl, "error code", mLastError);
        IHelper::debugAppend(resultEl, "error reason", mLastErrorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServiceSaltFetchSignedSaltQuery::step()
      {
        if (0 == mTotalToFetch) {
          ZS_LOG_DEBUG(log("step proceeding to cancel"))
          cancel();
          return;
        }

        if (mSaltMonitor) {
          ZS_LOG_DEBUG(log("waiting for salt monitor"))
          return;
        }

        if (!mBootstrappedNetwork->isPreparationComplete()) {
          ZS_LOG_DEBUG(log("waiting for bootstrapper to complete"))
          return;
        }

        if (!mBootstrappedNetwork->wasSuccessful(&mLastError, &mLastErrorReason)) {
          ZS_LOG_WARNING(Detail, log("salt fetch failed because of bootstrapper failure"))
          cancel();
          return;
        }

        SignedSaltGetRequestPtr request = SignedSaltGetRequest::create();
        request->domain(mBootstrappedNetwork->getDomain());
        request->salts(mTotalToFetch);

        mSaltMonitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate<SignedSaltGetResult>::convert(mThisWeak.lock()), BootstrappedNetwork::convert(mBootstrappedNetwork), "salt", "signed-salt-get", request, Seconds(OPENPEER_STACK_SERVICE_SALT_FETCH_SIGNED_SALT_QUERY_GET_TIMEOUT_IN_SECONDS));
        ZS_LOG_DEBUG(log("sending signed salt get request"))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceSaltFetchSignedSaltQueryFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceSaltFetchSignedSaltQueryFactory &IServiceSaltFetchSignedSaltQueryFactory::singleton()
      {
        return ServiceSaltFetchSignedSaltQueryFactory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceSaltFetchSignedSaltQueryPtr IServiceSaltFetchSignedSaltQueryFactory::fetchSignedSalt(
                                                                                                  IServiceSaltFetchSignedSaltQueryDelegatePtr delegate,
                                                                                                  IServiceSaltPtr serviceSalt,
                                                                                                  UINT totalToFetch
                                                                                                  )
      {
        if (this) {}
        return ServiceSaltFetchSignedSaltQuery::fetchSignedSalt(delegate, serviceSalt, totalToFetch);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServiceSaltFetchSignedSaltQuery
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IServiceSaltFetchSignedSaltQuery::toDebug(IServiceSaltFetchSignedSaltQueryPtr query)
    {
      return internal::ServiceSaltFetchSignedSaltQuery::toDebug(query);
    }

    //-------------------------------------------------------------------------
    IServiceSaltFetchSignedSaltQueryPtr IServiceSaltFetchSignedSaltQuery::fetchSignedSalt(
                                                                                          IServiceSaltFetchSignedSaltQueryDelegatePtr delegate,
                                                                                          IServiceSaltPtr serviceSalt,
                                                                                          UINT totalToFetch
                                                                                          )
    {
      return internal::IServiceSaltFetchSignedSaltQueryFactory::singleton().fetchSignedSalt(delegate, serviceSalt, totalToFetch);
    }
  }
}
