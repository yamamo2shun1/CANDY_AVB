/*
 * FILENAME: crypt_api.c
 *
 * Copyright 1999-2004 by InterNiche Technologies Inc. All rights reserved.
 *
 *
 * ROUTINES: 
 * ROUTINES: 
 *
 */
#ifndef _CRYPT_API_H_
#define _CRYPT_API_H_

#define PRINT_BUFFER

#define AES128_KEY_SIZE 16
#define AES128_IV_SIZE  16
#define DES64_KEY_SIZE 8
#define DES64_IV_SIZE  8
#define TDES192_KEY_SIZE 24
#define TDES192_IV_SIZE  8


#define MD5_DIGEST_LENGTH	16
#define SHA1_DIGEST_LENGTH	20
#define MAX_IV_SIZE		16	/* Max size of encryption IV */
#define MAX_BLOCK_SIZE		16	/* Max size of encryption block */
#define MAX_AUTH_DATA_SIZE	12	/* Bytes (96-bits) */
#define MAX_DIGEST_SIZE		64	/* SHA-512 digest size in bytes */
#define MAX_AUTH_KEY_SIZE	64	/* Max size of authentication key */
#define MAX_ENCR_KEY_SIZE	64	/* Max size of encryption key */
#define HMAC_MAX_CBLOCK     64
#define ENTROPY_NEEDED 20  /* require 160 bits = 20 bytes of randomness */

/*
 * IPSEC Algorithms
 */
/* ESP Transforms from RFC 2407 (IPSEC-DOI) */
#define ALG_ESP_DES_IV4		1
#define ALG_ESP_DES		    2
#define ALG_ESP_3DES		3
#define ALG_ESP_RC5		    4
#define ALG_ESP_IDEA		5
#define ALG_ESP_CAST	 	6
#define ALG_ESP_BLOWFISH	7
#define ALG_ESP_3IDEA		8
#define ALG_ESP_DES_IV32	9
#define ALG_ESP_RC4		    10
#define ALG_ESP_NULL	 	11
#define ALG_ESP_AES	 	    12

/* Private values used only at the API level. Not used internally */
#define ALG_ESP_AES_128	 	201
#define ALG_ESP_AES_192	 	202
#define ALG_ESP_AES_256	 	203

/* ESP Authentication Algorithms */
#define ALG_ESP_HMAC_NULL	  0	 /* private value, not defined by spec */
#define ALG_ESP_HMAC_MD5	  1
#define ALG_ESP_HMAC_SHA	  2
#define ALG_ESP_HMAC_SHA_256  5
#define ALG_ESP_HMAC_SHA_384  6
#define ALG_ESP_HMAC_SHA_512  7

/* AH Transforms from RFC 2407 (IPSEC-DOI) */
#define ALG_AH_NULL		  0
#define ALG_AH_MD5		  2
#define ALG_AH_SHA		  3
#define ALG_AH_DES		  4
#define ALG_AH_SHA_256	  5
#define ALG_AH_SHA_384	  6
#define ALG_AH_SHA_512	  7

/* Compression Algorithms from RFC 2407 (IPSEC-DOI) */
#define ALG_IPCOMP_OUI		1
#define ALG_IPCOMP_DEFLATE	2
#define ALG_IPCOMP_LZS		3

typedef struct DigestAlgs_s 
{
   int alg;
   int digest_len;
   int ctx_len;
   void (*init)(void *ctx);
   void (*update)(void *ctx, unsigned char *data, int len);
   void (*final)(unsigned char *digest, void *ctx);
} DigestAlgs;

typedef struct HmacContext_s 
{
	unsigned char ipad[64];
	unsigned char opad[64];
	const DigestAlgs *digest_alg;
	void *digest_ctx;
} HmacContext;

typedef struct KeyedContext_s 
{
	int	keylen;
	DigestAlgs	*digest_alg;
	void	*key;
	void	*digest_ctx;
} KeyedContext;

typedef struct EncrAlgs_s 
{
	int	alg;
	int	blocksize;	/* Algorithm block length (bytes) */
    int (*crypt)(unsigned char *in, unsigned int in_len, unsigned char *out, 
       unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);
} EncrAlgs;

EncrAlgs *enc_alg_lookup(int alg);
/*
 * Generic HMAC authentication APIs
 */
HmacContext	*hmac_init(int alg, unsigned char *key, int keylen);
void hmac_update(HmacContext *hctx, unsigned char *data, int len);
void hmac_final(HmacContext *hctx, unsigned char *digest, int len);
int	hmac_digest_len(HmacContext *hctx);

/*
 * Generic Keyed authentication APIs
 */
KeyedContext *keyed_init(int alg, unsigned char *key, int keylen);
void keyed_update(KeyedContext *kctx, unsigned char *data, int len);
void keyed_final(KeyedContext *kctx, unsigned char *digest, int len);
int	keyed_digest_len(KeyedContext *kctx);

