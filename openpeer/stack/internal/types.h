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

#include <openpeer/stack/types.h>
#include <openpeer/stack/message/types.h>

#define OPENPEER_STACK_CANDIDATE_NAMESPACE_ICE_CANDIDATES "https://meta.openpeer.org/candidate/ice"
#define OPENPEER_STACK_CANDIDATE_NAMESPACE_FINDER_RELAY   "https://meta.openpeer.org/candidate/finder-relay"

#define OPENPEER_STACK_TRANSPORT_JSON_MLS_RUDP "json-mls/rudp"
#define OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_MLS_TCP "multiplexed-json-mls/tcp"

#define OPENPEER_STACK_TRANSPORT_MULTIPLEXED_JSON_TCP "multiplexed-json/tcp"

#define OPENPEER_STACK_SERVER_TYPE_FINDER "finder"
#define OPENPEER_STACK_SERVER_TYPE_PUSH_MAILBOX "push-mailbox"

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      using zsLib::string;
      using zsLib::Noop;
      using zsLib::CSTR;
      using zsLib::Seconds;
      using zsLib::Hours;
      using zsLib::Timer;
      using zsLib::TimerPtr;
      using zsLib::ITimerDelegate;
      using zsLib::ITimerDelegatePtr;
      using zsLib::Log;
      using zsLib::MessageQueueAssociator;
      using zsLib::AutoBool;
      using zsLib::AutoWORD;
      using zsLib::AutoULONG;
      using zsLib::AutoPUID;
      using zsLib::PrivateGlobalLock;
      using zsLib::Singleton;
      using zsLib::SingletonLazySharedPtr;

      ZS_DECLARE_USING_PTR(zsLib::XML, Element)
      ZS_DECLARE_USING_PTR(zsLib::XML, Document)
      ZS_DECLARE_USING_PTR(zsLib::XML, Node)
      ZS_DECLARE_USING_PTR(zsLib::XML, Text)
      ZS_DECLARE_USING_PTR(zsLib::XML, Attribute)
      ZS_DECLARE_USING_PTR(zsLib::XML, Generator)

      using boost::dynamic_pointer_cast;

      ZS_DECLARE_USING_PTR(services, IBackgroundingNotifier)
      ZS_DECLARE_USING_PTR(services, IBackgroundingSubscription)
      ZS_DECLARE_USING_PTR(services, IDHKeyDomain)
      ZS_DECLARE_USING_PTR(services, IDHPrivateKey)
      ZS_DECLARE_USING_PTR(services, IDHPublicKey)
      ZS_DECLARE_USING_PTR(services, IDNS)
      ZS_DECLARE_USING_PTR(services, IDNSDelegate)
      ZS_DECLARE_USING_PTR(services, IDNSQuery)
      ZS_DECLARE_USING_PTR(services, IICESocket)
      ZS_DECLARE_USING_PTR(services, IICESocketDelegate)
      ZS_DECLARE_USING_PTR(services, IICESocketSubscription)
      ZS_DECLARE_USING_PTR(services, IICESocketSession)
      ZS_DECLARE_USING_PTR(services, IICESocketSessionDelegate)
      ZS_DECLARE_USING_PTR(services, IICESocketSessionSubscription)
      ZS_DECLARE_USING_PTR(services, IHTTP)
      ZS_DECLARE_USING_PTR(services, IHTTPQuery)
      ZS_DECLARE_USING_PTR(services, IHTTPQueryDelegate)
      ZS_DECLARE_USING_PTR(services, IMessageQueueManager)
      ZS_DECLARE_USING_PTR(services, IRUDPTransport)
      ZS_DECLARE_USING_PTR(services, IRUDPTransportDelegate)
      ZS_DECLARE_USING_PTR(services, IRUDPTransportSubscription)
      ZS_DECLARE_USING_PTR(services, IRUDPMessaging)
      ZS_DECLARE_USING_PTR(services, IRUDPMessagingDelegate)
      ZS_DECLARE_USING_PTR(services, ITransportStream)
      ZS_DECLARE_USING_PTR(services, ITCPMessaging)
      ZS_DECLARE_USING_PTR(services, ITCPMessagingDelegate)
      ZS_DECLARE_USING_PTR(services, ITCPMessagingSubscription)
      ZS_DECLARE_USING_PTR(services, ITransportStreamReader)
      ZS_DECLARE_USING_PTR(services, ITransportStreamReaderDelegate)
      ZS_DECLARE_USING_PTR(services, ITransportStreamReaderSubscription)
      ZS_DECLARE_USING_PTR(services, ITransportStreamWriter)
      ZS_DECLARE_USING_PTR(services, ITransportStreamWriterDelegate)
      ZS_DECLARE_USING_PTR(services, ITransportStreamWriterSubscription)

      ZS_DECLARE_USING_PROXY(services, IWakeDelegate)
      ZS_DECLARE_USING_PROXY(services, IBackgroundingDelegate)

      using namespace message;

      ZS_DECLARE_CLASS_PTR(Account)
      ZS_DECLARE_CLASS_PTR(AccountFinder)
      ZS_DECLARE_CLASS_PTR(AccountPeerLocation)
      ZS_DECLARE_CLASS_PTR(BootstrappedNetwork)
      ZS_DECLARE_CLASS_PTR(BootstrappedNetworkManager)
      ZS_DECLARE_CLASS_PTR(ServiceCertificatesValidateQuery)
      ZS_DECLARE_CLASS_PTR(Cache)
      ZS_DECLARE_CLASS_PTR(Diff)
      ZS_DECLARE_CLASS_PTR(Factory)
      ZS_DECLARE_CLASS_PTR(FinderRelayChannel)
      ZS_DECLARE_CLASS_PTR(FinderConnection)
      ZS_DECLARE_CLASS_PTR(FinderConnectionManager)
      ZS_DECLARE_CLASS_PTR(Helper)
      ZS_DECLARE_CLASS_PTR(KeyGenerator)
      ZS_DECLARE_CLASS_PTR(MessageIncoming)
      ZS_DECLARE_CLASS_PTR(Location)
      ZS_DECLARE_CLASS_PTR(Peer)
      ZS_DECLARE_CLASS_PTR(Publication)
      ZS_DECLARE_CLASS_PTR(PublicationMetaData)
      ZS_DECLARE_CLASS_PTR(PublicationRepository)
      ZS_DECLARE_CLASS_PTR(MessageMonitor)
      ZS_DECLARE_CLASS_PTR(MessageMonitorManager)
      ZS_DECLARE_CLASS_PTR(PeerFiles)
      ZS_DECLARE_CLASS_PTR(PeerFilePublic)
      ZS_DECLARE_CLASS_PTR(PeerFilePrivate)
      ZS_DECLARE_CLASS_PTR(PeerSubscription)
      ZS_DECLARE_CLASS_PTR(ServiceIdentitySession)
      ZS_DECLARE_CLASS_PTR(ServiceLockboxSession)
      ZS_DECLARE_CLASS_PTR(ServiceNamespaceGrantSession)
      ZS_DECLARE_CLASS_PTR(ServicePushMailboxSession)
      ZS_DECLARE_CLASS_PTR(ServiceSaltFetchSignedSaltQuery)
      ZS_DECLARE_CLASS_PTR(Settings)
      ZS_DECLARE_CLASS_PTR(Stack)

      ZS_DECLARE_INTERACTION_PTR(IFinderRelayChannel)
      ZS_DECLARE_INTERACTION_PTR(IFinderConnection)
      ZS_DECLARE_INTERACTION_PTR(IFinderConnectionRelayChannel)
      ZS_DECLARE_INTERACTION_PTR(IServiceNamespaceGrantSessionQuery)
      ZS_DECLARE_INTERACTION_PTR(IServiceNamespaceGrantSessionWait)

      ZS_DECLARE_INTERACTION_PROXY(IAccountFinderDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IAccountPeerLocationDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IFinderRelayChannelDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IFinderConnectionDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IFinderConnectionRelayChannelDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IMessageMonitorAsyncDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IServiceNamespaceGrantSessionQueryDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IServiceNamespaceGrantSessionWaitDelegate)
      ZS_DECLARE_INTERACTION_PROXY(IServiceLockboxSessionForInternalDelegate)

      ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IFinderRelayChannelSubscription, IFinderRelayChannelDelegate)
      ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IFinderConnectionSubscription, IFinderConnectionDelegate)
      ZS_DECLARE_INTERACTION_PROXY_SUBSCRIPTION(IServiceLockboxSessionForInternalSubscription, IServiceLockboxSessionForInternalDelegate)
    }
  }
}
