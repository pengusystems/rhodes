//-------------------------------------------------------------------------------------------------
//
// Copyright (c) 2008-2015 AlazarTech, Inc.
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
// This program demonstrates how to configure an ATS9350 to acquire to on-board memory,
// and use AlazarRead to transfer records from on-board memory to an application buffer.
//

#include <stdio.h>
#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"

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

// Forward declarations

BOOL ConfigureBoard(HANDLE boardHandle);
BOOL AcquireData(HANDLE boardHandle);
double samplesPerSec = 0.;

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
        printf("Error: Unable to open board system ID %u board ID %u\n", systemId, boardId);
        return 1;
    }

    // Configure the board's sample rate, input, and trigger settings

    if (!ConfigureBoard(boardHandle))
    {
        printf("Error: Board configuration failed\n");
        return 1;
    }

    // Make an acquisition, optionally saving sample data to a file

    if (!AcquireData(boardHandle))
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

BOOL ConfigureBoard(HANDLE boardHandle)
{
    RETURN_CODE retCode;

    // TODO: Specify the sample rate (see sample rate id below)

    samplesPerSec = 500000000.0;

    // TODO: Select clock parameters as required to generate this sample rate.
    //
    // For example: if samplesPerSec is 100.e6 (100 MS/s), then:
    // - select clock source INTERNAL_CLOCK and sample rate SAMPLE_RATE_100MSPS
    // - select clock source FAST_EXTERNAL_CLOCK, sample rate SAMPLE_RATE_USER_DEF, and connect a
    //   100 MHz signalto the EXT CLK BNC connector.

    retCode = AlazarSetCaptureClock(boardHandle,
                                    INTERNAL_CLOCK,
                                    SAMPLE_RATE_500MSPS,
                                    CLOCK_EDGE_RISING,
                                    0);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetCaptureClock failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    } 
    
    // TODO: Select channel A input parameters as required

    retCode = AlazarInputControlEx(boardHandle,
                                  CHANNEL_A,
                                  DC_COUPLING,
                                  INPUT_RANGE_PM_400_MV,
                                  IMPEDANCE_50_OHM);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarInputControlEx failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }
    
    // TODO: Select channel A bandwidth limit as required

    retCode = AlazarSetBWLimit(boardHandle,
                               CHANNEL_A,
                               0);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetBWLimit failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }
    
    // TODO: Select channel B input parameters as required

    retCode = AlazarInputControlEx(boardHandle,
                                  CHANNEL_B,
                                  DC_COUPLING,
                                  INPUT_RANGE_PM_400_MV,
                                  IMPEDANCE_50_OHM);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarInputControlEx failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }
    
    // TODO: Select channel B bandwidth limit as required

    retCode = AlazarSetBWLimit(boardHandle,
                               CHANNEL_B,
                               0);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetBWLimit failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }
     
    // TODO: Select trigger inputs and levels as required

    retCode = AlazarSetTriggerOperation(boardHandle,
                                        TRIG_ENGINE_OP_J,
                                        TRIG_ENGINE_J,
                                        TRIG_CHAN_A,
                                        TRIGGER_SLOPE_POSITIVE,
                                        150,
                                        TRIG_ENGINE_K,
                                        TRIG_DISABLE,
                                        TRIGGER_SLOPE_POSITIVE,
                                        128);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetTriggerOperation failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // TODO: Select external trigger parameters as required

    retCode = AlazarSetExternalTrigger(boardHandle,
                                       DC_COUPLING,
                                       ETR_5V);

    // TODO: Set trigger delay as required.

    double triggerDelay_sec = 0;
    U32 triggerDelay_samples = (U32)(triggerDelay_sec * samplesPerSec + 0.5);
    retCode = AlazarSetTriggerDelay(boardHandle, triggerDelay_samples);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetTriggerDelay failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // TODO: Set trigger timeout as required.

    // NOTE:
    // The board will wait for a for this amount of time for a trigger event.  If a trigger event
    // does not arrive, then the board will automatically trigger. Set the trigger timeout value to
    // 0 to force the board to wait forever for a trigger event.
    //
    // IMPORTANT:
    // The trigger timeout value should be set to zero after appropriate trigger parameters have
    // been determined, otherwise the board may trigger if the timeout interval expires before a
    // hardware trigger event arrives.

    double triggerTimeout_sec = 0;
    U32 triggerTimeout_clocks = (U32)(triggerTimeout_sec / 10.e-6 + 0.5);

    retCode = AlazarSetTriggerTimeOut(boardHandle, triggerTimeout_clocks);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetTriggerTimeOut failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // TODO: Configure AUX I/O connector as required

    retCode = AlazarConfigureAuxIO(boardHandle, AUX_OUT_TRIGGER, 0);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarConfigureAuxIO failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------------------
//
// Function    :  AcquireData
//
// Description :  Make acquisition and optionally save samples to a file
//
//-------------------------------------------------------------------------------------------------

