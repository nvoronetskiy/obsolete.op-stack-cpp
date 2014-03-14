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

#include <openpeer/stack/internal/stack_Factory.h>

#include <zsLib/helpers.h>
#include <zsLib/Log.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

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
      #pragma mark (helper)
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Factory
      #pragma mark

      //-----------------------------------------------------------------------
      void Factory::override(FactoryPtr override)
      {
        singleton().mOverride = override;
      }

      //-----------------------------------------------------------------------
      Factory &Factory::singleton()
      {
        static Singleton<Factory, false> factory;
        Factory &singleton = factory.singleton();
        if (singleton.mOverride) return (*singleton.mOverride);
        return singleton;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountFactory &IAccountFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      AccountPtr IAccountFactory::create(
                                         IAccountDelegatePtr delegate,
                                         IServiceLockboxSessionPtr peerContactSession
                                         )
      {
        if (this) {}
        return Account::create(delegate, peerContactSession);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountFinderFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountFinderFactory &IAccountFinderFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IAccountFinderForAccount::ForAccountPtr IAccountFinderFactory::create(
                                                                            IAccountFinderDelegatePtr delegate,
                                                                            AccountPtr outer
                                                                            )
      {
        if (this) {}
        return AccountFinder::create(delegate, outer);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountPeerLocationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IAccountPeerLocationFactory &IAccountPeerLocationFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IAccountPeerLocationForAccount::ForAccountPtr IAccountPeerLocationFactory::createFromIncomingPeerLocationFind(
                                                                                                                    IAccountPeerLocationDelegatePtr delegate,
                                                                                                                    AccountPtr outer,
                                                                                                                    PeerLocationFindRequestPtr request,
                                                                                                                    IDHPrivateKeyPtr localPrivateKey,
                                                                                                                    IDHPublicKeyPtr localPublicKey
                                                                                                                    )
      {
        if (this) {}
        return AccountPeerLocation::createFromIncomingPeerLocationFind(delegate, outer, request, localPrivateKey, localPublicKey);
      }

      //-----------------------------------------------------------------------
      IAccountPeerLocationForAccount::ForAccountPtr IAccountPeerLocationFactory::createFromPeerLocationFindResult(
                                                                                                                  IAccountPeerLocationDelegatePtr delegate,
                                                                                                                  AccountPtr outer,
                                                                                                                  PeerLocationFindRequestPtr request,
                                                                                                                  LocationInfoPtr locationInfo
                                                                                                                  )
      {
        if (this) {}
        return AccountPeerLocation::createFromPeerLocationFindResult(delegate, outer, request, locationInfo);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IBootstrappedNetworkFactory &IBootstrappedNetworkFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr IBootstrappedNetworkFactory::prepare(
                                                                  const char *domain,
                                                                  IBootstrappedNetworkDelegatePtr delegate
                                                                  )
      {
        if (this) {}
        return BootstrappedNetwork::prepare(domain, delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IBootstrappedNetworkManagerFactory &IBootstrappedNetworkManagerFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkManagerPtr IBootstrappedNetworkManagerFactory::createBootstrappedNetworkManager()
      {
        if (this) {}
        return BootstrappedNetworkManager::create();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderRelayChannelFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IFinderRelayChannelFactory &IFinderRelayChannelFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      FinderRelayChannelPtr IFinderRelayChannelFactory::connect(
                                                                IFinderRelayChannelDelegatePtr delegate,
                                                                AccountPtr account,
                                                                ITransportStreamPtr receiveStream,
                                                                ITransportStreamPtr sendStream,
                                                                IPAddress remoteFinderIP,
                                                                const char *localContextID,
                                                                const char *remoteContextID,
                                                                const char *relayDomain,
                                                                const char *relayAccessToken,
                                                                const char *relayAccessSecretProof,
                                                                IDHPrivateKeyPtr localPrivateKey,
                                                                IDHPublicKeyPtr localPublicKey,
                                                                IDHPublicKeyPtr remotePublicKey
                                                                )
      {
        if (this) {}
        return FinderRelayChannel::connect(delegate, account, receiveStream, sendStream, remoteFinderIP, localContextID, remoteContextID, relayDomain, relayAccessToken, relayAccessSecretProof, localPrivateKey, localPublicKey, remotePublicKey);
      }

      //-----------------------------------------------------------------------
      FinderRelayChannelPtr IFinderRelayChannelFactory::createIncoming(
                                                                       IFinderRelayChannelDelegatePtr delegate,
                                                                       AccountPtr account,
                                                                       ITransportStreamPtr outerReceiveStream,
                                                                       ITransportStreamPtr outerSendStream,
                                                                       ITransportStreamPtr wireReceiveStream,
                                                                       ITransportStreamPtr wireSendStream
                                                                       )
      {
        if (this) {}
        return FinderRelayChannel::createIncoming(delegate, account, outerReceiveStream, outerSendStream, wireReceiveStream, wireSendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IFinderConnectionRelayChannelFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelFactory &IFinderConnectionRelayChannelFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IFinderConnectionPtr IFinderConnectionRelayChannelFactory::connect(
                                                                         IFinderConnectionDelegatePtr delegate,
                                                                         const IPAddress &remoteFinderIP,
                                                                         ITransportStreamPtr receiveStream,
                                                                         ITransportStreamPtr sendStream
                                                                         )
      {
        if (this) {}
        return FinderConnection::connect(delegate, remoteFinderIP, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      IFinderConnectionRelayChannelPtr IFinderConnectionRelayChannelFactory::connect(
                                                                                     IFinderConnectionRelayChannelDelegatePtr delegate,
                                                                                     const IPAddress &remoteFinderIP,
                                                                                     const char *localContextID,
                                                                                     const char *remoteContextID,
                                                                                     const char *relayDomain,
                                                                                     const char *relayAccessToken,
                                                                                     const char *relayAccessSecretProof,
                                                                                     ITransportStreamPtr receiveStream,
                                                                                     ITransportStreamPtr sendStream
                                                                                     )
      {
        if (this) {}
        return FinderConnection::connect(delegate, remoteFinderIP, localContextID, remoteContextID, relayDomain, relayAccessToken, relayAccessSecretProof, receiveStream, sendStream);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IKeyGeneratorFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IKeyGeneratorFactory &IKeyGeneratorFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr IKeyGeneratorFactory::generatePeerFiles(
                                                              IKeyGeneratorDelegatePtr delegate,
                                                              const char *password,
                                                              ElementPtr signedSaltEl,
                                                              IKeyGeneratorPtr rsaKeyGenerator
                                                              )
      {
        if (this) {}
        return KeyGenerator::generatePeerFiles(delegate, password, signedSaltEl, rsaKeyGenerator);
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr IKeyGeneratorFactory::generateRSA(
                                                        IKeyGeneratorDelegatePtr delegate,
                                                        size_t keySizeInBits
                                                        )
      {
        if (this) {}
        return KeyGenerator::generateRSA(delegate, keySizeInBits);
      }

      //-----------------------------------------------------------------------
      KeyGeneratorPtr IKeyGeneratorFactory::generateDHKeyDomain(
                                                                IKeyGeneratorDelegatePtr delegate,
                                                                size_t keySizeInBits
                                                                )
      {
        if (this) {}
        return KeyGenerator::generateDHKeyDomain(delegate, keySizeInBits);
      }


      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageIncomingFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageIncomingFactory &IMessageIncomingFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      IMessageIncomingForAccount::ForAccountPtr IMessageIncomingFactory::create(
                                                                                AccountPtr account,
                                                                                LocationPtr location,
                                                                                message::MessagePtr message
                                                                                )
      {
        if (this) {}
        return MessageIncoming::create(account, location, message);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageMonitorFactory &IMessageMonitorFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitor(
                                                        IMessageMonitorDelegatePtr delegate,
                                                        message::MessagePtr requestMessage,
                                                        Duration timeout
                                                        )
      {
        if (this) {}
        return MessageMonitor::monitor(delegate, requestMessage, timeout);
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitorAndSendToLocation(
                                                                         IMessageMonitorDelegatePtr delegate,
                                                                         ILocationPtr peerLocation,
                                                                         message::MessagePtr message,
                                                                         Duration timeout
                                                                         )
      {
        if (this) {}
        return MessageMonitor::monitorAndSendToLocation(delegate, peerLocation, message, timeout);
      }

      //-----------------------------------------------------------------------
      MessageMonitorPtr IMessageMonitorFactory::monitorAndSendToService(
                                                                        IMessageMonitorDelegatePtr delegate,
                                                                        IBootstrappedNetworkPtr bootstrappedNetwork,
                                                                        const char *serviceType,
                                                                        const char *serviceMethodName,
                                                                        message::MessagePtr message,
                                                                        Duration timeout
                                                                        )
      {
        if (this) {}
        return MessageMonitor::monitorAndSendToService(delegate, bootstrappedNetwork, serviceType, serviceMethodName, message, timeout);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IMessageMonitorManagerFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IMessageMonitorManagerFactory &IMessageMonitorManagerFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      MessageMonitorManagerPtr IMessageMonitorManagerFactory::createMessageMonitorManager()
      {
        if (this) {}
        return MessageMonitorManager::create();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      ILocationFactory &ILocationFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      LocationPtr ILocationFactory::getForLocal(IAccountPtr account)
      {
        if (this) {}
        return Location::getForLocal(account);
      }

      //-----------------------------------------------------------------------
      LocationPtr ILocationFactory::getForFinder(IAccountPtr account)
      {
        if (this) {}
        return Location::getForFinder(account);
      }

      //-----------------------------------------------------------------------
      LocationPtr ILocationFactory::getForPeer(
                                               IPeerPtr peer,
                                               const char *locationID
                                               )
      {
        if (this) {}
        return Location::getForPeer(peer, locationID);
      }

      //-----------------------------------------------------------------------
      LocationPtr ILocationFactory::create(
                                           IMessageSourcePtr messageSource,
                                           const char *peerURI,
                                           const char *locationID
                                           )
      {
        if (this) {}
        return Location::create(messageSource, peerURI, locationID);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFactory &IPeerFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::create(
                                   IAccountPtr account,
                                   IPeerFilePublicPtr peerFilePublic
                                   )
      {
        if (this) {}
        return Peer::create(account, peerFilePublic);
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::getFromSignature(
                                             IAccountPtr account,
                                             ElementPtr signedElement
                                             )
      {
        if (this) {}
        return Peer::getFromSignature(account, signedElement);
      }

      //-----------------------------------------------------------------------
      PeerPtr IPeerFactory::create(
                                   AccountPtr account,
                                   const char *peerURI
                                   )
      {
        if (this) {}
        return Peer::create(account, peerURI);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilesFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFilesFactory &IPeerFilesFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr IPeerFilesFactory::generate(
                                               const char *password,
                                               ElementPtr signedSaltBundleEl
                                               )
      {
        if (this) {}
        return PeerFiles::generate(password, signedSaltBundleEl);
      }

      //-----------------------------------------------------------------------
      PeerFilesPtr IPeerFilesFactory::generate(
                                               IRSAPrivateKeyPtr privateKey,
                                               IRSAPublicKeyPtr publicKey,
                                               const char *password,
                                               ElementPtr signedSaltBundleEl
                                               )
      {
        if (this) {}
        return PeerFiles::generate(privateKey, publicKey, password, signedSaltBundleEl);
      }


      //-----------------------------------------------------------------------
      PeerFilesPtr IPeerFilesFactory::loadFromElement(
                                                      const char *password,
                                                      ElementPtr privatePeerRootElement
                                                      )
      {
        if (this) {}
        return PeerFiles::loadFromElement(password, privatePeerRootElement);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePrivateFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFilePrivateFactory &IPeerFilePrivateFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      bool IPeerFilePrivateFactory::generate(
                                             PeerFilesPtr peerFiles,
                                             PeerFilePrivatePtr &outPeerFilePrivate,
                                             PeerFilePublicPtr &outPeerFilePublic,
                                             const char *password,
                                             ElementPtr signedSalt
                                             )
      {
        if (this) {}
        return PeerFilePrivate::generate(peerFiles, outPeerFilePrivate, outPeerFilePublic, IRSAPrivateKeyPtr(), IRSAPublicKeyPtr(), password, signedSalt);
      }

      //-----------------------------------------------------------------------
      bool IPeerFilePrivateFactory::generate(
                                             PeerFilesPtr peerFiles,
                                             PeerFilePrivatePtr &outPeerFilePrivate,
                                             PeerFilePublicPtr &outPeerFilePublic,
                                             IRSAPrivateKeyPtr rsaPrivateKey,
                                             IRSAPublicKeyPtr rsaPublicKey,
                                             const char *password,
                                             ElementPtr signedSalt
                                             )
      {
        if (this) {}
        return PeerFilePrivate::generate(peerFiles, outPeerFilePrivate, outPeerFilePublic, rsaPrivateKey, rsaPublicKey, password, signedSalt);
      }

      //-----------------------------------------------------------------------
      bool IPeerFilePrivateFactory::loadFromElement(
                                                    PeerFilesPtr peerFiles,
                                                    PeerFilePrivatePtr &outPeerFilePrivate,
                                                    PeerFilePublicPtr &outPeerFilePublic,
                                                    const char *password,
                                                    ElementPtr peerFileRootElement
                                                    )
      {
        if (this) {}
        return PeerFilePrivate::loadFromElement(peerFiles, outPeerFilePrivate, outPeerFilePublic, password, peerFileRootElement);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFilePublicFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerFilePublicFactory &IPeerFilePublicFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicFactory::loadFromElement(ElementPtr publicPeerRootElement)
      {
        if (this) {}
        return PeerFilePublic::loadFromElement(publicPeerRootElement);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicFactory::loadFromCache(const char *peerURI)
      {
        if (this) {}
        return PeerFilePublic::loadFromCache(peerURI);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicFactory::createFromPublicKey(
                                                                    PeerFilesPtr peerFiles,
                                                                    DocumentPtr publicDoc,
                                                                    IRSAPublicKeyPtr publicKey,
                                                                    const String &peerURI
                                                                    )
      {
        if (this) {}
        return PeerFilePublic::createFromPublicKey(peerFiles, publicDoc, publicKey, peerURI);
      }

      //-----------------------------------------------------------------------
      PeerFilePublicPtr IPeerFilePublicFactory::loadFromElement(
                                                                PeerFilesPtr peerFiles,
                                                                DocumentPtr publicDoc
                                                                )
      {
        if (this) {}
        return PeerFilePublic::loadFromElement(peerFiles, publicDoc);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerSubscriptionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPeerSubscriptionFactory &IPeerSubscriptionFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr IPeerSubscriptionFactory::subscribeAll(
                                                                 IAccountPtr account,
                                                                 IPeerSubscriptionDelegatePtr delegate
                                                                 )
      {
        if (this) {}
        return PeerSubscription::subscribeAll(account, delegate);
      }

      //-----------------------------------------------------------------------
      PeerSubscriptionPtr IPeerSubscriptionFactory::subscribe(
                                                              IPeerPtr peer,
                                                              IPeerSubscriptionDelegatePtr delegate
                                                              )
      {
        if (this) {}
        return PeerSubscription::subscribe(peer, delegate);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationFactory &IPublicationFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
                                                 LocationPtr creatorLocation,
                                                 const char *name,
                                                 const char *mimeType,
                                                 const SecureByteBlock &data,
                                                 const PublishToRelationshipsMap &publishToRelationships,
                                                 LocationPtr publishedLocation,
                                                 Time expires
                                                 )
      {
        if (this) {}
        return Publication::create(creatorLocation, name, mimeType, data, publishToRelationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
                                                 LocationPtr creatorLocation,
                                                 const char *name,
                                                 const char *mimeType,
                                                 DocumentPtr documentToBeAdopted,
                                                 const PublishToRelationshipsMap &publishToRelationships,
                                                 LocationPtr publishedLocation,
                                                 Time expires
                                                 )
      {
        if (this) {}
        return Publication::create(creatorLocation, name, mimeType, documentToBeAdopted, publishToRelationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      PublicationPtr IPublicationFactory::create(
                                                 ULONG version,
                                                 ULONG baseVersion,
                                                 ULONG lineage,
                                                 LocationPtr creatorLocation,
                                                 const char *name,
                                                 const char *mimeType,
                                                 ElementPtr dataEl,
                                                 Encodings encoding,
                                                 const PublishToRelationshipsMap &publishToRelationships,
                                                 LocationPtr publishedLocation,
                                                 Time expires
                                                 )
      {
        if (this) {}
        return Publication::create(version, baseVersion, lineage, creatorLocation, name, mimeType, dataEl, encoding, publishToRelationships, publishedLocation, expires);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationMetaDataFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationMetaDataFactory &IPublicationMetaDataFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PublicationMetaDataPtr IPublicationMetaDataFactory::creatPublicationMetaData(
                                                                                   ULONG version,
                                                                                   ULONG baseVersion,
                                                                                   ULONG lineage,
                                                                                   LocationPtr creatorLocation,
                                                                                   const char *name,
                                                                                   const char *mimeType,
                                                                                   Encodings encoding,
                                                                                   const PublishToRelationshipsMap &relationships,
                                                                                   LocationPtr publishedLocation,
                                                                                   Time expires
                                                                                   )
      {
        if (this) {}
        return PublicationMetaData::create(version, baseVersion, lineage, creatorLocation, name, mimeType, encoding, relationships, publishedLocation, expires);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPublicationRepositoryFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IPublicationRepositoryFactory &IPublicationRepositoryFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      PublicationRepositoryPtr IPublicationRepositoryFactory::createPublicationRepository(AccountPtr account)
      {
        if (this) {}
        return PublicationRepository::create(account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceCertificatesValidateQueryFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceCertificatesValidateQueryFactory &IServiceCertificatesValidateQueryFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceCertificatesValidateQueryPtr IServiceCertificatesValidateQueryFactory::queryIfValidSignature(
                                                                                                          IServiceCertificatesValidateQueryDelegatePtr delegate,
                                                                                                          ElementPtr signedElement
                                                                                                          )
      {
        if (this) {}
        return ServiceCertificatesValidateQuery::queryIfValidSignature(delegate, signedElement);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceIdentitySessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceIdentitySessionFactory &IServiceIdentitySessionFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::loginWithIdentity(
                                                                                  IServiceIdentitySessionDelegatePtr delegate,
                                                                                  IServiceIdentityPtr provider,
                                                                                  IServiceNamespaceGrantSessionPtr grantSession,
                                                                                  IServiceLockboxSessionPtr existingLockbox,
                                                                                  const char *outerFrameURLUponReload,
                                                                                  const char *identityURI_or_identityBaseURI
                                                                                  )
      {
        if (this) {}
        return ServiceIdentitySession::loginWithIdentity(delegate, provider, grantSession, existingLockbox, outerFrameURLUponReload, identityURI_or_identityBaseURI);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::loginWithIdentityPreauthorized(
                                                                                               IServiceIdentitySessionDelegatePtr delegate,
                                                                                               IServiceIdentityPtr provider,
                                                                                               IServiceNamespaceGrantSessionPtr grantSession,
                                                                                               IServiceLockboxSessionPtr existingLockbox,  // pass NULL IServiceLockboxSessionPtr() if none exists
                                                                                               const char *identityURI,
                                                                                               const char *identityAccessToken,
                                                                                               const char *identityAccessSecret,
                                                                                               Time identityAccessSecretExpires
                                                                                               )
      {
        if (this) {}
        return ServiceIdentitySession::loginWithIdentityPreauthorized(delegate, provider, grantSession, existingLockbox, identityURI, identityAccessToken, identityAccessSecret, identityAccessSecretExpires);
      }

      //-----------------------------------------------------------------------
      ServiceIdentitySessionPtr IServiceIdentitySessionFactory::reload(
                                                                       BootstrappedNetworkPtr provider,
                                                                       IServiceNamespaceGrantSessionPtr grantSession,
                                                                       IServiceLockboxSessionPtr existingLockbox,
                                                                       const char *identityURI,
                                                                       const char *reloginKey
                                                                       )
      {
        if (this) {}
        return ServiceIdentitySession::reload(provider, grantSession, existingLockbox, identityURI, reloginKey);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceLockboxSessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceLockboxSessionFactory &IServiceLockboxSessionFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr IServiceLockboxSessionFactory::login(
                                                                    IServiceLockboxSessionDelegatePtr delegate,
                                                                    IServiceLockboxPtr serviceLockbox,
                                                                    IServiceNamespaceGrantSessionPtr grantSession,
                                                                    IServiceIdentitySessionPtr identitySession,
                                                                    bool forceNewAccount
                                                                    )
      {
        if (this) {}
        return ServiceLockboxSession::login(delegate, serviceLockbox, grantSession, identitySession, forceNewAccount);
      }

      //-----------------------------------------------------------------------
      ServiceLockboxSessionPtr IServiceLockboxSessionFactory::relogin(
                                                                      IServiceLockboxSessionDelegatePtr delegate,
                                                                      IServiceLockboxPtr serviceLockbox,
                                                                      IServiceNamespaceGrantSessionPtr grantSession,
                                                                      const char *lockboxAccountID,
                                                                      const SecureByteBlock &lockboxKey
                                                                      )
      {
        if (this) {}
        return ServiceLockboxSession::relogin(delegate, serviceLockbox, grantSession, lockboxAccountID, lockboxKey);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IServiceNamespaceGrantSessionFactory
      #pragma mark

      //-----------------------------------------------------------------------
      IServiceNamespaceGrantSessionFactory &IServiceNamespaceGrantSessionFactory::singleton()
      {
        return Factory::singleton();
      }

      //-----------------------------------------------------------------------
      ServiceNamespaceGrantSessionPtr IServiceNamespaceGrantSessionFactory::create(
                                                                                   IServiceNamespaceGrantSessionDelegatePtr delegate,
                                                                                   const char *outerFrameURLUponReload,
                                                                                   const char *grantID
                                                                                   )
      {
        if (this) {}
        return ServiceNamespaceGrantSession::create(delegate, outerFrameURLUponReload, grantID);
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
        return Factory::singleton();
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
    }
  }
}
