/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2003,2007,2010,2011  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/cryptodisk.h>
#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include <grub/err.h>
#include <grub/disk.h>
#include <grub/crypto.h>
#include <grub/partition.h>
#include <grub/i18n.h>

/* ---------------------------------------------------------------- begin secure boot extension -------------------------------------------------------------- */

#include "tpm/util.c"
#include "tpm/sable_tpm.h"
#include "tpm/hmac.c"
#include "tpm/tpm_error.h"
#include "tpm/asm.h"
#include "tpm/tis.c"
#include "tpm/sha.h"
#include "tpm/sha.c"
#include "tpm/tpm.c"

/* ---------------------------------------------------------------- end secure boot extension ---------------------------------------------------------------- */


GRUB_MOD_LICENSE ("GPLv3+");

#define MAX_PASSPHRASE 256

/* ---------------------------------------------------------------- begin secure boot extension -------------------------------------------------------------- */

#define MAX_COUNTERSIGN 192 //256(max passphrase value of Grub2, luks.c) - 64(max passphrase length for SABLE)
#define MAX_SECRET 256

/* ---------------------------------------------------------------- end secure boot extension ---------------------------------------------------------------- */

#define LUKS_KEY_ENABLED  0x00AC71F3

/* On disk LUKS header */
struct grub_luks_phdr
{
  grub_uint8_t magic[6];
#define LUKS_MAGIC        "LUKS\xBA\xBE"
  grub_uint16_t version;
  char cipherName[32];
  char cipherMode[32];
  char hashSpec[32];
  grub_uint32_t payloadOffset;
  grub_uint32_t keyBytes;
  grub_uint8_t mkDigest[20];
  grub_uint8_t mkDigestSalt[32];
  grub_uint32_t mkDigestIterations;
  char uuid[40];
  struct
  {
    grub_uint32_t active;
    grub_uint32_t passwordIterations;
    grub_uint8_t passwordSalt[32];
    grub_uint32_t keyMaterialOffset;
    grub_uint32_t stripes;
  } keyblock[8];
} GRUB_PACKED;

typedef struct grub_luks_phdr *grub_luks_phdr_t;

gcry_err_code_t AF_merge (const gcry_md_spec_t * hash, grub_uint8_t * src,
			  grub_uint8_t * dst, grub_size_t blocksize,
			  grub_size_t blocknumbers);

