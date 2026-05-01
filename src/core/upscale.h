#ifndef _UPSCALE_H_
#define _UPSCALE_H_

int upscale_is_remote_configured(void);
int upscale_create_temp_dir(char *out, int out_cap);
int upscale_remove_tree(const char *path);
int upscale_directory_local(const char *src_dir, const char *dst_dir,
                            int scale, char *error, int error_cap);
int upscale_directory_remote(const char *src_dir, const char *dst_dir,
                             int scale, const char *method,
                             char *error, int error_cap);

#endif