/* FILENAME: msring.h
 *
 * Definitions for ring buffer support for NicheLite M_SOCKs.
 * See misclib/msring.c for implementation.
 *
 * PORTABLE: YES
 * 
 * MODULE: H
 */

#ifndef MSRING_H_
#define MSRING_H_

#ifdef MINI_TCP

/* struct msring - a ring buffer structure for NicheLite sockets
 */
struct msring
{
   M_SOCK * buf;           /* ptr to storage for buffered M_SOCKs */
   unsigned buflen;        /* length (in M_SOCKs) of buf */
   unsigned in;            /* index into buf: where to add next M_SOCK */
   unsigned out;           /* index into buf: where to delete next M_SOCK */
};

/* function prototypes */
void msring_init(struct msring * ring,
                 M_SOCK * buf,
                 unsigned bufsize);
void msring_add(struct msring * ring,
                M_SOCK so);
int msring_del(struct msring * ring,
               M_SOCK * so);

#endif /* MINI_TCP */

#endif  /* MSRING_H_ */
