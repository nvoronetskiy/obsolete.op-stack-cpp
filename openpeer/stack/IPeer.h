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

namespace openpeer
{
  namespace stack
  {
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IPeer
    #pragma mark

    interaction IPeer
    {
      enum PeerFindStates
      {
        PeerFindState_Pending,
        PeerFindState_Idle,
        PeerFindState_Finding,
        PeerFindState_Completed,
      };

      static const char *toString(PeerFindStates state);

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

      static IPeerPtr create(
                             IAccountPtr account,
                             IPeerFilePublicPtr peerFilePublic
                             );

      static IPeerPtr create(
                             IAccountPtr account,
                             const char *peerURI
                             );

      // NOTE: return null if the signature does not contain a proper peer URI
      static IPeerPtr getFromSignature(
                                       IAccountPtr account,
                                       ElementPtr signedElement
                                       );

      virtual PUID getID() const = 0;

      virtual String getPeerURI() const = 0;

      virtual IPeerFilePublicPtr getPeerFilePublic() const = 0;
      virtual bool verifySignature(ElementPtr signedElement) const = 0; // unless it has a public peer file it cannot validate

      virtual PeerFindStates getFindState() const = 0;

      virtual LocationListPtr getLocationsForPeer(bool includeOnlyConnectedLocations) const = 0;
    };
  }
}
