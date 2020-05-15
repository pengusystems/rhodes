#ifndef _ALAZARAPI_H
#define _ALAZARAPI_H

/**
 * @file AlazarApi.h
 *
 * @author Alazar Technologies Inc
 *
 * @copyright Copyright (c) 2015 Alazar Technologies Inc. All Rights
 * Reserved.  Unpublished - rights reserved under the Copyright laws
 * of the United States And Canada.
 * This product contains confidential information and trade secrets
 * of Alazar Technologies Inc. Use, disclosure, or reproduction is
 * prohibited without the prior express written permission of Alazar
 * Technologies Inc
 *
 * API used for Alazar Acquisition Products
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
#include <stdbool.h>
#endif

#include "AlazarCmd.h"
#include "AlazarError.h"

#include "wchar.h"
#ifdef _WIN32
#include <windows.h>
typedef signed char S8, *PS8;
typedef unsigned char U8, *PU8, BOOLEAN;
typedef signed short S16, *PS16;
typedef unsigned short U16, *PU16;
typedef signed long S32, *PS32;
typedef unsigned long U32, *PU32;
typedef __int64 S64, *PS64;
typedef unsigned __int64 U64, *PU64;
typedef void *HANDLE;
#else // _WIN32
#include <linux/types.h>
#ifndef __PCITYPES_H // Types are already defined in PciTypes.h
typedef __s8 S8, *PS8;
typedef __u8 U8, *PU8, BOOLEAN;
typedef __s16 S16, *PS16;
typedef __u16 U16, *PU16;
typedef __s32 S32, *PS32;
typedef __u32 U32, *PU32;
typedef __s64 LONGLONG, S64, *PS64;
typedef __u64 ULONGLONG, U64, *PU64;
typedef void *HANDLE;
typedef int PLX_DRIVER_HANDLE; // Linux-specific driver handle
#define INVALID_HANDLE_VALUE (HANDLE) - 1
#endif // __PCITYPES_H
#endif // _WIN32

#ifndef EXPORT
#define EXPORT
#endif

#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef BYTE
#define BYTE U8
#endif

#ifndef INT64
#define INT64 S64
#endif

typedef enum BoardTypes {
  ATS_NONE, // 0
  ATS850,   // 1
  ATS310,   // 2
  ATS330,   // 3
  ATS855,   // 4
  ATS315,   // 5
  ATS335,   // 6
  ATS460,   // 7
  ATS860,   // 8
  ATS660,   // 9
  ATS665,   // 10
  ATS9462,  // 11
  ATS9434,  // 12
  ATS9870,  // 13
  ATS9350,  // 14
  ATS9325,  // 15
  ATS9440,  // 16
  ATS9410,  // 17
  ATS9351,  // 18
  ATS9310,  // 19
  ATS9461,  // 20
  ATS9850,  // 21
  ATS9625,  // 22
  ATG6500,  // 23
  ATS9626,  // 24
  ATS9360,  // 25
  AXI8870,  // 26
  ATS9370,  // 27
  ATU7825,  // 28
  ATS9373,  // 29
  ATS9416,  // 30
  ATS_LAST
} ALAZAR_BOARDTYPES;

// Board Definition structure
typedef struct _BoardDef {
  U32 RecordCount;
  U32 RecLength;
  U32 PreDepth;
  U32 ClockSource;
  U32 ClockEdge;
  U32 SampleRate;
  U32 CouplingChanA;
  U32 InputRangeChanA;
  U32 InputImpedChanA;
  U32 CouplingChanB;
  U32 InputRangeChanB;
  U32 InputImpedChanB;
  U32 TriEngOperation;
  U32 TriggerEngine1;
  U32 TrigEngSource1;
  U32 TrigEngSlope1;
  U32 TrigEngLevel1;
  U32 TriggerEngine2;
  U32 TrigEngSource2;
  U32 TrigEngSlope2;
  U32 TrigEngLevel2;
} BoardDef, *pBoardDef;

// Constants to be used in the Application when dealing with Custom FPGAs
#define FPGA_GETFIRST 0xFFFFFFFF
#define FPGA_GETNEXT 0xFFFFFFFE
#define FPGA_GETLAST 0xFFFFFFFC
RETURN_CODE EXPORT
    AlazarGetOEMFPGAName(int opcodeID, char *FullPath, unsigned long *error);
RETURN_CODE EXPORT
    AlazarOEMSetWorkingDirectory(char *wDir, unsigned long *error);
RETURN_CODE EXPORT
    AlazarOEMGetWorkingDirectory(char *wDir, unsigned long *error);
RETURN_CODE EXPORT AlazarParseFPGAName(const char *FullName, char *Name,
                                       U32 *Type, U32 *MemSize, U32 *MajVer,
                                       U32 *MinVer, U32 *MajRev, U32 *MinRev,
                                       U32 *error);
RETURN_CODE EXPORT AlazarDownLoadFPGA(HANDLE h, char *FileName, U32 *RetValue);
RETURN_CODE EXPORT
    AlazarOEMDownLoadFPGA(HANDLE h, char *FileName, U32 *RetValue);

#define ADMA_CLOCKSOURCE 0x00000001
#define ADMA_CLOCKEDGE 0x00000002
#define ADMA_SAMPLERATE 0x00000003
#define ADMA_INPUTRANGE 0x00000004
#define ADMA_INPUTCOUPLING 0x00000005
#define ADMA_IMPUTIMPEDENCE 0x00000006
#define ADMA_INPUTIMPEDANCE 0x00000006
#define ADMA_EXTTRIGGERED 0x00000007
#define ADMA_CHA_TRIGGERED 0x00000008
#define ADMA_CHB_TRIGGERED 0x00000009
#define ADMA_TIMEOUT 0x0000000A
#define ADMA_THISCHANTRIGGERED 0x0000000B
#define ADMA_SERIALNUMBER 0x0000000C
#define ADMA_SYSTEMNUMBER 0x0000000D
#define ADMA_BOARDNUMBER 0x0000000E
#define ADMA_WHICHCHANNEL 0x0000000F
#define ADMA_SAMPLERESOLUTION 0x00000010
#define ADMA_DATAFORMAT 0x00000011

struct _HEADER0 {
  unsigned int SerialNumber : 18;
  unsigned int SystemNumber : 4;
  unsigned int WhichChannel : 1;
  unsigned int BoardNumber : 4;
  unsigned int SampleResolution : 3;
  unsigned int DataFormat : 2;
};

struct _HEADER1 {
  unsigned int RecordNumber : 24;
  unsigned int BoardType : 8;
};

struct _HEADER2 {
  unsigned int TimeStampLowPart;
};

struct _HEADER3 {
  unsigned int TimeStampHighPart : 8;
  unsigned int ClockSource : 2;
  unsigned int ClockEdge : 1;
  unsigned int SampleRate : 7;
  unsigned int InputRange : 5;
  unsigned int InputCoupling : 2;
  unsigned int InputImpedence : 2;
  unsigned int ExternalTriggered : 1;
  unsigned int ChannelBTriggered : 1;
  unsigned int ChannelATriggered : 1;
  unsigned int TimeOutOccurred : 1;
  unsigned int ThisChannelTriggered : 1;
};

typedef struct _ALAZAR_HEADER {
  struct _HEADER0 hdr0;
  struct _HEADER1 hdr1;
  struct _HEADER2 hdr2;
  struct _HEADER3 hdr3;
} ALAZAR_HEADER, *PALAZAR_HEADER;

typedef enum _AUTODMA_STATUS {
  ADMA_Completed = 0,
  ADMA_Buffer1Invalid,
  ADMA_Buffer2Invalid,
  ADMA_BoardHandleInvalid,
  ADMA_InternalBuffer1Invalid,
  ADMA_InternalBuffer2Invalid,
  ADMA_OverFlow,
  ADMA_InvalidChannel,
  ADMA_DMAInProgress,
  ADMA_UseHeaderNotSet,
  ADMA_HeaderNotValid,
  ADMA_InvalidRecsPerBuffer,
  ADMA_InvalidTransferOffset,
  ADMA_InvalidCFlags
} AUTODMA_STATUS;
#define ADMA_Success ADMA_Completed


// ****************************************************************************************
// ALAZAR CUSTOMER SUPPORT API
//
// Global API Functions
typedef enum _MSILS {
  KINDEPENDENT = 0,
  KSLAVE = 1,
  KMASTER = 2,
  KLASTSLAVE = 3
} MSILS;

/******************************************
* Trouble Shooting Alazar Functions
******************************************/

