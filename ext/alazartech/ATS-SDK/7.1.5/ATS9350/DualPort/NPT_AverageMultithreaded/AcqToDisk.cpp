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
// This program demonstrates how to configure a ATS9350 to make a multithreaded average
// NPT acquisition.
//

#include <stdio.h>
#include <string.h>

#include "AlazarError.h"
#include "AlazarApi.h"
#include "AlazarCmd.h"

#ifdef _WIN32
#include <conio.h>
#include <process.h>
#else // ifndef _WIN32
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define TRUE  1
#define FALSE 0

#define _snprintf snprintf

inline U32 GetTickCount(void);
inline void Sleep(U32 dwTime_ms);
inline int _kbhit (void);
inline int GetLastError();
#endif // ifndef _WIN32

// TODO: Select the number of DMA buffers to allocate.

#define BUFFER_COUNT 4
// TODO: Select the maximum number of threads to use for signal processing

#define MAX_THREAD_COUNT 16

// Structure passed to each worker thread

typedef struct _THREAD_PARAMS
{
    BOOL threadShouldExit;
#ifdef _WIN32
    UINT threadId;
    HANDLE threadHandle;
    HANDLE threadEvent;
    HANDLE threadDoneEvent;
#else
    pthread_t thread;
    pthread_mutex_t event_mutex;
    BOOL event;
    pthread_cond_t event_cv;
    pthread_mutex_t done_mutex;
    BOOL done;
    pthread_cond_t done_cv;
#endif
    U16 *pRecord;    // Address of first record in DMA buffer for this thread sum
    U32 recordsToProcess; // Number of records in DMA buffer for this thread to sum
    U32 *pAccRecord; // Address of accumulator buffer to store sum of records
    U32 samplesPerRecord;
} THREAD_PARAMS;

// Globals variables
U16 *BufferArray[BUFFER_COUNT] = { NULL };
double samplesPerSec = 0.0;
THREAD_PARAMS *ThreadParams = NULL;
U32 ThreadCount = 0; 


// Forward declarations

