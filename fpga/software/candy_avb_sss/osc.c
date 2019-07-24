/*
 * osc.c
 *
 *  Created on: 2019/07/24
 *      Author: shun
 */

#include "osc.h"

// Network
static BOOLEAN initReceiveFlag = FALSE;
static BOOLEAN initSendFlag = FALSE;
static BOOLEAN initDiscoverFlag = FALSE;
static BOOLEAN chCompletedFlag = FALSE;
static char hostName[17] = "picrouter";
static char* stdPrefix = NULL;

static INT32S oscTotalSize;
static char sndOSCData[MAX_MESSAGE_LEN];

// Remote IP Address Initialization
static INT8U remoteIP[] = {224ul, 0ul, 0ul, 1ul};

// Port Number Initialization
static INT16U remotePort = 8000;
static INT16U localPort  = 10000;

// Variables
static INT8U indexA = 0;
static INT8U indexA0 = 0;
static INT8U ringBufIndex = 0;
static INT8U ringProcessIndex = 0;
static INT8U udpPacket[MAX_BUF_SIZE][MAX_PACKET_SIZE] = {0};
static INT8U oscPacket[MAX_PACKET_SIZE] = {0};
static char rcvAddressStrings[MAX_ADDRESS_LEN] = {0};
static INT16U rcvPrefixLength = 0;
static INT16U rcvAddressLength = 0;
static INT16U rcvTypesStartIndex = 0;
static INT16S rcvArgumentsLength = 0;
static char rcvArgsTypeArray[MAX_ARGS_LEN] = {0};
static INT16U rcvArgumentsStartIndex[MAX_ARGS_LEN] = {0};
static INT16U rcvArgumentsIndexLength[MAX_ARGS_LEN] = {0};
static INT8U bundleData[512] = {0};
static INT16U bundleAppendIndex = 0;

// Static Functions
static BOOLEAN copyOSCPacketFromUDPPacket();
static BOOLEAN extractAddressFromOSCPacket();
static BOOLEAN extractTypeTagFromOSCPacket();
static BOOLEAN extractArgumentsFromOSCPacket();

void setInitReceiveFlag(BOOLEAN flag)
{
    initReceiveFlag = flag;
}

BOOLEAN getInitReceiveFlag(void)
{
    return initReceiveFlag;
}

void setInitSendFlag(BOOLEAN flag)
{
    initSendFlag = flag;
}

BOOLEAN getInitSendFlag(void)
{
    return initSendFlag;
}

void setInitDiscoverFlag(BOOLEAN flag)
{
    initDiscoverFlag = flag;
}

BOOLEAN getInitDiscoverFlag(void)
{
    return initDiscoverFlag;
}

void setChCompletedFlag(BOOLEAN flag)
{
    chCompletedFlag = flag;
}

BOOLEAN getChCompletedFlag(void)
{
    return chCompletedFlag;
}

void setRemoteIpAtIndex(INT8U index, INT8U number)
{
    remoteIP[index] = number;
}

INT8U getRemoteIpAtIndex(INT8U index)
{
    return remoteIP[index];
}

INT8U* getRemoteIp(void)
{
    return remoteIP;
}

void setRemotePort(INT16U number)
{
    remotePort = number;
}

INT16U getRemotePort(void)
{
    return remotePort;
}

void setLocalPort(INT16U number)
{
    localPort = number;
}

INT16U getLocalPort(void)
{
    return localPort;
}

void setOSCPrefix(char* prefix_string)
{
    stdPrefix = (char *)calloc(strlen(prefix_string), sizeof(char));
    memcpy(stdPrefix, prefix_string, strlen(prefix_string));
}

char* getOSCPrefix(void)
{
    return stdPrefix;
}

void clearOSCPrefix(void)
{
    free(stdPrefix);
    stdPrefix = NULL;
}

void setOSCHostName(char* host_name)
{
    INT8U hnlen = strlen(host_name);
    //hostName = (char *)calloc(strlen(host_name), sizeof(char));
    memset(hostName, 0, MAX_HOST_NAME_LEN + 1);
    if(hnlen <= MAX_HOST_NAME_LEN)
        memcpy(hostName, host_name, hnlen);
    else
        memcpy(hostName, host_name, MAX_HOST_NAME_LEN);
}