int aes_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);
int bf_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);

int null_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);

int des_do_ncbc(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);

int des_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);

int tdes_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned int out_len, unsigned char *key, unsigned char *iv, int enc);

int hmac_ipsec_do(int alg_id, unsigned char *alg_key, int alg_keylen, void *in, 
   unsigned int in_offset, unsigned char *digest, unsigned int digest_len);

int keyed_do_ipsec(int alg_id, unsigned char *alg_key, int alg_keylen, void *in, 
   unsigned int in_offset, unsigned char *digest, unsigned int digest_len);

void md4_do(unsigned char *in, unsigned int in_len, unsigned char *digest);
char bits2hex(unsigned int num);
void convert2hex(char *out, char *msg, int size);
int sha1_do(unsigned char *digest, unsigned char *text, unsigned int text_len);
unsigned char *SHA1(const unsigned char *d, unsigned long n, unsigned char *md);
int sha1_do2(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, char *digest);
int sha1_do3(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, char *digest);
int sha1_do4(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, char *digest);
int sha1_do5(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, 
   unsigned char *s5, int s5_len, char *digest);
int sha1_do6(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, 
   unsigned char *s5, int s5_len, unsigned char *s6, int s6_len, char *digest);

int md5_do(unsigned char *digest, unsigned char *text, unsigned int text_len);
unsigned char *MD5(const unsigned char *d, unsigned long n, unsigned char *md);
int md5_do2(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, char *digest);
int md5_do3(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, char *digest);
int md5_do4(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, char *digest);
int md5_do5(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, 
   unsigned char *s5, int s5_len, char *digest);
int md5_do6(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, 
   unsigned char *s3, int s3_len, unsigned char *s4, int s4_len, 
   unsigned char *s5, int s5_len, unsigned char *s6, int s6_len, char *digest);
int des_do_ecb(unsigned char *in, unsigned char *out, unsigned char *key, int enc);

int sha256_do(unsigned char *digest, unsigned char *text, unsigned int text_len);
int sha256_do2(unsigned char *s1, int s1_len, unsigned char *s2, int s2_len, char *digest);
int sha384_do(unsigned char *digest, unsigned char *text, unsigned int text_len);
int sha512_do(unsigned char *digest, unsigned char *text, unsigned int text_len);

int p2k_md5(unsigned char *digest, unsigned char *text, unsigned int text_len);

#ifdef PRINT_BUFFER
void print_buffer(unsigned char *buf, int len);
#endif

void *alloc_md5_ctx(void);
void x_md5_init(void *ctx);
void x_md5_update(void *ctx, unsigned char *data, int len);
void x_md5_final(unsigned char *digest, void *ctx);

void *alloc_sha1_ctx(void);
void x_sha1_init(void *ctx);
void x_sha1_update(void *ctx, unsigned char *data, int len);
void x_sha1_final(unsigned char *digest, void *ctx);

void *alloc_sha256_ctx(void);
void x_sha256_init(void *ctx);
void x_sha256_update(void *ctx, unsigned char *data, int len);
void x_sha256_final(unsigned char *digest, void *ctx);

void *alloc_sha384_ctx(void);
void x_sha384_init(void *ctx);
void x_sha384_update(void *ctx, unsigned char *data, int len);
void x_sha384_final(unsigned char *digest, void *ctx);

void *alloc_sha512_ctx(void);
void x_sha512_init(void *ctx);
void x_sha512_update(void *ctx, unsigned char *data, int len);
void x_sha512_final(unsigned char *digest, void *ctx);

void hmac_md5(unsigned char* auth_key, int key_len, unsigned char* text, 
   int text_len, unsigned char* digest); 
void hmac_sha(unsigned char *k, int lk, unsigned char *d, int ld, 
   unsigned char *out, int t);

int hmac_do(int alg_id, unsigned char *alg_key, int alg_keylen, unsigned char *in, 
   unsigned int in_len, unsigned char *digest, unsigned int digest_len);

int hmac_do2(int alg_id, unsigned char *alg_key, int alg_keylen, unsigned char *s1, 
   unsigned int s1_len, unsigned char *s2, unsigned int s2_len, 
   unsigned char *digest, unsigned int digest_len);

int hmac_do3(int alg_id, unsigned char *alg_key, int alg_keylen, unsigned char *s1, 
   unsigned int s1_len, unsigned char *s2, unsigned int s2_len, 
   unsigned char *s3, unsigned int s3_len, 
   unsigned char *digest, unsigned int digest_len);

int rc4_do(unsigned char *in, unsigned int in_len, unsigned char *out, 
   unsigned char *key, unsigned key_len);

int concat_buffer(void *in, unsigned char **out, unsigned int in_offset, unsigned int *out_len);

void crypt_test(void);

#endif /* _CRYPT_API_H_ */
