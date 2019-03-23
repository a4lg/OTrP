/* Copyright (c) Microsoft Corporation.  All Rights Reserved. */
#include <openenclave/enclave.h>
#include "OTrPAgent_t.h"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "TrustedApplication.h"
#define FILE void
extern "C" {
#include "jansson.h"
#include "joseinit.h"
#include "jose/b64.h"
#include "jose/jwe.h"
#include "jose/jwk.h"
#include "jose/jws.h"
#include "jose/openssl.h"
char* strdup(const char* str);
#include "../OTrPCommonTALib/common.h"
};
#include "../jansson/JsonAuto.h"
#include "openssl/bio.h"
#include "openssl/evp.h"
#include "openssl/x509.h"

#ifdef OE_USE_SGX
# define TEE_NAME "Intel SGX"
#endif

// List of TA's requested.
TrustedApplication* g_TARequestList = nullptr;

JsonAuto g_AgentSigningKey;

json_t* GetAgentSigningKey()
{
    if ((json_t*)g_AgentSigningKey == nullptr) {
        g_AgentSigningKey = CreateNewJwkRS256();
    }
    return (json_t*)g_AgentSigningKey;
}

const unsigned char* g_AgentDerCertificate = nullptr;
size_t g_AgentDerCertificateSize = 0;

const unsigned char* GetAgentDerCertificate(size_t *pCertLen)
{
    if (g_AgentDerCertificate == nullptr) {
        // Construct a self-signed DER certificate based on the JWK.

        // First get the RSA key.
        json_t* jwk = GetAgentSigningKey();
        g_AgentDerCertificate = GetDerCertificate(jwk, &g_AgentDerCertificateSize);
    }

    *pCertLen = g_AgentDerCertificateSize;
    return g_AgentDerCertificate;
}

/* Compose a DeviceStateInformation message. */
const char* ComposeDeviceStateInformation(void)
{
    JsonAuto object(json_object(), true);
    if ((json_t*)object == nullptr) {
        return nullptr;
    }

    JsonAuto dsi = object.AddObjectToObject("dsi");
    if ((json_t*)dsi == nullptr) {
        return nullptr;
    }

#ifndef OE_USE_SGX
    /* Add tfwdata. */
    JsonAuto tfwdata = dsi.AddObjectToObject("tfwdata");
    if (tfwdata == nullptr) {
        return nullptr;
    }
    if (tfwdata.AddStringToObject("tbs", "<TFW to be signed data is the tid>") == nullptr) {
        return nullptr;
    }
    if (tfwdata.AddStringToObject("cert", "<BASE64 encoded TFW certificate>") == nullptr) {
        return nullptr;
    }
    if (tfwdata.AddStringToObject("sigalg", "Signing method") == nullptr) {
        return nullptr;
    }
    if (tfwdata.AddStringToObject("sig", "<TFW signed data, BASE64 encoded>") == nullptr) {
        return nullptr;
    }
#endif

    /* Add tee. */
    JsonAuto tee = dsi.AddObjectToObject("tee");
    if (tee == nullptr) {
        return nullptr;
    }
    if (tee.AddStringToObject("name", TEE_NAME) == nullptr) {
        return nullptr;
    }
    if (tee.AddStringToObject("ver", "<TEE version>") == nullptr) {
        return nullptr;
    }
    size_t certLen;
    const unsigned char* cert = GetAgentDerCertificate(&certLen);
    json_t* certJson = jose_b64_enc(cert, certLen);
    if (tee.AddObjectToObject("cert", certJson) == nullptr) {
        return nullptr;
    }
    if (tee.AddObjectToObject("cacert", json_array()) == nullptr) {
        return nullptr;
    }

    // sdlist is optional, so we omit it.

    JsonAuto teeaiklist = tee.AddArrayToObject("teeaiklist");
    if (teeaiklist == nullptr) {
        return nullptr;
    }
    JsonAuto teeaik = teeaiklist.AddObjectToArray();
    if (teeaik == nullptr) {
        return nullptr;
    }
#if 0
    if (teeaik.AddStringToObject("spaik", "<SP AIK public key, BASE64 encoded>") == nullptr) {
        return nullptr;
    }
    if (teeaik.AddStringToObject("spaiktype", "RSA") == nullptr) { // RSA or ECC
        return nullptr;
    }
    if (teeaik.AddStringToObject("spid", "<sp id>") == nullptr) {
        return nullptr;
    }
#endif

    JsonAuto talist = tee.AddArrayToObject("talist");
    if (talist == nullptr) {
        return nullptr;
    }
#if 0
    // TODO: for each TA installed...
    {
        JsonAuto ta = talist.AddObjectToArray();
        if (ta == nullptr) {
            return nullptr;
        }
        if (ta.AddStringToObject("taid", "<TA application identifier>") == nullptr) {
            return nullptr;
        }
        // "taname" field is optional
    }
#endif

    JsonAuto tarequestlist = tee.AddArrayToObject("tarequestlist");
    if (tarequestlist == nullptr) {
        return nullptr;
    }
    for (TrustedApplication* ta = g_TARequestList; ta != nullptr; ta = ta->Next) {
        JsonAuto jta(tarequestlist.AddObjectToArray());
        if (jta == nullptr) {
            return nullptr;
        }
        if (jta.AddStringToObject("taid", ta->ID) == nullptr) {
            return nullptr;
        }
        if (ta->Name[0] != 0) {  // "taname" field is optional
            if (jta.AddStringToObject("taname", ta->Name) == nullptr) {
                return nullptr;
            }
        }
    }

    /* Convert to message buffer. */
    const char* message = json_dumps(object, 0);
    if (message == nullptr) {
        return nullptr;
    }
    return strdup(message);
}

