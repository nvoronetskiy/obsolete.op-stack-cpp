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

#pragma once

#include <openpeer/stack/internal/types.h>

#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IServicePeerFileLookupQuery.h>
#include <openpeer/stack/IMessageMonitor.h>

#include <openpeer/stack/message/peer/PeerFilesGetRequest.h>
#include <openpeer/stack/message/peer/PeerFilesGetResult.h>

#include <openpeer/services/IWakeDelegate.h>

#include <zsLib/MessageQueueAssociator.h>

#define OPENPEER_STACK_SETTING_SERVICE_PEER_LOOKUP_PEER_FILES_GET_TIMEOUT_IN_SECONDS "openpeer/stack/service-peer-lookup-peer-files-get-timeout-in-seconds"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IBootstrappedNetworkForServices;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ServicePeerFileLookup
      #pragma mark

      class ServicePeerFileLookup : public Noop,
                                    public zsLib::MessageQueueAssociator,
                                    public SharedRecursiveLock,
                                    public IWakeDelegate,
                                    public IBootstrappedNetworkDelegate,
                                    public IMessageMonitorResultDelegate<message::peer::PeerFilesGetResult>
      {
      public:
        friend interaction IServicePeerFileLookupFactory;
        friend IServicePeerFileLookupQuery;

        ZS_DECLARE_CLASS_PTR(Lookup)
        friend class Lookup;

        ZS_DECLARE_TYPEDEF_PTR(IBootstrappedNetworkForServices, UseBootstrappedNetwork)
        ZS_DECLARE_TYPEDEF_PTR(message::peer::PeerFilesGetRequest, PeerFilesGetRequest)
        ZS_DECLARE_TYPEDEF_PTR(message::peer::PeerFilesGetResult, PeerFilesGetResult)

        ZS_DECLARE_TYPEDEF_PTR(IServicePeerFileLookupQuery::PeerFileLookupList, PeerFileLookupList)

        typedef String Domain;
        typedef std::map<Domain, UseBootstrappedNetworkPtr> BootstrapperMap;

        typedef std::list<LookupPtr> LookupList;
        typedef std::map<IMessageMonitorPtr, PeerFilesGetRequestPtr> MonitorMap;

      protected:
        ServicePeerFileLookup(IMessageQueuePtr queue);

        ServicePeerFileLookup(Noop) :
          Noop(true),
          MessageQueueAssociator(IMessageQueuePtr()),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}
        
        void init();

      public:
        ~ServicePeerFileLookup();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup
        #pragma mark

        static ElementPtr toDebug(ServicePeerFileLookupPtr lookup);

        static ServicePeerFileLookupPtr singleton();
        static ServicePeerFileLookupPtr create();

        static IServicePeerFileLookupQueryPtr createBogusFetchPeerFiles(
                                                                        IServicePeerFileLookupQueryDelegatePtr delegate,
                                                                        const PeerFileLookupList &peerFilesToLookup
                                                                        );

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IServicePeerFileLookupQuery
        #pragma mark

        virtual ElementPtr toDebug(IServicePeerFileLookupQueryPtr query);

        virtual IServicePeerFileLookupQueryPtr fetchPeerFiles(
                                                              IServicePeerFileLookupQueryDelegatePtr delegate,
                                                              const PeerFileLookupList &peerFilesToLookup
                                                              );
        // (duplicate) static Log::Params slog(const char *message);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup => IWakeDelegate
        #pragma mark

        virtual void onWake();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup => IBootstrappedNetworkDelegate
        #pragma mark

        virtual void onBootstrappedNetworkPreparationCompleted(IBootstrappedNetworkPtr bootstrappedNetwork);

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup => IMessageMonitorResultDelegate<SignedSaltGetResult>
        #pragma mark

        virtual bool handleMessageMonitorResultReceived(
                                                        IMessageMonitorPtr monitor,
                                                        PeerFilesGetResultPtr response
                                                        );

        virtual bool handleMessageMonitorErrorResultReceived(
                                                             IMessageMonitorPtr monitor,
                                                             PeerFilesGetResultPtr response, // will always be NULL
                                                             message::MessageResultPtr result
                                                             );

      public:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup::Lookoup
        #pragma mark

        class Lookup : public SharedRecursiveLock,
                       public IServicePeerFileLookupQuery
        {
        public:
          friend ServicePeerFileLookup;

          typedef std::map<PeerURI, PeerURI> PeerFileLookupMap;
          typedef std::map<PeerURI, IPeerFilePublicPtr> PeerFileResultMap;

        protected:
          Lookup(
                 SharedRecursiveLock lock,
                 ServicePeerFileLookupPtr outer,
                 IServicePeerFileLookupQueryDelegatePtr delegate,
                 const PeerFileLookupList &peerFilesToLookup
                 );

          void init();

        public:
          ~Lookup();

          static LookupPtr convert(IServicePeerFileLookupQueryPtr query);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePeerFileLookup::Lookoup => friend ServicePeerFileLookup
          #pragma mark

          static LookupPtr create(
                                  SharedRecursiveLock lock,
                                  ServicePeerFileLookupPtr outer,
                                  IServicePeerFileLookupQueryDelegatePtr delegate,
                                  const PeerFileLookupList &peerFilesToLookup
                                  );

          virtual ElementPtr toDebug();

          virtual void notifyResult(IPeerFilePublicPtr peerFile);
          virtual void notifyResultEmpty(const String &peerURI);
          virtual void notifyDomainFailure(const String &domain);
          virtual void notifyLookupComplete(
                                            const PeerFilesGetResult::PeerFileList &peerFiles,
                                            const PeerFilesGetRequest::PeerURIList &originalPeerURIs
                                            );

          virtual void notifyError(
                                   WORD errorCode,
                                   const char *reason = NULL
                                   );

          // (duplilcate) virtual void cancel();

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePeerFileLookup::Lookoup => friend IServicePeerFileLookupQuery
          #pragma mark

          virtual PUID getID() const {return mID;}

          virtual bool isComplete(
                                  WORD *outErrorCode = NULL,
                                  String *outErrorReason = NULL
                                  ) const;

          virtual void cancel();

          virtual IPeerFilePublicPtr getPeerFileByPeerURI(const String &peerURI);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePeerFileLookup::Lookoup => (internal)
          #pragma mark

          Log::Params log(const char *message) const;

          static void convert(
                              const PeerFileLookupList &inList,
                              PeerFileLookupMap &outMap
                              );

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark ServicePeerFileLookup::Lookoup => (data)
          #pragma mark

          AutoPUID mID;
          LookupWeakPtr mThisWeak;
          ServicePeerFileLookupWeakPtr mOuter;

          IServicePeerFileLookupQueryDelegatePtr mDelegate;

          PeerFileLookupMap mFiles;
          PeerFileResultMap mResults;

          AutoWORD mLastError;
          String mLastErrorReason;
        };

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup => (internal)
        #pragma mark

        Log::Params log(const char *message) const;
        static Log::Params slog(const char *message);
        Log::Params debug(const char *message) const;

        virtual ElementPtr toDebug() const;

        void step();

        void cancel();

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark ServicePeerFileLookup => (data)
        #pragma mark

        AutoPUID mID;
        mutable RecursiveLock mLock;
        ServicePeerFileLookupWeakPtr mThisWeak;

        AutoBool mShutdown;

        BootstrapperMap mPendingBootstrappers;

        PeerFileLookupList mPendingPeerURIs;

        LookupList mPendingLookups;

        MonitorMap mPendingRequests;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServicePeerFileLookupFactory
      #pragma mark

      interaction IServicePeerFileLookupFactory
      {
        static IServicePeerFileLookupFactory &singleton();

        virtual ServicePeerFileLookupPtr createServicePeerFileLookup();
      };

    }
  }
}
