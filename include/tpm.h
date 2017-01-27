/*
 * \brief   macros, enums and headers for tpm.c
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006,2007,2010  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

typedef struct {
  TPM_AUTHHANDLE authHandle;
  TPM_NONCE nonceEven;
  TPM_NONCE nonceOdd;
  TPM_BOOL continueAuthSession;
} TPM_SESSION;

typedef struct {
  TPM_SESSION session;
  TPM_NONCE nonceEvenOSAP;
  TPM_NONCE nonceOddOSAP;
} TPM_OSAP_SESSION;

///////////////////////////////////////////////////////////////////////////
TPM_RESULT TPM_Startup(TPM_STARTUP_TYPE startupType_in);
TPM_RESULT TPM_GetRandom(BYTE *randomBytes_out /* out */,
                         UINT32 bytesRequested_in);
typedef struct {
  TPM_RESULT returnCode;
  TPM_PCRVALUE outDigest;
} TPM_PCRRead_t;
TPM_PCRRead_t TPM_PCRRead(TPM_PCRINDEX pcrIndex_in);
TPM_RESULT TPM_Extend(TPM_PCRINDEX pcrNum_in, TPM_DIGEST inDigest_in,
                      TPM_PCRVALUE *outDigest_out /* out */);
TPM_RESULT TPM_OIAP(TPM_SESSION *session /* out */);
TPM_RESULT TPM_OSAP(TPM_ENTITY_TYPE entityType_in, UINT32 entityValue_in,
                    TPM_OSAP_SESSION *osap_session /* out */);
TPM_RESULT TPM_NV_WriteValueAuth(const BYTE *data_in, UINT32 dataSize_in,
                                 TPM_NV_INDEX nvIndex_in, UINT32 offset_in,
                                 const TPM_AUTHDATA *nv_auth,
                                 TPM_SESSION *session);
TPM_RESULT TPM_NV_ReadValue(BYTE *data_out /* out */, TPM_NV_INDEX nvIndex_in,
                            UINT32 offset_in, UINT32 dataSize_in,
                            const TPM_AUTHDATA *ownerAuth_in,
                            TPM_SESSION *session);
TPM_RESULT TPM_Extend(TPM_PCRINDEX pcrNum_in, TPM_DIGEST inDigest_in,
                      TPM_PCRVALUE *outDigest_out /* out */);
TPM_RESULT
TPM_Unseal(const TPM_STORED_DATA *inData_in /* in */,
           BYTE *secret_out /* out */, UINT32 secretSizeMax,
           TPM_KEY_HANDLE parentHandle_in, const TPM_AUTHDATA *parentAuth,
           TPM_SESSION *parentSession, const TPM_AUTHDATA *dataAuth,
           TPM_SESSION *dataSession);
typedef struct {
  TPM_RESULT returnCode;
  TPM_STORED_DATA12 sealedData;
} TPM_Seal_t;
TPM_Seal_t TPM_Seal(BYTE *rawData /* out */, UINT32 rawDataSize,
                    TPM_KEY_HANDLE keyHandle_in, TPM_ENCAUTH encAuth_in,
                    const void *pcrInfo_in, UINT32 pcrInfoSize_in,
                    const BYTE *inData_in, UINT32 inDataSize_in,
                    TPM_SESSION *session, const TPM_SECRET *sharedSecret);
