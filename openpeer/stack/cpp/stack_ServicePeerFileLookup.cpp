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

#include <openpeer/stack/internal/stack_ServicePeerFileLookup.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/stack/IPeerFilePublic.h>

#include <openpeer/stack/IPeer.h>

#include <openpeer/services/IHelper.h>
#include <openpeer/services/ISettings.h>

#include <zsLib/Log.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>


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

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::ServicePeerFileLookup(IMessageQueuePtr queue) :
        zsLib::MessageQueueAssociator(queue),
        SharedRecursiveLock(SharedRecursiveLock::create())
      {
        ZS_LOG_DEBUG(log("created"))
      }

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::~ServicePeerFileLookup()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::init()
      {
        AutoRecursiveLock lock(*this);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup => IServicePeerFileLookup
      #pragma mark

      //-----------------------------------------------------------------------
      ElementPtr ServicePeerFileLookup::toDebug(ServicePeerFileLookupPtr lookup)
      {
        if (!lookup) return ElementPtr();
        return lookup->toDebug();
      }

      //-----------------------------------------------------------------------
      ServicePeerFileLookupPtr ServicePeerFileLookup::singleton()
      {
        static SingletonLazySharedPtr<ServicePeerFileLookup> singleton(IServicePeerFileLookupFactory::singleton().createServicePeerFileLookup());
        ServicePeerFileLookupPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      ServicePeerFileLookupPtr ServicePeerFileLookup::create()
      {
        ServicePeerFileLookupPtr pThis(new ServicePeerFileLookup(UseStack::queueStack()));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      IServicePeerFileLookupQueryPtr ServicePeerFileLookup::createBogusFetchPeerFiles(
                                                                                      IServicePeerFileLookupQueryDelegatePtr inDelegate,
                                                                                      const PeerFileLookupMap &peerFilesToLookup
                                                                                      )
      {
        ZS_DECLARE_CLASS_PTR(BogusServicePeerFileLookupQuery)

        class BogusServicePeerFileLookupQuery : public IServicePeerFileLookupQuery
        {
        public:
          static BogusServicePeerFileLookupQueryPtr create(IServicePeerFileLookupQueryDelegatePtr inDelegate)
          {
            IServicePeerFileLookupQueryDelegatePtr delegate = IServicePeerFileLookupQueryDelegateProxy::create(internal::IStackForInternal::queueDelegate(), inDelegate);
            BogusServicePeerFileLookupQueryPtr pThis(new BogusServicePeerFileLookupQuery);

            if (delegate) {
              delegate->onServicePeerFileLookupQueryCompleted(pThis);
            }

            return pThis;
          }

          virtual PUID getID() const {return mID;}

          virtual bool isComplete(
                                  WORD *outErrorCode = NULL,
                                  String *outErrorReason = NULL
                                  ) const
          {
            if (outErrorCode) *outErrorCode = IHTTP::HTTPStatusCode_Gone;
            if (outErrorReason) *outErrorReason = "singleton gone";

            return true;
          }

          virtual void cancel() {}

          virtual IPeerFilePublicPtr getPeerFileByPeerURI(const String &peerURI) {return IPeerFilePublicPtr();}
          
        private:
          internal::AutoPUID mID;
        };

        return BogusServicePeerFileLookupQuery::create(inDelegate);
      }

      //---------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePeerFileLookupQuery
      #pragma mark

      //---------------------------------------------------------------------
      ElementPtr ServicePeerFileLookup::toDebug(IServicePeerFileLookupQueryPtr query)
      {
        AutoRecursiveLock lock(*this);

        if (!query) return ElementPtr();

        return (Lookup::convert(query))->toDebug();
      }

      //---------------------------------------------------------------------
      IServicePeerFileLookupQueryPtr ServicePeerFileLookup::fetchPeerFiles(
                                                                           IServicePeerFileLookupQueryDelegatePtr delegate,
                                                                           const PeerFileLookupMap &peerFilesToLookup
                                                                           )
      {
        AutoRecursiveLock lock(*this);
        if (mShutdown) {
          return createBogusFetchPeerFiles(delegate, peerFilesToLookup);
        }

        LookupPtr lookup = Lookup::create(*this, mThisWeak.lock(), delegate, peerFilesToLookup);

        for (PeerFileLookupMap::const_iterator iter = peerFilesToLookup.begin(); iter != peerFilesToLookup.end(); ++iter)
        {
          const String &peerURI = (*iter).first;
          const String &peerFindSecret = (*iter).second;

          IPeerFilePublicPtr peerFile = IPeerFilePublic::loadFromCache(peerURI);

          if (peerFile) {
            ZS_LOG_DEBUG(log("peer file loaded from cache") + ZS_PARAM("uri", peerURI))
            lookup->notifyResult(peerFile);
            continue;
          }

          // no peer file loaded
          String domain;
          String contactID;

          if (!IPeer::splitURI(peerURI, domain, contactID)) {
            ZS_LOG_WARNING(Detail, log("peer URI failed to parse") + ZS_PARAM("uri", peerURI))
            lookup->notifyResultEmpty(peerURI);
            continue;
          }

          BootstrapperMap::iterator found = mPendingBootstrappers.find(domain);
          if (found == mPendingBootstrappers.end()) {
            UseBootstrappedNetworkPtr bootstrapper = UseBootstrappedNetwork::prepare(domain, mThisWeak.lock());
            mPendingBootstrappers[domain] = bootstrapper;
          }

          mPendingPeerURIs[peerURI] = peerFindSecret;
        }

        mPendingLookups.push_back(lookup);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();

        return lookup;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::onWake()
      {
        ZS_LOG_TRACE(log("wake"))
        AutoRecursiveLock lock(*this);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup => IBootstrappedNetworkDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr inBootstrappedNetwork)
      {
        ZS_LOG_DEBUG(log("bootstrapper reported complete"))

        UseBootstrappedNetworkPtr bootstrappedNetwork = BootstrappedNetwork::convert(inBootstrappedNetwork);

        AutoRecursiveLock lock(*this);
        for (BootstrapperMap::iterator iter_doNotUse = mPendingBootstrappers.begin(); iter_doNotUse != mPendingBootstrappers.end(); )
        {
          BootstrapperMap::iterator current = iter_doNotUse;
          ++iter_doNotUse;

          String domain = (*current).first;
          UseBootstrappedNetworkPtr network = (*current).second;

          if (network != bootstrappedNetwork) continue;

          mPendingBootstrappers.erase(current);

          if (!network->wasSuccessful()) {
            ZS_LOG_WARNING(Detail, log("bootstrapped network failure thus peers on domain cannot be looked up") + ZS_PARAM("domain", domain))
            for (LookupList::iterator iterLookup = mPendingLookups.begin(); iterLookup != mPendingLookups.end(); ++iterLookup)
            {
              LookupPtr lookup = (*iterLookup);
              lookup->notifyDomainFailure(domain);
            }
            continue;
          }

          PeerFilesGetRequest::PeerURIMap peersToLookup;

          for (PeerFileLookupMap::iterator iterPendingURI_doNotUse = mPendingPeerURIs.begin(); iterPendingURI_doNotUse != mPendingPeerURIs.end(); )
          {
            PeerFileLookupMap::iterator currentPendingURI = iterPendingURI_doNotUse;
            ++iterPendingURI_doNotUse;

            const String &peerURI = (*currentPendingURI).first;
            const String &findSecret = (*currentPendingURI).second;

            String peerDomain;
            String peerContact;
            ZS_THROW_INVALID_ASSUMPTION_IF(!IPeer::splitURI(peerURI, peerDomain, peerContact))

            if (peerDomain != domain) {
              ZS_LOG_TRACE(log("peer URI still waiting for another bootstrapper to complete") + ZS_PARAM("uri", peerURI) + ZS_PARAM("complete domain", domain))
              continue;
            }

            ZS_LOG_DEBUG(log("peer URI being added to lookup request") + ZS_PARAM("uri", peerURI))
            peersToLookup[peerURI] = findSecret;

            mPendingPeerURIs.erase(currentPendingURI);
          }

          if (peersToLookup.size() < 1) {
            ZS_LOG_WARNING(Detail, log("no peer URIs to lookup for this domain") + ZS_PARAM("domain", domain))
            continue;
          }

          PeerFilesGetRequestPtr request = PeerFilesGetRequest::create();
          request->domain(domain);
          request->peerURIs(peersToLookup);

          IMessageMonitorPtr monitor = IMessageMonitor::monitorAndSendToService(IMessageMonitorResultDelegate::convert(mThisWeak.lock()), inBootstrappedNetwork, "peer", "peer-files-get", request, Seconds(services::ISettings::getUInt(OPENPEER_STACK_SETTING_SERVICE_PEER_LOOKUP_PEER_FILES_GET_TIMEOUT_IN_SECONDS)));

          ZS_THROW_INVALID_ASSUMPTION_IF(!monitor)

          mPendingRequests[monitor] = request;
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup => IMessageMonitorResultDelegate<PeerFilesGetResult>
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePeerFileLookup::handleMessageMonitorResultReceived(
                                                                     IMessageMonitorPtr monitor,
                                                                     PeerFilesGetResultPtr response
                                                                     )
      {
        ZS_LOG_DEBUG(log("peer files get result received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mPendingRequests.find(monitor);

        if (found == mPendingRequests.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        PeerFilesGetRequestPtr request = (*found).second;

        mPendingRequests.erase(found);

        const PeerFilesGetResult::PeerFileList &peerFiles = response->peerFiles();
        const PeerFilesGetRequest::PeerURIMap &peerURIs = request->peerURIs();

        for (LookupList::iterator iter = mPendingLookups.begin(); iter != mPendingLookups.end(); ++iter)
        {
          LookupPtr lookup = (*iter);

          lookup->notifyLookupComplete(peerFiles, peerURIs);
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      bool ServicePeerFileLookup::handleMessageMonitorErrorResultReceived(
                                                                          IMessageMonitorPtr monitor,
                                                                          PeerFilesGetResultPtr response,
                                                                          message::MessageResultPtr result
                                                                          )
      {
        ZS_LOG_DEBUG(log("peer files get result error received") + IMessageMonitor::toDebug(monitor))

        AutoRecursiveLock lock(*this);

        MonitorMap::iterator found = mPendingRequests.find(monitor);

        if (found == mPendingRequests.end()) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete monitor") + IMessageMonitor::toDebug(monitor))
          return false;
        }

        PeerFilesGetRequestPtr request = (*found).second;

        mPendingRequests.erase(found);

        PeerFilesGetResult::PeerFileList peerFiles;
        const PeerFilesGetRequest::PeerURIMap &peerURIs = request->peerURIs();

        for (LookupList::iterator iter = mPendingLookups.begin(); iter != mPendingLookups.end(); ++iter)
        {
          LookupPtr lookup = (*iter);

          lookup->notifyLookupComplete(peerFiles, peerURIs);
          lookup->notifyError(result->errorCode(), result->errorReason());
        }

        step();
        return true;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePeerFileLookup::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePeerFileLookup");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params ServicePeerFileLookup::debug(const char *message) const
      {
        return Log::Params(message, toDebug());
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePeerFileLookup::toDebug() const
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePeerFileLookup");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "pending bootstrappers", mPendingBootstrappers.size());

        IHelper::debugAppend(resultEl, "pending peer uris", mPendingPeerURIs.size());

        IHelper::debugAppend(resultEl, "pending lookups", mPendingLookups.size());

        IHelper::debugAppend(resultEl, "pending requests", mPendingRequests.size());

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::step()
      {
        AutoRecursiveLock lock(*this);

        ZS_LOG_TRACE(log("step") + toDebug())

        for (LookupList::iterator iter_doNotuse = mPendingLookups.begin(); iter_doNotuse != mPendingLookups.end();)
        {
          LookupList::iterator current = iter_doNotuse;
          ++iter_doNotuse;

          LookupPtr lookup = (*current);

          if (!lookup->isComplete()) {
            ZS_LOG_TRACE(log("lookup is still pending") + lookup->toDebug())
            continue;
          }

          ZS_LOG_DEBUG(log("lookup is now complete") + lookup->toDebug())
          mPendingLookups.erase(current);
        }

        ZS_LOG_TRACE(log("step complete") + toDebug())
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (mShutdown) {
          ZS_LOG_TRACE(log("already shutdown"))
          return;
        }

        ZS_LOG_DEBUG(log("cancel called"))

        get(mShutdown) = true;

        mPendingBootstrappers.clear();
        mPendingPeerURIs.clear();

        for (LookupList::iterator iter = mPendingLookups.begin(); iter != mPendingLookups.end(); ++iter)
        {
          LookupPtr lookup = (*iter);
          lookup->cancel();
        }

        mPendingLookups.clear();

        for (MonitorMap::iterator iter = mPendingRequests.begin(); iter != mPendingRequests.end(); ++iter)
        {
          IMessageMonitorPtr monitor = (*iter).first;
          monitor->cancel();
        }

        mPendingRequests.clear();
      }
      

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup::Lookoup
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::Lookup::Lookup(
                                            SharedRecursiveLock lock,
                                            ServicePeerFileLookupPtr outer,
                                            IServicePeerFileLookupQueryDelegatePtr delegate,
                                            const PeerFileLookupMap &peerFilesToLookup
                                            ) :
        SharedRecursiveLock(lock),
        mOuter(outer),
        mDelegate(IServicePeerFileLookupQueryDelegateProxy::createWeak(UseStack::queueDelegate(), delegate)),
        mFiles(peerFilesToLookup)
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("created"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::init()
      {
      }

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::Lookup::~Lookup()
      {
        mThisWeak.reset();
        ZS_LOG_DEBUG(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::LookupPtr ServicePeerFileLookup::Lookup::convert(IServicePeerFileLookupQueryPtr query)
      {
        return boost::dynamic_pointer_cast<Lookup>(query);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup::Lookoup => friend ServicePeerFileLookup
      #pragma mark

      //-----------------------------------------------------------------------
      ServicePeerFileLookup::LookupPtr ServicePeerFileLookup::Lookup::create(
                                                                             SharedRecursiveLock lock,
                                                                             ServicePeerFileLookupPtr outer,
                                                                             IServicePeerFileLookupQueryDelegatePtr delegate,
                                                                             const PeerFileLookupMap &peerFilesToLookup
                                                                             )
      {
        LookupPtr pThis(new Lookup(lock, outer, delegate, peerFilesToLookup));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      ElementPtr ServicePeerFileLookup::Lookup::toDebug()
      {
        AutoRecursiveLock lock(*this);

        ElementPtr resultEl = Element::create("ServicePeerFileLookup::Lookup");

        IHelper::debugAppend(resultEl, "id", mID);

        IHelper::debugAppend(resultEl, "delegate", (bool)mDelegate);

        IHelper::debugAppend(resultEl, "peer uris", mFiles.size());

        IHelper::debugAppend(resultEl, "total results", mResults.size());

        IHelper::debugAppend(resultEl, "last error", mLastError);
        IHelper::debugAppend(resultEl, "last error reason", mLastErrorReason);

        return resultEl;
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::notifyResult(IPeerFilePublicPtr peerFile)
      {
        AutoRecursiveLock lock(*this);

        if (mFiles.end() == mFiles.find(peerFile->getPeerURI())) {
          ZS_LOG_TRACE(log("peer is not related to this lookup") + ZS_PARAM("uri", peerFile->getPeerURI()))
          return;
        }

        ZS_LOG_DEBUG(log("peer file found") + ZS_PARAM("uri", peerFile->getPeerURI()))
        mResults[peerFile->getPeerURI()] = peerFile;
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::notifyResultEmpty(const String &peerURI)
      {
        AutoRecursiveLock lock(*this);
        if (mFiles.end() == mFiles.find(peerURI)) {
          ZS_LOG_TRACE(log("peer URI is not related to this lookup") + ZS_PARAM("uri", peerURI))
          return;
        }

        ZS_LOG_DEBUG(log("peer file result not found") + ZS_PARAM("uri", peerURI))
        mResults[peerURI] = IPeerFilePublicPtr();
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::notifyDomainFailure(const String &inDomain)
      {
        AutoRecursiveLock lock(*this);
        for (PeerFileLookupMap::iterator iter = mFiles.begin(); iter != mFiles.end(); ++iter)
        {
          const String &peerURI = (*iter).first;

          String domain;
          String contact;
          if (!IPeer::splitURI(peerURI, domain, contact)) {
            ZS_LOG_WARNING(Detail, log("peer URI failed to parse") + ZS_PARAM("uri", peerURI))
            continue;
          }

          if (domain != inDomain) {
            ZS_LOG_TRACE(log("peer uri unrelated to domain") + ZS_PARAM("uri", peerURI) + ZS_PARAM("domain", inDomain))
            continue;
          }

          PeerFileResultMap::iterator found = mResults.find(peerURI);
          if (found != mResults.end()) {
            ZS_LOG_TRACE(log("already have a result for peer uri") + ZS_PARAM("uri", peerURI))
            continue;
          }

          ZS_LOG_DEBUG(log("domain failure for peer URI") + ZS_PARAM("uri", peerURI) + ZS_PARAM("domain", domain))
          mResults[peerURI] = IPeerFilePublicPtr();
        }
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::notifyLookupComplete(
                                                               const PeerFilesGetResult::PeerFileList &peerFiles,
                                                               const PeerFilesGetRequest::PeerURIMap &originalPeerURIs
                                                               )
      {
        typedef PeerFilesGetResult::PeerFileList PeerFileList;
        typedef PeerFilesGetRequest::PeerURIMap PeerURIMap;

        ZS_LOG_DEBUG(log("lookup complete") + ZS_PARAM("files", peerFiles.size()) + ZS_PARAM("original peer URIs", originalPeerURIs.size()))

        AutoRecursiveLock lock(*this);
        for (PeerFileList::const_iterator iter = peerFiles.begin(); iter != peerFiles.end(); ++iter)
        {
          IPeerFilePublicPtr peerFile = (*iter);
          notifyResult(peerFile);
        }

        for (PeerURIMap::const_iterator iter = originalPeerURIs.begin(); iter != originalPeerURIs.end(); ++iter)
        {
          const String &peerURI = (*iter).first;

          if (mResults.end() != mResults.find(peerURI)) {
            ZS_LOG_TRACE(log("already have known result for peer URI") + ZS_PARAM("uri", peerURI))
            continue;
          }

          notifyResultEmpty(peerURI);
        }
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::notifyError(
                                                      WORD errorCode,
                                                      const char *inReason
                                                      )
      {
        AutoRecursiveLock lock(*this);

        String reason(inReason);
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastError) {
          ZS_LOG_WARNING(Detail, log("already have an existing error") + ZS_PARAM("existing error", mLastError) + ZS_PARAM("existing reason", mLastErrorReason) + ZS_PARAM("new error", errorCode) + ZS_PARAM("new reason", reason))
          return;
        }

        get(mLastError) = errorCode;
        mLastErrorReason = reason;

        ZS_LOG_WARNING(Detail, log("error set") + ZS_PARAM("error", mLastError) + ZS_PARAM("reason", mLastErrorReason))
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup::Lookoup => friend IServicePeerFileLookupQuery
      #pragma mark

      //-----------------------------------------------------------------------
      bool ServicePeerFileLookup::Lookup::isComplete(
                                                     WORD *outErrorCode,
                                                     String *outErrorReason
                                                     ) const
      {
        AutoRecursiveLock lock(*this);

        bool complete = mFiles.size() == mResults.size();

        if (complete) {
          const_cast<Lookup *>(this)->cancel();
        }

        return complete;
      }

      //-----------------------------------------------------------------------
      void ServicePeerFileLookup::Lookup::cancel()
      {
        AutoRecursiveLock lock(*this);

        if (mDelegate) {
          try {
            mDelegate->onServicePeerFileLookupQueryCompleted(mThisWeak.lock());
          } catch(IServicePeerFileLookupQueryDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }

        if (mFiles.size() != mResults.size()) {
          for (PeerFileLookupMap::iterator iter = mFiles.begin(); iter != mFiles.end(); ++iter)
          {
            const String &peerURI = (*iter).first;

            PeerFileResultMap::iterator found = mResults.find(peerURI);
            if (found != mResults.end()) {
              ZS_LOG_TRACE(log("already have a result") + ZS_PARAM("uri", peerURI))
              continue;
            }

            ZS_LOG_TRACE(log("cancel called before peer file lookup complete") + ZS_PARAM("uri", peerURI))
            mResults[peerURI] = IPeerFilePublicPtr();
          }
        }
      }

      //-----------------------------------------------------------------------
      IPeerFilePublicPtr ServicePeerFileLookup::Lookup::getPeerFileByPeerURI(const String &peerURI)
      {
        AutoRecursiveLock lock(*this);
        PeerFileResultMap::iterator found = mResults.find(peerURI);
        if (found == mResults.end()) {
          ZS_LOG_TRACE(log("not result found") + ZS_PARAM("uri", peerURI))
          return IPeerFilePublicPtr();
        }

        ZS_LOG_TRACE(log("result found") + ZS_PARAM("uri", peerURI))

        IPeerFilePublicPtr result = (*found).second;
        return result;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup::Lookoup => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params ServicePeerFileLookup::Lookup::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("ServicePeerFileLookup::Lookup");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IServicePeerFileLookupQuery
    #pragma mark

    //-------------------------------------------------------------------------
    ElementPtr IServicePeerFileLookupQuery::toDebug(IServicePeerFileLookupQueryPtr query)
    {
      internal::ServicePeerFileLookupPtr singleton = internal::ServicePeerFileLookup::singleton();
      if (!singleton) {
        ZS_LOG_WARNING(Detail, internal::ServicePeerFileLookup::slog("singleton gone"))
        return ElementPtr();
      }
      return singleton->toDebug(query);
    }

    //-------------------------------------------------------------------------
    IServicePeerFileLookupQueryPtr IServicePeerFileLookupQuery::fetchPeerFiles(
                                                                               IServicePeerFileLookupQueryDelegatePtr delegate,
                                                                               const PeerFileLookupMap &peerFilesToLookup
                                                                               )
    {
      internal::ServicePeerFileLookupPtr singleton = internal::ServicePeerFileLookup::singleton();
      if (!singleton) {
        ZS_LOG_WARNING(Detail, internal::ServicePeerFileLookup::slog("singleton gone"))
        return internal::ServicePeerFileLookup::createBogusFetchPeerFiles(delegate, peerFilesToLookup);
      }
      return singleton->fetchPeerFiles(delegate, peerFilesToLookup);
    }

  }
}