char* getOSCHostName(void)
{
    return hostName;
}

void clearOSCHostName(void)
{
    memset(hostName, 0, MAX_HOST_NAME_LEN + 1);
    //free(hostName);
    //hostName = NULL;
}

void getOSCPacket(INT8U* buf, INT16U len)
{
	memcpy(udpPacket[ringBufIndex], buf, MAX_PACKET_SIZE);

#if 0//debug
	for (int i = 0; i < len; i++)
	{
		printf("udpPacket[%d] = %c %02X\n", i, udpPacket[ringBufIndex][i], udpPacket[ringBufIndex][i]);
	}
	printf("\n");
#endif

	ringBufIndex++;
	ringBufIndex &= (MAX_BUF_SIZE - 1);
}

BOOLEAN processOSCPacket(void)
{
    static INT8U state_index = 0;
    switch(state_index)
    {
        case 0:
            if(!copyOSCPacketFromUDPPacket())
                return FALSE;
            state_index = 1;
            break;
        case 1:
            if(!extractAddressFromOSCPacket())
            {
                state_index = 0;
                return FALSE;
            }
            state_index = 2;
            break;
        case 2:
            if(!extractTypeTagFromOSCPacket())
            {
                state_index = 0;
                return FALSE;
            }
            state_index = 3;
            break;
        case 3:
            if(!extractArgumentsFromOSCPacket())
            {
                state_index = 0;
                return FALSE;
            }
            state_index = 4;
            break;
        case 4:
            state_index = 0;
            return TRUE;
            break;
        default:
            state_index = 0;
            break;
    }
    return FALSE;
}

static BOOLEAN copyOSCPacketFromUDPPacket()
{
    int i, j;
    INT32U len = 0;

    if(!strcmp(udpPacket[ringProcessIndex], "#bundle"))
    {
        j = 16;
        while(TRUE)
        {
            len = 0;
            len += (INT32U)udpPacket[ringProcessIndex][j] << 24;
            len += (INT32U)udpPacket[ringProcessIndex][j + 1] << 16;
            len += (INT32U)udpPacket[ringProcessIndex][j + 2] << 8;
            len += (INT32U)udpPacket[ringProcessIndex][j + 3];

            if(len == 0)
                break;

            for(i = 0; i < len; i++)
                udpPacket[ringBufIndex][i] = udpPacket[ringProcessIndex][j + 4 + i];

            j += (4 + len);

            ringBufIndex = (ringBufIndex + 1) & (MAX_BUF_SIZE - 1);
        }

        memset(udpPacket[ringProcessIndex++], 0, MAX_PACKET_SIZE);
        ringProcessIndex &= (MAX_BUF_SIZE - 1);
    }

    if(udpPacket[ringProcessIndex][0] != '/')
    {
        if(udpPacket[ringProcessIndex][0] != NULL)
            memset(udpPacket[ringProcessIndex], 0, MAX_PACKET_SIZE);

        ringProcessIndex++;
        ringProcessIndex &= (MAX_BUF_SIZE - 1);

        return FALSE;
    }

    memcpy(oscPacket, udpPacket[ringProcessIndex], MAX_PACKET_SIZE);
    memset(udpPacket[ringProcessIndex++], 0, MAX_PACKET_SIZE);

    ringProcessIndex &= (MAX_BUF_SIZE - 1);

    return TRUE;
}

static BOOLEAN extractAddressFromOSCPacket()
{
    memset(rcvAddressStrings, 0, sizeof(rcvAddressStrings));

    indexA = 3;
    while(oscPacket[indexA])
    {
        indexA += 4;
        if(indexA >= MAX_PACKET_SIZE)
            return FALSE;
    }
    indexA0 = indexA;
    indexA--;
    while(!oscPacket[indexA])
        indexA--;
    indexA++;

    memcpy(rcvAddressStrings, oscPacket, indexA);

    //debug printf("address = %s\n", rcvAddressStrings);

    rcvAddressLength = indexA;

    return TRUE;
}