BOOL AcquireData(HANDLE boardHandle)
{
    // TODO: Select the number of pre-trigger samples

    U32 preTriggerSamples = 1024;

    // TODO: Select the number of post-trigger samples per record

    U32 postTriggerSamples = 1024;

    // TODO: Specify the number of records to acquire to on-board memory

    U32 recordsPerCapture = 100;

    // TODO: Select the amount of time, in milliseconds, to wait for the
    //       acquisition to complete to on-board memory.

    U32 timeout_ms = 10 *1000;
    
    // TODO: Select if you wish to save the sample data to a file

    BOOL saveData = false;
    
    // TODO: Select if you wish to save the record timestamps to a file

    BOOL saveTimestamps = true;

    
    // TODO: Select which channels read from on-board memory (A, B, or both)

    U32 channelMask = CHANNEL_A | CHANNEL_B;

    // Calculate the number of enabled channels from the channel mask

    int channelCount = 0;
    const int channelsPerBoard = 2;
    for (int channel = 0; channel < channelsPerBoard; channel++)
    {
        U32 channelId = 1U << channel;
        if ((channelId & channelMask) != 0)
            channelCount++;
    }

    if ((channelCount < 1) || (channelCount > channelsPerBoard))
    {
        printf("Error: Invalid channel mask %08X\n", channelMask);
        return FALSE;
    }

    // Get the sample and memory size

    U8 bitsPerSample;
    U32 maxSamplesPerChannel;
    RETURN_CODE retCode = AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarGetChannelInfo failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }
    
    // Get the number of samples per timestamp count if required

    long samplesPerTimestampCount = 0;

    if (saveTimestamps)
    {
        retCode = AlazarGetParameter(boardHandle, 0, GET_SAMPLES_PER_TIMESTAMP_CLOCK,
                                     &samplesPerTimestampCount);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarGetParameter failed -- %s\n", AlazarErrorToText(retCode));
            return FALSE;
        }
    }
    

    // Calculate the size of each record in bytes

    U32 bytesPerSample = (bitsPerSample + 7) / 8;
    U32 samplesPerRecord = preTriggerSamples + postTriggerSamples;
    U32 bytesPerRecord = bytesPerSample * samplesPerRecord;

    // Calculate the size of a record buffer in bytes
    // Note that the buffer must be at least 16 samples larger than the transfer
    // size

    U32 bytesPerBuffer = bytesPerSample * (samplesPerRecord + 16);

    // Configure the number of samples per record

    retCode = AlazarSetRecordSize(boardHandle, preTriggerSamples, postTriggerSamples);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetRecordSize failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // Configure the number of records in the acquisition

    retCode = AlazarSetRecordCount(boardHandle, recordsPerCapture);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarSetRecordCount failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // Arm the board system to begin the acquisition

    retCode = AlazarStartCapture(boardHandle);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarStartCapture failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // Wait for the board to capture all records to on-board memory

    printf("Capturing %d records ... press any key to abort\n", recordsPerCapture);

    U32 startTickCount = GetTickCount();
    U32 timeoutTickCount = startTickCount + timeout_ms;
    BOOL captureDone = FALSE;

    while (!captureDone)
    {
        if (AlazarBusy(boardHandle) == 0)
        {
            // The capture to on-board memory is done
            captureDone = TRUE;
        }
        else if (GetTickCount() > timeoutTickCount)
        {
            // The acquisition timeout expired before the capture completed
            // The board may not be triggering, or the capture timeout may be
            // too short.
            printf("Error: Capture timeout after %lu ms -- verify trigger.\n", timeout_ms);
            break;
        }
        else if (_kbhit())
        {
            // A key was pressed
            printf("Error: Acquisition aborted\n");
            break;
        }
        else
        {
            // The acquisition is in progress
            Sleep(10);
        }
    }

    if (!captureDone)
    {
        // Abort the acquisition
        retCode = AlazarAbortCapture(boardHandle);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarAbortCapture failed -- %s\n", AlazarErrorToText(retCode));
        }

        return FALSE;
    }

    // The board captured all records to on-board memory

    double captureTime_sec = (GetTickCount() - startTickCount) / 1000.;
    double recordsPerSec;
    if (captureTime_sec > 0.)
        recordsPerSec = recordsPerCapture / captureTime_sec;
    else
        recordsPerSec = 0.;

    printf("Captured %u records in %g sec (%.4g records / sec)\n", recordsPerCapture,
           captureTime_sec, recordsPerSec);
    // Allocate a buffer to store a record

    U16 *buffer = (U16 *)malloc(bytesPerBuffer + 16); // Acquire 16 bytes more than necessary
    if (buffer == NULL)
    {
        printf("Error: alloc %d bytes failed\n", bytesPerBuffer);
        return FALSE;
    }

    // Create a data file if required

    FILE *fpData = NULL;

    if (saveData)
    {
        fpData = fopen("data.bin", "wb");
        if (fpData == NULL)
        {
            printf("Error: Unable to create data file -- %u\n", GetLastError());
            free(buffer);
            return FALSE;
        }
    }
    
    // Create a timestamp file if required

    FILE *fpTimestamp = NULL;

    if (saveTimestamps)
    {
        fpTimestamp = fopen("timestamp.txt", "w");
        if (fpTimestamp == NULL)
        {
            printf("Error: Unable to create timestamp file -- %u\n", GetLastError());
            fclose(fpData);
            return FALSE;
        }
        else
        {
            // Write a header to the timestamp file

            if (fprintf(fpTimestamp, "Record \tTimestamp (sec)\n") < 0)
            {
                printf("Error: Write timestamp header failed -- %u\n", GetLastError());
                fclose(fpData);
                fclose(fpTimestamp);
                return FALSE;
            }
        }
    }
    
    // Transfer the records from on-board memory to our buffer

    printf("Transferring %u records ... press any key to cancel\n", recordsPerCapture);
    startTickCount = GetTickCount();
    INT64 bytesTransferred = 0;
    BOOL success = TRUE;

    U32 record;
    for (record = 0; (record < recordsPerCapture) && (success == TRUE); record++)
    {
        if (saveTimestamps)
        {
            // Get record timestamp

            U32 triggerAddress;
            U32 timestampHigh;
            U32 timestampLow;

            retCode = AlazarGetTriggerAddress(boardHandle, record + 1, &triggerAddress,
                                              &timestampHigh, &timestampLow);
            if (retCode != ApiSuccess)
            {
                printf("Error: AlazarGetTriggerAddress %d failed -- %s\n", record,
                       AlazarErrorToText(retCode));
                success = FALSE;
            }
            else
            {
                // Convert the record timestamp from counts to seconds

                INT64 timeStamp_cnt;
                timeStamp_cnt = ((INT64)timestampHigh) << 8;
                timeStamp_cnt |= timestampLow & 0x0ff;

                double timeStamp_sec =
                    (double)samplesPerTimestampCount * timeStamp_cnt / samplesPerSec;

                // Write the record timestamp to the timestamp file

                if (fprintf(fpTimestamp, "%d \t%.9lf\n", record + 1, timeStamp_sec) < 0)
                {
                    printf("Error: Write record timestamp %d failed -- %u\n", record,
                           GetLastError());
                    success = FALSE;
                }
            }
        }
        for (int channel = 0; (channel < channelsPerBoard) && (success == TRUE); channel++)
        {
            // Find channel Id from channel index

            U32 channelId = 1U << channel;

            // Skip this channel if it's not in channel mask

            if ((channelId & channelMask) == 0)
                continue;

            
            // Transfer one full record from on-board memory to our buffer

            retCode = (RETURN_CODE)AlazarRead(boardHandle, channelId, buffer, (int)bytesPerSample,
                                              (long)record + 1, -((long)preTriggerSamples),
                                              samplesPerRecord);
            if (retCode != ApiSuccess)
            {
                printf("Error: AlazarRead record %u failed -- %s\n", record,
                       AlazarErrorToText(retCode));
                success = FALSE;
            }
            else
            {
                bytesTransferred += bytesPerRecord;

                // TODO: Process record here.
                //
                // A 12-bit sample code is stored in the most significant bits of in each 16-bit
                // sample value.
                //
                // Sample codes are unsigned by default. As a result:
                // - a sample code of 0x0000 represents a negative full scale input signal.
                // - a sample code of 0x8000 represents a ~0V signal.
                // - a sample code of 0xFFFF represents a positive full scale input signal.
                if (saveData)
                {
                    size_t samplesWritten =
                        fwrite(buffer, bytesPerSample, samplesPerRecord, fpData);
                    if (samplesWritten != samplesPerRecord)
                    {
                        printf("Error: Write record %u failed -- %u\n", record, GetLastError());
                        success = FALSE;
                    }
                }
                
            }
            
        }

        // If a key was pressed, then stop processing records.

        if (_kbhit())
        {
            printf("Error: Transfer aborted ...\n");
            success = FALSE;
        }
    }

    // Display results

    double transferTime_sec = (GetTickCount() - startTickCount) / 1000.;

    double bytesPerSec;
    if (transferTime_sec > 0.)
        bytesPerSec = bytesTransferred / transferTime_sec;
    else
        bytesPerSec = 0.;

    printf("Transferred %I64d bytes in %g sec (%.4g bytes per sec)\n", bytesTransferred,
           transferTime_sec, bytesPerSec);

    // Free the DMA buffer

    free(buffer);

    // Close the data file

    if (fpData != NULL)
        fclose(fpData);
    
    // Close the timetamp file
    if (fpTimestamp != NULL)
        fclose(fpTimestamp);
    

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