/*
 * FILENAME: crypt_port.h
 *
 * Copyright 2002-2003 InterNiche Technologies Inc.  All rights reserved.
 *
 * Crypt library configuration.
 *
 * MODULE: IPSECCRYPTO
 *
 * PORTABLE: yes
 */
#ifndef _CRYPT_PORT_H_
#define _CRYPT_PORT_H_

/* You enable the defines that you need in your particular application
and undef the rest of the algorithms to reduce code space. */
/* These are the highest level defines */

#define USE_MD5                                1
#define USE_SHA1                               1
#define USE_MD4                                1
#define USE_3DES                               1
#define USE_DES                                1
#define USE_RC4                                1
#define USE_BF                                 1
#define USE_AES                                1
#define USE_SHA2                               1

#define USE_HMAC        	               1

#ifdef NOT_USED
Make sure "NOT_USED" is really undefined                 

/* Note SSL must be enables for this because it resides in SSL directory */
#define USE_SSL_DSA                            1

#endif 

/* This define is used in CryptoEngine. It might help the performance. But
this is totally platform dependent */

#define KICK_CRYPTO_ENGINE  1

/* This define compiles in the CryptoEngine demo. When you have IPSEC defined
and USE_SEC_HARDWARE make sure this is undefed because with this macro defined
different IDs are passed to hardware accelerator and therefore IPSEC would not
work properly. In real applications there is no need for INCLUDE_CE_DEMO anyway
and it should be undefed */

#define INCLUDE_CE_DEMO    1

/* This macro compiles in the hardware Security engine driver and links it
to the application code. For example it should be turned on in MCF5235 and MCF5485
platforms */

/*#define USE_SEC_HARDWARE   1*/

/* Note that most of the time
either _HW or _SW macros for algorithms are enabled. But in some cases
depending on product configuration both macros might be enabled */

/* AES is isolated pretty good in current applicatiion code and
probably it is safe to either enable software or hardware support (assuming it is 
available) for AES  */

#ifdef USE_AES
#ifdef USE_SEC_HARDWARE
#define USE_AES_HW         1
#else
#define USE_AES_SW         1
#endif
#endif

#ifdef USE_DES

#ifdef USE_SEC_HARDWARE
#define USE_DES_HW         1
#endif

#define USE_DES_3DES      1

#endif /* USE_DES */

/* In the current code base 3DES is also pretty clean and
perhaps it is safe to define either _HW or _SW for 3DES 
algorithm */
#ifdef USE_3DES 
#define USE_DES_3DES      1

#ifdef USE_SEC_HARDWARE
#define USE_3DES_HW        1
#else
#define USE_3DES_SW        1
#endif

#endif  /* USE_3DES */

#ifdef USE_DES_3DES
/*  Here you can breakdown what you want to include even further */
#define USE_DES_CBC_CKSM                       1  /* only used in SSL */
#define USE_DES_CFB64_EDE                      1  /* only used in SSL */
#define USE_DES_OFB64_EDE                      1  /* only used in SSL */
#define USE_DES_CFB64_ENC                      1  /* only used in SSL */
#define USE_DES_OFB64_ENC                      1  /* only used in SSL */
#define USE_DES_ECB3_ENC                       1  /* only used in SSL */
#define USE_DES_ECB_ENC                        1  /* used in SSL and MSCHAP */
#define USE_DES_XCBC_ENC                       1  /* only used in SSL */

/*#define USE_DES_CFB_ENC                      1*/  /* might be used in SSL */
/*#define USE_DES_STR2KEY                      1*/  /* might be used in SSL */

#endif

#ifdef USE_MD5

#ifdef USE_SEC_HARDWARE
#define USE_MD5_HW        1
#else
#define USE_MD5_SW        1
#endif

#ifdef INCLUDE_SNMPV3         /* P2K algorithm is only used in SNMPV3 */
#ifdef USE_SEC_HARDWARE
#define USE_P2K_MD5_HW     1  /* This feature is not complete yet*/
#else
#define USE_P2K_MD5_SW     1
#endif
#endif

#endif /* USE_MD5 */

#ifdef USE_SHA1

#ifdef USE_SEC_HARDWARE
#define USE_SHA1_HW        1
#else
#define USE_SHA1_SW        1
#endif

#endif /* USE_SHA1 */


#ifdef USE_HMAC

#ifdef USE_SEC_HARDWARE
#define USE_HMAC_HW        1
#define USE_HMAC_MD5_HW    1
#define USE_HMAC_SHA_HW    1
#else
#define USE_HMAC_SW        1
#define USE_HMAC_MD5_SW    1
#define USE_HMAC_SHA_SW    1
#endif

/* This macro could yield to hardware or software
 computation based on definition of USE_HMAC_MD5_HW */
#define USE_IPSEC_MD5      1

/* This macro could yield to hardware or software
 computation based on definition of USE_HMAC_SHA_HW */
#define USE_IPSEC_SHA      1

#ifdef USE_SHA2
#define USE_HMAC_SHA_256  	                   1
#define USE_HMAC_SHA_384  	                   1
#define USE_HMAC_SHA_512  	                   1
#define USE_IPSEC_SHA_256  	                   1
#define USE_IPSEC_SHA_384  	                   1
#define USE_IPSEC_SHA_512  	                   1
#endif

#endif /* USE_HMAC */

/* a lot of code from the asyrmmetric libraries still reside in SSL directory
and is not moved to crypt directory yet. Therefore compiling RSA require
SSL to be enabled. */ 

#ifdef ENABLE_SSL  
#define USE_RSA                                1
#endif

/* Only one for the following should be defined */
/* The prime number generation stuff may not work when
 * EIGHT_BIT but I don't care since I've only used this mode
 * for debuging the bignum libraries */
#undef SIXTY_FOUR_BIT_LONG
#undef SIXTY_FOUR_BIT
#define THIRTY_TWO_BIT
#undef SIXTEEN_BIT
#undef EIGHT_BIT

#ifndef EIGHT_BIT
#define NUMPRIMES 2048
#else
#define NUMPRIMES 54
#endif

#define CRYPT_MALLOC(size) npalloc(size)
#define CRYPT_FREE(p)      npfree(p)
#define CRYPT_FREE_FUNC    npfree
#define CRYPT_REALLOC(a,n)    nprealloc(a,n)
#define CRYPT_ASSERT(x)

#ifdef USE_PROFILER
#define CRYPT_TICKS()   get_ptick()
#else
#define CRYPT_TICKS   CTICKS
#endif /* USE_PROFILER */

/* This is just a CE demo macro. Is has no other significance. */
#define CU_MAX_BUF_SIZE (64*40)

/* port specific defines */
#define CU_ACQUIRE_RESOURCE(x)  ENTER_CRIT_SECTION(x)
#define CU_RELEASE_RESOURCE(x)  EXIT_CRIT_SECTION(x)
#define CU_TK_YIELD             tk_yield

/* BIO library is only part of SSL code currently */
#ifndef ENABLE_SSL  
#define NO_BIO
#endif

#define NO_FP_API
/*#define NO_STDIO*/
#define NO_LHASH
                                    
#if defined(ENABLE_SSL) || defined(IKE) || defined(IPSEC)

#define MD_RAND                                1
#define CRYPT_LIB                              1
#define USE_BN                                 1
#define USE_DH                                 1
#define USE_CRYPT_RAND                         1
#define USE_RAND_WIN                           1
#define USE_CRYPT_STACK                        1
#define USE_CRYPT_ERR                          1
#define USE_CRYPT_LHASH                        1

#endif  

#endif /* _CRYPT_PORT_H_ */
