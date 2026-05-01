#include <stdio.h>

#include "core/export_common.h"

void export_make_screenshot_name(char *out, int out_cap, int num)
{
    if (out == NULL || out_cap <= 0)
        return;

    snprintf(out, out_cap, "screenshot-%05i.png", num);
}