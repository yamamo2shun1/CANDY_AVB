/* FILE: asm_cksum.c
 *
 * Internet checksum calculation for Altera NIOS-II processor.
 * Refer to RFC 1071 for details.
 */

/* FUNCTION: asm_cksum
 *
 * Internet checksum calculation
 *
 * PARAM1: ptr	     r4, pointer to packet data (16-bit aligned)
 * PARAM2: count     r5, number of unsigned halfwords
 *
 *  RETURN: cksum     r2, 16-bit checksum
 */

/*
 * r2 = accum
 * r4 = ptr
 * r5 = count
 * r6 = carry accum
 * r7 = load
 * r8 = carry
 * r9 = odd
 * r10 = temp
 */

      .text

      .global	  asm_cksum

asm_cksum:
      mov   r2, zero		       /* accumulator = 0 */
      ble   r5, zero, done	       /* count <= 0 ? */

      mov   r6, zero		       /* carry accumulator */

      andi  r3, r4, 2		       /* ptr 32-bit aligned? */
      beq   r3, zero, asm1
      ldhu  r2, (r4)		       /* no - process first 16-bits */
      addi  r4, r4, 2
      subi  r5, r5, 1
/*
 * adjust ptr by ((count/2) mod 16) * 4 bytes
 * jump to location: loop0 - (count/2 mod 16) * 4 instructions
 */
asm1:
      movia r11, loop0
      andi  r9, r5, 1		       /* r9 = last halfword flag */
      srai  r5, r5, 1		       /* count = number of words */
      andi  r10, r5, 0xf	       /* modulo 16 */
      slli  r10, r10, 2		       /*      * 4 bytes per word */
      add   r4, r10, r4		       /* adjust ptr */
      slli  r10, r10, 2		       /*      * 4 instructions per 4 bytes */
      sub   r11, r11, r10
      jmp   r11
      
loop:
      addi  r4, r4, 64		       /* increment data pointer */

      ldw   r7, -64(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -60(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -56(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -52(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -48(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -44(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -40(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -36(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -32(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -28(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -24(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -20(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -16(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -12(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -8(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6

      ldw   r7, -4(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6
loop0:
      subi  r5, r5, 16
      bge   r5, zero, loop
/*
 * process last halfword (if any)
 */
      beq   r9, zero, fold
      ldhu  r7, 0(r4)
      add   r2, r7, r2
      cmpltu r8, r2, r7
      add   r6, r8, r6
      
fold:
      srli  r7, r2, 16
      andi  r2, r2, 0xffff
      add   r2, r7, r2		    /* add the upper and lower halfwords */
      add   r2, r6, r2		    /* add the carries */
/* the accumulator is 18 bits */
      srli  r7, r2, 16
      andi  r2, r2, 0xffff
      add   r2, r7, r2		    /* add 2 carry bits to lower halfword */
/* the accumulator is 17 bits */
      srli  r7, r2, 16
      andi  r2, r2, 0xffff
      add   r2, r7, r2		    /* add carry to lower halfword */

done:
      ret			    		/* r2 = 16-bit checksum */

