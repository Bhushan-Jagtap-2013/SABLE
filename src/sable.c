/*
 * \brief   OSLO - Open Secure Loader
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

#include "version.h"
#include "dev.h"
#include "mp.h"
#include "elf.h"
#include "hmac.h"
#include "keyboard.h"
#include "string.h"
#include "tpm.h"
#include "tpm_error.h"
#include "tpm_struct.h"
#include "util.h"

#define SLB_PCR_ORD 17
#define MODULE_PCR_ORD 19

/***********************************************************
 * SABLE globals
 **********************************************************/

/* plaintext global secrets */
static struct {
  TPM_AUTHDATA pp_auth;
  TPM_AUTHDATA srk_auth;
  char passphrase[PASSPHRASE_STR_SIZE];
} secrets;

/* ciphertext global secrets */
static TPM_STORED_DATA12 pp_data;
static BYTE pp_blob[400];

static void configure(void) {
  /* local secrets */
  static struct {
    TPM_AUTHDATA nv_auth;
    TPM_SECRET sharedSecret;
  } lsecrets;
  /* other static locals */
  static TPM_OSAP_SESSION srk_osap_session;
  static TPM_SESSION nv_session;
  static TPM_SECRET encAuth;
  static BYTE pcr_select_bytes[3] = {0x0, 0x0, 0xa};
  static const TPM_PCR_SELECTION pcr_select = {
      .sizeOfSelect = sizeof(pcr_select_bytes), .pcrSelect = pcr_select_bytes};
  static TPM_PCRVALUE pcr_values[2];
  static BYTE pcr_info_packed[sizeof(TPM_STRUCTURE_TAG) +
                              2 * sizeof(TPM_LOCALITY_SELECTION) +
                              2 * (sizeof(pcr_select.sizeOfSelect) +
                                   sizeof(pcr_select_bytes)) +
                              2 * sizeof(TPM_COMPOSITE_HASH)];
  /* auto locals */
  Pack_Context pctx;
  TPM_RESULT res;

  // Construct pcr_info, which contains the TPM state conditions under which
  // the passphrase may be sealed/unsealed
  res = TPM_PCRRead(17, &pcr_values[0]);
  TPM_ERROR(res, "Failed to read PCR17");
  res = TPM_PCRRead(19, &pcr_values[1]);
  TPM_ERROR(res, "Failed to read PCR19");
  TPM_PCR_COMPOSITE composite = {.select = pcr_select,
                                 .valueSize = sizeof(pcr_values),
                                 .pcrValue = pcr_values};
  TPM_COMPOSITE_HASH composite_hash = get_TPM_COMPOSITE_HASH(composite);
  TPM_PCR_INFO_LONG pcr_info = {.tag = TPM_TAG_PCR_INFO_LONG,
                                .localityAtCreation = TPM_LOC_TWO,
                                .localityAtRelease = TPM_LOC_TWO,
                                .creationPCRSelection = pcr_select,
                                .releasePCRSelection = pcr_select,
                                .digestAtCreation = composite_hash,
                                .digestAtRelease = composite_hash};
  pack_init(&pctx, pcr_info_packed, sizeof(pcr_info_packed));
  marshal_TPM_PCR_INFO_LONG(&pcr_info, &pctx, NULL);
  UINT32 bytes_packed = pack_finish(&pctx);
  assert(bytes_packed == sizeof(pcr_info_packed));

  // get the passphrase, passphrase authdata, and SRK authdata
  out_string(s_Please_enter_the_passphrase);
  UINT32 lenPassphrase =
      get_string(secrets.passphrase, sizeof(secrets.passphrase) - 1, true) + 1;
  get_authdata(s_enter_passPhraseAuthData, &secrets.pp_auth);
  get_authdata(s_enter_srkAuthData, &secrets.srk_auth);

  // Initialize an OSAP session for the SRK
  res = TPM_GetRandom(srk_osap_session.nonceOddOSAP.nonce, sizeof(TPM_NONCE));
  TPM_ERROR(res, s_nonce_generation_failed);
  res = TPM_OSAP(TPM_ET_KEYHANDLE, TPM_KH_SRK, &srk_osap_session);
  TPM_ERROR(res, s_TPM_Start_OSAP);
  srk_osap_session.session.continueAuthSession = FALSE;

  // Generate the shared secret (for SRK authorization)
  sharedSecret_gen(&lsecrets.sharedSecret, &secrets.srk_auth,
                   &srk_osap_session.nonceEvenOSAP,
                   &srk_osap_session.nonceOddOSAP);

  // Generate nonceOdd
  res =
      TPM_GetRandom(srk_osap_session.session.nonceOdd.nonce, sizeof(TPM_NONCE));
  TPM_ERROR(res, s_nonce_generation_failed);

  // Encrypt the new passphrase authdata
  encAuth_gen(&encAuth, &secrets.pp_auth, &lsecrets.sharedSecret,
              &srk_osap_session.session.nonceEven);

  // Encrypt the passphrase using the SRK
  res = TPM_Seal(&pp_data, pp_blob, sizeof(pp_blob), TPM_KH_SRK, encAuth,
                 pcr_info_packed, sizeof(pcr_info_packed),
                 (const BYTE *)secrets.passphrase, lenPassphrase,
                 &srk_osap_session.session, &lsecrets.sharedSecret);
  TPM_ERROR(res, s_TPM_Seal);

  get_authdata(s_enter_nvAuthData, &lsecrets.nv_auth);
  res = TPM_OIAP(&nv_session);
  TPM_ERROR(res, s_TPM_Start_OIAP);

  res = TPM_NV_WriteValueAuth(pp_blob, sizeof(pp_blob), 0x04, 0,
                              &lsecrets.nv_auth, &nv_session);
  TPM_ERROR(res, s_TPM_NV_WriteValueAuth);
}