static grub_cryptodisk_t
configure_ciphers (grub_disk_t disk, const char *check_uuid,
		   int check_boot)
{
  grub_cryptodisk_t newdev;
  const char *iptr;
  struct grub_luks_phdr header;
  char *optr;
  char uuid[sizeof (header.uuid) + 1];
  char ciphername[sizeof (header.cipherName) + 1];
  char ciphermode[sizeof (header.cipherMode) + 1];
  char *cipheriv = NULL;
  char hashspec[sizeof (header.hashSpec) + 1];
  grub_crypto_cipher_handle_t cipher = NULL, secondary_cipher = NULL;
  grub_crypto_cipher_handle_t essiv_cipher = NULL;
  const gcry_md_spec_t *hash = NULL, *essiv_hash = NULL;
  const struct gcry_cipher_spec *ciph;
  grub_cryptodisk_mode_t mode;
  grub_cryptodisk_mode_iv_t mode_iv = GRUB_CRYPTODISK_MODE_IV_PLAIN64;
  int benbi_log = 0;
  grub_err_t err;

  if (check_boot)
    return NULL;

  /* Read the LUKS header.  */
  err = grub_disk_read (disk, 0, 0, sizeof (header), &header);
  if (err)
    {
      if (err == GRUB_ERR_OUT_OF_RANGE)
	grub_errno = GRUB_ERR_NONE;
      return NULL;
    }

  /* Look for LUKS magic sequence.  */
  if (grub_memcmp (header.magic, LUKS_MAGIC, sizeof (header.magic))
      || grub_be_to_cpu16 (header.version) != 1)
    return NULL;

  optr = uuid;
  for (iptr = header.uuid; iptr < &header.uuid[ARRAY_SIZE (header.uuid)];
       iptr++)
    {
      if (*iptr != '-')
	*optr++ = *iptr;
    }
  *optr = 0;

  if (check_uuid && grub_strcasecmp (check_uuid, uuid) != 0)
    {
      grub_dprintf ("luks", "%s != %s\n", uuid, check_uuid);
      return NULL;
    }

  /* Make sure that strings are null terminated.  */
  grub_memcpy (ciphername, header.cipherName, sizeof (header.cipherName));
  ciphername[sizeof (header.cipherName)] = 0;
  grub_memcpy (ciphermode, header.cipherMode, sizeof (header.cipherMode));
  ciphermode[sizeof (header.cipherMode)] = 0;
  grub_memcpy (hashspec, header.hashSpec, sizeof (header.hashSpec));
  hashspec[sizeof (header.hashSpec)] = 0;

  ciph = grub_crypto_lookup_cipher_by_name (ciphername);
  if (!ciph)
    {
      grub_error (GRUB_ERR_FILE_NOT_FOUND, "Cipher %s isn't available",
		  ciphername);
      return NULL;
    }

  /* Configure the cipher used for the bulk data.  */
  cipher = grub_crypto_cipher_open (ciph);
  if (!cipher)
    return NULL;

  if (grub_be_to_cpu32 (header.keyBytes) > 1024)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid keysize %d",
		  grub_be_to_cpu32 (header.keyBytes));
      grub_crypto_cipher_close (cipher);
      return NULL;
    }

  /* Configure the cipher mode.  */
  if (grub_strcmp (ciphermode, "ecb") == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_ECB;
      mode_iv = GRUB_CRYPTODISK_MODE_IV_PLAIN;
      cipheriv = NULL;
    }
  else if (grub_strcmp (ciphermode, "plain") == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_CBC;
      mode_iv = GRUB_CRYPTODISK_MODE_IV_PLAIN;
      cipheriv = NULL;
    }
  else if (grub_memcmp (ciphermode, "cbc-", sizeof ("cbc-") - 1) == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_CBC;
      cipheriv = ciphermode + sizeof ("cbc-") - 1;
    }
  else if (grub_memcmp (ciphermode, "pcbc-", sizeof ("pcbc-") - 1) == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_PCBC;
      cipheriv = ciphermode + sizeof ("pcbc-") - 1;
    }
  else if (grub_memcmp (ciphermode, "xts-", sizeof ("xts-") - 1) == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_XTS;
      cipheriv = ciphermode + sizeof ("xts-") - 1;
      secondary_cipher = grub_crypto_cipher_open (ciph);
      if (!secondary_cipher)
	{
	  grub_crypto_cipher_close (cipher);
	  return NULL;
	}
      if (cipher->cipher->blocksize != GRUB_CRYPTODISK_GF_BYTES)
	{
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "Unsupported XTS block size: %d",
		      cipher->cipher->blocksize);
	  grub_crypto_cipher_close (cipher);
	  grub_crypto_cipher_close (secondary_cipher);
	  return NULL;
	}
      if (secondary_cipher->cipher->blocksize != GRUB_CRYPTODISK_GF_BYTES)
	{
	  grub_crypto_cipher_close (cipher);
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "Unsupported XTS block size: %d",
		      secondary_cipher->cipher->blocksize);
	  grub_crypto_cipher_close (secondary_cipher);
	  return NULL;
	}
    }
  else if (grub_memcmp (ciphermode, "lrw-", sizeof ("lrw-") - 1) == 0)
    {
      mode = GRUB_CRYPTODISK_MODE_LRW;
      cipheriv = ciphermode + sizeof ("lrw-") - 1;
      if (cipher->cipher->blocksize != GRUB_CRYPTODISK_GF_BYTES)
	{
	  grub_error (GRUB_ERR_BAD_ARGUMENT, "Unsupported LRW block size: %d",
		      cipher->cipher->blocksize);
	  grub_crypto_cipher_close (cipher);
	  return NULL;
	}
    }
  else
    {
      grub_crypto_cipher_close (cipher);
      grub_error (GRUB_ERR_BAD_ARGUMENT, "Unknown cipher mode: %s",
		  ciphermode);
      return NULL;
    }

  if (cipheriv == NULL);
  else if (grub_memcmp (cipheriv, "plain", sizeof ("plain") - 1) == 0)
      mode_iv = GRUB_CRYPTODISK_MODE_IV_PLAIN;
  else if (grub_memcmp (cipheriv, "plain64", sizeof ("plain64") - 1) == 0)
      mode_iv = GRUB_CRYPTODISK_MODE_IV_PLAIN64;
  else if (grub_memcmp (cipheriv, "benbi", sizeof ("benbi") - 1) == 0)
    {
      if (cipher->cipher->blocksize & (cipher->cipher->blocksize - 1)
	  || cipher->cipher->blocksize == 0)
	grub_error (GRUB_ERR_BAD_ARGUMENT, "Unsupported benbi blocksize: %d",
		    cipher->cipher->blocksize);
	/* FIXME should we return an error here? */
      for (benbi_log = 0; 
	   (cipher->cipher->blocksize << benbi_log) < GRUB_DISK_SECTOR_SIZE;
	   benbi_log++);
      mode_iv = GRUB_CRYPTODISK_MODE_IV_BENBI;
    }
  else if (grub_memcmp (cipheriv, "null", sizeof ("null") - 1) == 0)
      mode_iv = GRUB_CRYPTODISK_MODE_IV_NULL;
  else if (grub_memcmp (cipheriv, "essiv:", sizeof ("essiv:") - 1) == 0)
    {
      char *hash_str = cipheriv + 6;

      mode_iv = GRUB_CRYPTODISK_MODE_IV_ESSIV;

      /* Configure the hash and cipher used for ESSIV.  */
      essiv_hash = grub_crypto_lookup_md_by_name (hash_str);
      if (!essiv_hash)
	{
	  grub_crypto_cipher_close (cipher);
	  grub_crypto_cipher_close (secondary_cipher);
	  grub_error (GRUB_ERR_FILE_NOT_FOUND,
		      "Couldn't load %s hash", hash_str);
	  return NULL;
	}
      essiv_cipher = grub_crypto_cipher_open (ciph);
      if (!essiv_cipher)
	{
	  grub_crypto_cipher_close (cipher);
	  grub_crypto_cipher_close (secondary_cipher);
	  return NULL;
	}
    }
  else
    {
      grub_crypto_cipher_close (cipher);
      grub_crypto_cipher_close (secondary_cipher);
      grub_error (GRUB_ERR_BAD_ARGUMENT, "Unknown IV mode: %s",
		  cipheriv);
      return NULL;
    }

  /* Configure the hash used for the AF splitter and HMAC.  */
  hash = grub_crypto_lookup_md_by_name (hashspec);
  if (!hash)
    {
      grub_crypto_cipher_close (cipher);
      grub_crypto_cipher_close (essiv_cipher);
      grub_crypto_cipher_close (secondary_cipher);
      grub_error (GRUB_ERR_FILE_NOT_FOUND, "Couldn't load %s hash",
		  hashspec);
      return NULL;
    }

  newdev = grub_zalloc (sizeof (struct grub_cryptodisk));
  if (!newdev)
    {
      grub_crypto_cipher_close (cipher);
      grub_crypto_cipher_close (essiv_cipher);
      grub_crypto_cipher_close (secondary_cipher);
      return NULL;
    }
  newdev->cipher = cipher;
  newdev->offset = grub_be_to_cpu32 (header.payloadOffset);
  newdev->source_disk = NULL;
  newdev->benbi_log = benbi_log;
  newdev->mode = mode;
  newdev->mode_iv = mode_iv;
  newdev->secondary_cipher = secondary_cipher;
  newdev->essiv_cipher = essiv_cipher;
  newdev->essiv_hash = essiv_hash;
  newdev->hash = hash;
  newdev->log_sector_size = 9;
  newdev->total_length = grub_disk_get_size (disk) - newdev->offset;
  grub_memcpy (newdev->uuid, uuid, sizeof (newdev->uuid));
  newdev->modname = "luks";
  COMPILE_TIME_ASSERT (sizeof (newdev->uuid) >= sizeof (uuid));
  return newdev;
}

