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

#ifndef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_OUTGOING
#include <openpeer/stack/internal/stack_PublicationRepository.h>
#else

#if 0
namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      class PublicationRepository : ...
#endif //0

        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        //---------------------------------------------------------------------
        #pragma mark
        #pragma mark PublicationRepository::PeerSubscriptionOutgoing
        #pragma mark

        class PeerSubscriptionOutgoing : public MessageQueueAssociator,
                                         public SharedRecursiveLock,
                                         public IPublicationSubscription,
                                         public IMessageMonitorDelegate
        {
        public:
          typedef IPublicationMetaData::SubscribeToRelationshipsMap SubscribeToRelationshipsMap;

          friend class PublicationRepository;

        protected:
          PeerSubscriptionOutgoing(
                                   IMessageQueuePtr queue,
                                   PublicationRepositoryPtr outer,
                                   IPublicationSubscriptionDelegatePtr delegate,
                                   UsePublicationMetaDataPtr subscriptionInfo
                                   );

          void init();

        public:
          ~PeerSubscriptionOutgoing();

          static PeerSubscriptionOutgoingPtr convert(IPublicationSubscriptionPtr subscription);

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => friend PublicationRepository
          #pragma mark

          static ElementPtr toDebug(PeerSubscriptionOutgoingPtr subscription);

          static PeerSubscriptionOutgoingPtr create(
                                                    IMessageQueuePtr queue,
                                                    PublicationRepositoryPtr outer,
                                                    IPublicationSubscriptionDelegatePtr delegate,
                                                    UsePublicationMetaDataPtr subscriptionInfo
                                                    );

          // (duplicate) PUID getID() const;
          // (duplicate) virtual void cancel();

          // (duplicate) virtual IPublicationMetaDataPtr getSource() const;

          void setMonitor(IMessageMonitorPtr monitor);
          void notifyUpdated(UsePublicationMetaDataPtr metaData);

        protected:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IPublicationSubscription
          #pragma mark

          // (duplicate) virtual void cancel();

          virtual PublicationSubscriptionStates getState() const;
          virtual IPublicationMetaDataPtr getSource() const;

          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => IMessageMonitorDelegate
          #pragma mark

          virtual bool handleMessageMonitorMessageReceived(
                                                           IMessageMonitorPtr monitor,
                                                           message::MessagePtr message
                                                           );

          virtual void onMessageMonitorTimedOut(IMessageMonitorPtr monitor);

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => (internal)
          #pragma mark

          PUID getID() const {return mID;}
          Log::Params log(const char *message) const;

          virtual ElementPtr toDebug() const;

          bool isPending() const {return PublicationSubscriptionState_Pending == mCurrentState;}
          bool isEstablished() const {return PublicationSubscriptionState_Established == mCurrentState;}
          bool isShuttingDown() const {return PublicationSubscriptionState_ShuttingDown == mCurrentState;}
          bool isShutdown() const {return PublicationSubscriptionState_Shutdown == mCurrentState;}

          void setState(PublicationSubscriptionStates state);

          void cancel();

        private:
          //-------------------------------------------------------------------
          #pragma mark
          #pragma mark PublicationRepository::PeerSubscriptionOutgoing => (data)
          #pragma mark

          AutoPUID mID;
          PublicationRepositoryWeakPtr mOuter;

          PeerSubscriptionOutgoingWeakPtr mThisWeak;
          PeerSubscriptionOutgoingPtr mGracefulShutdownReference;

          IPublicationSubscriptionDelegatePtr mDelegate;

          UsePublicationMetaDataPtr mSubscriptionInfo;

          IMessageMonitorPtr mMonitor;
          IMessageMonitorPtr mCancelMonitor;

          bool mSucceeded;
          WORD mErrorCode;
          String mErrorReason;

          PublicationSubscriptionStates mCurrentState;
        };

#if 0
      };

    }
  }
}
#endif //0

#endif //ndef OPENPEER_STACK_PUBLICATION_REPOSITORY_PEER_SUBSCRIPTION_OUTGOING