static void unsealPassphrase(void) {

  TPM_RESULT res;
  TPM_SESSION parent_oiap_session;
  TPM_SESSION entity_oiap_session;
  BYTE unsealedData[400];
  UINT32 unsealedData_size = NULL;
  TPM_STORED_DATA pp_data2;

  get_authdata(s_enter_srkAuthData, &secrets.srk_auth);
  get_authdata(s_enter_passPhraseAuthData, &secrets.pp_auth);

  res = TPM_NV_ReadValue(pp_data2.encData, 0x04, 0, sizeof(pp_blob), NULL, NULL);
  TPM_ERROR(res, s_TPM_NV_ReadValueAuth);

  //res = TPM_Start_OIAP(parent_oiap_session);
  res = TPM_OIAP(&parent_oiap_session);
  TPM_ERROR(res, s_TPM_Start_OIAP);

  //res = TPM_Start_OIAP(buffer, sctxEntity);
  res = TPM_OIAP(&entity_oiap_session);
  TPM_ERROR(res, s_TPM_Start_OIAP);

  //res = TPM_Unseal(buffer, sealedData, unsealedData, STRING_BUF_SIZE,
  //unsealedDataSize,
  //                 sctxParent, sctxEntity);
  res = TPM_Unseal(&pp_data2, unsealedData, unsealedData_size, TPM_KH_SRK,
		&secrets.srk_auth, &parent_oiap_session,
		&secrets.pp_auth, &entity_oiap_session);
  TPM_WARNING(res, s_TPM_Unseal);

  /*

  out_string(s_Please_confirm_that_the_passphrase);
  out_string(s_Passphrase);
  out_string((char *)unsealedData);

  out_string(s_If_this_is_correct);
 // get_string(3, true);

  //if (bufcmp(s_YES, string_buf, 3))
    //reboot();
  */
}

/**
 *  Hash all multiboot modules.
 */
