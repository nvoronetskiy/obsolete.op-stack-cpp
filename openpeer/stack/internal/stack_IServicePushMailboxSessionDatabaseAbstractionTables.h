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

#include <easySQLite/SqlField.h>
#include <easySQLite/SqlTable.h>
#include <easySQLite/SqlRecord.h>
#include <easySQLite/SqlValue.h>

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

      interaction IServicePushMailboxSessionDatabaseAbstractionTables
      {
        ZS_DECLARE_TYPEDEF_PTR(sql::Field, SqlField)
        ZS_DECLARE_TYPEDEF_PTR(sql::Table, SqlTable)
        ZS_DECLARE_TYPEDEF_PTR(sql::Record, SqlRecord)
        ZS_DECLARE_TYPEDEF_PTR(sql::Value, SqlValue)

        static const char *version;
        static const char *keyID;
        static const char *lastDownloadedVersionForFolders;
        static const char *uniqueID;
        static const char *folderName;
        static const char *serverVersion;
        static const char *downloadedVersion;
        static const char *totalUnreadMessages;
        static const char *totalMessages;
        static const char *updateNext;
        static const char *indexFolderRecord;
        static const char *messageID;
        static const char *removedFlag;
        static const char *to;
        static const char *from;
        static const char *cc;
        static const char *bcc;
        static const char *type;
        static const char *mimeType;
        static const char *encoding;
        static const char *pushType;
        static const char *pushInfo;
        static const char *sent;
        static const char *expires;
        static const char *encryptedDataLength;
        static const char *encryptedData;
        static const char *decryptedData;
        static const char *encryptedFileName;
        static const char *decryptedFileName;
        static const char *hasDecryptedData;
        static const char *downloadedEncryptedData;
        static const char *downloadFailures;
        static const char *downloadRetryAfter;
        static const char *processedKey;
        static const char *decryptKeyID;
        static const char *decryptLater;
        static const char *decryptFailure;
        static const char *needsNotification;
        static const char *indexMessageRecord;
        static const char *flag;
        static const char *uri;
        static const char *errorCode;
        static const char *errorReason;
        static const char *remoteFolder;
        static const char *copyToSent;
        static const char *subscribeFlags;
        static const char *listID;
        static const char *needsDownload;
        static const char *failedDownload;
        static const char *indexListRecord;
        static const char *order;
        static const char *keyDomain;
        static const char *rsaPassphrase;
        static const char *dhPassphrase;
        static const char *dhStaticPrivateKey;
        static const char *dhStaticPublicKey;
        static const char *dhEphemeralPrivateKey;
        static const char *dhEphemeralPublicKey;
        static const char *listSize;
        static const char *totalWithDHPassphraseSet;
        static const char *ackDHPassphraseSet;
        static const char *activeUntil;
        static const char *passphrase;


        //---------------------------------------------------------------------
        static SqlField *Version()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(version, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Version_name() {return "version";}

        //---------------------------------------------------------------------
        static SqlField *Settings()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(lastDownloadedVersionForFolders, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Settings_name() {return "settings";}

        //---------------------------------------------------------------------
        static SqlField *Folder()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(uniqueID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(folderName, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(serverVersion, sql::type_text, sql::flag_not_null),
            SqlField(downloadedVersion, sql::type_text, sql::flag_not_null),
            SqlField(totalUnreadMessages, sql::type_int, sql::flag_not_null),
            SqlField(totalMessages, sql::type_int, sql::flag_not_null),
            SqlField(updateNext, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Folder_name() {return "folder";}

        //---------------------------------------------------------------------
        static SqlField *FolderMessage()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexFolderRecord, sql::type_int, sql::flag_not_null),
            SqlField(messageID, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *FolderMessage_name() {return "folder_message";}

        //---------------------------------------------------------------------
        static SqlField *FolderVersionedMessage()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY_AUTOINCREMENT),
            SqlField(indexFolderRecord, sql::type_int, sql::flag_not_null),
            SqlField(messageID, sql::type_text, sql::flag_not_null),
            SqlField(removedFlag, sql::type_bool, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *FolderVersionedMessage_name() {return "folder_versioned_message";}

        //---------------------------------------------------------------------
        static SqlField *Message()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(messageID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(serverVersion, sql::type_text, sql::flag_not_null),
            SqlField(downloadedVersion, sql::type_text, sql::flag_not_null),
            SqlField(to, sql::type_text, sql::flag_not_null),
            SqlField(from, sql::type_text, sql::flag_not_null),
            SqlField(cc, sql::type_text, sql::flag_not_null),
            SqlField(bcc, sql::type_text, sql::flag_not_null),
            SqlField(type, sql::type_text, sql::flag_not_null),
            SqlField(mimeType, sql::type_text, sql::flag_not_null),
            SqlField(encoding, sql::type_text, sql::flag_not_null),
            SqlField(pushType, sql::type_text, sql::flag_not_null),
            SqlField(pushInfo, sql::type_text, sql::flag_not_null),
            SqlField(sent, sql::type_int, sql::flag_not_null),
            SqlField(expires, sql::type_int, sql::flag_not_null),
            SqlField(encryptedDataLength, sql::type_int, sql::flag_not_null),
            SqlField(encryptedData, sql::type_text, sql::flag_not_null),
            SqlField(decryptedData, sql::type_text, sql::flag_not_null),
            SqlField(encryptedFileName, sql::type_text, sql::flag_not_null),
            SqlField(decryptedFileName, sql::type_text, sql::flag_not_null),
            SqlField(hasDecryptedData, sql::type_bool, sql::flag_not_null),
            SqlField(downloadedEncryptedData, sql::type_bool, sql::flag_not_null),
            SqlField(downloadFailures, sql::type_int, sql::flag_not_null),
            SqlField(downloadRetryAfter, sql::type_int, sql::flag_not_null),
            SqlField(processedKey, sql::type_bool, sql::flag_not_null),
            SqlField(decryptKeyID, sql::type_text, sql::flag_not_null),
            SqlField(decryptLater, sql::type_bool, sql::flag_not_null),
            SqlField(decryptFailure, sql::type_bool, sql::flag_not_null),
            SqlField(needsNotification, sql::type_bool, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *Message_name() {return "message";}

        //---------------------------------------------------------------------
        static SqlField *MessageDeliveryState()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexMessageRecord, sql::type_int, sql::flag_not_null),
            SqlField(flag, sql::type_text, sql::flag_not_null),
            SqlField(uri, sql::type_text, sql::flag_not_null),
            SqlField(errorCode, sql::type_int, sql::flag_not_null),
            SqlField(errorReason, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *MessageDeliveryState_name() {return "message_delivery_state";}

        //---------------------------------------------------------------------
        static SqlField *PendingDeliveryMessage()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexMessageRecord, sql::type_int, sql::flag_not_null | sql::flag_unique),
            SqlField(remoteFolder, sql::type_text, sql::flag_not_null),
            SqlField(copyToSent, sql::type_bool, sql::flag_not_null),
            SqlField(subscribeFlags, sql::type_int, sql::flag_not_null),
            SqlField(encryptedDataLength, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *PendingDeliveryMessage_name() {return "pending_message_delivery";}

        //---------------------------------------------------------------------
        static SqlField *List()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(listID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(needsDownload, sql::type_bool, sql::flag_not_null),
            SqlField(downloadFailures, sql::type_int, sql::flag_not_null),
            SqlField(downloadRetryAfter, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *List_name() {return "list";}

        //---------------------------------------------------------------------
        static SqlField *ListURI()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(indexListRecord, sql::type_int, sql::flag_not_null),
            SqlField(order, sql::type_int, sql::flag_not_null),
            SqlField(uri, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *ListURI_name() {return "list_uri";}

        //---------------------------------------------------------------------
        static SqlField *KeyDomain()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(keyDomain, sql::type_int, sql::flag_not_null | sql::flag_unique),
            SqlField(dhStaticPrivateKey, sql::type_text, sql::flag_not_null),
            SqlField(dhStaticPublicKey, sql::type_text, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *KeyDomain_name() {return "key_domain";}

        //---------------------------------------------------------------------
        static SqlField *SendingKey()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(keyID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(uri, sql::type_text, sql::flag_not_null),
            SqlField(rsaPassphrase, sql::type_text, sql::flag_not_null),
            SqlField(dhPassphrase, sql::type_text, sql::flag_not_null),
            SqlField(keyDomain, sql::type_int, sql::flag_not_null),
            SqlField(dhEphemeralPrivateKey, sql::type_text, sql::flag_not_null),
            SqlField(dhEphemeralPublicKey, sql::type_text, sql::flag_not_null),
            SqlField(listSize, sql::type_int, sql::flag_not_null),
            SqlField(totalWithDHPassphraseSet, sql::type_int, sql::flag_not_null),
            SqlField(ackDHPassphraseSet, sql::type_text, sql::flag_not_null),
            SqlField(activeUntil, sql::type_int, sql::flag_not_null),
            SqlField(expires, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *SendingKey_name() {return "sending_key";}

        //---------------------------------------------------------------------
        static SqlField *ReceivingKey()
        {
          static SqlField table[] = {
            SqlField(sql::FIELD_KEY),
            SqlField(keyID, sql::type_text, sql::flag_not_null | sql::flag_unique),
            SqlField(uri, sql::type_text, sql::flag_not_null),
            SqlField(passphrase, sql::type_text, sql::flag_not_null),
            SqlField(keyDomain, sql::type_int, sql::flag_not_null),
            SqlField(dhEphemeralPrivateKey, sql::type_text, sql::flag_not_null),
            SqlField(dhEphemeralPublicKey, sql::type_text, sql::flag_not_null),
            SqlField(expires, sql::type_int, sql::flag_not_null),
            SqlField(sql::DEFINITION_END)
          };
          return table;
        }

        //---------------------------------------------------------------------
        static const char *ReceivingKey_name() {return "receiving_key";}

      };
    }
  }
}
