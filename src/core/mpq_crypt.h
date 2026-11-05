/* MPQ name hashing and table encryption. The archive format's own scheme; no
 * third-party library is involved. */
#ifndef DS1EDIT_MPQ_CRYPT_H
#define DS1EDIT_MPQ_CRYPT_H

#define MPQ_HASH_OFFSET 0
#define MPQ_HASH_A      1
#define MPQ_HASH_B      2
#define MPQ_HASH_KEY    3

/* Hash of name, ignoring case, for the table slot (MPQ_HASH_OFFSET), the two
   name checks (MPQ_HASH_A, MPQ_HASH_B) or an encryption key (MPQ_HASH_KEY). */
unsigned int mpq_hash_string(const char * name, int type);

/* In place, over host-order dwords; the caller converts to and from the file's
   little-endian bytes. */
void mpq_encrypt(unsigned int * data, int dwords, unsigned int key);
void mpq_decrypt(unsigned int * data, int dwords, unsigned int key);

#endif
