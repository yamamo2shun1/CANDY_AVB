/*
 * FILENAME: memwrap.h
 *
 * Copyright  2000 By InterNiche Technologies Inc.  All rights reserved.
 *
 *
 * MODULE: MISCLIB
 *
 *
 * PORTABLE: yes
 */

#ifndef MEMWRAP_H
#define MEMWRAP_H

int      wrap_stats(void * pio);
char *   wrap_alloc(unsigned size, char * (*alloc_rtn)(unsigned));
void     wrap_free(char * ptr, void (*free_rtn)(char *));

#ifndef ALIGN_TYPE
#error ALIGN_TYPE must be defined for this system
#endif

#endif /* MEMWRAP_H */