RETURN_CODE EXPORT
    AlazarReadWriteTest(HANDLE h, U32 *Buffer, U32 SizeToWrite, U32 SizeToRead);
RETURN_CODE EXPORT AlazarMemoryTest(HANDLE h, U32 *errors);
RETURN_CODE EXPORT AlazarBusyFlag(HANDLE h, BOOLEAN *BusyFlag);
RETURN_CODE EXPORT AlazarTriggeredFlag(HANDLE h, BOOLEAN *TriggeredFlag);
RETURN_CODE EXPORT AlazarGetSDKVersion(U8 *Major, U8 *Minor, U8 *Revision);
RETURN_CODE EXPORT AlazarGetDriverVersion(U8 *Major, U8 *Minor, U8 *Revision);
RETURN_CODE EXPORT AlazarGetBoardRevision(HANDLE hBoard, U8 *Major, U8 *Minor);

U32 EXPORT AlazarBoardsFound();
HANDLE EXPORT AlazarOpen(char *BoardNameID); // e.x. ATS850-0, ATS850-1 ....
void EXPORT AlazarClose(HANDLE h);
U32 EXPORT AlazarGetBoardKind(HANDLE h);
RETURN_CODE EXPORT AlazarGetCPLDVersion(HANDLE h, U8 *Major, U8 *Minor);
RETURN_CODE EXPORT AlazarAutoCalibrate(HANDLE h);
RETURN_CODE EXPORT AlazarGetChannelInfo(HANDLE h, U32 *MemSize, U8 *SampleSize);