json_t* AddEdsiToObject(JsonAuto& request, const json_t* jwke)
{
    const char* dsi = ComposeDeviceStateInformation();
    if (dsi == nullptr) {
        return nullptr;
    }
    size_t dsilen = strlen(dsi);

    JsonAuto jwe(json_object(), true);
    bool ok = jose_jwe_enc(
        nullptr,    // Configuration context (optional)
        jwe,     // The JWE object
        nullptr,    // The JWE recipient object(s) or nullptr
        jwke,    // The JWK(s) or JWKSet used for wrapping.
        dsi,     // The plaintext.
        dsilen); // The length of the plaintext.

    free((void*)dsi);
    dsi = nullptr;

    if (!ok) {
        return nullptr;
    }
    return request.AddObjectToObject("edsi", jwe);
}

int ecall_ProcessError(void* sessionHandle)
{
    // TODO: process transport error
    return 0;
}

int ecall_RequestPolicyCheck(void)
{
    // TODO: request policy check
    return 0;
}

/* Compose a GetDeviceTEEStateTBSResponse message. */
const char* ComposeGetDeviceTEEStateTBSResponse(
    const json_t* request,    // Request we're responding to.
    const char* statusValue,  // Status string to return.
    const json_t* jwk)       // Public key to encrypt with.
{
    /* Compose a GetDeviceStateResponse message. */
    JsonAuto object(json_object(), true);
    if (object == nullptr) {
        return nullptr;
    }
    JsonAuto response = object.AddObjectToObject("GetDeviceTEEStateTBSResponse");
    if (response == nullptr) {
        return nullptr;
    }
    if (response.AddStringToObject("ver", "1.0") == nullptr) {
        return nullptr;
    }
    if (response.AddStringToObject("status", statusValue) == nullptr) {
        return nullptr;
    }

    /* Copy rid from request. */
    json_t* rid = json_object_get(request, "rid");
    if (!json_is_string(rid) || (json_string_value(rid) == nullptr)) {
        return nullptr;
    }
    if (response.AddStringToObject("rid", json_string_value(rid)) == nullptr) {
        return nullptr;
    }

    /* Copy tid from request. */
    json_t* tid = json_object_get(request, "tid");
    if (!json_is_string(tid) || (json_string_value(tid) == nullptr)) {
        return nullptr;
    }
    if (response.AddStringToObject("tid", json_string_value(tid)) == nullptr) {
        return nullptr;
    }

    /* Support for signerreq false is optional, so pass true for now. */
    if (response.AddStringToObject("signerreq", "true") == nullptr) {
        return nullptr;
    }

    JsonAuto edsi = response.AddObjectToObject("edsi");
    if (edsi == nullptr) {
        return nullptr;
    }

    if (AddEdsiToObject(response, jwk) == nullptr) {
        return nullptr;
    }

    /* Convert to message buffer. */
    const char* message = json_dumps(object, 0);
    if (message == nullptr) {
        return nullptr;
    }

    return strdup(message);
}