static grub_err_t
luks_recover_key (grub_disk_t source,
		  grub_cryptodisk_t dev)
{
  struct grub_luks_phdr header;
  grub_size_t keysize;
  grub_uint8_t *split_key = NULL;
  char passphrase[MAX_PASSPHRASE] = "";
  grub_uint8_t candidate_digest[sizeof (header.mkDigest)];
  unsigned i;
  grub_size_t length;
  grub_err_t err;
  grub_size_t max_stripes = 1;
  char *tmp;

  err = grub_disk_read (source, 0, 0, sizeof (header), &header);
  if (err)
    return err;

//  grub_puts_ (N_("Attempting to decrypt master key..."));
  keysize = grub_be_to_cpu32 (header.keyBytes);
  if (keysize > GRUB_CRYPTODISK_MAX_KEYLEN)
    return grub_error (GRUB_ERR_BAD_FS, "key is too long");

  for (i = 0; i < ARRAY_SIZE (header.keyblock); i++)
    if (grub_be_to_cpu32 (header.keyblock[i].active) == LUKS_KEY_ENABLED
	&& grub_be_to_cpu32 (header.keyblock[i].stripes) > max_stripes)
      max_stripes = grub_be_to_cpu32 (header.keyblock[i].stripes);

  split_key = grub_malloc (keysize * max_stripes);
  if (!split_key)
    return grub_errno;

/* ---------------------------------------------------------------- begin secure boot extension -------------------------------------------------------------- */

#if 0

  /* Get the passphrase from the user.  */
  tmp = NULL;
  if (source->partition)
    tmp = grub_partition_get_name (source->partition);
  grub_printf_ (N_("Enter passphrase for %s%s%s (%s): "), source->name,
	       source->partition ? "," : "", tmp ? : "",
	       dev->uuid);
  grub_free (tmp);
  if (!grub_password_get (passphrase, MAX_PASSPHRASE))
    {
      grub_free (split_key);
      return grub_error (GRUB_ERR_BAD_ARGUMENT, "Passphrase not supplied");
    }

#endif

	grub_size_t secretSize = 0;
	char secret[MAX_SECRET] = "";

	if (tis_init(TIS_BASE))
	{
		if (tis_access(TIS_LOCALITY_2, 0) == 0)
		{
			grub_printf("\nCould not acquire requested locality\n");
			return GRUB_ERR_IO;
		}
		else
		{
			TPM_RESULT res;

			BYTE *in_buffer = grub_malloc(TCG_BUFFER_SIZE);
			if (in_buffer == NULL)
			{
				grub_printf("\nCould not allocate memory for 'in_buffer'\n");
				return GRUB_ERR_IO;
			}
			grub_memset(in_buffer, 0, sizeof(stTPM_PCRREAD));
	
			SessionCtx *sctx = grub_malloc(sizeof(SessionCtx));
			if (sctx == NULL)
			{
				grub_printf("\nCould not allocate memory for 'sctx'\n");
				grub_free(in_buffer);
				return GRUB_ERR_IO;
			}
			grub_memset(sctx, 0, sizeof(SessionCtx));
		  
			SessionCtx *sctxParent = grub_malloc(sizeof(SessionCtx));
			if (sctxParent == NULL)
			{
				grub_printf("\nCould not allocate memory for 'sctxParent'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				return GRUB_ERR_IO;
			}
			grub_memset(sctxParent, 0, sizeof(SessionCtx));
		
			SessionCtx *sctxEntity = grub_malloc(sizeof(SessionCtx));
			if (sctxEntity == NULL)
			{
				grub_printf("\nCould not allocate memory for 'sctxEntity'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				return GRUB_ERR_IO;
			}
			grub_memset(sctxEntity, 0, sizeof(SessionCtx));
		
			char *entry = grub_malloc(sizeof(char));
			if (entry == NULL)
			{
				grub_printf("\nCould not allocate memory for 'entry'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				grub_free(sctxEntity);
				return GRUB_ERR_IO;
			}
			grub_memset(entry, 0, sizeof(char));

			BYTE *usageAuthSRK = grub_malloc(20);	
			if (usageAuthSRK == NULL)
			{
				grub_printf("\nCould not allocate memory for 'usageAuthSRK'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				grub_free(sctxEntity);
				grub_free(entry);
				return GRUB_ERR_IO;
			}
			grub_memset(usageAuthSRK, 0, 20);

			BYTE *sealedData = grub_malloc(400);
			if (sealedData == NULL)
			{
				grub_printf("\nCould not allocate memory for 'sealedData'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				grub_free(sctxEntity);
				grub_free(entry);
				grub_free(usageAuthSRK);
				return GRUB_ERR_IO;
			}
			grub_memset(sealedData, 0, 400);

			BYTE *unsealedData = grub_malloc(100);
			if (unsealedData == NULL)
			{
				grub_printf("\nCould not allocate memory for 'unsealedData'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				grub_free(sctxEntity);
				grub_free(entry);
				grub_free(usageAuthSRK);
				grub_free(sealedData);
				return GRUB_ERR_IO;
			}
			grub_memset(unsealedData, 0, 100);

			UINT32 *unsealedDataSize = grub_malloc(sizeof(UINT32));
			if (unsealedDataSize == NULL)
			{
				grub_printf("\nCould not allocate memory for 'unsealedDataSize'\n");
				grub_free(in_buffer);
				grub_free(sctx);
				grub_free(sctxParent);
				grub_free(sctxEntity);
				grub_free(entry);
				grub_free(usageAuthSRK);
				grub_free(sealedData);
				grub_free(unsealedData);
				return GRUB_ERR_IO;
			}
			grub_memset(unsealedDataSize, 0, sizeof(UINT32));

			res = TPM_Start_OIAP(in_buffer, sctx);

			if (res != 0)
			{
				grub_printf("\nTPM_Start_OIAP error, res is %lu\n", res);
				goto cleanup;			
			}

			res = TPM_NV_ReadValueAuth(in_buffer, sealedData, 400, sctx);
			if (res != 0)
			{
				grub_printf("\nTPM_NV_ReadValueAuth error, res is %lu\n", res);
				goto cleanup;
			}		
			res = TPM_Start_OIAP(in_buffer, sctxParent);
			if (res != 0)
			{
				grub_printf("\nTPM_Start_OIAP error (2nd), res is %lu\n", res);
				goto cleanup;
			}
			res = TPM_Start_OIAP(in_buffer, sctxEntity);
			if (res != 0)
			{
				grub_printf("\nTPM_Start_OIAP error (3rd), res is %lu\n", res);
				goto cleanup;
			}
			
			// Get Data Password and SRK Password from User
			char dataPass[sizeof(TPM_AUTHDATA)];
			grub_printf("Please enter the passPhraseAuthData\n");
			grub_password_get(dataPass, sizeof(TPM_AUTHDATA));
			int resData = strnlen_sable(dataPass, sizeof(TPM_AUTHDATA));
			
			char srkPass[sizeof(TPM_AUTHDATA)];
			grub_printf("Please enter the srkAuthData\n");
			grub_password_get(srkPass, sizeof(TPM_AUTHDATA));
			int resSRK = strnlen_sable(srkPass, sizeof(TPM_AUTHDATA));
			
			// SHA1 hash both passwords, and pass to TPM_Unseal
			TPM_AUTHDATA dataHash;
			TPM_AUTHDATA srkHash;
			
			grub_printf("Hashing Data Pass\n");
			struct SHA1_Context dctx;
			sha1_init(&dctx);
			sha1(&dctx, (BYTE *)dataPass, resData);
			sha1_finish(&dctx);
			dataHash = *(TPM_AUTHDATA *)&dctx.hash; 
			grub_printf("Hashed Data Pass\n");
			
			grub_printf("Hashing SRK Pass\n");
			struct SHA1_Context rctx;
			sha1_init(&rctx);
			sha1(&rctx, (BYTE *)srkPass, resSRK);
			sha1_finish(&rctx);
			srkHash = *(TPM_AUTHDATA *)&rctx.hash; 
			grub_printf("Hashed SRK Pass\n");
			
			res = TPM_Unseal(in_buffer, sealedData, unsealedData, 100, unsealedDataSize, sctxParent, sctxEntity, &dataHash, &srkHash);
			if (res != 0)
			{
				grub_printf("\nTPM_Unseal error, res is %lu\n", res);
				goto cleanup;
			}

			grub_printf("Unsealing passphrase from the TPM...\n\n\n");
			grub_printf("Passphrase: ");
	
			grub_printf("%s\n\n", unsealedData);
                                                                                                                //
			grub_printf("Please confirm that the passphrase presented above matches the one which was entered during\n");
			grub_printf("system configuration. If the passphrase does not match, contact your systems administrator\n");
			grub_printf("immediately. Otherwise proceed to provide the countersign.\n\n");

			char countersign[MAX_COUNTERSIGN] = "";
	
			grub_printf("\nEnter countersign (190 characters max): ");
			grub_password_get(countersign, MAX_COUNTERSIGN);
			
			grub_printf("\n\nAttempting to open encrypted container...\n\n");

			secretSize = (*unsealedDataSize - 1) + grub_strlen(countersign);

			if ( secretSize > MAX_SECRET )
			{
				grub_printf("\nMaximum secret length exceeded\n");
				goto cleanup;
			}
		
			const BYTE *sp = unsealedData;
			UINT32 i;
			for (i = 0; i < *unsealedDataSize-1 ; i++)
		  {
  		  secret[i] = *sp;
  		  sp++;
  		}
			for (UINT32 c = 0; c < (grub_strlen(countersign)) ; c++)
  		{
    		secret[i+c] = countersign[c];
  		}
		
cleanup:
			grub_free(in_buffer);
		  grub_free(sctx);
  		grub_free(sctxParent);
		  grub_free(sctxEntity);
			grub_free(entry);
		  grub_free(usageAuthSRK);
		  grub_free(sealedData);
		  grub_free(unsealedData);
  		grub_free(unsealedDataSize);
		}
	}
	else
	{
		grub_printf("tis_init() failed");
		return GRUB_ERR_IO;
	}

/* ---------------------------------------------------------------- end secure boot extension ---------------------------------------------------------------- */

  /* Try to recover master key from each active keyslot.  */
  for (i = 0; i < ARRAY_SIZE (header.keyblock); i++)
    {
      gcry_err_code_t gcry_err;
      grub_uint8_t candidate_key[GRUB_CRYPTODISK_MAX_KEYLEN];
      grub_uint8_t digest[GRUB_CRYPTODISK_MAX_KEYLEN];

      /* Check if keyslot is enabled.  */
      if (grub_be_to_cpu32 (header.keyblock[i].active) != LUKS_KEY_ENABLED)
	continue;

      grub_dprintf ("luks", "Trying keyslot %d\n", i);

      /* Calculate the PBKDF2 of the unsealed passphrase/user-supplied countersign.  */
      gcry_err = grub_crypto_pbkdf2 (dev->hash, (grub_uint8_t *) secret,
				     secretSize,
				     header.keyblock[i].passwordSalt,
				     sizeof (header.keyblock[i].passwordSalt),
				     grub_be_to_cpu32 (header.keyblock[i].
						       passwordIterations),
				     digest, keysize);

      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      grub_dprintf ("luks", "PBKDF2 done\n");

      gcry_err = grub_cryptodisk_setkey (dev, digest, keysize); 
      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      length = (keysize * grub_be_to_cpu32 (header.keyblock[i].stripes));

      /* Read and decrypt the key material from the disk.  */
      err = grub_disk_read (source,
			    grub_be_to_cpu32 (header.keyblock
					      [i].keyMaterialOffset), 0,
			    length, split_key);
      if (err)
	{
	  grub_free (split_key);
	  return err;
	}

      gcry_err = grub_cryptodisk_decrypt (dev, split_key, length, 0);
      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      /* Merge the decrypted key material to get the candidate master key.  */
      gcry_err = AF_merge (dev->hash, split_key, candidate_key, keysize,
			   grub_be_to_cpu32 (header.keyblock[i].stripes));
      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      grub_dprintf ("luks", "candidate key recovered\n");

      /* Calculate the PBKDF2 of the candidate master key.  */
      gcry_err = grub_crypto_pbkdf2 (dev->hash, candidate_key,
				     grub_be_to_cpu32 (header.keyBytes),
				     header.mkDigestSalt,
				     sizeof (header.mkDigestSalt),
				     grub_be_to_cpu32
				     (header.mkDigestIterations),
				     candidate_digest,
				     sizeof (candidate_digest));
      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      /* Compare the calculated PBKDF2 to the digest stored
         in the header to see if it's correct.  */
      if (grub_memcmp (candidate_digest, header.mkDigest,
		       sizeof (header.mkDigest)) != 0)
	{
	  grub_dprintf ("luks", "bad digest\n");
	  continue;
	}

      /* TRANSLATORS: It's a cryptographic key slot: one element of an array
	 where each element is either empty or holds a key.  */
      grub_printf_ (N_("Slot %d opened\n"), i);

      /* Set the master key.  */
      gcry_err = grub_cryptodisk_setkey (dev, candidate_key, keysize); 
      if (gcry_err)
	{
	  grub_free (split_key);
	  return grub_crypto_gcry_error (gcry_err);
	}

      grub_free (split_key);

      return GRUB_ERR_NONE;
    }

  grub_free (split_key);
  return GRUB_ACCESS_DENIED;
}

struct grub_cryptodisk_dev luks_crypto = {
  .scan = configure_ciphers,
  .recover_key = luks_recover_key
};

GRUB_MOD_INIT (luks)
{
  COMPILE_TIME_ASSERT (sizeof (((struct grub_luks_phdr *) 0)->uuid)
		       < GRUB_CRYPTODISK_MAX_UUID_LENGTH);
  grub_cryptodisk_dev_register (&luks_crypto);
}

GRUB_MOD_FINI (luks)
{
  grub_cryptodisk_dev_unregister (&luks_crypto);
}