// ****************************************************************************************
// Input Control API Functions
RETURN_CODE EXPORT AlazarInputControl(HANDLE h, U8 Channel, U32 Coupling,
                                      U32 InputRange, U32 Impedance);

RETURN_CODE EXPORT AlazarInputControlEx(HANDLE hBoard, U32 uChannel, U32 uCouplingId,
                                        U32 uRangeId, U32 uImpedanceId);

RETURN_CODE EXPORT
    AlazarSetPosition(HANDLE h, U8 Channel, int PMPercent, U32 InputRange);
RETURN_CODE EXPORT AlazarSetExternalTrigger(HANDLE h, U32 Coupling, U32 Range);

// ****************************************************************************************
// Trigger API Functions
RETURN_CODE EXPORT AlazarSetTriggerDelay(HANDLE h, U32 Delay);
RETURN_CODE EXPORT AlazarSetTriggerTimeOut(HANDLE h, U32 to_ns);
U32 EXPORT AlazarTriggerTimedOut(HANDLE h);
RETURN_CODE EXPORT
    AlazarGetTriggerAddress(HANDLE h, U32 Record, U32 *TriggerAddress,
                            U32 *TimeStampHighPart, U32 *TimeStampLowPart);
RETURN_CODE EXPORT
    AlazarSetTriggerOperation(HANDLE h, U32 TriggerOperation,
                              U32 TriggerEngine1 /*j,K*/, U32 Source1,
                              U32 Slope1, U32 Level1,
                              U32 TriggerEngine2 /*j,K*/, U32 Source2,
                              U32 Slope2, U32 Level2);
RETURN_CODE EXPORT AlazarGetTriggerTimestamp(HANDLE Handle, U32 Record,
                                             U64 *Timestamp_samples);
RETURN_CODE EXPORT AlazarSetTriggerOperationForScanning(HANDLE h, U32 slope,
                                                        U32 level, U32 options);

