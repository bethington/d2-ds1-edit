#ifndef _ANIMDATA_H_

#define _ANIMDATA_H_

UBYTE animdata_hash_value   (char * name);
void  animdata_load         (void);
int   animdata_get_cof_info (char * name, long * fpd, long * speed);

#endif
