//-------------------------------------------------------------------------------------------------
//
// Copyright (c) 2008-2016 AlazarTech, Inc.
//
// AlazarTech, Inc. licenses this software under specific terms and conditions. Use of any of the
// software or derivatives thereof in any product without an AlazarTech digitizer board is strictly
// prohibited.
//
// AlazarTech, Inc. provides this software AS IS, WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED,
// INCLUDING, WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
// PURPOSE. AlazarTech makes no guarantee or representations regarding the use of, or the results of
// the use of, the software and documentation in terms of correctness, accuracy, reliability,
// currentness, or otherwise; and you rely on the software, documentation and results solely at your
// own risk.
//
// IN NO EVENT SHALL ALAZARTECH BE LIABLE FOR ANY LOSS OF USE, LOSS OF BUSINESS, LOSS OF PROFITS,
// INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES OF ANY KIND. IN NO EVENT SHALL
// ALAZARTECH'S TOTAL LIABILITY EXCEED THE SUM PAID TO ALAZARTECH FOR THE PRODUCT LICENSED
// HEREUNDER.
//
//-------------------------------------------------------------------------------------------------

// AcqToDisk.cpp :
//
// This program demonstrates how to configure a ATS9350 to make a on-FPGA FFT
// NPT_on-FPGA_FFT acquisition.
//

#include <vector>
#include <stdio.h>
#include <string.h>

#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"
#include "AlazarDSP.h"

#ifdef _WIN32
#include <conio.h>
#else // ifndef _WIN32
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TRUE  1
#define FALSE 0

#define _snprintf snprintf

inline U32 GetTickCount(void);
inline void Sleep(U32 dwTime_ms);
inline int _kbhit (void);
inline int GetLastError();
#endif // ifndef _WIN32

#define ERR_CHECK(x)                                                                               \
    do                                                                                             \
    {                                                                                              \
        RETURN_CODE retCode = (x);                                                                 \
        if (retCode != ApiSuccess)                                                                 \
        {                                                                                          \
            printf("Error : %s -- %s\n", #x, AlazarErrorToText(retCode));                          \
            return FALSE;                                                                          \
        }                                                                                          \
    } while (0)

// TODO: Select the number of DMA buffers to allocate.

#define BUFFER_COUNT 4

// Globals variables

U16 *BufferArray[BUFFER_COUNT] = { NULL };
double samplesPerSec = 0.0;

// Forward declarations

BOOL ConfigureBoard(HANDLE boardHandle, dsp_module_handle fftHandle, U32 recordLength_samples);
BOOL AcquireData(HANDLE boardHandle, dsp_module_handle fftHandle, U32 recordLength_samples);

//-------------------------------------------------------------------------------------------------
//
// Function    :  main
//
// Description :  Program entry point
//
//-------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // TODO: Select a board

    U32 systemId = 1;
    U32 boardId = 1;

    // Get a handle to the board

    HANDLE boardHandle = AlazarGetBoardBySystemID(systemId, boardId);
    if (boardHandle == NULL)
    {
        printf("Error: Unable to open board system Id %u board Id %u\n", systemId, boardId);
        return 1;
    }

    // Get a handle to the FFT module

    U32 numModules;

    ERR_CHECK(AlazarDSPGetModules(boardHandle, 0, NULL, &numModules));

    if (numModules < 1) {
        printf("This board does any DSP modules.\n");
        return FALSE;
    }

    std::vector<dsp_module_handle> dspHandles(numModules);

    ERR_CHECK(AlazarDSPGetModules(boardHandle, numModules, &dspHandles[0], NULL));

    dsp_module_handle fftHandle = dspHandles[0];

    // TODO: Select the record length

    U32 recordLength_samples = 2048;


    // Configure the board's sample rate, input, and trigger settings

    if (!ConfigureBoard(boardHandle, fftHandle, recordLength_samples))
    {
        printf("Error: Configure board failed\n");
        return 1;
    }

    // Make an acquisition, optionally saving sample data to a file

    if (!AcquireData(boardHandle, fftHandle, recordLength_samples))
    {
        printf("Error: Acquisition failed\n");
        return 1;
    }

    return 0;
}

//-------------------------------------------------------------------------------------------------
//
// Function    :  ConfigureBoard
//
// Description :  Configure sample rate, input, and trigger settings
//
//-------------------------------------------------------------------------------------------------