// ****************************************************************************************
// Capture API Functions
RETURN_CODE EXPORT AlazarAbortCapture(HANDLE h);
RETURN_CODE EXPORT AlazarForceTrigger(HANDLE h);
RETURN_CODE EXPORT AlazarForceTriggerEnable(HANDLE h);
RETURN_CODE EXPORT AlazarStartCapture(HANDLE h);
RETURN_CODE EXPORT AlazarCaptureMode(HANDLE h, U32 Mode);

// ****************************************************************************************
// OEM API Functions
RETURN_CODE EXPORT AlazarStreamCapture(HANDLE h, void *Buffer, U32 BufferSize,
                                       U32 DeviceOption, U32 ChannelSelect,
                                       U32 *error);

RETURN_CODE EXPORT AlazarHyperDisp(HANDLE h, void *Buffer, U32 BufferSize,
                                   U8 *ViewBuffer, U32 ViewBufferSize,
                                   U32 NumOfPixels, U32 Option,
                                   U32 ChannelSelect, U32 Record,
                                   long TransferOffset, U32 *error);

RETURN_CODE EXPORT AlazarFastPRRCapture(HANDLE h, void *Buffer, U32 BufferSize,
                                        U32 DeviceOption, U32 ChannelSelect,
                                        U32 *error);

// ****************************************************************************************
// Status API Functions
U32 EXPORT AlazarBusy(HANDLE h);
U32 EXPORT AlazarTriggered(HANDLE h);
U32 EXPORT AlazarGetStatus(HANDLE h);

// ****************************************************************************************
// MulRec API Functions
U32 EXPORT AlazarDetectMultipleRecord(HANDLE h);
RETURN_CODE EXPORT AlazarSetRecordCount(HANDLE h, U32 Count);
RETURN_CODE EXPORT AlazarSetRecordSize(HANDLE h, U32 PreSize, U32 PostSize);

// ****************************************************************************************
// Clock Control API Functions
RETURN_CODE EXPORT AlazarSetCaptureClock(HANDLE h, U32 Source, U32 Rate,
                                         U32 Edge, U32 Decimation);
RETURN_CODE EXPORT AlazarSetExternalClockLevel(HANDLE h, float level_percent);
RETURN_CODE EXPORT AlazarSetClockSwitchOver(HANDLE hBoard, U32 uMode,
                                            U32 uDummyClockOnTime_ns,
                                            U32 uReserved);

#define CSO_DISABLE 0
#define CSO_ENABLE_DUMMY_CLOCK 1
#define CSO_TRIGGER_LOW_DUMMY_CLOCK 2

// ****************************************************************************************
// Data Transfer API Functions
U32 EXPORT AlazarRead(HANDLE h, U32 Channel, void *Buffer, int ElementSize,
                      long Record, long TransferOffset, U32 TransferLength);

// ****************************************************************************************
// Individual Parameter API Functions
RETURN_CODE EXPORT
    AlazarSetParameter(HANDLE h, U8 Channel, U32 Parameter, long Value);
RETURN_CODE EXPORT
    AlazarSetParameterUL(HANDLE h, U8 Channel, U32 Parameter, U32 Value);
RETURN_CODE EXPORT
    AlazarGetParameter(HANDLE h, U8 Channel, U32 Parameter, long *RetValue);
RETURN_CODE EXPORT
    AlazarGetParameterUL(HANDLE h, U8 Channel, U32 Parameter, U32 *RetValue);

// ****************************************************************************************
// Handle and System Management API Functions
HANDLE EXPORT AlazarGetSystemHandle(U32 sid);
U32 EXPORT AlazarNumOfSystems();
U32 EXPORT AlazarBoardsInSystemBySystemID(U32 sid);
U32 EXPORT AlazarBoardsInSystemByHandle(HANDLE systemHandle);
HANDLE EXPORT AlazarGetBoardBySystemID(U32 sid, U32 brdNum);
HANDLE EXPORT AlazarGetBoardBySystemHandle(HANDLE systemHandle, U32 brdNum);
RETURN_CODE EXPORT AlazarSetLED(HANDLE h, U32 state);

