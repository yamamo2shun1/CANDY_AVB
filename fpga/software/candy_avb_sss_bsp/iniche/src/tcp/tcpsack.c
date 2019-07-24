/*
 * FILENAME: tcpsack.c
 *
 * Copyright 2001 By InterNiche Technologies Inc. All rights reserved
 *
 *
 * MODULE: TCP
 *
 * ROUTINES: tcp_resendsack(), tcp_buildsack(), 
 *
 * PORTABLE: yes
 */

#include "ipport.h"
#include "tcpport.h"

#ifdef INCLUDE_TCP  /* include/exclude whole file at compile time */
#ifdef TCP_SACK      /* Whole file goes away w/o this ifdef */

#include "tcp.h"
#include "tcp_fsm.h"
#include "tcp_seq.h"
#include "tcp_timr.h"
#include "tcp_var.h"
#include "tcpip.h"
#include "in_pcb.h"


int tcp_sackComp(tcp_seq * qs1, tcp_seq * qs2);


/* FUNCTION: tcp_resendsack()
 *
 * Resend data which is indicated to be missing by a SACK option.
 *
 * 
 * PARAM1: char * opt - pointer to received SACK option
 * PARAM2: struct tcpcb * tp - the connection.
 *
 * RETURNS: 
 */

void
tcp_resendsack(u_char * opt, struct tcpcb * tp)
{
   int      len;        /* length of SACK header as read from header */
   int      blocks;     /* number of blocks in received header */
   int      holes;      /* number of SACK holes written to tp */
   int      i,j;        /* scratch */
   tcp_seq  start;      /* start sequence of current hole */
   tcp_seq  end;        /* end sequence of current hole */
   tcp_seq  nextstart;     /* start of next hole */
   tcp_seq  sack_trigger;  /* sequnce number which triggered sack header */
   tcp_seq  seq;           /* scratch sequnce number */
   tcp_seq	Sacks[SACK_BLOCKS][2];

   tcpstat.tcps_sackrcvd++;

   len = (int)*(opt + 1);     /* length of options fields */

   /* Length should be (n (blocks) * 8) + 2 bytes. Verify this: */
   if((len & 0x07) != 0x02)
   {
      dtrap();    /* Badly formed sack header */
      return;
   }

   /* Set up to traverse block list: */
   opt += 2;                     /* point to block list */
   blocks = len >> 3;            /* number of blocks */

   if(blocks > (int)tcpstat.tcps_sackmaxblk)
      tcpstat.tcps_sackmaxblk = blocks;

   /* extract all the sack opts < SACK_BLOCKS */
   for(i = 0; (i < blocks) && (i < SACK_BLOCKS); i++)
   {
      Sacks[i][0] = tcp_getseq(opt + i * 2 * sizeof(tcp_seq));   /* start */
      Sacks[i][1] = tcp_getseq(opt + i * 2 * sizeof(tcp_seq) + sizeof(tcp_seq));  /* end */
   }
   blocks = i;
 
   /* sort */   
#ifdef INCLUDE_QSORT
      iniche_qsort((char *)Sacks, blocks, 2 * sizeof(tcp_seq),
         (int (*) (const void *, const void *))tcp_sackComp);
#else
   qsort((char *)Sacks, blocks, 2 * sizeof(tcp_seq),
    (int (*) (const void *, const void *))tcp_sackComp);
#endif

   /* |- blocks are now in ascending order */

      IPV6LOG(("+  tcp_resendsack tp->snd_una = %lx, tp->snd_max = %lx\n", tp->snd_una, tp->snd_max));

   for(i = 0; i < blocks; i++)
       IPV6LOG(("+ %lx, %lx \n", Sacks[i][0], Sacks[i][1]));


   /* SACK sends a block list, but what we are interested in is the holes
    * which precede each block so we can fill these holes. The loop below
    * finds the holes, in order, and resends the segments to fill them.
    */
   start = tp->snd_una;    /* 1st hole starts at last unacked byte */
   for(i = 0, holes = 0; i < blocks; i++, holes++)
   {
      /* end of this block will be start of next hole */
      nextstart = Sacks[i][1];

	  if (start == Sacks[i][0])
	  {
		  start = nextstart;
		  continue;
	  }

	  end = Sacks[i][0]; 

      /* Add the SACK hole to the tp. Any old holes info is overwritten. The
       * Resend timer should end up using the latest SACK info when if
       * fires instead of the snd_una field.
       */
      tp->sack_hole_start[holes] = start;
      tp->sack_hole_end[holes] = end;
     
   IPV6LOG(("+  tcp_resendsack = %lx, %lx \n", start, end));
	  
      start = nextstart;
   }

   /* room for end hole? */
   if ((holes < SACK_BLOCKS) && (start < tp->snd_max))
   {
      /* Add the SACK hole to the tp. Any old holes info is overwritten. The
       * Resend timer should end up using the latest SACK info when if
       * fires instead of the snd_una field.
       */
      tp->sack_hole_start[holes] = start;
      tp->sack_hole_end[holes] = tp->snd_max;
	  holes++;


   IPV6LOG(("+  tcp_resendsack end = %lx, %lx \n", start, end));


   }


   /* if we have identified SACK holes to fill, and the triggering seq is
    * not the same as the last one we parsed, set resend time to fire ASAP
    */
   if((holes) && (tp->sack_trigger != sack_trigger))
   {
      tp->t_timer[TCPT_REXMT] = 1;        /* go off on next tcp_slowtmo() */
      tp->sack_trigger = sack_trigger;    /* save new trigger */
      tp->t_flags |= TF_SACKREPLY;         /* flag for tcp_input() */
   }

   /* fill in any remaining holes with zeros */
   while(holes < SACK_BLOCKS)
   {
      tp->sack_hole_start[holes] = 0;
      tp->sack_hole_end[holes] = 0;
      holes++;
   }
   return;
}


