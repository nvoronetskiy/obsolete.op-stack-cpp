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
#include <openpeer/stack/ILocation.h>
#include <openpeer/stack/IMessageSource.h>

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      interaction IAccountForLocation;
      interaction IPeerForLocation;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForAccount
      #pragma mark

      interaction ILocationForAccount
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationForAccount, ForAccount)

        static ElementPtr toDebug(ForAccountPtr location);

        static ForAccountPtr getForLocal(AccountPtr account);
        static ForAccountPtr getForFinder(AccountPtr account);
        static ForAccountPtr getForPeer(
                                        PeerPtr peer,
                                        const char *locationID
                                        );

        virtual PUID getID() const = 0;

        virtual String getLocationID() const = 0;
        virtual LocationInfoPtr getLocationInfo() const = 0;

        virtual PeerPtr getPeer(bool internal = true) const = 0;

        virtual bool isConnected() const = 0;

        virtual PromisePtr send(message::MessagePtr message) const = 0;

        virtual String getPeerURI() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForLocationDatabases
      #pragma mark

      interaction ILocationForLocationDatabases
      {
        typedef ILocation::LocationTypes LocationTypes;

        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationDatabases, ForLocationDatabases)

        static ElementPtr toDebug(ForLocationDatabasesPtr location);

        virtual PUID getID() const = 0;

        virtual LocationTypes getLocationType() const = 0;

        virtual AccountPtr getAccount() const = 0;

        virtual ElementPtr toDebug() const = 0;

        virtual String getPeerURI() const = 0;
        virtual String getLocationID() const = 0;

        virtual bool isConnected() const = 0;

        virtual PromisePtr send(message::MessagePtr message) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForLocationSubscription
      #pragma mark

      interaction ILocationForLocationSubscription
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationForLocationSubscription, ForLocationSubscription)

        static ElementPtr toDebug(ForLocationSubscriptionPtr location);

        virtual PUID getID() const = 0;

        virtual AccountPtr getAccount() const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForMessages
      #pragma mark

      interaction ILocationForMessages
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationForMessages, ForMessages)

        static ForMessagesPtr convert(IMessageSourcePtr messageSource);

        static ElementPtr toDebug(ForMessagesPtr location);

        static ForMessagesPtr getForLocal(AccountPtr account);

        static ForMessagesPtr create(
                                     IMessageSourcePtr messageSource,
                                     const char *peerURI,
                                     const char *locationID
                                     );

        virtual String getLocationID() const = 0;

        virtual PeerPtr getPeer(bool internal = true) const = 0;

        virtual String getPeerURI() const = 0;

        virtual AccountPtr getAccount() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForPeerSubscription
      #pragma mark

      interaction ILocationForPeerSubscription
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationForPeerSubscription, ForPeerSubscription)

        virtual PeerPtr getPeer(bool internal = true) const = 0;

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForMessageMonitor
      #pragma mark

      interaction ILocationForMessageMonitor
      {
        virtual PromisePtr send(message::MessagePtr message) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForPublication
      #pragma mark

      interaction ILocationForPublication
      {
        static int locationCompare(
                                   const ILocationPtr &left,
                                   const ILocationPtr &right,
                                   const char * &outReason
                                   );

        virtual ElementPtr toDebug() const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationForPublicationRepository
      #pragma mark

      interaction ILocationForPublicationRepository
      {
        ZS_DECLARE_TYPEDEF_PTR(ILocationForPublicationRepository, ForPublicationRepository)

        typedef ILocation::LocationTypes LocationTypes;

        static int locationCompare(
                                   const ILocationPtr &left,
                                   const ILocationPtr &right,
                                   const char * &outReason
                                   );

        static LocationPtr getForLocal(AccountPtr account);

        virtual LocationTypes getLocationType() const = 0;

        virtual PeerPtr getPeer(bool internal = true) const = 0;

        virtual ElementPtr toDebug() const = 0;

        virtual PromisePtr send(message::MessagePtr message) const = 0;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Location
      #pragma mark

      class Location : public Noop,
                       public ILocation,
                       public ILocationForAccount,
                       public ILocationForLocationDatabases,
                       public ILocationForLocationSubscription,
                       public ILocationForMessages,
                       public ILocationForPeerSubscription,
                       public ILocationForMessageMonitor,
                       public ILocationForPublication,
                       public ILocationForPublicationRepository,
                       public IMessageSource
      {
      public:
        friend interaction ILocationFactory;
        friend interaction ILocation;
        friend interaction ILocationForLocationDatabases;
        friend interaction ILocationForLocationSubscription;
        friend interaction ILocationForMessages;

        ZS_DECLARE_TYPEDEF_PTR(IAccountForLocation, UseAccount)
        ZS_DECLARE_TYPEDEF_PTR(IPeerForLocation, UsePeer)

        typedef ILocation::LocationTypes LocationTypes;

      protected:
        Location(
                 UseAccountPtr account,
                 LocationTypes type,
                 UsePeerPtr peer,
                 const char *locationID
                 );
        
        Location(Noop) : Noop(true) {};

        void init();

      public:
        ~Location();

        static LocationPtr convert(ILocationPtr location);
        static LocationPtr convert(ForAccountPtr location);
        static LocationPtr convert(ForLocationDatabasesPtr location);
        static LocationPtr convert(ForLocationSubscriptionPtr location);
        static LocationPtr convert(ForMessagesPtr location);
        static LocationPtr convert(ForPeerSubscriptionPtr location);
        static LocationPtr convert(ForPublicationRepositoryPtr location);

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocation
        #pragma mark

        static ElementPtr toDebug(ILocationPtr location);

        static LocationPtr getForLocal(IAccountPtr account);
        static LocationPtr getForFinder(IAccountPtr account);
        static LocationPtr getForPeer(
                                      IPeerPtr peer,
                                      const char *locationID
                                      );
        static LocationPtr convert(IMessageSourcePtr messageSource);

        virtual PUID getID() const {return mID;}

        virtual LocationTypes getLocationType() const;

        virtual String getPeerURI() const;
        virtual String getLocationID() const;
        virtual LocationInfoPtr getLocationInfo() const;

        virtual IPeerPtr getPeer() const;

        virtual bool isConnected() const;
        virtual LocationConnectionStates getConnectionState() const;

        virtual PromisePtr send(message::MessagePtr message) const;

        virtual void hintNowAvailable();

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForAccount
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual String getLocationID() const;
        // (duplicate) virtual LocationInfoPtr getLocationInfo() const;

        // (duplicate) virtual PeerPtr getPeer(bool) const;

        // (duplicate) virtual bool isConnected() const;

        // (duplicate) virtual PromisePtr send(message::MessagePtr message) const;

        // (duplicate) virtual String getPeerURI() const;

        // (duplicate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForLocationDatabases
        #pragma mark

        // (duplicate) virtual PUID getID() const = 0;

        // (duplicate) virtual LocationTypes getLocationType() const;

        // (duplicate) virtual AccountPtr getAccount() const = 0;

        // (duplicate) virtual ElementPtr toDebug() const = 0;

        // (duplicate) virtual String getPeerURI() const = 0;
        // (duplicate) virtual String getLocationID() const = 0;

        // (duplicate) virtual bool isConnected() const = 0;

        // (duplicate) virtual PromisePtr send(message::MessagePtr message) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForLocationSubscription
        #pragma mark

        // (duplicate) virtual PUID getID() const;

        // (duplicate) virtual AccountPtr getAccount() const;

        // (duplicate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForMessages
        #pragma mark

        // (duplicate) static LocationPtr convert(IMessageSourcePtr messageSource);

        static LocationPtr create(
                                  IMessageSourcePtr messageSource,
                                  const char *peerURI,
                                  const char *locationID
                                  );

        virtual PeerPtr getPeer(bool) const;

        // (duplicate) virtual String getLocationID() const;
        // (duplicate) virtual String getPeerURI() const;

        virtual AccountPtr getAccount() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForPeerSubscription
        #pragma mark

        // (duplicate) virtual PeerPtr getPeer(bool) const;

        virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForMessageMonitor
        #pragma mark

        // (duplicate) virtual PromisePtr send(message::MessagePtr message) const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForPublication
        #pragma mark

        // (duplicate) virtual ElementPtr toDebug() const;

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => ILocationForPublicationRepository
        #pragma mark

        // (duplicate) virtual LocationTypes getLocationType() const;

        // (duplicate) virtual PeerPtr getPeer(bool) const;

        // (duplicate) virtual ElementPtr toDebug() const;

        // (duplicate) virtual PromisePtr send(message::MessagePtr message) const

        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => IMessageSource
        #pragma mark

        // (duplicate) virtual PUID getID() const

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => (internal)
        #pragma mark

        static Log::Params slog(const char *message);
        Log::Params log(const char *message) const;
        Log::Params debug(const char *message) const;

      protected:
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark Location => (data)
        #pragma mark

        PUID mID;
        LocationWeakPtr mThisWeak;

        UseAccountWeakPtr mAccount;
        LocationTypes mType;
        UsePeerPtr mPeer;
        String mLocationID;
      };

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark ILocationFactory
      #pragma mark

      interaction ILocationFactory
      {
        static ILocationFactory &singleton();

        virtual LocationPtr getForLocal(IAccountPtr account);
        virtual LocationPtr getForFinder(IAccountPtr account);
        virtual LocationPtr getForPeer(
                                       IPeerPtr peer,
                                       const char *locationID
                                       );
        virtual LocationPtr create(
                                   IMessageSourcePtr messageSource,
                                   const char *peerURI,
                                   const char *locationID
                                   );
      };

      class LocationFactory : public IFactory<ILocationFactory> {};

    }
  }
}