BOOL ConfigureBoard(HANDLE boardHandle, dsp_module_handle fftHandle, U32 recordLength_samples)
{
    // TODO: Specify the sample rate (see sample rate id below)

    samplesPerSec = 500000000.0;
    // TODO: Select the window function type

    U32 windowType = DSP_WINDOW_HANNING;

    // TODO: Select the background subtraction record

    std::vector<S16> backgroundSubtractionRecord(recordLength_samples, 0);

    // TODO: Select clock parameters as required to generate this sample rate.
    //
    // For example: if samplesPerSec is 100.e6 (100 MS/s), then:
    // - select clock source INTERNAL_CLOCK and sample rate SAMPLE_RATE_100MSPS
    // - select clock source FAST_EXTERNAL_CLOCK, sample rate SAMPLE_RATE_USER_DEF, and connect a
    //   100 MHz signal to the EXT CLK BNC connector.

    ERR_CHECK(AlazarSetCaptureClock(boardHandle,
                                    INTERNAL_CLOCK,
                                    SAMPLE_RATE_500MSPS,
                                    CLOCK_EDGE_RISING,
                                    0));

    
    // TODO: Select channel A input parameters as required

    ERR_CHECK(AlazarInputControlEx(boardHandle,
                                   CHANNEL_A,
                                   DC_COUPLING,
                                   INPUT_RANGE_PM_400_MV,
                                   IMPEDANCE_50_OHM));

    
    // TODO: Select channel A bandwidth limit as required

    ERR_CHECK(AlazarSetBWLimit(boardHandle,
                               CHANNEL_A,
                               0));

    
    // TODO: Select channel B input parameters as required

    ERR_CHECK(AlazarInputControlEx(boardHandle,
                                   CHANNEL_B,
                                   DC_COUPLING,
                                   INPUT_RANGE_PM_400_MV,
                                   IMPEDANCE_50_OHM));

    
    // TODO: Select channel B bandwidth limit as required

    ERR_CHECK(AlazarSetBWLimit(boardHandle,
                               CHANNEL_B,
                               0));

    

    // TODO: Select trigger inputs and levels as required

    ERR_CHECK(AlazarSetTriggerOperation(boardHandle,
                                        TRIG_ENGINE_OP_J,
                                        TRIG_ENGINE_J,
                                        TRIG_CHAN_A,
                                        TRIGGER_SLOPE_POSITIVE,
                                        150,
                                        TRIG_ENGINE_K,
                                        TRIG_DISABLE,
                                        TRIGGER_SLOPE_POSITIVE,
                                        128));

    // TODO: Select external trigger parameters as required

    ERR_CHECK(AlazarSetExternalTrigger(boardHandle,
                                       DC_COUPLING,
                                       ETR_5V));

    // TODO: Set trigger delay as required.

    double triggerDelay_sec = 0;
    U32 triggerDelay_samples = (U32)(triggerDelay_sec * samplesPerSec + 0.5);
    ERR_CHECK(AlazarSetTriggerDelay(boardHandle, triggerDelay_samples));

    // TODO: Set trigger timeout as required.

    // NOTE:
    // The board will wait for a for this amount of time for a trigger event.  If a trigger event
    // does not arrive, then
    // the board will automatically trigger. Set the trigger timeout value to 0 to force the board
    // to wait forever for a
    // trigger event.
    //
    // IMPORTANT:
    // The trigger timeout value should be set to zero after appropriate trigger parameters have
    // been determined,
    // otherwise the board may trigger if the timeout interval expires before a hardware trigger
    // event arrives.

    double triggerTimeout_sec = 0;
    U32 triggerTimeout_clocks = (U32)(triggerTimeout_sec / 10.e-6 + 0.5);

    ERR_CHECK(AlazarSetTriggerTimeOut(boardHandle, triggerTimeout_clocks));

    // TODO: Configure AUX I/O connector as required

    ERR_CHECK(AlazarConfigureAuxIO(boardHandle, AUX_OUT_TRIGGER, 0));

    // FFT Configuration

    U32 dspModuleId;
    ERR_CHECK(AlazarDSPGetInfo(fftHandle, &dspModuleId, NULL, NULL, NULL, NULL, NULL));

    if (dspModuleId != DSP_MODULE_FFT) {
        printf("Error: DSP module is not FFT\n");
        return FALSE;
    }

    U32 fftLength_samples = 1;
    while (fftLength_samples < recordLength_samples)
        fftLength_samples *= 2;

    // Create and fill the window function

    std::vector<float> window(fftLength_samples);

    ERR_CHECK(AlazarDSPGenerateWindowFunction(windowType,
                                              &window[0],
                                              recordLength_samples,
                                              fftLength_samples - recordLength_samples));

    // Set the window function

    ERR_CHECK(AlazarFFTSetWindowFunction(fftHandle,
                                         fftLength_samples,
                                         &window[0],
                                         NULL));

    // Background subtraction

    ERR_CHECK(AlazarFFTBackgroundSubtractionSetRecordS16(fftHandle, &backgroundSubtractionRecord[0],
                                                         recordLength_samples));

    ERR_CHECK(AlazarFFTBackgroundSubtractionSetEnabled(fftHandle, TRUE));

    return TRUE;
}