static int mbi_calc_hash(struct mbi *mbi) {
  TPM_RESULT res;
  SHA1_Context sctx;

  CHECK3(-11, ~mbi->flags & MBI_FLAG_MODS, s_module_flag_missing);
  CHECK3(-12, !mbi->mods_count, s_no_module_to_hash);
  out_description(s_Hashing_modules_count, mbi->mods_count);

  struct module *m = (struct module *)(mbi->mods_addr);
  for (unsigned i = 0; i < mbi->mods_count; i++, m++) {
    sha1_init(&sctx);

    CHECK3(-13, m->mod_end < m->mod_start, s_mod_end_less_than_start);

    sha1(&sctx, (BYTE *)m->mod_start, m->mod_end - m->mod_start);
    sha1_finish(&sctx);
    res = TPM_Extend(MODULE_PCR_ORD, sctx.hash, NULL);
    TPM_ERROR(res, s_TPM_Extend);
  }

  return 0;
}

/**
 * Prepare the TPM for skinit.
 * Returns a TIS_INIT_* value.
 */
static int prepare_tpm(void) {
  int tpm;
  TPM_RESULT res;

  tpm = tis_init(TIS_BASE);

  CHECK4(-60, 0 >= tpm, s_tis_init_failed, tpm);
  CHECK3(-61, !tis_access(TIS_LOCALITY_0, 0), s_could_not_gain_tis_ownership);

  res = TPM_Startup(TPM_ST_CLEAR);
  if (res && res != TPM_E_INVALID_POSTINIT)
    TPM_ERROR(res, s_TPM_Startup_Clear);

  CHECK3(-62, tis_deactivate_all(), s_tis_deactivate_failed);

  return tpm;
}

/**
 * This function runs before skinit and has to enable SVM in the processor
 * and disable all localities.
 */
int _main(struct mbi *mbi, unsigned flags) {
  out_string(s_version_string);
  ERROR(10, !mbi || flags != MBI_MAGIC2, s_not_loaded_via_multiboot);

  // set bootloader name
  mbi->flags |= MBI_FLAG_BOOT_LOADER_NAME;
  mbi->boot_loader_name = (unsigned)s_version_string;

  int revision = check_cpuid();
  if (0 >= prepare_tpm() || (0 > revision)) {
    if (0 > revision)
      out_info(s_No_SVM_platform);
    else
      out_info(s_Could_not_prepare_TPM);

    ERROR(11, start_module(mbi), s_start_module_failed);
  }

  out_description(s_SVM_revision, revision);
  ERROR(12, enable_svm(), s_SVM_revision);
  ERROR(13, stop_processors(), s_sending_an_INIT_IPI);

#ifndef NDEBUG
  out_info(s_call_skinit);
  wait(1000);
#endif
  do_skinit();

  return 0;
}

/**
 * This code is executed after skinit.
 */
int sable(struct mbi *mbi) {
  revert_skinit();

  ERROR(20, !mbi, s_no_mbi_in_sable);

  if (tis_init(TIS_BASE)) {
    ERROR(21, !tis_access(TIS_LOCALITY_2, 0), s_could_not_gain_TIS_ownership);
    ERROR(22, mbi_calc_hash(mbi), s_calc_hash_failed);

#ifndef NDEBUG
    TPM_RESULT res;
    TPM_PCRVALUE pcr;

    res = TPM_PCRRead(SLB_PCR_ORD, &pcr);
    TPM_ERROR(res, s_TPM_PcrRead);
    show_hash(s_PCR17, pcr);

    res = TPM_PCRRead(MODULE_PCR_ORD, &pcr);
    TPM_ERROR(res, s_TPM_PcrRead);
    show_hash(s_PCR19, pcr);

    wait(1000);
#endif

    char config_str[2];
    out_string("Configure now? [y/n]: ");
    get_string(config_str, sizeof(config_str) - 1, true);
    if (config_str[0] == 'y') {
      configure();
      ERROR(25, tis_deactivate_all(), s_tis_deactivate_failed);
      out_string(s_Configuration_complete_Rebooting_now);
      wait(5000);
      reboot();
    } else {
      unsealPassphrase();
    }

    ERROR(25, tis_deactivate_all(), s_tis_deactivate_failed);
  }

  // FIXME: take a closer look at how we could do this better
  memset(&secrets, 0, sizeof(secrets));

  ERROR(27, start_module(mbi), s_start_module_failed);
  return 28;
}
