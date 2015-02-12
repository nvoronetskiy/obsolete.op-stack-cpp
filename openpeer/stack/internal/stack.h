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
#include <openpeer/stack/internal/stack_Account.h>
#include <openpeer/stack/internal/stack_BootstrappedNetwork.h>
#include <openpeer/stack/internal/stack_BootstrappedNetworkManager.h>
#include <openpeer/stack/internal/stack_Cache.h>
#include <openpeer/stack/internal/stack_Diff.h>
#include <openpeer/stack/internal/stack_IFinderRelayChannel.h>
#include <openpeer/stack/internal/stack_FinderRelayChannel.h>
#include <openpeer/stack/internal/stack_IFinderConnection.h>
#include <openpeer/stack/internal/stack_IFinderConnectionRelayChannel.h>
#include <openpeer/stack/internal/stack_FinderConnection.h>
#include <openpeer/stack/internal/stack_Helper.h>
#include <openpeer/stack/internal/stack_KeyGenerator.h>
#include <openpeer/stack/internal/stack_Location.h>
#include <openpeer/stack/internal/stack_LocationDatabase.h>
#include <openpeer/stack/internal/stack_LocationDatabaseLocalTearAway.h>
#include <openpeer/stack/internal/stack_LocationDatabaseTearAway.h>
#include <openpeer/stack/internal/stack_LocationDatabases.h>
#include <openpeer/stack/internal/stack_LocationDatabasesManager.h>
#include <openpeer/stack/internal/stack_LocationDatabasesTearAway.h>
#include <openpeer/stack/internal/stack_ILocationDatabaseAbstractionTables.h>
#include <openpeer/stack/internal/stack_LocationDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_MessageIncoming.h>
#include <openpeer/stack/internal/stack_MessageMonitor.h>
#include <openpeer/stack/internal/stack_MessageMonitorManager.h>
#include <openpeer/stack/internal/stack_Peer.h>
#include <openpeer/stack/internal/stack_PeerFiles.h>
#include <openpeer/stack/internal/stack_PeerFilePublic.h>
#include <openpeer/stack/internal/stack_PeerFilePrivate.h>
#include <openpeer/stack/internal/stack_PeerSubscription.h>
#include <openpeer/stack/internal/stack_Publication.h>
#include <openpeer/stack/internal/stack_PublicationMetaData.h>
#include <openpeer/stack/internal/stack_PublicationRepository.h>
#include <openpeer/stack/internal/stack_ServerMessaging.h>
#include <openpeer/stack/internal/stack_ServiceCertificatesValidateQuery.h>
#include <openpeer/stack/internal/stack_ServiceIdentitySession.h>
#include <openpeer/stack/internal/stack_ServiceLockboxSession.h>
#include <openpeer/stack/internal/stack_ServiceNamespaceGrantSession.h>
#include <openpeer/stack/internal/stack_ServicePeerFileLookup.h>
#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_ServicePushMailboxSessionDatabaseAbstraction.h>
#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstractionTables.h>
#include <openpeer/stack/internal/stack_ServicePushMailboxSession.h>
#include <openpeer/stack/internal/stack_ServiceSaltFetchSignedSaltQuery.h>
#include <openpeer/stack/internal/stack_Settings.h>
#include <openpeer/stack/internal/stack_Stack.h>