//-------------------------------------------------------------------------------------------------
//
// Function    :  AcquireData
//
// Description :  Perform an acquisition, optionally saving data to file.
//
//-------------------------------------------------------------------------------------------------

BOOL AcquireData(HANDLE boardHandle, dsp_module_handle fftHandle, U32 recordLength_samples)
{
    RETURN_CODE retCode = ApiSuccess;

    // TODO: Specify the number of records per DMA buffer

    U32 recordsPerBuffer = 10;

    // TODO: Specify the total number of buffers to capture

    U32 buffersPerAcquisition = 10;

    // TODO: Select which channels to capture (A, B, or both)

    U16 channelMask = CHANNEL_A;

    // TODO: Select if you wish to save the sample data to a file

    BOOL saveData = false;

    // TODO: Select the FFT output format

    U32 outputFormat = FFT_OUTPUT_FORMAT_U16_LOG;

    // TODO: Select the presence of NPT footers

    U32 footer = FFT_FOOTER_NONE;

    // Calculate the number of enabled channels from the channel mask

    int channelCount = 0;
    int channelsPerBoard = 2;
    for (int channel = 0; channel < channelsPerBoard; channel++)
    {
        U32 channelId = 1U << channel;
        if (channelMask & channelId)
            channelCount++;
    }

    // Get the sample size in bits, and the on-board memory size in samples per channel

    U8 bitsPerSample;
    U32 maxSamplesPerChannel;
    ERR_CHECK(AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample));

    // Create a data file if required

    FILE *fpData = NULL;

    if (saveData)
    {
        fpData = fopen("data.bin", "wb");
        if (fpData == NULL)
        {
            printf("Error: Unable to create data file -- %u\n", GetLastError());
            return FALSE;
        }
    }

    // Configure the FFT

    U32 fftLength_samples = 1;
    while (fftLength_samples < recordLength_samples)
        fftLength_samples *= 2;

    U32 bytesPerOutputRecord;

    BOOL success = TRUE;

    if (success)
    {
        retCode = AlazarFFTSetup(fftHandle,
                                 channelMask,
                                 recordLength_samples,
                                 fftLength_samples,
                                 outputFormat,
                                 footer,
                                 0,
                                 &bytesPerOutputRecord);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarSetRecordSize failed -- %s\n", AlazarErrorToText(retCode));
            success = FALSE;
        }
    }

    U32 bytesPerBuffer = bytesPerOutputRecord * recordsPerBuffer;

    // Allocate memory for DMA buffers

    U32 bufferIndex;
    for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && success; bufferIndex++)
    {
#ifdef _WIN32 // Allocate page aligned memory
        BufferArray[bufferIndex] =
            (U16 *)VirtualAlloc(NULL, bytesPerBuffer, MEM_COMMIT, PAGE_READWRITE);
#else
        BufferArray[bufferIndex] = (U16 *)valloc(bytesPerBuffer);
#endif
        if (BufferArray[bufferIndex] == NULL)
        {
            printf("Error: Alloc %u bytes failed\n", bytesPerBuffer);
            success = FALSE;
        }
    }

    // Configure the board to make an NPT AutoDMA acquisition

    if (success)
    {
        U32 recordsPerAcquisition = recordsPerBuffer * buffersPerAcquisition;

        U32 admaFlags = ADMA_EXTERNAL_STARTCAPTURE | ADMA_NPT | ADMA_DSP;

        retCode = AlazarBeforeAsyncRead(boardHandle, channelMask, 0,
                                        bytesPerOutputRecord, recordsPerBuffer, 0x7FFFFFFF,
                                        admaFlags);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarBeforeAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
            success = FALSE;
        }
    }

    // Add the buffers to a list of buffers available to be filled by the board

    for (bufferIndex = 0; (bufferIndex < BUFFER_COUNT) && success; bufferIndex++)
    {
        U16 *pBuffer = BufferArray[bufferIndex];
        retCode = AlazarPostAsyncBuffer(boardHandle, pBuffer, bytesPerBuffer);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarPostAsyncBuffer %u failed -- %s\n", bufferIndex,
                   AlazarErrorToText(retCode));
            success = FALSE;
        }
    }


    // Arm the board system to wait for a trigger event to begin the acquisition

    if (success)
    {
        retCode = AlazarStartCapture(boardHandle);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarStartCapture failed -- %s\n", AlazarErrorToText(retCode));
            success = FALSE;
        }
    }

    // Wait for each buffer to be filled, process the buffer, and re-post it to
    // the board.

    if (success)
    {
        printf("Capturing %d buffers ... press any key to abort\n", buffersPerAcquisition);

        U32 startTickCount = GetTickCount();
        U32 buffersCompleted = 0;
        INT64 bytesTransferred = 0;


            while (buffersCompleted < buffersPerAcquisition)
            {
                // TODO: Set a buffer timeout that is longer than the time
                //       required to capture all the records in one buffer.

                U32 timeout_ms = 5000;

                // Wait for the buffer at the head of the list of available buffers
                // to be filled by the board.

                bufferIndex = buffersCompleted % BUFFER_COUNT;

                U16 *pBuffer = BufferArray[bufferIndex];

                retCode = AlazarDSPGetBuffer(boardHandle, pBuffer, timeout_ms);
                if (retCode != ApiSuccess)
                {
                    printf("Error: AlazarDSPGetBuffer failed -- %s\n",
                           AlazarErrorToText(retCode));
                    success = FALSE;
                }


                if (success)
                {
                    // The buffer is full and has been removed from the list
                    // of buffers available for the board

                    buffersCompleted++;
                    bytesTransferred += bytesPerBuffer;

                    // TODO: Process sample data in this buffer.

                    // NOTE:
                    //
                    // While you are processing this buffer, the board is already filling the next
                    // available buffer(s).
                    //
                    // You MUST finish processing this buffer and post it back to the board before
                    // the board fills all of its available DMA buffers and on-board memory.
                    //


                    if (saveData)
                    {
                        // Write record to file
                        size_t bytesWritten = fwrite(pBuffer, sizeof(BYTE), bytesPerBuffer, fpData);
                        if (bytesWritten != bytesPerBuffer)
                        {
                            printf("Error: Write buffer %u failed -- %u\n", buffersCompleted,
                                   GetLastError());
                            success = FALSE;
                        }
                    }
                }

                // Add the buffer to the end of the list of available buffers.

                if (success)
                {
                    retCode = AlazarPostAsyncBuffer(boardHandle, pBuffer, bytesPerBuffer);
                    if (retCode != ApiSuccess)
                    {
                        printf("Error: AlazarPostAsyncBuffer failed -- %s\n",
                               AlazarErrorToText(retCode));
                        success = FALSE;
                    }
                }

                // If the acquisition failed, exit the acquisition loop

                if (!success)
                    break;

                // If a key was pressed, exit the acquisition loop

                if (_kbhit())
                {
                    printf("Aborted...\n");
                    break;
                }

                // Display progress

                printf("Completed %u buffers\r", buffersCompleted);
            }

        // Display results

        double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;
        printf("Capture completed in %.2lf sec\n", transferTime_sec);

        double buffersPerSec;
        double bytesPerSec;
        double recordsPerSec;
        U32 recordsTransferred = recordsPerBuffer * buffersCompleted;

        if (transferTime_sec > 0.)
        {
            buffersPerSec = buffersCompleted / transferTime_sec;
            bytesPerSec = bytesTransferred / transferTime_sec;
            recordsPerSec = recordsTransferred / transferTime_sec;
        }
        else
        {
            buffersPerSec = 0.;
            bytesPerSec = 0.;
            recordsPerSec = 0.;
        }

        printf("Captured %u buffers (%.4g buffers per sec)\n", buffersCompleted, buffersPerSec);
        printf("Captured %u records (%.4g records per sec)\n", recordsTransferred, recordsPerSec);
        printf("Transferred %I64d bytes (%.4g bytes per sec)\n", bytesTransferred, bytesPerSec);
    }

    // Abort the acquisition

    retCode = AlazarDSPAbortCapture(boardHandle);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarAbortAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
        success = FALSE;
    }

    // Free all memory allocated


    for (bufferIndex = 0; bufferIndex < BUFFER_COUNT; bufferIndex++)
    {
        if (BufferArray[bufferIndex] != NULL)
        {
#ifdef _WIN32
            VirtualFree(BufferArray[bufferIndex], 0, MEM_RELEASE);
#else
            free(BufferArray[bufferIndex]);
#endif
        }
    }

    // Close the data file
    if (fpData != NULL)
        fclose(fpData);

    return success;
}

#ifndef WIN32
inline U32 GetTickCount(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL) != 0)
		return 0;
	return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

inline void Sleep(U32 dwTime_ms)
{
	usleep(dwTime_ms * 1000);
}

inline int _kbhit (void)
{
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET (STDIN_FILENO, &rdfs);

  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tv);
  return FD_ISSET(STDIN_FILENO, &rdfs);
}

inline int GetLastError()
{
	return errno;
}
#endif