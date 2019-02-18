/*
 * File:           C:\Users\shun\Desktop\CurrentProjects\CANDY\CANDY_AVB\fpga\software\candy_avb_test\candy_avb_IC_1_PARAM.h
 *
 * Created:        Monday, February 18, 2019 4:34:48 PM
 * Description:    candy_avb:IC 1 parameter RAM definitions.
 *
 * This software is distributed in the hope that it will be useful,
 * but is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * This software may only be used to program products purchased from
 * Analog Devices for incorporation by you into audio products that
 * are intended for resale to audio product end users. This software
 * may not be distributed whole or in any part to third parties.
 *
 * Copyright ©2019 Analog Devices, Inc. All rights reserved.
 */
#ifndef __CANDY_AVB_IC_1_PARAM_H__
#define __CANDY_AVB_IC_1_PARAM_H__


/* Module Modulo Size - Modulo Size*/
#define MOD_MODULOSIZE_COUNT                           1
#define MOD_MODULOSIZE_DEVICE                          "IC1"
#define MOD_MODULOSIZE_MODULO_SIZE_ADDR                0
#define MOD_MODULOSIZE_MODULO_SIZE_FIXPT               0x00000FFE
#define MOD_MODULOSIZE_MODULO_SIZE_VALUE               SIGMASTUDIOTYPE_INTEGER_CONVERT(4094)
#define MOD_MODULOSIZE_MODULO_SIZE_TYPE                SIGMASTUDIOTYPE_INTEGER

/* Module Single 1 - Single Volume*/
#define MOD_SINGLE1_COUNT                              4
#define MOD_SINGLE1_DEVICE                             "IC1"
#define MOD_SINGLE1_ALG0_GAINS200ALG1GAINTARGET_ADDR   8
#define MOD_SINGLE1_ALG0_GAINS200ALG1GAINTARGET_FIXPT  0x00287A26
#define MOD_SINGLE1_ALG0_GAINS200ALG1GAINTARGET_VALUE  SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.316227766016838)
#define MOD_SINGLE1_ALG0_GAINS200ALG1GAINTARGET_TYPE   SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE1_ALG1_GAIN1940ALGNS1_ADDR           11
#define MOD_SINGLE1_ALG1_GAIN1940ALGNS1_FIXPT          0x00287A26
#define MOD_SINGLE1_ALG1_GAIN1940ALGNS1_VALUE          SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.316227766016838)
#define MOD_SINGLE1_ALG1_GAIN1940ALGNS1_TYPE           SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA0_ADDR       8190
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA0_FIXPT      0x007FF259
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA0_VALUE      SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.999583420126834)
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA0_TYPE       SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA1_ADDR       8191
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA1_FIXPT      0x00000DA6
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA1_VALUE      SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.000416579873166234)
#define MOD_SINGLE1_ALG0_GAINS200ALG1ALPHA1_TYPE       SIGMASTUDIOTYPE_FIXPOINT

#endif
