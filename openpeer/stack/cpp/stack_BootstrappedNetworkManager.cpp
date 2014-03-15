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

#include <openpeer/stack/internal/stack_BootstrappedNetworkManager.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_Stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/helpers.h>
#include <zsLib/XML.h>

namespace openpeer { namespace stack { ZS_DECLARE_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    namespace internal
    {
      typedef IStackForInternal UseStack;

      using services::IHelper;

      typedef IBootstrappedNetworkManagerForBootstrappedNetwork::ForBootstrappedNetworkPtr ForBootstrappedNetworkPtr;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IBootstrappedNetworkManagerForBootstrappedNetwork
      #pragma mark

      //-----------------------------------------------------------------------
      ForBootstrappedNetworkPtr IBootstrappedNetworkManagerForBootstrappedNetwork::singleton()
      {
        return BootstrappedNetworkManager::singleton();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetworkManager
      #pragma mark

      //-----------------------------------------------------------------------
      BootstrappedNetworkManager::BootstrappedNetworkManager() :
        SharedRecursiveLock(SharedRecursiveLock::create())
      {
        ZS_LOG_DETAIL(log("created"))
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetworkManager::init()
      {
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkManager::~BootstrappedNetworkManager()
      {
        if(isNoop()) return;

        mThisWeak.reset();
        ZS_LOG_DETAIL(log("destroyed"))
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkManagerPtr BootstrappedNetworkManager::create()
      {
        BootstrappedNetworkManagerPtr pThis(new BootstrappedNetworkManager);
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetworkManager => IBootstrappedNetworkManagerForBootstrappedNetwork
      #pragma mark

      //-----------------------------------------------------------------------
      BootstrappedNetworkManagerPtr BootstrappedNetworkManager::singleton()
      {
        static SingletonLazySharedPtr<BootstrappedNetworkManager> singleton(IBootstrappedNetworkManagerFactory::singleton().createBootstrappedNetworkManager());
        BootstrappedNetworkManagerPtr result = singleton.singleton();
        if (!result) {
          ZS_LOG_WARNING(Detail, slog("singleton gone"))
        }
        return result;
      }

      //-----------------------------------------------------------------------
      BootstrappedNetworkPtr BootstrappedNetworkManager::findExistingOrUse(BootstrappedNetworkPtr inNetwork)
      {
        UseBootstrappedNetworkPtr network = inNetwork;

        AutoRecursiveLock lock(*this);

        String domain = network->getDomain();

        BootstrappedNetworkMap::iterator found = mBootstrappedNetworks.find(domain);
        if (found != mBootstrappedNetworks.end()) {
          ZS_LOG_DEBUG(log("using existing bootstrapped network object") + ZS_PARAM("domain", domain))
          UseBootstrappedNetworkPtr &result = (*found).second;
          return BootstrappedNetwork::convert(result);
        }

        ZS_LOG_DEBUG(log("using new bootstrapped network obejct") + ZS_PARAM("domain", domain))

        mBootstrappedNetworks[domain] = network;
        return BootstrappedNetwork::convert(network);
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetworkManager::registerDelegate(
                                                        BootstrappedNetworkPtr inNetwork,
                                                        IBootstrappedNetworkDelegatePtr inDelegate
                                                        )
      {
        UseBootstrappedNetworkPtr network = inNetwork;

        ZS_THROW_INVALID_ARGUMENT_IF(!network)
        ZS_THROW_INVALID_ARGUMENT_IF(!inDelegate)

        AutoRecursiveLock lock(*this);

        IBootstrappedNetworkDelegatePtr delegate = IBootstrappedNetworkDelegateProxy::createWeak(UseStack::queueDelegate(), inDelegate);

        if (network->isPreparationComplete()) {
          ZS_LOG_DEBUG(log("bootstrapper has already completed"))

          try {
            delegate->onBootstrappedNetworkPreparationCompleted(BootstrappedNetwork::convert(network));
          } catch(IBootstrappedNetworkDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
          return;
        }

        mPendingDelegates.push_back(PendingDelegatePair(network, delegate));
      }

      //-----------------------------------------------------------------------
      void BootstrappedNetworkManager::notifyComplete(BootstrappedNetworkPtr inNetwork)
      {
        UseBootstrappedNetworkPtr network = inNetwork;

        AutoRecursiveLock lock(*this);
        for (PendingDelegateList::iterator iter = mPendingDelegates.begin(); iter != mPendingDelegates.end(); )
        {
          PendingDelegateList::iterator current = iter;
          ++iter;

          UseBootstrappedNetworkPtr &pendingNetwork = (*current).first;

          if (network->getID() == pendingNetwork->getID()) {
            // this pending network is now complete...
            IBootstrappedNetworkDelegatePtr delegate = (*current).second;

            try {
              delegate->onBootstrappedNetworkPreparationCompleted(BootstrappedNetwork::convert(network));
            } catch(IBootstrappedNetworkDelegateProxy::Exceptions::DelegateGone &) {
              ZS_LOG_DEBUG(log("delegate gone"))
            }

            mPendingDelegates.erase(current);
          }
        }
      }

      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark BootstrappedNetworkManager => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      Log::Params BootstrappedNetworkManager::log(const char *message) const
      {
        ElementPtr objectEl = Element::create("BootstrappedNetworkManager");
        IHelper::debugAppend(objectEl, "id", mID);
        return Log::Params(message, objectEl);
      }

      //-----------------------------------------------------------------------
      Log::Params BootstrappedNetworkManager::slog(const char *message)
      {
        return Log::Params(message, "BootstrappedNetworkManager");
      }
    }
  }
}
