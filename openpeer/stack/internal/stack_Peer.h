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

#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/internal/types.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForPeer;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForAccount
      #pragma mark

      interaction IPeerForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerForAccount, ForAccount)

        static ElementPtr toDebug(ForAccountPtr peer);

        static ForAccountPtr create(
                                    AccountPtr account,
                                    IPeerFilePublicPtr peerFilePublic
                                    );

        virtual String getPeerURI() const = 0;

        virtual IPeerFilePublicPtr getPeerFilePublic() const = 0;

        virtual void setPeerFilePublic(IPeerFilePublicPtr peerFilePublic) = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForLocation
      #pragma mark

      interaction IPeerForLocation
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerForLocation, ForLocation)

        static ElementPtr toDebug(ForLocationPtr peer);

        static PeerPtr create(
                              AccountPtr account,
                              const char *peerURI
                              );

        static bool isValid(const char *peerURI);

        virtual AccountPtr getAccount() const = 0;

        virtual String getPeerURI() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForLocationDatabases
      #pragma mark

      interaction IPeerForLocationDatabases
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerForLocationDatabases, ForLocationDatabases)

        static ForLocationDatabasesPtr create(
                                              AccountPtr account,
                                              const char *peerURI
                                              );

        virtual String getPeerURI() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForMessages
      #pragma mark

      interaction IPeerForMessages
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerForMessages, ForMessages)

        static ElementPtr toDebug(ForMessagesPtr peer);

        static ForMessagesPtr create(
                                     AccountPtr account,
                                     IPeerFilePublicPtr peerFilePublic
                                     );

        static ForMessagesPtr getFromSignature(
                                               AccountPtr account,
                                               ElementPtr signedElement
                                               );

        virtual PUID getID() const = 0;

        virtual String getPeerURI() const = 0;

        virtual IPeerFilePublicPtr getPeerFilePublic() const = 0;

        virtual bool verifySignature(ElementPtr signedElement) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForPeerSubscription
      #pragma mark

      interaction IPeerForPeerSubscription
      {
        ZS_DECLARE_TYPEDEF_PTR(IPeerForPeerSubscription, ForPeerSubscription)

        static ElementPtr toDebug(ForPeerSubscriptionPtr peer);

        virtual AccountPtr getAccount() const = 0;

        virtual String getPeerURI() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerForPeerPublicationRepository
      #pragma mark

      interaction IPeerForPeerPublicationRepository
      {
        virtual String getPeerURI() const = 0;
      };



      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Peer
      #pragma mark

      class Peer : public Noop,
                   public SharedRecursiveLock,
                   public IPeer,
                   public IPeerForAccount,
                   public IPeerForLocation,
                   public IPeerForLocationDatabases,
                   public IPeerForMessages,
                   public IPeerForPeerSubscription,
                   public IPeerForPeerPublicationRepository
      {
      public:
        friend interaction IPeerFactory;
        friend interaction IPeerForAccount;
        friend interaction IPeerForMessages;
        friend interaction IPeerForLocationDatabases;
        friend interaction IPeerForPeerSubscription;

        friend interaction IPeer;
        friend interaction IPeerForLocation;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForPeer, UseAccount)

      protected:
        Peer(
             AccountPtr account,
             IPeerFilePublicPtr peerFilePublic,
             const String &peerURI
             );
        
        Peer(Noop) :
          Noop(true),
          SharedRecursiveLock(SharedRecursiveLock::create())
        {}

        void init();

      public:
        ~Peer();

        static PeerPtr convert(IPeerPtr peer);
        static PeerPtr convert(ForAccountPtr peer);
        static PeerPtr convert(ForMessagesPtr peer);
        static PeerPtr convert(ForLocationPtr peer);
        static PeerPtr convert(ForLocationDatabasesPtr peer);
        static PeerPtr convert(ForPeerSubscriptionPtr peer);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => IPeer
        #pragma mark

        static bool isValid(const char *peerURI);

        static bool splitURI(
                             const char *peerURI,
                             String &outDomain,
                             String &outContactID
                             );

        static String joinURI(
                              const char *domain,
                              const char *contactID
                              );

        static ElementPtr toDebug(IPeerPtr peer);

        static PeerPtr create(
                              AccountPtr account,
                              IPeerFilePublicPtr peerFilePublic
                              );

        // (duplicate) static IPeerPtr create(
        //                                    AccountPtr account,
        //                                    const char *peerURI
        //                                    );

        static PeerPtr getFromSignature(
                                        AccountPtr account,
                                        ElementPtr signedElement
                                        );

        virtual PUID getID() const  {return mID;}

        virtual String getPeerURI() const;

        virtual IPeerFilePublicPtr getPeerFilePublic() const;
        virtual bool verifySignature(ElementPtr signedElement) const;

        virtual PeerFindStates getFindState() const;

        virtual LocationListPtr getLocationsForPeer(bool includeOnlyConnectedLocations) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => IPeerForAccount
        #pragma mark

        // (duplicate) virtual String getPeerURI() const;

        // (duplicate) virtual IPeerFilePublicPtr getPeerFilePublic() const;

        virtual void setPeerFilePublic(IPeerFilePublicPtr peerFilePublic);

        // (duplciate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => IPeerForLocation
        #pragma mark

        static PeerPtr create(
                              AccountPtr account,
                              const char *peerURI
                              );

        virtual AccountPtr getAccount() const;

        // (duplicate) virtual String getPeerURI() const;

        virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => IPeerForLocation
        #pragma mark

        // (duplicate) static PeerPtr create(
        //                                   AccountPtr account,
        //                                   const char *peerURI
        //                                   );

        // (duplicate) virtual String getPeerURI() const = 0;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => IPeerForMessages
        #pragma mark

        // (duplicate) static PeerPtr create(
        //                                   AccountPtr account,
        //                                   const char *peerURI
        //                                   );

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual String getPeerURI() const;

        // (duplicate) virtual IPeerFilePublicPtr getPeerFilePublic() const;

        // (duplicate) virtual bool verifySignature(ElementPtr signedElement) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IPeerForPeerSubscription
        #pragma mark

        // (duplicate) virtual AccountPtr getAccount() const;

        // (duplicate) virtual String getPeerURI() const;

        // (duplciate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark IPeerForPeerPublicationRepository
        #pragma mark

        // (duplicate) virtual String getPeerURI() const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => (internal)
        #pragma mark

        RecursiveLock &getLock() const;

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Peer => (data)
        #pragma mark

        AutoPUID mID;
        PeerWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;

        IPeerFilePublicPtr mPeerFilePublic;

        String mPeerURI;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IPeerFactory
      #pragma mark

      interaction IPeerFactory
      {
        static IPeerFactory &singleton();

        virtual PeerPtr create(
                               AccountPtr account,
                               IPeerFilePublicPtr peerFilePublic
                               );
        virtual PeerPtr getFromSignature(
                                         AccountPtr account,
                                         ElementPtr signedElement
                                         );
        virtual PeerPtr create(
                               AccountPtr account,
                               const char *peerURI
                               );
      };

      class PeerFactory : public IFactory<IPeerFactory> {};

    }
  }
}
