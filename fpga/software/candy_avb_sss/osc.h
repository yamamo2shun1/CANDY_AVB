/*
 * osc.h
 *
 *  Created on: 2019/07/24
 *      Author: shun
 */

#ifndef OSC_H_
#define OSC_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "includes.h"
#include "ipport.h"

#define MAX_HOST_NAME_LEN 16
#define MAX_BUF_SIZE    64
#define MAX_PACKET_SIZE 192// 1024
#define MAX_MESSAGE_LEN 160
#define MAX_ADDRESS_LEN 64
#define MAX_ARGS_LEN 40

void setInitReceiveFlag(BOOLEAN flag);
BOOLEAN getInitReceiveFlag(void);
void setInitSendFlag(BOOLEAN flag);
BOOLEAN getInitSendFlag(void);
void setInitDiscoverFlag(BOOLEAN flag);
BOOLEAN getInitDiscoverFlag(void);
void setChCompletedFlag(BOOLEAN flag);
BOOLEAN getChCompletedFlag(void);

void setRemoteIpAtIndex(INT8U index,INT8U number);
INT8U getRemoteIpAtIndex(INT8U index);
INT8U* getRemoteIp(void);
void setRemotePort(INT16U number);
INT16U getRemotePort(void);
void setLocalPort(INT16U number);
INT16U getLocalPort(void);

void setOSCPrefix(char* prefix_string);
char* getOSCPrefix(void);
void clearOSCPrefix(void);
void setOSCHostName(char* host_name);
char* getOSCHostName(void);
void clearOSCHostName(void);

void getOSCPacket(INT8U* buf, INT16U len);
BOOLEAN processOSCPacket(void);

void sendOSCMessage(const char* prefix, const char* command, const char* type, ...);
void setOSCAddress(const char* prefix, const char* command);
void setOSCTypeTag(const char* type);
void addOSCIntArgument(int value);
void addOSCFloatArgument(float value);
void addOSCStringArgument(char* str);
void clearOSCMessage(void);
void flushOSCMessage(void);
void clearOSCBundle(void);
void appendOSCMessageToBundle(const char* prefix, const char* command, const char* type, ...);
void sendOSCBundle(void);
BOOLEAN compareOSCPrefix(const char* prefix);
BOOLEAN compareOSCAddress(const char* address);
BOOLEAN compareTypeTagAtIndex(const INT16U index, const char typetag);
INT16U getArgumentsLength(void);
INT32S getIntArgumentAtIndex(const INT16U index);
float getFloatArgumentAtIndex(const INT16U index);
char* getStringArgumentAtIndex(const INT16U index);


#endif /* OSC_H_ */