static BOOLEAN extractTypeTagFromOSCPacket()
{
    rcvTypesStartIndex = indexA0 + 1;
    indexA = indexA0 + 2;

    while(oscPacket[indexA])
    {
        indexA++;
        if(indexA >= MAX_PACKET_SIZE)
            return FALSE;
    }
    memcpy(rcvArgsTypeArray, oscPacket + rcvTypesStartIndex + 1, indexA - rcvTypesStartIndex);

    //debug printf("typetag = %s\n", rcvArgsTypeArray);

    return TRUE;
}

static BOOLEAN extractArgumentsFromOSCPacket()
{
    INT16S i = 0, n = 0, u = 0, length = 0;

    if(indexA == 0 || indexA >= MAX_PACKET_SIZE)
        return FALSE;

    rcvArgumentsLength = indexA - rcvTypesStartIndex;
    n = ((rcvArgumentsLength / 4) + 1) * 4;

    //debug printf("arglen = %d\n", rcvArgumentsLength - 1);

    if(!rcvArgumentsLength)
        return FALSE;

    for(i = 0; i < rcvArgumentsLength - 1; i++)
    {
        rcvArgumentsStartIndex[i] = rcvTypesStartIndex + length + n;
        switch(rcvArgsTypeArray[i])
        {
            case 'i':
            case 'f':
                length += 4;
                rcvArgumentsIndexLength[i] = 4;
                break;
            case 's':
                u = 0;
                while(oscPacket[rcvArgumentsStartIndex[i] + u])
                {
                    u++;
                    if((rcvArgumentsStartIndex[i] + u) >= MAX_PACKET_SIZE)
                        return FALSE;
                }

                rcvArgumentsIndexLength[i] = ((u / 4) + 1) * 4;

                length += rcvArgumentsIndexLength[i];
                break;
            default: // T, F,N,I and others
                break;
        }
    }
    return TRUE;
}

void sendOSCMessage(const char* prefix, const char* command, const char* type, ...)
{
    va_list list;

    INT32S prefixSize = strchr(prefix, 0) - prefix;
    INT32S commandSize = strchr(command, 0) - command;
    INT32S addressSize = prefixSize + commandSize;
    INT32S typeSize = strchr(type, 0) - type;
    INT32S zeroSize = 0;
    INT32S totalSize = 0;

    int ivalue = 0;
    float fvalue = 0.0;
    char* fchar = NULL;
    char* cstr = NULL;
    int cstr_len = 0;
    char str[MAX_MESSAGE_LEN];
    memset(str, 0, MAX_MESSAGE_LEN);

    //if(isOSCPutReady() || isDiscoverPutReady())
    {
        //debug LED_1_On();

        sprintf(str, "%s%s", prefix, command);

        zeroSize = (addressSize ^ ((addressSize >> 3) << 3)) == 0 ? 0 : 8 - (addressSize ^ ((addressSize >> 3) << 3));
        if(zeroSize == 0)
            zeroSize = 4;
        else if(zeroSize > 4 && zeroSize < 8)
            zeroSize -= 4;

        totalSize = (addressSize + zeroSize);
        sprintf((str + totalSize), ",%s", type);

        typeSize++;
        zeroSize = (typeSize ^ ((typeSize >> 2) << 2)) == 0 ? 0 : 4 - (typeSize ^ ((typeSize >> 2) << 2));
        if(zeroSize == 0)
            zeroSize = 4;

        totalSize += (typeSize + zeroSize);

        va_start(list, type);
        while(*type)
        {
            switch(*type)
            {
                case 'i':
                    ivalue = va_arg(list, int);
                    str[totalSize++] = (ivalue >> 24) & 0xFF;
                    str[totalSize++] = (ivalue >> 16) & 0xFF;
                    str[totalSize++] = (ivalue >> 8) & 0xFF;
                    str[totalSize++] = (ivalue >> 0) & 0xFF;
                    break;
                case 'f':
                    fvalue = (float)va_arg(list, double);
                    fchar = (char *)&fvalue;
                    str[totalSize++] = fchar[3] & 0xFF;
                    str[totalSize++] = fchar[2] & 0xFF;
                    str[totalSize++] = fchar[1] & 0xFF;
                    str[totalSize++] = fchar[0] & 0xFF;
                    break;
                case 's':
                    cstr = va_arg(list, char*);
                    cstr_len = 0;
                    while(*cstr)
                    {
                        str[totalSize + cstr_len] = *(cstr++) & 0xFF;
                        cstr_len++;
                    }
                    totalSize += ((cstr_len / 4) + 1) * 4;
                    break;
                default: // T, F, N, I and others
                    break;
            }
            type++;
        }
        va_end(list);

        //UDPPutArray((INT8U *)str, totalSize);
        //UDPFlush();

        //debug LED_1_Off();
    }
}

