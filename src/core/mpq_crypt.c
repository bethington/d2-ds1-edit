/* MPQ name hashing and table encryption. See mpq_crypt.h. */
#include <ctype.h>
#include "core/mpq_crypt.h"

static unsigned int s_table[0x500];
static int s_ready = 0;

static void prepare(void)
{
   unsigned int seed = 0x00100001, index1, index2;
   int i;

   if (s_ready)
      return;
   for (index1 = 0; index1 < 0x100; index1++)
      for (index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
      {
         unsigned int t1, t2;
         seed = (seed * 125 + 3) % 0x2AAAAB;
         t1 = (seed & 0xFFFF) << 16;
         seed = (seed * 125 + 3) % 0x2AAAAB;
         t2 = seed & 0xFFFF;
         s_table[index2] = t1 | t2;
      }
   s_ready = 1;
}

unsigned int mpq_hash_string(const char * name, int type)
{
   unsigned int seed1 = 0x7FED7FED, seed2 = 0xEEEEEEEE;

   prepare();
   for (; *name != '\0'; name++)
   {
      unsigned int c = (unsigned int) toupper((unsigned char) *name);
      seed1 = s_table[(type << 8) + c] ^ (seed1 + seed2);
      seed2 = c + seed1 + seed2 + (seed2 << 5) + 3;
   }
   return seed1;
}

void mpq_encrypt(unsigned int * data, int dwords, unsigned int key)
{
   unsigned int seed = 0xEEEEEEEE;
   int i;

   prepare();
   for (i = 0; i < dwords; i++)
   {
      unsigned int plain = data[i];
      seed += s_table[0x400 + (key & 0xFF)];
      data[i] = plain ^ (key + seed);
      key = ((~key << 21) + 0x11111111) | (key >> 11);
      seed = plain + seed + (seed << 5) + 3;
   }
}

void mpq_decrypt(unsigned int * data, int dwords, unsigned int key)
{
   unsigned int seed = 0xEEEEEEEE;
   int i;

   prepare();
   for (i = 0; i < dwords; i++)
   {
      unsigned int plain;
      seed += s_table[0x400 + (key & 0xFF)];
      plain = data[i] ^ (key + seed);
      data[i] = plain;
      key = ((~key << 21) + 0x11111111) | (key >> 11);
      seed = plain + seed + (seed << 5) + 3;
   }
}
