#include "hmac.h"
#include "alloc.h"
#include "tcg.h"
#include "util.h"

#define HMAC_GEN(Type) void hmac_##Type(Type val) { \
  sha1_##Type(val); \
}

HMAC_GEN(BYTE);
HMAC_GEN(UINT16);
HMAC_GEN(UINT32);
HMAC_GEN(TPM_DIGEST);

struct HMAC_Context {
  BYTE key[HMAC_BLOCK_SIZE];
} hctx;

void do_xor(BYTE *in1, BYTE *in2, BYTE *out, UINT32 size) {
  for (UINT32 i = 0; i < size; i++)
    out[i] = in1[i] ^ in2[i];
}

void pad(BYTE *in, BYTE val, BYTE insize, BYTE outsize) {
  memset(in + insize, val, outsize - insize);
}

void hmac_init(const void *key_in, UINT32 key_size) {
  const BYTE *key = key_in;
  HMAC_OPad *ipad = alloc(heap, sizeof(HMAC_OPad), 0);

  memset(hctx.key, 0, HMAC_BLOCK_SIZE);
  memset(ipad->pad, 0x36, HMAC_BLOCK_SIZE);

  if (key_size <= HMAC_BLOCK_SIZE)
    memcpy(hctx.key, key, key_size);
  else {
    sha1_init();
    sha1_ptr(key, key_size);
    TPM_DIGEST hash = sha1_finish();
    memcpy(hctx.key, hash.bytes, TPM_SHA1_160_HASH_LEN);
  }

  do_xor(ipad->pad, hctx.key, ipad->pad, HMAC_BLOCK_SIZE);

  sha1_init();
  sha1_ptr(ipad->pad, HMAC_BLOCK_SIZE);

  // cleanup
  dealloc(heap, ipad, sizeof(HMAC_IPad));
}

void hmac_ptr(const void *data, UINT32 dataSize) {
  sha1_ptr(data, dataSize);
}

TPM_DIGEST hmac_finish(void) {
  HMAC_OPad *opad = alloc(heap, sizeof(HMAC_OPad), 0);
  TPM_DIGEST hash = sha1_finish();

  memset(opad->pad, 0x5c, HMAC_BLOCK_SIZE);
  do_xor(opad->pad, hctx.key, opad->pad, HMAC_BLOCK_SIZE);

  sha1_init();
  sha1_ptr(opad->pad, HMAC_BLOCK_SIZE);
  sha1_ptr(hash.bytes, TPM_SHA1_160_HASH_LEN);
  hash = sha1_finish();

  // cleanup
  dealloc(heap, opad, sizeof(HMAC_OPad));

  return hash;
}
