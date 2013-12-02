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

#include <openpeer/stack/internal/stack.h>
#include <openpeer/stack/stack.h>

#include <openpeer/services/IHelper.h>

#include <zsLib/Log.h>
#include <zsLib/XML.h>
#include <zsLib/Stringize.h>

namespace openpeer { namespace stack { ZS_IMPLEMENT_SUBSYSTEM(openpeer_stack) } }

namespace openpeer
{
  namespace stack
  {
    using internal::Helper;
    typedef services::IHelper OPIHelper;
    using zsLib::string;
    using namespace zsLib::XML;

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark Candidate
    #pragma mark

    //-------------------------------------------------------------------------
    Candidate::Candidate() :
      IICESocket::Candidate()
    {
    }

    //-------------------------------------------------------------------------
    Candidate::Candidate(const Candidate &candidate) :
      IICESocket::Candidate(candidate)
    {
      mNamespace = candidate.mNamespace;
      mTransport = candidate.mTransport;

      mAccessToken = candidate.mAccessToken;
      mAccessSecretProof = candidate.mAccessSecretProof;
    }

    //-------------------------------------------------------------------------
    Candidate::Candidate(const IICESocket::Candidate &candidate) :
      IICESocket::Candidate(candidate)
    {
    }

    //-------------------------------------------------------------------------
    bool Candidate::hasData() const
    {
      bool hasData = IICESocket::Candidate::hasData();
      if (hasData) return true;

      return ((mNamespace.hasData()) ||
              (mTransport.hasData()) ||
              (mAccessToken.hasData()) ||
              (mAccessSecretProof.hasData()));
    }

    //-------------------------------------------------------------------------
    ElementPtr Candidate::toDebug() const
    {
      ElementPtr resultEl = Element::create("stack::Candidate");

      OPIHelper::debugAppend(resultEl, IICESocket::Candidate::toDebug());
      OPIHelper::debugAppend(resultEl, "class", mNamespace);

      OPIHelper::debugAppend(resultEl, "transport", mTransport);
      OPIHelper::debugAppend(resultEl, "access token", mAccessToken);
      OPIHelper::debugAppend(resultEl, "access secret proof", mAccessSecretProof);

      return resultEl;
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark LocationInfo
    #pragma mark

    //-------------------------------------------------------------------------
    bool LocationInfo::hasData() const
    {
      return (((bool)mLocation) ||
              (!mIPAddress.isEmpty()) ||
              (mDeviceID.hasData()) ||
              (mUserAgent.hasData()) ||
              (mOS.hasData()) ||
              (mSystem.hasData()) ||
              (mHost.hasData()) ||
              (mCandidates.size() > 0));
    }

    //-------------------------------------------------------------------------
    ElementPtr LocationInfo::toDebug() const
    {
      ElementPtr resultEl = Element::create("LocationInfo");

      OPIHelper::debugAppend(resultEl, ILocation::toDebug(mLocation));
      OPIHelper::debugAppend(resultEl, "IP addres", !mIPAddress.isEmpty() ? mIPAddress.string() : String());
      OPIHelper::debugAppend(resultEl, "device ID", mDeviceID);
      OPIHelper::debugAppend(resultEl, "user agent", mUserAgent);
      OPIHelper::debugAppend(resultEl, "os", mOS);
      OPIHelper::debugAppend(resultEl, "system", mSystem);
      OPIHelper::debugAppend(resultEl, "host", mHost);
      OPIHelper::debugAppend(resultEl, "candidates", mCandidates.size());

      return resultEl;
    }
  }
}