// ****************************************************************************************
// Board capability query functions
RETURN_CODE EXPORT
    AlazarQueryCapability(HANDLE h, U32 request, U32 value, U32 *retValue);
U32 EXPORT AlazarMaxSglTransfer(ALAZAR_BOARDTYPES bt);
RETURN_CODE EXPORT
    AlazarGetMaxRecordsCapable(HANDLE h, U32 RecordLength, U32 *num);

// ****************************************************************************************
// Trigger Inquiry Functions
// Return values:
//              NEITHER   = 0
//              Channel A = 1
//              Channel B = 2
//              External  = 3
//              A AND B   = 4
//              A AND Ext = 5
//              B And Ext = 6
//              Timeout   = 7
U32 EXPORT AlazarGetWhoTriggeredBySystemHandle(HANDLE systemHandle, U32 brdNum,
                                               U32 recNum);
U32 EXPORT AlazarGetWhoTriggeredBySystemID(U32 sid, U32 brdNum, U32 recNum);

RETURN_CODE EXPORT AlazarSetBWLimit(HANDLE h, U32 Channel, U32 enable);
RETURN_CODE EXPORT AlazarSleepDevice(HANDLE h, U32 state);

// AUTODMA Related Routines
//
// Control Flags for AutoDMA used in AlazarStartAutoDMA
#define ADMA_EXTERNAL_STARTCAPTURE 0x00000001
#define ADMA_ENABLE_RECORD_HEADERS 0x00000008
#define ADMA_SINGLE_DMA_CHANNEL 0x00000010
#define ADMA_ALLOC_BUFFERS 0x00000020
#define ADMA_TRADITIONAL_MODE 0x00000000
#define ADMA_CONTINUOUS_MODE 0x00000100
#define ADMA_NPT 0x00000200
#define ADMA_TRIGGERED_STREAMING 0x00000400
#define ADMA_FIFO_ONLY_STREAMING 0x00000800
#define ADMA_INTERLEAVE_SAMPLES 0x00001000
#define ADMA_GET_PROCESSED_DATA 0x00002000
#define ADMA_DSP 0x00004000
#define ADMA_ENABLE_RECORD_FOOTERS 0x00010000

RETURN_CODE EXPORT AlazarStartAutoDMA(HANDLE h, void *Buffer1, U32 UseHeader,
                                      U32 ChannelSelect, long TransferOffset,
                                      U32 TransferLength, long RecordsPerBuffer,
                                      long RecordCount, AUTODMA_STATUS *error,
                                      U32 r1, U32 r2, U32 *r3, U32 *r4);
RETURN_CODE EXPORT
    AlazarGetNextAutoDMABuffer(HANDLE h, void *Buffer1, void *Buffer2,
                               long *WhichOne, long *RecordsTransfered,
                               AUTODMA_STATUS *error, U32 r1, U32 r2,
                               long *TriggersOccurred, U32 *r4);
RETURN_CODE EXPORT AlazarGetNextBuffer(HANDLE h, void *Buffer1, void *Buffer2,
                                       long *WhichOne, long *RecordsTransfered,
                                       AUTODMA_STATUS *error, U32 r1, U32 r2,
                                       long *TriggersOccurred, U32 *r4);
RETURN_CODE EXPORT AlazarCloseAUTODma(HANDLE h);
RETURN_CODE EXPORT AlazarAbortAutoDMA(HANDLE h, void *Buffer,
                                      AUTODMA_STATUS *error, U32 r1, U32 r2,
                                      U32 *r3, U32 *r4);
U32 EXPORT AlazarGetAutoDMAHeaderValue(HANDLE h, U32 Channel, void *DataBuffer,
                                       U32 Record, U32 Parameter,
                                       AUTODMA_STATUS *error);
float EXPORT AlazarGetAutoDMAHeaderTimeStamp(HANDLE h, U32 Channel,
                                             void *DataBuffer, U32 Record,
                                             AUTODMA_STATUS *error);
void EXPORT *AlazarGetAutoDMAPtr(HANDLE h, U32 DataOrHeader, U32 Channel,
                                 void *DataBuffer, U32 Record,
                                 AUTODMA_STATUS *error);
