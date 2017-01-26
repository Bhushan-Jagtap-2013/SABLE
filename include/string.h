#define DECLARE_STRING(name) extern const char *const s_##name
#define AUTHDATA_STR_SIZE 64
#define PASSPHRASE_STR_SIZE 128

DECLARE_STRING(WARNING);
DECLARE_STRING(dashes);
DECLARE_STRING(ERROR);
DECLARE_STRING(TPM_Start_OSAP);
DECLARE_STRING(TPM_Seal);
DECLARE_STRING(TPM_NV_DefineSpace);
DECLARE_STRING(TPM_Start_OIAP);
DECLARE_STRING(TPM_NV_WriteValueAuth);
DECLARE_STRING(TPM_Unseal);
DECLARE_STRING(Please_confirm_that_the_passphrase);
DECLARE_STRING(Passphrase);
DECLARE_STRING(If_this_is_correct);
DECLARE_STRING(YES);
DECLARE_STRING(module_flag_missing);
DECLARE_STRING(no_module_to_hash);
DECLARE_STRING(Hashing_modules_count);
DECLARE_STRING(config_magic_detected);
DECLARE_STRING(Please_enter_the_passphrase);
DECLARE_STRING(mod_end_less_than_start);
DECLARE_STRING(TPM_Extend);
DECLARE_STRING(tis_init_failed);
DECLARE_STRING(could_not_gain_tis_ownership);
DECLARE_STRING(TPM_Startup_Clear);
DECLARE_STRING(not_loaded_via_multiboot);
DECLARE_STRING(No_SVM_platform);
DECLARE_STRING(Could_not_prepare_TPM);
DECLARE_STRING(start_module_failed);
DECLARE_STRING(sending_an_INIT_IPI);
DECLARE_STRING(call_skinit);
DECLARE_STRING(SVM_revision);
DECLARE_STRING(nonce_generation_failed);
DECLARE_STRING(no_mbi_in_sable);
DECLARE_STRING(enter_srkAuthData);
DECLARE_STRING(enter_passPhraseAuthData);
DECLARE_STRING(could_not_gain_TIS_ownership);
DECLARE_STRING(TPM_PcrRead);
DECLARE_STRING(PCR17);
DECLARE_STRING(PCR19);
DECLARE_STRING(calc_hash_failed);
DECLARE_STRING(enter_nvAuthData);
DECLARE_STRING(tis_deactivate_failed);
DECLARE_STRING(Configuration_complete_Rebooting_now);
DECLARE_STRING(version_string);