/* Compose a GetDeviceTEEStateResponse message. */
json_t* ComposeGetDeviceTEEStateResponse(
    const json_t* request,    // Request we're responding to.
    const char* statusValue,  // Status string to return.
    const json_t* jwkTam,     // TAM Public key to encrypt with.
    const json_t* jwkAgent)   // Agent private key to sign with.
{
    /* Get a GetDeviceTEEStateTBSResponse. */
    const char* tbsResponse = ComposeGetDeviceTEEStateTBSResponse(request, statusValue, jwkTam);
    if (tbsResponse == nullptr) {
        return nullptr;
    }
#ifdef _DEBUG
    printf("Sending TBS: %s\n", tbsResponse);
#endif
    size_t len = strlen(tbsResponse);
    json_t* b64Response = jose_b64_enc(tbsResponse, len);
    free((void*)tbsResponse);
    if (b64Response == nullptr) {
        return nullptr;
    }

    // Create a signed message.
    JsonAuto jws(json_pack("{s:o}", "payload", b64Response, true));
    if ((json_t*)jws == nullptr) {
        return nullptr;
    }
    bool ok = jose_jws_sig(
        nullptr,    // Configuration context (optional)
        jws,     // The JWE object
        nullptr,
        jwkAgent);   // The JWK(s) or JWKSet used for wrapping.
    if (!ok) {
        return nullptr;
    }

    return jws.Detach();
}

// Compose a GetDeviceStateResponse message.
// Returns the message composed, or nullptr on error.
const char* ComposeGetDeviceStateResponse(
    const json_t* request,    // Request we're responding to.
    const char* statusValue,  // Status string to return.
    const json_t* jwkTam,     // TAM public key to encrypt with.
    const json_t* jwkAgent)   // Agent private key to sign with.
{
    JsonAuto jws(ComposeGetDeviceTEEStateResponse(request, statusValue, jwkTam, jwkAgent), true);
    if ((json_t*)jws == nullptr) {
        return nullptr;
    }

    /* Create the final GetDeviceStateResponse message. */
    JsonAuto object(json_object(), true);
    if ((json_t*)object == nullptr) {
        return nullptr;
    }
    JsonAuto dnlist = object.AddArrayToObject("GetDeviceStateResponse");
    if (dnlist == nullptr) {
        return nullptr;
    }
    JsonAuto dn = dnlist.AddObjectToArray();
    if (dn == nullptr) {
        return nullptr;
    }
    if (dn.AddObjectToObject("GetDeviceTEEStateResponse", jws) == nullptr) {
        return nullptr;
    }

    const char* message = json_dumps(object, 0);
    return message;
}

