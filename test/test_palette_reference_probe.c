/*
 * Candidate probe for the palette-nearest-113c equivalence contract.
 *
 *   test_palette_reference_probe --palette palette.rgba --queries queries.rgb
 *                                --out actual.json
 *
 * palette.rgba is exactly 256 RGBA entries (1024 bytes) in RGBA_PALETTE order.
 * queries.rgb is packed RGB triples (a non-zero multiple of 3 bytes).
 * Output: {"indices":[...],"palette_unchanged":true|false}, one index per
 * query, from the real production palette_find_closest().
 *
 * Malformed arguments or input lengths exit nonzero without writing output.
 * This probe never computes a reference answer; it only reports the editor's.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/palette.h"

#define MAX_QUERY_BYTES (3L * 1024L * 1024L)

static int read_file(const char *path, unsigned char **out, long *len, long max_len)
{
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *buf;

    if (f == NULL)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || n > max_len ||
        fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return -1;
    }
    buf = (unsigned char *)malloc(n > 0 ? (size_t)n : 1);
    if (buf == NULL || (n > 0 && fread(buf, 1, (size_t)n, f) != (size_t)n))
    {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    *out = buf;
    *len = n;
    return 0;
}

static int usage(void)
{
    fprintf(stderr, "usage: test_palette_reference_probe --palette FILE "
                    "--queries FILE --out FILE\n");
    return 2;
}

int main(int argc, char **argv)
{
    const char *pal_path = NULL, *q_path = NULL, *out_path = NULL;
    unsigned char *pal_raw = NULL, *q = NULL;
    long pal_len = 0, q_len = 0, i, nq;
    RGBA_PALETTE pal, before;
    unsigned char *indices;
    FILE *out;
    int a;

    for (a = 1; a < argc; a++)
    {
        if (a + 1 >= argc)
            return usage();
        if (strcmp(argv[a], "--palette") == 0)
            pal_path = argv[++a];
        else if (strcmp(argv[a], "--queries") == 0)
            q_path = argv[++a];
        else if (strcmp(argv[a], "--out") == 0)
            out_path = argv[++a];
        else
            return usage();
    }
    if (pal_path == NULL || q_path == NULL || out_path == NULL)
        return usage();

    if (read_file(pal_path, &pal_raw, &pal_len, 4096) != 0 ||
        pal_len != (long)sizeof(RGBA_PALETTE))
    {
        fprintf(stderr, "probe: palette must be exactly %u bytes\n",
                (unsigned)sizeof(RGBA_PALETTE));
        free(pal_raw);
        return 3;
    }
    if (read_file(q_path, &q, &q_len, MAX_QUERY_BYTES) != 0 || q_len == 0 || q_len % 3 != 0)
    {
        fprintf(stderr, "probe: queries must be a non-empty multiple of 3 bytes\n");
        free(pal_raw);
        free(q);
        return 3;
    }

    for (i = 0; i < PAL_COLORS; i++)
    {
        pal.colors[i].r = pal_raw[4 * i + 0];
        pal.colors[i].g = pal_raw[4 * i + 1];
        pal.colors[i].b = pal_raw[4 * i + 2];
        pal.colors[i].a = pal_raw[4 * i + 3];
    }
    memcpy(&before, &pal, sizeof(pal));

    nq = q_len / 3;
    indices = (unsigned char *)malloc((size_t)nq);
    if (indices == NULL)
    {
        free(pal_raw);
        free(q);
        return 4;
    }
    for (i = 0; i < nq; i++)
        indices[i] = palette_find_closest(&pal, q[3 * i], q[3 * i + 1], q[3 * i + 2]);

    out = fopen(out_path, "wb");
    if (out == NULL)
    {
        fprintf(stderr, "probe: cannot open output\n");
        free(pal_raw);
        free(q);
        free(indices);
        return 4;
    }
    fputs("{\"indices\":[", out);
    for (i = 0; i < nq; i++)
        fprintf(out, "%s%u", i ? "," : "", (unsigned)indices[i]);
    fprintf(out, "],\"palette_unchanged\":%s}\n",
            memcmp(&before, &pal, sizeof(pal)) == 0 ? "true" : "false");
    a = ferror(out);
    if (fclose(out) != 0 || a)
    {
        fprintf(stderr, "probe: write failed\n");
        free(pal_raw);
        free(q);
        free(indices);
        return 4;
    }
    free(pal_raw);
    free(q);
    free(indices);
    return 0;
}
