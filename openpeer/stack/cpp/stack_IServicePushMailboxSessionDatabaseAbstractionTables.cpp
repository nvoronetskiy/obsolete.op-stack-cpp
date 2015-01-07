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

#include <openpeer/stack/internal/stack_IServicePushMailboxSessionDatabaseAbstractionTables.h>

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
      #pragma mark IServicePushMailboxSessionDatabaseAbstractionTables
      #pragma mark

      const char *IServicePushMailboxSessionDatabaseAbstractionTables::version = "version";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::keyID = "keyID";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::lastDownloadedVersionForFolders = "lastDownloadedVersionForFolders";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::uniqueID = "uniqueID";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::folderName = "folderName";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::serverVersion = "serverVersion";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::downloadedVersion = "downloadedVersion";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::totalUnreadMessages = "totalUnreadMessages";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::totalMessages = "totalMessages";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::updateNext = "updateNext";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::indexFolderRecord = "indexFolderRecord";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::messageID = "messageID";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::removedFlag = "removedFlag";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::to = "_to";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::from = "_from";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::cc = "cc";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::bcc = "bcc";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::type = "type";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::mimeType = "mimeType";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::encoding = "encoding";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::pushType = "pushType";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::pushInfo = "pushInfo";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::sent = "sent";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::expires = "expires";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::encryptedDataLength = "encryptedDataLength";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::encryptedData = "encryptedData";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::decryptedData = "decryptedData";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::encryptedFileName = "encryptedFileName";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::decryptedFileName = "decryptedFileName";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::hasDecryptedData = "hasDecryptedData";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::downloadedEncryptedData = "downloadedEncryptedData";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::downloadFailures = "downloadFailures";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::downloadRetryAfter = "downloadRetryAfter";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::processedKey = "processedKey";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::decryptKeyID = "decryptKeyID";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::decryptLater = "decryptLater";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::decryptFailure = "decryptFailure";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::needsNotification = "needsNotification";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::indexMessageRecord = "indexMessageRecord";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::flag = "flag";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::uri = "uri";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::errorCode = "errorCode";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::errorReason = "errorReason";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::remoteFolder = "remoteFolder";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::copyToSent = "copyToSent";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::subscribeFlags = "subscribeFlags";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::listID = "listID";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::needsDownload = "needsDownload";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::failedDownload = "failedDownload";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::indexListRecord = "indexListRecord";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::order = "_order";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::keyDomain = "keyDomain";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::rsaPassphrase = "rsaPassphrase";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::dhPassphrase = "dhPassphrase";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::dhStaticPrivateKey = "dhStaticPrivateKey";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::dhStaticPublicKey = "dhStaticPublicKey";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::dhEphemeralPrivateKey = "dhEphemeralPrivateKey";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::dhEphemeralPublicKey = "dhEphemeralPublicKey";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::listSize = "listSize";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::totalWithDHPassphraseSet = "totalWithDHPassphraseSet";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::ackDHPassphraseSet = "ackDHPassphraseSet";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::activeUntil = "activeUntil";
      const char *IServicePushMailboxSessionDatabaseAbstractionTables::passphrase = "passphrase";
    }
  }
}