U32 EXPORT AlazarWaitForBufferReady(HANDLE h, long tms);
RETURN_CODE EXPORT AlazarEvents(HANDLE h, U32 enable);

RETURN_CODE EXPORT
    AlazarBeforeAsyncRead(HANDLE hBoard, U32 uChannelSelect,
                          long lTransferOffset, U32 uSamplesPerRecord,
                          U32 uRecordsPerBuffer, U32 uRecordsPerAcquisition,
                          U32 uFlags);

RETURN_CODE EXPORT AlazarAbortAsyncRead(HANDLE hBoard);

RETURN_CODE EXPORT AlazarPostAsyncBuffer(HANDLE hDevice, void *pBuffer,
                                         U32 uBufferLength_bytes);

RETURN_CODE EXPORT AlazarWaitAsyncBufferComplete(HANDLE hDevice, void *pBuffer,
                                                 U32 uTimeout_ms);

RETURN_CODE EXPORT
    AlazarWaitNextAsyncBufferComplete(HANDLE hDevice, void *pBuffer,
                                      U32 uBufferLength_bytes, U32 uTimeout_ms);

#ifdef _WIN32
RETURN_CODE EXPORT
AlazarCreateStreamFileA(
    HANDLE hDevice,
    const char *pszFilePath);

RETURN_CODE EXPORT
AlazarCreateStreamFileW(
    HANDLE hDevice,
    const wchar_t *pszFilePath);
#ifdef UNICODE
#define AlazarCreateStreamFile AlazarCreateStreamFileW
#else // UNICODE
#define AlazarCreateStreamFile AlazarCreateStreamFileA
#endif // UNICODE
#else  // WIN32
RETURN_CODE EXPORT
AlazarCreateStreamFile(HANDLE hDevice, const char *pszFilePath);
#endif

long EXPORT AlazarFlushAutoDMA(HANDLE h);
void EXPORT AlazarStopAutoDMA(HANDLE h);

// TimeStamp Control Api
RETURN_CODE EXPORT AlazarResetTimeStamp(HANDLE h, U32 resetFlag);

// NOT FOR THE CUSTOMERS USE
RETURN_CODE EXPORT
    AlazarReadRegister(HANDLE hDevice, U32 offset, U32 *retVal, U32 pswrd);
RETURN_CODE EXPORT
    AlazarWriteRegister(HANDLE hDevice, U32 offset, U32 Val, U32 pswrd);

RETURN_CODE EXPORT
    ReadC(HANDLE h, U8 *DmaBuffer, U32 SizeToRead, U32 LocalAddress);

void EXPORT WriteC(HANDLE h, U8 *DmaBuffer, U32 SizeToRead, U32 LocalAddress);

RETURN_CODE EXPORT
    AlazarGetTriggerAddressA(HANDLE h, U32 Record, U32 *TriggerAddress,
                             U32 *TimeStampHighPart, U32 *TimeStampLowPart);
RETURN_CODE EXPORT
    AlazarGetTriggerAddressB(HANDLE h, U32 Record, U32 *TriggerAddress,
                             U32 *TimeStampHighPart, U32 *TimeStampLowPart);

RETURN_CODE EXPORT
    ATS9462FlashSectorPageRead(HANDLE h, U32 address, U16 *PageBuff);
RETURN_CODE EXPORT
    ATS9462PageWriteToFlash(HANDLE h, U32 address, U16 *PageBuff);
RETURN_CODE EXPORT ATS9462FlashSectorErase(HANDLE h, int sectorNum);
RETURN_CODE EXPORT ATS9462FlashChipErase(HANDLE h);
RETURN_CODE EXPORT SetControlCommand(HANDLE h, int cmd);

RETURN_CODE EXPORT
    AlazarDACSetting(HANDLE h, U32 SetGet, U32 OriginalOrModified, U8 Channel,
                     U32 DACNAME, U32 Coupling, U32 InputRange, U32 Impedance,
                     U32 *getVal, U32 setVal, U32 *error);

