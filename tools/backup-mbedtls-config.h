// Only the primitives needed by the existing backup envelope. No TLS stack,
// certificates, PSA global state, CPU-specific assembly or network entropy.
#ifndef NV_BACKUP_MBEDTLS_CONFIG_H
#define NV_BACKUP_MBEDTLS_CONFIG_H
#define MBEDTLS_AES_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_GCM_C
#define MBEDTLS_MD_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_SHA256_C
#endif