BOOL ConfigureBoard(HANDLE boardHandle);
BOOL AcquireData(HANDLE boardHandle);
BOOL StartWorkerThreads();
void StopWorkerThreads();
#ifdef _WIN32
unsigned __stdcall ThreadFunc(void *pArguments);
#else
void* ThreadFunc(void* pArguments);
#endif

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

    // Configure the board's sample rate, input, and trigger settings

    if (!ConfigureBoard(boardHandle))
    {
        printf("Error: Configure board failed\n");
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
    //   100 MHz signal to the EXT CLK BNC connector.

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
// Description :  Perform an acquisition, optionally saving data to file.
//
//-------------------------------------------------------------------------------------------------

BOOL AcquireData(HANDLE boardHandle)
{
    // There are no pre-trigger samples in NPT mode
    U32 preTriggerSamples = 0;

    // TODO: Select the number of post-trigger samples per record
    U32 postTriggerSamples = 2048;

    // TODO: Specify the number of records per DMA buffer
    U32 recordsPerBuffer = 10;
    

    // TODO: Specify the total number of buffers to capture
    U32 buffersPerAcquisition = 10;
    
    // Channel to capture
    U32 channelMask = CHANNEL_A;

    // TODO: Select if you wish to save the sample data to a file
    BOOL saveData = false;
    // TODO: Select if you wish to save the averaged points to a file

    BOOL saveAverage = true;

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
    RETURN_CODE retCode = AlazarGetChannelInfo(boardHandle, &maxSamplesPerChannel, &bitsPerSample);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarGetChannelInfo failed -- %s\n", AlazarErrorToText(retCode));
        return FALSE;
    }

    // Calculate the size of each DMA buffer in bytes
    float bytesPerSample = (float) ((bitsPerSample + 7) / 8);
    U32 samplesPerRecord = preTriggerSamples + postTriggerSamples;
    U32 bytesPerRecord = (U32)(bytesPerSample * samplesPerRecord +
                               0.5); // 0.5 compensates for double to integer conversion 
    U32 bytesPerBuffer = bytesPerRecord * recordsPerBuffer * channelCount;
    
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
    
    // Create a processed data file if required
    FILE *fpAverage = NULL;

    if (saveAverage)
    {
        fpAverage = fopen("data_ave.bin", "wb");
        if (fpAverage == NULL)
        {
            printf("Error: Unable to create data file -- %u\n", GetLastError());
            return FALSE;
        }
    }

    // Allocate memory for DMA buffers
    BOOL success = TRUE;

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
    // Allocate a buffer to store a co-average record
    U16 *pAveRecord = (U16*) malloc(samplesPerRecord * channelCount * sizeof(U16));
    if (pAveRecord == NULL)
    {
        printf("Error: Alloc %u bytes failed\n", samplesPerRecord * channelCount * sizeof(U16));
        success = FALSE;
    }
	else
	{
		// Start worker threads to co-average record data
		success = StartWorkerThreads();
	}

    // Allocate a buffer for to co-accumulate samples from records in an AutoDMA buffer.
    U32 *pAccRecord = NULL;

    if (success)
    {
        U32 bytesPerAccBuffer = samplesPerRecord * channelCount * sizeof(U32) * ThreadCount;
        pAccRecord = (U32*) malloc(bytesPerAccBuffer);
        if (pAccRecord == NULL)
        {
            printf("Error: Alloc %u bytes failed\n", bytesPerAccBuffer);
            success = FALSE;
        }
    } 

    
    // Configure the record size
    if (success)
    {
        retCode = AlazarSetRecordSize(boardHandle, preTriggerSamples, postTriggerSamples);
        if (retCode != ApiSuccess)
        {
            printf("Error: AlazarSetRecordSize failed -- %s\n", AlazarErrorToText(retCode));
            success = FALSE;
        }
    }

    // Configure the board to make an NPT AutoDMA acquisition
    if (success)
    {
        U32 recordsPerAcquisition = recordsPerBuffer * buffersPerAcquisition;

        U32 admaFlags = ADMA_EXTERNAL_STARTCAPTURE | ADMA_NPT;

        retCode = AlazarBeforeAsyncRead(boardHandle, channelMask, -(long)preTriggerSamples,
                                        samplesPerRecord, recordsPerBuffer, recordsPerAcquisition,
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
             
            retCode = AlazarWaitAsyncBufferComplete(boardHandle, pBuffer, timeout_ms);
            if (retCode != ApiSuccess)
            {
                printf("Error: AlazarWaitAsyncBufferComplete failed -- %s\n",
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
                // Samples are arranged in the buffer as follows: S0A, S0B, ..., S1A, S1B, ...
                // with SXY the sample number X of channel Y.
                //
                // A 12-bit sample code is stored in the most significant bits of in each 16-bit
                // sample value. 
                // Sample codes are unsigned by default. As a result:
                // - a sample code of 0x0000 represents a negative full scale input signal.
                // - a sample code of 0x8000 represents a ~0V signal.
                // - a sample code of 0xFFFF represents a positive full scale input signal.  
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
                // Imagine the buffer as a 2D matrix where each row corresponds
                // to a record, and each column to a sample in a record.
                //  Row 1 = Record 1 [A1 B1 A2 B2 ... Am Bm ]
                //  Row 2 = Record 2 [A1 B1 A2 B2 ... Am Bm ]
                //  ...
                //  Row n = Record n [A1 B1 A2 B2 ... Am Bm ]
                // Find the sum of each column, and divide the sum by
                // the number of records in the buffer to find the average record.

                U32 recordInBuffer = 0;
                U32 recordsRemaining = recordsPerBuffer;
                U32 recordsPerThread = recordsPerBuffer / ThreadCount;

                for (U32 threadIndex = 0; threadIndex < ThreadCount; threadIndex++)
                {
                    // Let each worker thread find the vector sum of
                    // some of the records in the buffer.
                    THREAD_PARAMS *pThreadParams = &ThreadParams[threadIndex];
                    pThreadParams->pRecord =
                        pBuffer + samplesPerRecord * recordInBuffer;
                    pThreadParams->pAccRecord = pAccRecord + threadIndex * samplesPerRecord;
                    pThreadParams->samplesPerRecord = samplesPerRecord;

                    if (threadIndex == (ThreadCount - 1))
                        pThreadParams->recordsToProcess = recordsRemaining;
                    else
                        pThreadParams->recordsToProcess = recordsPerThread;

                    // Signal this thread to start summing a number of records in the buffer
#ifdef _WIN32
                    if (!SetEvent(pThreadParams->threadEvent))
                    {
                        printf("Error: SetEvent failed -- %u", GetLastError());
                        success = FALSE;
                        break;
                    }
#else
                    pthread_mutex_lock(&pThreadParams->event_mutex);
                    pThreadParams->event = TRUE;
                    pthread_cond_signal(&pThreadParams->event_cv);
                    pthread_mutex_unlock(&pThreadParams->event_mutex);
#endif

                    recordInBuffer += recordsPerThread;
                    recordsRemaining -= recordsPerThread;
                }

                if (success)
                {
                    // Wait for worker threads to finish summing their rows
                    for (U32 threadIndex = 0; threadIndex < ThreadCount; threadIndex++)
                    {
#ifdef _WIN32
                        U32 result = WaitForSingleObject(
                            ThreadParams[threadIndex].threadDoneEvent, INFINITE);
                        if (result != WAIT_OBJECT_0)
                        {
                            printf("Error: WaitForSingleObject failed -- %u\n", GetLastError());
                            success = FALSE;
                        }
#else
                        pthread_mutex_lock(&ThreadParams[threadIndex].done_mutex);
                        while (ThreadParams[threadIndex].done == FALSE)
                            pthread_cond_wait(&ThreadParams[threadIndex].done_cv,
                                              &ThreadParams[threadIndex].done_mutex);
                        ThreadParams[threadIndex].done = FALSE;
                        pthread_mutex_unlock(&ThreadParams[threadIndex].done_mutex);
#endif
                    }
                }

                if (success)
                {
                    // Find average sample value and write to output buffer
                    for (U32 sample = 0; sample < samplesPerRecord; sample++)
                    {
                        // Add the accumulator vectors from each thread together to find
                        // the sum of all of the rows in each column of the buffer.
                        U32 sum = 0;
                        for (U32 threadIndex = 0; threadIndex < ThreadCount; threadIndex++)
                        {
                            THREAD_PARAMS *pThreadParams = &ThreadParams[threadIndex];
                            sum += *(pThreadParams->pAccRecord + sample);
                        }

                        // Calcualte the average record from the sum vector
                        *(pAveRecord + sample) = (U16)(sum / recordsPerBuffer);
                    }

                    // Write co-average record to file
                    if (saveAverage)
                    {
                        size_t bytesWritten =
                            fwrite(pAveRecord, sizeof(BYTE), bytesPerRecord, fpAverage);
                        if (bytesWritten != bytesPerRecord)
                        {
                            printf("Error: Write co-average record %u failed -- %u\n",
                                   buffersCompleted,
                                   GetLastError());
                            success = FALSE;
                        }
                    }
                } // Acquisition mode == NPT || TR
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
    retCode = AlazarAbortAsyncRead(boardHandle);
    if (retCode != ApiSuccess)
    {
        printf("Error: AlazarAbortAsyncRead failed -- %s\n", AlazarErrorToText(retCode));
        success = FALSE;
    }
    // Stop worker threads
    StopWorkerThreads();

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
    if (pAveRecord != NULL)
        free(pAveRecord);

    if (pAccRecord != NULL)
        free(pAccRecord);

    // Close the data file
    
    if (fpData != NULL)
        fclose(fpData);
    
    if (fpAverage != NULL)
        fclose(fpAverage);

    return success;
}
//-------------------------------------------------------------------------------------------------
//
// Function    :  StartWorkerThreads
//
// Description :  Start one worker threads per processor core for data processing
//
//-------------------------------------------------------------------------------------------------

BOOL StartWorkerThreads()
{
    StopWorkerThreads();

    // Find the number of processor cores available
#ifdef _WIN32
    SYSTEM_INFO systemInfo;
    memset(&systemInfo, 0, sizeof(systemInfo));
    GetSystemInfo(&systemInfo);
    U32 processorCount = systemInfo.dwNumberOfProcessors;
#else
    U32 processorCount = sysconf(_SC_NPROCESSORS_ONLN);
#endif
    // Start one worker thread per processor core, up to the specified limit
    U32 threadCount;
    if (processorCount < 1)
        threadCount = 1;
    else if (processorCount > MAX_THREAD_COUNT)
        threadCount = MAX_THREAD_COUNT;
    else
        threadCount = processorCount;

    // Allocate a THREAD_PARAM structure for each thread
    size_t allocBytes = threadCount * sizeof(THREAD_PARAMS);
    ThreadParams = (THREAD_PARAMS *)malloc(allocBytes);
    if (ThreadParams == NULL)
    {
        printf("Error: Alloc %u bytes failed", allocBytes);
        return FALSE;
    }

    memset(ThreadParams, 0, allocBytes);
    ThreadCount = threadCount;

    // Create worker threads
    for (U32 threadIndex = 0; threadIndex < threadCount; threadIndex++)
    {
        THREAD_PARAMS *pThreadParams = &ThreadParams[threadIndex];

        #ifdef _WIN32
        // The main thread signals a worker thread with this event
        pThreadParams->threadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (pThreadParams->threadEvent == NULL)
        {
            printf("Error: Create thread event failed -- %d", GetLastError());
            return FALSE;
        }

        // The worker thread signals the main thread with this event
        pThreadParams->threadDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (pThreadParams->threadDoneEvent == NULL)
        {
            printf("Error: Create thread event failed -- %d", GetLastError());
            return FALSE;
        }
        #else
        pthread_mutex_init(&pThreadParams->event_mutex, NULL);
        pthread_cond_init(&pThreadParams->event_cv, NULL);
        pthread_mutex_init(&pThreadParams->done_mutex, NULL);
        pthread_cond_init(&pThreadParams->done_cv, NULL);
        #endif

        #ifdef _WIN32
        // Create the worker thread
        pThreadParams->threadShouldExit = FALSE;
        pThreadParams->threadHandle =
            (HANDLE)_beginthreadex(NULL, 0, ThreadFunc, pThreadParams, 0, &pThreadParams->threadId);
        if (pThreadParams->threadHandle == NULL)
        {
            printf("Error: Create thread failed -- %d", GetLastError());
            return FALSE;
        }
        #else
        int rc = pthread_create(&pThreadParams->thread, NULL, ThreadFunc, (void *)pThreadParams);
        if (rc)
        {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
        #endif
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------------------
//
// Function    :  StopWorkerThreads
//
// Description :  Stop the worker threads started by StartWorkerThreads
//
//-------------------------------------------------------------------------------------------------

void StopWorkerThreads(void)
{
    if (ThreadParams != NULL)
    {
        for (U32 threadIndex = 0; threadIndex < ThreadCount; threadIndex++)
        {
            THREAD_PARAMS *pThreadParams = &ThreadParams[threadIndex];

            // Exit the thread
#ifdef _WIN32
            if (pThreadParams->threadHandle != NULL)
            {
                pThreadParams->threadShouldExit = TRUE;
                if (!SetEvent(pThreadParams->threadEvent))
                {
                    printf("Error: SetEvent failed -- %d", GetLastError());
                }

                WaitForSingleObject(pThreadParams->threadHandle, INFINITE);

                CloseHandle(pThreadParams->threadHandle);
                pThreadParams->threadHandle = NULL;
                pThreadParams->threadId = 0;
            }

            // Close notify event handle
            if (pThreadParams->threadEvent != NULL)
            {
                CloseHandle(pThreadParams->threadEvent);
                pThreadParams->threadEvent = NULL;
            }

            // Close processing complete event handle
            if (pThreadParams->threadDoneEvent != NULL)
            {
                CloseHandle(pThreadParams->threadDoneEvent);
                pThreadParams->threadDoneEvent = NULL;
            }
#else
            pthread_cancel(pThreadParams->thread);
            pthread_mutex_destroy(&pThreadParams->event_mutex);
            pthread_cond_destroy(&pThreadParams->event_cv);
            pthread_mutex_destroy(&pThreadParams->done_mutex);
            pthread_cond_destroy(&pThreadParams->done_cv);
            pthread_join(pThreadParams->thread, NULL);
#endif
        }

        // Free THREAD_PARAMS structures

        free(ThreadParams);
        ThreadParams = NULL;
    }

    ThreadCount = 0;
}

//-------------------------------------------------------------------------------------------------
//
// Function    :  ThreadFunc
//
// Description :  Process a portion of a DMA buffer in the context of
//                a worker thread
//
//-------------------------------------------------------------------------------------------------

#ifdef _WIN32
unsigned __stdcall ThreadFunc(void* pArguments)
#else
void* ThreadFunc(void* pArguments)
#endif
{
    THREAD_PARAMS *pThreadParams = (THREAD_PARAMS *)pArguments;

    for (;;)
    {
        // Wait for signal from main thread
#ifdef _WIN32
        WaitForSingleObject(pThreadParams->threadEvent, INFINITE);
#else
        pthread_mutex_lock(&pThreadParams->event_mutex);
        while (pThreadParams->event == FALSE)
            pthread_cond_wait(&pThreadParams->event_cv,
                              &pThreadParams->event_mutex);
        pThreadParams->event = FALSE;
        pthread_mutex_unlock(&pThreadParams->event_mutex);
#endif
        if (pThreadParams->threadShouldExit)
            break;
        // Imagine the buffer as a 2D matrix where each row corresponds
		// to a record, and each column to a sample in a record.

		//  Row 1 = Record 1 [A1 B1 A2 B2 ... Am Bm ]
		//  Row 2 = Record 2 [A1 B1 A2 B2 ... Am Bm ]
		//  ...
		//  Row n = Record n [A1 B1 A2 B2 ... Am Bm ]

		// Find the vector sum of each column in a number of
		// rows from the buffer
		U32 recordsToProcess = pThreadParams->recordsToProcess;
		U32 samplesPerRecord = pThreadParams->samplesPerRecord;
        U16 *pRecord = pThreadParams->pRecord;

		// Clear the accumulator record
		U32 *pAccumulatorRecord = pThreadParams->pAccRecord;
		memset(pAccumulatorRecord, 0, sizeof(U32) * samplesPerRecord);

		// Find the vector sum of all the columns in the specified rows
        U16* pSample = pRecord;
		for (U32 record = 0; record < recordsToProcess; record++)
		{
			U32 *pAccumulatorSample = pAccumulatorRecord;

			for (U32 sample = 0; sample < samplesPerRecord; sample++)
			{
				*pAccumulatorSample += *pSample;
				pAccumulatorSample++;
				pSample++;
			}
		}

        // Signal the main thread that processing is complete
#ifdef _WIN32
        SetEvent(pThreadParams->threadDoneEvent);
#else
        pthread_mutex_lock(&pThreadParams->done_mutex);
        pThreadParams->done = TRUE;
        pthread_cond_signal(&pThreadParams->done_cv);
        pthread_mutex_unlock(&pThreadParams->done_mutex);
#endif
    }

#ifdef _WIN32
    return 0;
#else
    pthread_exit(NULL);
#endif
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