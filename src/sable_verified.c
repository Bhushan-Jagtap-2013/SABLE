#include "sable_verified.h"

#define PASSPHRASE_STR_SIZE 128
#define AUTHDATA_STR_SIZE 64

extern TPM_AUTHDATA get_authdata(void);
extern TPM_NONCE get_nonce(void);

/***********************************************************
 * SABLE globals
 **********************************************************/

static BYTE pp_blob[400];

/* TPM sessions */
static TPM_OSAP_SESSION srk_osap_session;
static TPM_SESSION nv_session;
static TPM_SESSION srk_session;
static TPM_SESSION pp_session;
#ifdef NV_OWNER_REQUIRED
static TPM_SESSION owner_session;
#endif

// Construct pcr_info, which contains the TPM state conditions under which
// the passphrase may be sealed/unsealed
TPM_PCR_INFO_LONG get_pcr_info(void) {
  TPM_PCRVALUE *pcr_values = alloc(2 * sizeof(TPM_PCRVALUE));
  BYTE *pcr_select_bytes = alloc(3);
  pcr_select_bytes[0] = 0x00;
  pcr_select_bytes[1] = 0x00;
  pcr_select_bytes[2] = 0x0a;
  TPM_PCR_SELECTION pcr_select = {.sizeOfSelect = sizeof(pcr_select_bytes),
                                  .pcrSelect = (BYTE *)pcr_select_bytes};
  struct TPM_PCRRead_ret pcr17 = TPM_PCRRead(17);
  TPM_ERROR(pcr17.returnCode, TPM_PCRRead);
  pcr_values[0] = pcr17.outDigest;
  struct TPM_PCRRead_ret pcr19 = TPM_PCRRead(19);
  TPM_ERROR(pcr19.returnCode, TPM_PCRRead);
  pcr_values[1] = pcr19.outDigest;
  TPM_PCR_COMPOSITE composite = {.select = pcr_select,
                                 .valueSize = sizeof(pcr_values),
                                 .pcrValue = (TPM_PCRVALUE *)pcr_values};
  TPM_COMPOSITE_HASH composite_hash = get_TPM_COMPOSITE_HASH(composite);
  TPM_PCR_INFO_LONG pcr_info = {.tag = TPM_TAG_PCR_INFO_LONG,
                                .localityAtCreation = TPM_LOC_TWO,
                                .localityAtRelease = TPM_LOC_TWO,
                                .creationPCRSelection = pcr_select,
                                .releasePCRSelection = pcr_select,
                                .digestAtCreation = composite_hash,
                                .digestAtRelease = composite_hash};
  return pcr_info;
}

void seal_passphrase(TPM_AUTHDATA srk_auth, TPM_AUTHDATA pp_auth,
                     UINT32 lenPassphrase) {
  TPM_RESULT res;

  // Initialize an OSAP session for the SRK
  srk_osap_session.nonceOddOSAP = get_nonce();
  res = TPM_OSAP(TPM_ET_KEYHANDLE, TPM_KH_SRK, &srk_osap_session);
  TPM_ERROR(res, TPM_Start_OSAP);
  srk_osap_session.session.continueAuthSession = FALSE;

  // Generate the shared secret (for SRK authorization)
  TPM_SECRET sharedSecret = sharedSecret_gen(
      srk_auth, srk_osap_session.nonceEvenOSAP, srk_osap_session.nonceOddOSAP);

  // Generate nonceOdd
  srk_osap_session.session.nonceOdd = get_nonce();

  // Encrypt the new passphrase authdata
  TPM_ENCAUTH encAuth =
      encAuth_gen(pp_auth, sharedSecret, srk_osap_session.session.nonceEven);

  TPM_PCR_INFO_LONG pcr_info = get_pcr_info();

  // Encrypt the passphrase using the SRK
  struct TPM_Seal_ret seal_ret =
      TPM_Seal(pp_blob, sizeof(pp_blob), TPM_KH_SRK, encAuth, pcr_info,
               (const BYTE *)passphrase, lenPassphrase,
               &srk_osap_session.session, sharedSecret);
  TPM_ERROR(seal_ret.returnCode, TPM_Seal);
}