/* FUNCTION: tcp_setsackinfo()
 *
 * Called from tcp_input() to store the information to send a sack
 * request header later on.
 *
 * PARAM1: struct tcpcb * tp - connection to SACK
 * PARAM2: struct tcpiphdr * ti - received segment header
 *
 * RETURNS: length of option field, 0 if none was built.
 */

void
tcp_setsackinfo(struct tcpcb * tp, struct tcpiphdr * ti)
{
   int i;


   IPV6LOG(("+  tp->t_flags |= TF_SACKNOW, ti->ti_seq = %lx \n", ti->ti_seq));
    
   
   /* if the received seq is not the last one for which we built 
    * a sack header, then build new sack data blocks. Code in 
    * tcpsack.c will use this info to generate sack headers.
    */
   if(tp->sack_seq[0] != ti->ti_seq)
   {
      /* check for deeper dupes and for new seq being earlier */
      for(i = 0; i < tp->sack_seqct; i++)
         if(tp->sack_seq[i] == ti->ti_seq)
			 return;   /* dupe */
		 else
         if(tp->sack_seq[i] > ti->ti_seq)
		 {
			/* new seq is earlier than an entry, wipe out! */
            tp->sack_seq[0] = ti->ti_seq;
            tp->sack_seqct = 1;
			return;
		 }
	   
	  /* rotate the older sack sequences toward back of list */
      for(i = SACK_BLOCKS - 1; i > 0; i--)
         tp->sack_seq[i] = tp->sack_seq[i - 1];

      if(tp->sack_seqct < SACK_BLOCKS)
         tp->sack_seqct++;    /* indicate another valid block */

      /* save segment number which triggered SACK at front of list */
      tp->sack_seq[0] = ti->ti_seq;
   }
   tp->t_flags |= TF_SACKNOW; /* set flag to send SACK */

   return;
}




/* FUNCTION: tcp_buildsack()
 *
 * Build a TCP option field for SACK info
 *
 * PARAM1: struct tcpcb * tp - connection to SACK
 * PARAM2: u_char * opt - pointer to option buffer to build.
 *
 * RETURNS: length of option field, 0 if none was built.
 */