void setOSCAddress(const char* prefix, const char* command)
{
    INT32S prefixSize = strchr(prefix, 0) - prefix;
    INT32S commandSize = strchr(command, 0) - command;
    INT32S addressSize = prefixSize + commandSize;
    INT32S zeroSize = 0;

    memset(sndOSCData, 0, MAX_MESSAGE_LEN);
    oscTotalSize = 0;

    sprintf(sndOSCData, "%s%s", prefix, command);

    zeroSize = (addressSize ^ ((addressSize >> 3) << 3)) == 0 ? 0 : 8 - (addressSize ^ ((addressSize >> 3) << 3));
    if(zeroSize == 0)
        zeroSize = 4;
    else if(zeroSize > 4 && zeroSize < 8)
        zeroSize -= 4;

    oscTotalSize = (addressSize + zeroSize);
}

void setOSCTypeTag(const char* type)
{
    INT32S typeSize = strchr(type, 0) - type;
    INT32S zeroSize = 0;

    sprintf((sndOSCData + oscTotalSize), ",%s", type);

    typeSize++;
    zeroSize = (typeSize ^ ((typeSize >> 2) << 2)) == 0 ? 0 : 4 - (typeSize ^ ((typeSize >> 2) << 2));
    if(zeroSize == 0)
        zeroSize = 4;

    oscTotalSize += (typeSize + zeroSize);
}

void addOSCIntArgument(int value)
{
    sndOSCData[oscTotalSize++] = (value >> 24) & 0xFF;
    sndOSCData[oscTotalSize++] = (value >> 16) & 0xFF;
    sndOSCData[oscTotalSize++] = (value >> 8) & 0xFF;
    sndOSCData[oscTotalSize++] = (value >> 0) & 0xFF;
}

void addOSCFloatArgument(float value)
{
    char* fchar = NULL;
    fchar = (char *)&value;
    sndOSCData[oscTotalSize++] = fchar[3] & 0xFF;
    sndOSCData[oscTotalSize++] = fchar[2] & 0xFF;
    sndOSCData[oscTotalSize++] = fchar[1] & 0xFF;
    sndOSCData[oscTotalSize++] = fchar[0] & 0xFF;
}

void addOSCStringArgument(char* str)
{
    int cstr_len = 0;
    while(*str)
    {
        sndOSCData[oscTotalSize + cstr_len] = *(str++) & 0xFF;
        cstr_len++;
    }
    oscTotalSize += ((cstr_len / 4) + 1) * 4;
}

void clearOSCMessage(void)
{
    memset(sndOSCData, 0, MAX_MESSAGE_LEN);
    oscTotalSize = 0;
}

void flushOSCMessage(void)
{
}

void clearOSCBundle(void)
{
    memset(bundleData, 0, sizeof(bundleData));
    bundleData[0] = '#';
    bundleData[1] = 'b';
    bundleData[2] = 'u';
    bundleData[3] = 'n';
    bundleData[4] = 'd';
    bundleData[5] = 'l';
    bundleData[6] = 'e';

    bundleAppendIndex = 16;
}