void write_passphrase(TPM_AUTHDATA nv_auth) {
  TPM_RESULT res;

  res = TPM_OIAP(&nv_session);
  TPM_ERROR(res, TPM_Start_OIAP);

  res = TPM_NV_WriteValueAuth(pp_blob, sizeof(pp_blob), 0x04, 0, nv_auth,
                              &nv_session);
  TPM_ERROR(res, TPM_NV_WriteValueAuth);
}

void configure(void) {
  // get the passphrase, passphrase authdata, and SRK authdata
  EXCLUDE(out_string("Please enter the passphrase (" xstr(
      PASSPHRASE_STR_SIZE) " char max): ");)
  UINT32 lenPassphrase =
      get_string(passphrase, sizeof(passphrase) - 1, true) + 1;
  EXCLUDE(out_string("Please enter the passPhraseAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  TPM_AUTHDATA pp_auth = get_authdata();
  EXCLUDE(out_string("Please enter the srkAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  TPM_AUTHDATA srk_auth = get_authdata();

  // seal the passphrase to the pp_blob buffer
  seal_passphrase(srk_auth, pp_auth, lenPassphrase);

  EXCLUDE(out_string("Please enter the nvAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  TPM_AUTHDATA nv_auth = get_authdata();

  // write the sealed passphrase to disk
  write_passphrase(nv_auth);
}

TPM_STORED_DATA12 read_passphrase(void) {
  TPM_RESULT res;
  OPTION(TPM_AUTHDATA) nv_auth;

#ifdef NV_OWNER_REQUIRED
  EXCLUDE(out_string("Please enter the nvAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  nv_auth.value = get_authdata();
  nv_auth.hasValue = true;

  res = TPM_OIAP(&owner_session);
  TPM_ERROR(res, TPM_OIAP);

  res =
      TPM_NV_ReadValue(pp_blob, 404, 0, sizeof(pp_blob), nv_auth, &nv_session);
  TPM_ERROR(res, TPM_NV_ReadValue);
#else
  nv_auth.hasValue = false;
  res = TPM_NV_ReadValue(pp_blob, 4, 0, sizeof(pp_blob), nv_auth, NULL);
  TPM_ERROR(res, TPM_NV_ReadValue);
#endif

  return unpack_TPM_STORED_DATA12(pp_blob, sizeof(pp_blob));
}

void unseal_passphrase(TPM_AUTHDATA srk_auth, TPM_AUTHDATA pp_auth,
                       TPM_STORED_DATA12 sealed_pp) {
  TPM_RESULT res;

  res = TPM_OIAP(&srk_session);
  TPM_ERROR(res, TPM_OIAP);

  res = TPM_OIAP(&pp_session);
  TPM_ERROR(res, TPM_OIAP);

  res = TPM_Unseal(sealed_pp, (BYTE *)passphrase, sizeof(passphrase),
                   TPM_KH_SRK, srk_auth, &srk_session, pp_auth, &pp_session);
  TPM_ERROR(res, TPM_Unseal);
}

void trusted_boot(void) {
  TPM_STORED_DATA12 sealed_pp = read_passphrase();

  EXCLUDE(out_string("Please enter the passPhraseAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  TPM_AUTHDATA pp_auth = get_authdata();
  EXCLUDE(out_string("Please enter the srkAuthData (" xstr(
      AUTHDATA_STR_SIZE) " char max): ");)
  TPM_AUTHDATA srk_auth = get_authdata();

  unseal_passphrase(srk_auth, pp_auth, sealed_pp);

  EXCLUDE(out_string("Please confirm that the passphrase is correct:\n\n");)
  EXCLUDE(out_string(passphrase);)
  EXCLUDE(
      out_string("\n\nIf this is correct, please type YES in all capitals: ");)
  // get_string(3, true);

  // if (bufcmp(s_YES, string_buf, 3))
  // reboot();
}
