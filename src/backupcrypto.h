#ifndef NV_BACKUPCRYPTO_H
#define NV_BACKUPCRYPTO_H
#include <stddef.h>

/* Password-encrypted portable envelope. AES-256-GCM (128-bit tag), PBKDF2
 * HMAC-SHA256/600000, independent OS-random 16-byte salt and 12-byte nonce.
 * The full header and owner/profile/purpose context are authenticated.
 * No password, key, settings or account details are written to logs.
 * Blocking worker-only calls. Returned buffers belong to the caller. */
#define BACKUP_CRYPTO_MAX (24u * 1024u * 1024u)
#define BACKUP_CRYPTO_CONTEXT_MAX 192u
#define BACKUP_CRYPTO_OVERHEAD (54u + BACKUP_CRYPTO_CONTEXT_MAX + 16u)
int backupcrypto_disponivel(void);
void backupcrypto_limpar(void *, size_t);
/* Structural check before key derivation; never implies authenticity. */
int backupcrypto_envelope(const void *, size_t, const char *contexto);
int backupcrypto_selar(const void *, size_t, const void *senha, size_t,
                       const char *contexto, unsigned char **, size_t *);
/* Returns no plaintext on wrong password, tampering or malformed input. */
int backupcrypto_abrir(const void *, size_t, const void *senha, size_t,
                       const char *contexto, unsigned char **, size_t *);
#endif