RETURN_CODE EXPORT
AlazarConfigureAuxIO(HANDLE hDevice, U32 uMode, U32 uParameter);

const char EXPORT *AlazarErrorToText(RETURN_CODE code);

RETURN_CODE EXPORT AlazarConfigureSampleSkipping(HANDLE hBoard, U32 uMode,
                                                 U32 uSampleClocksPerRecord,
                                                 U16 *pwClockSkipMask);

RETURN_CODE EXPORT
    AlazarCoprocessorRegisterRead(HANDLE hDevice, U32 offset, U32 *pValue);
RETURN_CODE EXPORT
    AlazarCoprocessorRegisterWrite(HANDLE hDevice, U32 offset, U32 value);
RETURN_CODE EXPORT
    AlazarCoprocessorDownloadA(HANDLE hBoard, char *pszFileName, U32 uOptions);
RETURN_CODE EXPORT AlazarCoprocessorDownloadW(HANDLE hBoard,
                                              wchar_t *pszFileName,
                                              U32 uOptions);

#ifdef WIN32
#ifdef UNICODE
#define AlazarCoprocessorDownload AlazarCoprocessorDownloadW
#else
#define AlazarCoprocessorDownload AlazarCoprocessorDownloadA
#endif
#else
#define AlazarCoprocessorDownload AlazarCoprocessorDownloadA
#endif

// FPGA averaging
RETURN_CODE EXPORT AlazarConfigureRecordAverage(HANDLE hBoard, U32 uMode,
                                                U32 uSamplesPerRecord,
                                                U32 uRecordsPerAverage,
                                                U32 uOptions);

#define CRA_MODE_DISABLE 0
#define CRA_MODE_ENABLE_FPGA_AVE 1

#define CRA_OPTION_UNSIGNED (0U << 1)
#define CRA_OPTION_SIGNED (1U << 1)

EXPORT U8 *AlazarAllocBufferU8(HANDLE hBoard, U32 uSampleCount);
RETURN_CODE EXPORT AlazarFreeBufferU8(HANDLE hBoard, U8 *pBuffer);

EXPORT U16 *AlazarAllocBufferU16(HANDLE hBoard, U32 uSampleCount);
RETURN_CODE EXPORT AlazarFreeBufferU16(HANDLE hBoard, U16 *pBuffer);

RETURN_CODE EXPORT AlazarConfigureLSB(HANDLE hBoard, U32 uValueLsb0, U32 uValueLsb1);

RETURN_CODE EXPORT AlazarDspRegisterRead(HANDLE hDevice, U32 offset, U32 *pValue);
RETURN_CODE EXPORT AlazarDspRegisterWrite(HANDLE hDevice, U32 offset, U32 value);

// NPT Footers

typedef struct _NPTFoooter {
    U64 triggerTimestamp;
    U32 recordNumber;
    U32 frameCount;
    BOOL aux_in_state;
} NPTFooter;

RETURN_CODE EXPORT AlazarExtractNPTFooters(void *buffer,
                                           U32 recordSize_bytes,
                                           U32 bufferSize_bytes,
                                           NPTFooter *footersArray,
                                           U32 numFootersToExtract);

RETURN_CODE EXPORT AlazarEnableFFT(HANDLE boardHandle, BOOL enable);

RETURN_CODE EXPORT AlazarOCTIgnoreBadClock(HANDLE  hBoardHandle,
                                           U32     uEnable,
                                           double  dGoodClockDuration,
                                           double  dBadClockDuration,
                                           double  *pTriggerCycleTime,
                                           double  *pTriggerPulseWidth);

U32 EXPORT AlazarReadEx(HANDLE h,
                        U32 Channel,
                        void *Buffer,
                        int ElementSize,
                        long Record,
                        INT64 TransferOffset,
                        U32 TransferLength);
                        
RETURN_CODE EXPORT AlazarAsyncRead(HANDLE hBoard,
                                   void *pvBuffer,
                                   U32 uBytesToTransfer,
                                   OVERLAPPED *pOverlapped);

#ifdef __cplusplus
}
#endif

#endif //_ALAZARAPI_H
