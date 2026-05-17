/* Cross-check the bootloader's bespoke AES-256 against a known-good reference.
 *
 * Compile + run on the host (x86):
 *   gcc -O0 -o aes_xref aes_xref.c ../../00_Bootloader/Middlewares/AES/src/aes.c \
 *       -I ../../00_Bootloader/Middlewares/AES/inc
 *   ./aes_xref
 *
 * Encrypts 16 zero bytes with the bootloader's hardcoded key/iv and dumps
 * the ciphertext. Compare to pycryptodome's AES-256-CBC of the same input
 * to see whether the bootloader library matches FIPS-197 AES.
 */
#include <stdio.h>
#include <string.h>
#include "aes.h"

int main(void)
{
    unsigned char key[32];
    unsigned char iv[16];
    unsigned char plain[16] = {0};

    /* Match bootmanager.c s_key_256 / s_iv_default exactly. */
    for (int i = 0; i < 32; i++) key[i] = (i & 1) ? 0x32 : 0x31;
    for (int i = 0; i < 16; i++) iv [i] = (i & 1) ? 0x32 : 0x31;

    unsigned char state[16];
    unsigned char iv_work[16];

    memcpy(state, plain, 16);
    memcpy(iv_work, iv, 16);

    Aes_IV_key256bit_Encrypt(iv_work, state, key);

    printf("Bootloader AES-256-CBC(plaintext=16 zero bytes,\n"
           "                       key=32 bytes 0x31,0x32 alternating,\n"
           "                       iv=16 bytes 0x31,0x32 alternating)\n");
    printf("ciphertext (hex) = ");
    for (int i = 0; i < 16; i++) printf("%02x", state[i]);
    printf("\n");
    return 0;
}