// Returns 0 on success, non-zero on error.
int OTrPHandleGetDeviceStateRequest(void* sessionHandle, const json_t* request)
{
    if (!json_is_object(request)) {
        return 1; /* Error */
    }

    int err = 1;
    oe_result_t result;
    const char* statusValue = "fail";

    /* 1.  Validate JSON message signing.  If it doesn't pass, an error message is returned. */
    char* payload = DecodeJWS(request, nullptr);
    if (!payload) {
        return 1; /* Error */
    }
#ifdef _DEBUG
    printf("Received TBS: %s\n", payload);
#endif
    json_error_t error;
    JsonAuto object(json_loads(payload, 0, &error), true);
    if ((json_t*)object == nullptr) {
        return 1;
    }

    /* 2.  Validate that the request TAM certificate is chained to a trusted
     *     CA that the TEE embeds as its trust anchor.
     *
     *     *  Cache the CA OCSP stapling data and certificate revocation
     *        check status for other subsequent requests.
     *
     *     *  A TEE can use its own clock time for the OCSP stapling data
     *        validation.
     */
    json_t* tbsRequest = json_object_get(object, "GetDeviceStateTBSRequest");
    if (tbsRequest == nullptr) {
        return 1;
    }

    // Get the TAM's cert from the request.
    // Each string in the x5c array is a base64 (not base64url) encoded DER certificate.
    json_t* header = json_object_get(request, "header");
    if (header == nullptr) {
        return 1;
    }
    json_t* x5c = json_object_get(header, "x5c");
    if (x5c == nullptr || !json_is_array(x5c) || json_array_size(x5c) == 0) {
        return 1;
    }
    json_t* x5celt = json_array_get(x5c, 0);
    size_t certSize = jose_b64_dec(x5celt, nullptr, 0);
    void* certBuffer = malloc(certSize);
    if (certBuffer == nullptr) {
        return 1;
    }
    if (jose_b64_dec(x5celt, certBuffer, certSize) != certSize) {
        free(certBuffer);
        return 1;
    }

    // TODO: Validate that the request TAM certificate is chained to a trusted
    //       CA that the TEE embeds as its trust anchor.

    // Create a JWK from the server's cert.

    // Read DER buffer into X509 structure per https://stackoverflow.com/questions/6689584/how-to-convert-the-certificate-string-into-x509-structure
    // since the openssl version we currently use does not have d2i_x509() directly.
    BIO* bio = BIO_new(BIO_s_mem());
    BIO_write(bio, certBuffer, certSize);
    X509* x509 = d2i_X509_bio(bio, nullptr);
    free(certBuffer);
    BIO_free(bio);

    EVP_PKEY *pkey = X509_get_pubkey(x509);
    RSA* rsa = EVP_PKEY_get1_RSA(pkey);
    JsonAuto jwkTemp(jose_openssl_jwk_from_RSA(nullptr, rsa), true);
    JsonAuto jwkTam(CopyToJweKey(jwkTemp, "RSA1_5"), true);
    EVP_PKEY_free(pkey);

    /* TODO: Cache the CA OCSP stapling data and certificate revocation
    *        check status for other subsequent requests.
    */

    /* 3.  Optionally collect Firmware signed data
     *
     *     *  This is a capability in ARM architecture that allows a TEE to
     *        query Firmware to get FW signed data.It isn't required for
     *        all TEE implementations.When TFW signed data is absent, it
     *        is up to a TAM's policy how it will trust a TEE.
     */
     /* Do nothing since this is optional. */

     /*
      * 4.  Collect SD information for the SD owned by this TAM
      */
      /* TODO */

    statusValue = "pass";

    json_t* jwkAgent = GetAgentSigningKey();
    const char* message = ComposeGetDeviceStateResponse(tbsRequest, statusValue, jwkTam, jwkAgent);
    if (message == nullptr) {
        return 1; /* Error */
    }

    printf("Sending GetDeviceStateResponse...\n");

    result = ocall_QueueOutboundOTrPMessage(&err, sessionHandle, message);
    if (result != OE_OK) {
        return result;
    }
    return err;
}

// Returns 0 on success, non-zero on error.
int OTrPHandleMessage(void* sessionHandle, const char* key, const json_t* messageObject)
{
    if (strcmp(key, "GetDeviceStateRequest") == 0) {
        return OTrPHandleGetDeviceStateRequest(sessionHandle, messageObject);
    }

    /* Unrecognized message. */
    return 1;
}

int ecall_RequestTA(
    const char* taid,
    const char* tamUri)
{
    int err = 0;
    oe_result_t result = OE_OK;
    size_t responseLength = 0;

    // TODO: See whether taid is already installed.
    // For now we skip this step and pretend it's not.
    bool isInstalled = false;

    if (isInstalled) {
        // Already installed, nothing to do.
        // This counts as "pass no data back" in the broker spec.
        return 0;
    }

    // See whether taid is already requested.
    TrustedApplication* ta;
    for (ta = g_TARequestList; ta != nullptr; ta = ta->Next) {
        if (strcmp(ta->ID, taid) == 0) {
            // Already requested, nothing to do.
            // This counts as "pass no data back" in the broker spec.
            return 0;
        }
    }

    // Add taid to the request list.
    ta = new TrustedApplication(taid);
    ta->Next = g_TARequestList;
    g_TARequestList = ta;

    // TODO: we may want to modify the TAM URI here.

    // TODO: see whether we already have a TAM cert we trust.
    // For now we skip this step and say we don't.
    bool haveTrustedTamCert = false;

    if (!haveTrustedTamCert) {
        // Pass back a TAM URI with no buffer.
        result = ocall_Connect(&err, tamUri);
        if (result != OE_OK) {
            return result;
        }
        if (err != 0) {
            return err;
        }
    } else {
        // TODO: implement going on to the next message.
        assert(false);
    }

    return err;
}