void appendOSCMessageToBundle(const char* prefix, const char* command, const char* type, ...)
{
    va_list list;

    INT32S prefixSize = strchr(prefix, 0) - prefix;
    INT32S commandSize = strchr(command, 0) - command;
    INT32S addressSize = prefixSize + commandSize;
    INT32S typeSize = strchr(type, 0) - type;
    INT32S zeroSize = 0;
    INT32S totalSize = 0;

    int ivalue = 0;
    float fvalue = 0.0;
    char* fchar = NULL;
    char* cstr = NULL;
    int cstr_len = 0;
    char str[MAX_MESSAGE_LEN];
    memset(str, 0, MAX_MESSAGE_LEN);

    sprintf(str, "%s%s", prefix, command);

    zeroSize = (addressSize ^ ((addressSize >> 3) << 3)) == 0 ? 0 : 8 - (addressSize ^ ((addressSize >> 3) << 3));
    if(zeroSize == 0)
        zeroSize = 4;
    else if(zeroSize > 4 && zeroSize < 8)
        zeroSize -= 4;

    totalSize = (addressSize + zeroSize);
    sprintf((str + totalSize), ",%s", type);

    typeSize++;
    zeroSize = (typeSize ^ ((typeSize >> 2) << 2)) == 0 ? 0 : 4 - (typeSize ^ ((typeSize >> 2) << 2));
    if(zeroSize == 0)
        zeroSize = 4;

    totalSize += (typeSize + zeroSize);

    va_start(list, type);
    while(*type)
    {
        switch(*type)
        {
            case 'i':
                ivalue = va_arg(list, int);
                str[totalSize++] = (ivalue >> 24) & 0xFF;
                str[totalSize++] = (ivalue >> 16) & 0xFF;
                str[totalSize++] = (ivalue >> 8) & 0xFF;
                str[totalSize++] = (ivalue >> 0) & 0xFF;
                break;
            case 'f':
                fvalue = (float)va_arg(list, double);
                fchar = (char *)&fvalue;
                str[totalSize++] = fchar[3] & 0xFF;
                str[totalSize++] = fchar[2] & 0xFF;
                str[totalSize++] = fchar[1] & 0xFF;
                str[totalSize++] = fchar[0] & 0xFF;
                break;
            case 's':
                cstr = va_arg(list, char*);
                cstr_len = 0;
                while(*cstr)
                {
                    str[totalSize + cstr_len] = *(cstr++) & 0xFF;
                    cstr_len++;
                }
                totalSize += ((cstr_len / 4) + 1) * 4;
                break;
            default: // T, F, N, I and others
                break;
        }
        type++;
    }
    va_end(list);

    bundleData[bundleAppendIndex++] = (totalSize >> 24) & 0xFF;
    bundleData[bundleAppendIndex++] = (totalSize >> 16) & 0xFF;
    bundleData[bundleAppendIndex++] = (totalSize >> 8) & 0xFF;
    bundleData[bundleAppendIndex++] = (totalSize) & 0xFF;
    memcpy(bundleData + bundleAppendIndex, str, totalSize);
    bundleAppendIndex += totalSize;
}

void sendOSCBundle(void)
{
}

BOOLEAN compareOSCPrefix(const char* prefix)
{
    int i = 0;
    rcvPrefixLength = strlen(prefix);

    if(rcvAddressLength < rcvPrefixLength)
        return FALSE;

    while(prefix[i])
    {
        if(rcvAddressStrings[i] != prefix[i])
            return FALSE;

        i++;
        if(i > rcvPrefixLength)
            return FALSE;
    }
    return TRUE;
}

BOOLEAN compareOSCAddress(const char* address)
{
    int i = rcvPrefixLength, j = 0;
    INT16U address_len = strlen(address);

    if(rcvAddressLength > rcvPrefixLength + address_len)
        return FALSE;

    while(address[j])
    {
        if(rcvAddressStrings[i] != address[j])
            return FALSE;

        i++;
        j++;
        if((i > rcvPrefixLength + address_len) || (j > address_len))
            return FALSE;
    }
    return TRUE;
}

BOOLEAN compareTypeTagAtIndex(const INT16U index, const char typetag)
{
    if(index >= rcvArgumentsLength - 1 || rcvArgsTypeArray[index] != typetag)
        return FALSE;

    return TRUE;
}

INT16U getArgumentsLength(void)
{
    return rcvArgumentsLength - 1;
}

