/* Copyright (c) Microsoft Corporation.  All Rights Reserved. */
#pragma once

#define OTRP_JSON_MEDIA_TYPE "application/otrp+json"
#define TEEP_JSON_MEDIA_TYPE "application/teep+json"

int OTrPHandleJsonMessage(void* sessionHandle, const char* message, unsigned int messageLength);
int TeepHandleJsonMessage(void* sessionHandle, const char* message, unsigned int messageLength);

char *DecodeJWS(const json_t *jws, const json_t *jwk);

json_t* CreateNewJwkRS256(void);
json_t* CreateNewJwkR1_5(void);
json_t* CreateNewJwk(const char* alg);
json_t* CopyToJweKey(json_t* jwk1, const char* alg);

const unsigned char* GetDerCertificate(json_t* jwk, size_t *pCertificateSize);

typedef enum {
    TEEP_QUERY_REQUEST       = 1,
    TEEP_QUERY_RESPONSE      = 2,
    TEEP_TRUSTED_APP_INSTALL = 3,
    TEEP_TRUSTED_APP_DELETE  = 4,
    TEEP_SUCCESS             = 5,
    TEEP_ERROR               = 6
} teep_message_type_t;

typedef enum {
    ERR_ILLEGAL_PARAMETER          = 1,
    ERR_UNSUPPORTED_EXTENSION      = 2,
    ERR_REQUEST_SIGNATURE_FAILED   = 3,
    ERR_UNSUPPORTED_MSG_VERSION    = 4,
    ERR_UNSUPPORTED_CRYPTO_ALG     = 5,
    ERR_BAD_CERTIFICATE            = 6,
    ERR_UNSUPPORTED_CERTIFICATE    = 7,
    ERR_CERTIFICATE_REVOKED        = 8,
    ERR_CERTIFICATE_EXPIRED        = 9,
    ERR_INTERNAL_ERROR             = 10,
    ERR_RESOURCE_FULL              = 11,
    ERR_TA_NOT_FOUND               = 12,
    ERR_TA_ALREADY_INSTALLED       = 13,
    ERR_TA_UNKNOWN_FORMAT          = 14,
    ERR_TA_DECRYPTION_FAILED       = 15,
    ERR_TA_DECOMPRESSION_FAILED    = 16,
    ERR_MANIFEST_PROCESSING_FAILED = 17,
    ERR_PD_PROCESSING_FAILED       = 18
} teep_error_code_t;

typedef enum {
    TEEP_ATTESTATION    = 1,
    TEEP_TRUSTED_APPS   = 2,
    TEEP_EXTENSIONS     = 3,
    TEEP_SUIT_COMMANDS  = 4
} teep_data_items_t;
