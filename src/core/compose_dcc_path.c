#include <stdio.h>
#include <string.h>

#include "core/compose_dcc_path.h"

static const char *s_layer_codes[COMPOSE_DCC_PATH_LAYER_COUNT] = {
   "HD", "TR", "LG", "RA", "LA", "RH", "LH", "SH",
   "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8"
};

const char *compose_dcc_path_layer_code(int slot)
{
   if (slot < 0 || slot >= COMPOSE_DCC_PATH_LAYER_COUNT)
      return NULL;
   return s_layer_codes[slot];
}

int compose_dcc_path_build(char *out_buf, int out_cap,
                           const char *base,
                           const char *token,
                           int slot,
                           const char *skin,
                           const char *mode,
                           const char *wclass)
{
   const char *layer;
   int written;

   if (out_buf == NULL || out_cap <= 0
       || base == NULL || token == NULL || mode == NULL)
      return 0;

   layer = compose_dcc_path_layer_code(slot);
   if (layer == NULL)
      return 0;

   if (skin   == NULL) skin   = "";
   if (wclass == NULL) wclass = "";

   /* Pattern: <base>\<token>\<layer>\<token><layer><skin><mode><wclass>.dcc */
   written = snprintf(out_buf, (size_t) out_cap,
                      "%s\\%s\\%s\\%s%s%s%s%s.dcc",
                      base, token, layer,
                      token, layer, skin, mode, wclass);

   if (written < 0 || written >= out_cap)
   {
      out_buf[0] = 0;
      return 0;
   }
   return 1;
}