INT32S getIntArgumentAtIndex(const INT16U index)
{
    INT16S s = 0;
    INT32S sign = 0, exponent = 0, mantissa = 0;
    long long lvalue = 0;
    float fvalue = 0.0;
    float sum = 0.0;

    if(index >= rcvArgumentsLength - 1)
        return 0;

    switch(rcvArgsTypeArray[index])
    {
        case 'i':
            lvalue = ((oscPacket[rcvArgumentsStartIndex[index]] & 0xFF) << 24) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 1] & 0xFF) << 16) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 2] & 0xFF) << 8) |
                      (oscPacket[rcvArgumentsStartIndex[index] + 3] & 0xFF);
            break;
        case 'f':
            lvalue = ((oscPacket[rcvArgumentsStartIndex[index]] & 0xFF) << 24) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 1] & 0xFF) << 16) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 2] & 0xFF) << 8) |
                      (oscPacket[rcvArgumentsStartIndex[index] + 3] & 0xFF);
            lvalue &= 0xffffffff;

            sign = ((lvalue >> 31) & 0x01) ? -1 : 1;
            exponent = ((lvalue >> 23) & 0xFF) - 127;
            mantissa = lvalue & 0x7FFFFF;

            for(s = 0; s < 23; s++)
            {
                int onebit = (mantissa >> (22 - s)) & 0x1;
                sum += (float)onebit * (1.0 / (float)(1 << (s + 1)));
            }
            sum += 1.0;

            if(exponent >= 0)
                fvalue = sign * sum * (1 << exponent);
            else
                fvalue = sign * sum * (1.0 / (float)(1 << abs(exponent)));
            lvalue = (int)fvalue;
            break;
    }
    return lvalue;
}

float getFloatArgumentAtIndex(const INT16U index)
{
    INT16S s = 0;
    INT32S sign = 0, exponent = 0, mantissa = 0;
    long long lvalue = 0;
    float fvalue = 0.0;
    float sum = 0.0;

    if(index >= rcvArgumentsLength - 1)
        return 0.0;

    switch(rcvArgsTypeArray[index])
    {
        case 'i':
            lvalue = (oscPacket[rcvArgumentsStartIndex[index]] << 24) |
                     (oscPacket[rcvArgumentsStartIndex[index] + 1] << 16) |
                     (oscPacket[rcvArgumentsStartIndex[index] + 2] << 8) |
                      oscPacket[rcvArgumentsStartIndex[index] + 3];
            fvalue = (float)lvalue;
            break;
        case 'f':
            lvalue = ((oscPacket[rcvArgumentsStartIndex[index]] & 0xFF) << 24) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 1] & 0xFF) << 16) |
                     ((oscPacket[rcvArgumentsStartIndex[index] + 2] & 0xFF) << 8) |
                      (oscPacket[rcvArgumentsStartIndex[index] + 3] & 0xFF);
            lvalue &= 0xffffffff;

            sign = ((lvalue >> 31) & 0x01) ? -1 : 1;
            exponent = ((lvalue >> 23) & 0xFF) - 127;
            mantissa = lvalue & 0x7FFFFF;

            for(s = 0; s < 23; s++)
            {
                int onebit = (mantissa >> (22 - s)) & 0x1;
                sum += (float)onebit * (1.0 / (float)(1 << (s + 1)));
            }
            sum += 1.0;

            if(exponent >= 0)
                fvalue = sign * sum * (1 << exponent);
            else
                fvalue = sign * sum * (1.0 / (float)(1 << abs(exponent)));
            break;
    }
    return fvalue;
}

char* getStringArgumentAtIndex(const INT16U index)
{
    if(index >= rcvArgumentsLength - 1)
        return "error";

    switch(rcvArgsTypeArray[index])
    {
        case 'i':
        case 'f':
            return "error";
            break;
        case 's':
            return oscPacket + rcvArgumentsStartIndex[index];
            break;
    }
    return "error";
}

BOOLEAN getBOOLEANeanArgumentAtIndex(const INT16U index)
{
    BOOLEAN flag = FALSE;

    if(index >= rcvArgumentsLength - 1)
        return flag;

    switch(rcvArgsTypeArray[index])
    {
        case 'T':
            flag = TRUE;
            break;
        case 'F':
            flag = FALSE;
            break;
    }
    return flag;
}
