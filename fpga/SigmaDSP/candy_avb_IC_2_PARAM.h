/*
 * File:           C:\Users\shun\Desktop\CurrentProjects\CANDY\CANDY_AVB\fpga\SigmaDSP\candy_avb_IC_2_PARAM.h
 *
 * Created:        Monday, February 18, 2019 4:34:40 PM
 * Description:    candy_avb:IC 2 parameter RAM definitions.
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
#ifndef __CANDY_AVB_IC_2_PARAM_H__
#define __CANDY_AVB_IC_2_PARAM_H__


/* Module Single 2 - Single Volume*/
#define MOD_SINGLE2_COUNT                              4
#define MOD_SINGLE2_DEVICE                             "IC2"
#define MOD_SINGLE2_ALG0_GAINS200ALG2GAINTARGET_ADDR   8
#define MOD_SINGLE2_ALG0_GAINS200ALG2GAINTARGET_FIXPT  0x00287A26
#define MOD_SINGLE2_ALG0_GAINS200ALG2GAINTARGET_VALUE  SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.316227766016838)
#define MOD_SINGLE2_ALG0_GAINS200ALG2GAINTARGET_TYPE   SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE2_ALG1_GAIN1940ALGNS2_ADDR           11
#define MOD_SINGLE2_ALG1_GAIN1940ALGNS2_FIXPT          0x00287A26
#define MOD_SINGLE2_ALG1_GAIN1940ALGNS2_VALUE          SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.316227766016838)
#define MOD_SINGLE2_ALG1_GAIN1940ALGNS2_TYPE           SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA0_ADDR       8190
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA0_FIXPT      0x007FF259
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA0_VALUE      SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.999583420126834)
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA0_TYPE       SIGMASTUDIOTYPE_FIXPOINT
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA1_ADDR       8191
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA1_FIXPT      0x00000DA6
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA1_VALUE      SIGMASTUDIOTYPE_FIXPOINT_CONVERT(0.000416579873166234)
#define MOD_SINGLE2_ALG0_GAINS200ALG2ALPHA1_TYPE       SIGMASTUDIOTYPE_FIXPOINT

#endif