int
tcp_buildsack(struct tcpcb * tp, u_char * opt)
{
   int      blocks;  /* number of sack info blocks created */
   int      len;     /* length of sack data header */
   int      indx;
   int      sack_Cnt;
   tcp_seq  start;   /* scratch */
   tcp_seq  end;
   struct tcpiphdr * ti;
   struct tcpiphdr * next;
   tcp_seq bag_end[SACK_BLOCKS]; 

   *opt = TCPOPT_SACKDATA;     /* fill in option type */

      IPV6LOG(("+  tcp_buildsack \n"));


   for(blocks = 0; blocks < tp->sack_seqct; blocks++)  
   {

      IPV6LOG(("+  SACK[%d] = %lx \n", blocks, tp->sack_seq[blocks]));

   }

       for(ti = tp->seg_next; ti != (struct tcpiphdr *)tp;
         ti = (struct tcpiphdr *)(ti->ti_next) )
      {
	        IPV6LOG(("+  reasm  = %lx , %lx\n", ti->ti_t.th_seq, ti->ti_t.th_seq + ti->ti_i.ih_len));
	}

   len = 2;          /* init for just type and len bytes */

   /* build sack block list from reasm queue */
   for(blocks = 0, sack_Cnt = 0; blocks < tp->sack_seqct; blocks++, sack_Cnt++)
   {
      /* Search reasm queue for holes. We will use the info from the
       * reasm queue to flesh out the hole list entry in the sack 
       * header. The list is build in tcp_in() when it sets the 
       * TF_SACKNOW flag which got us here.
       */
      for(ti = tp->seg_next; ti != (struct tcpiphdr *)tp;
         ti = (struct tcpiphdr *)(ti->ti_next) )
      {
         if((ti->ti_t.th_seq <= tp->sack_seq[blocks]) &&
            ((ti->ti_t.th_seq + ti->ti_i.ih_len) > tp->sack_seq[blocks]))
         {
            break;   /* found segment containing sack seq. */
         }
      }
      if(ti == (struct tcpiphdr *)tp)
      {
          /* 
		   * sack_seq not in reasm queue, remove it from SACK.
           */
		   for (indx = blocks; indx < tp->sack_seqct - 1; indx++)
		   {			   
			  /* merge */
		      tp->sack_seq[indx] = tp->sack_seq[indx + 1];
              bag_end[indx] = bag_end[indx + 1];

		   	        IPV6LOG(("+ move down 2  = %d, %lx\n", indx));
		   }

		   /* dec. counts */
		   tp->sack_seqct--;
		   if (indx < sack_Cnt)
			  sack_Cnt--;

         /* Break out of
          * for(blocks) loop, finish option header, and return.
          */
         break;
      }

      /* find end of sack block by searching reasm queue for
       * next hole or end of queue.
       */
      next = (struct tcpiphdr *)(ti->ti_next);
      end = ti->ti_seq + ti->ti_len;   /* end of segment */
      while(next != (struct tcpiphdr *)tp)
      {
         /* if this blocks data is adjacent to next, keep looking */
         if(end >= next->ti_seq)
         {
            ti = next;
            end = ti->ti_seq + ti->ti_len;   /* next end of new segment */
            next = (struct tcpiphdr *)(ti->ti_next);
         }
         else  /* hole follows it, exit search loop */
         {
            break;
         }
      }

	  /* save the endpoint of this block */
	  bag_end[blocks] = end;
   } /* for */

   /* go thru merging blocks with common ends */
   for(blocks = 0; blocks < sack_Cnt - 1; blocks++)
	   if (bag_end[blocks] == bag_end[blocks + 1])
	   {
           /* move down over dupe, save older seq */
		   for (indx = blocks; indx < tp->sack_seqct - 1; indx++)
		   {			   
			   /* merge */
		      tp->sack_seq[indx] = tp->sack_seq[indx + 1];
              bag_end[indx] = bag_end[indx + 1];
		   	        IPV6LOG(("+ move down   = %d, %lx\n", indx, bag_end[indx]));
		   }

		   /* dec. counts */
		   tp->sack_seqct--;
		   if (blocks < sack_Cnt)
			  sack_Cnt--;
	   }

   /* punch out 1 - 3 options */
   for(blocks = 0; (blocks < sack_Cnt) && (blocks < 3); blocks++)
   {

	        IPV6LOG(("+  SACK start  = %lx, end = %lx \n", tp->sack_seq[blocks], bag_end[blocks]));


      /* set the SACK "hole" data at the correct offsets in opt buffer */
      tcp_putseq((opt + 2 + (8 * blocks)) , tp->sack_seq[blocks]);
      tcp_putseq((opt + 6 + (8 * blocks)) , bag_end[blocks]);
      len += 8;
   }

   if(len == 2)   /* if no blocks were set return a zero */
      return 0;

   tcpstat.tcps_sacksent++;
   *(opt + 1) = (u_char)len;
   return len;
}


/* FUNCTION: tcp_sackComp()
 * 
 * PARAM1: tcp_seq *qs1
 * PARAM2: tcp_seq *qs2
 *
 * RETURNS: 
 */

int
tcp_sackComp(tcp_seq * qs1, tcp_seq * qs2)
{
   return (int)(*qs1 - *qs2);
}

#endif   /* TCP_SACK */
#endif /* INCLUDE_TCP */



