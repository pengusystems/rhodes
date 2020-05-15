#pragma once
#include <windows.h>
#include <functional>
#include "AlazarTech/ATS-SDK/7.1.5/Include/AlazarError.h"
#include "AlazarTech/ATS-SDK/7.1.5/Include/AlazarCmd.h"
#include "core0/types.h"
#include "core0/api_export.h"


// DAQ callbacks on buffer receive type.
using cb_on_buffer_recv = std::function<void(u16* const data_ptr, const size_t data_len, const u64 data_index)>;  // data_len is the number of bytes in the buffer.
using cb_on_buffer_timout = std::function<void()>;


// DAQ configuration parameters.
struct DAQParams {
	API_EXPORT DAQParams();
	
	enum ACQUISTION_MODE {
		ACQUISTION_SINGLE = 0,  // Once the number of buffers per acquisition has been received, the acquisition ends.
		                        // It is guaranteed that all the buffers are handed back to the application by the last trigger.
		ACQUISTION_NORMAL  // The acquisition is infinite.
		                   // Once a buffer fills up, it is handed back to the application.
	};

	u32 records_per_buffer;  // An acquisition is made of x buffers, each has y records (a trigger corresponds to a record).
	int buffer_count;  // This is used internally for the DMA, don't need to change.
	u32 samples_per_record;  // Each record, will capture this number of samples.
	u32 buffers_per_acquisition;
	ACQUISTION_MODE acquisition_mode;  // See description above.
	u32 acquisition_timeout_ms;
	u32 channel_mask;
	u32 voltage_range;
	f64 trigger_delay_sec;
	cb_on_buffer_recv on_recv;
	cb_on_buffer_timout on_timeout;
};


class DAQ {
public:
	API_EXPORT DAQ();
	API_EXPORT ~DAQ();

	// Configures the hardware for the experiment.
	API_EXPORT RETURN_CODE configure(const DAQParams& daq_params);
  
	// Starts the capture.
	API_EXPORT RETURN_CODE capture();

	// Stops the capture.
	API_EXPORT void stop();

	// Sets the buffer receive callback.
	API_EXPORT void set_cb_on_buffer_recv(cb_on_buffer_recv on_recv);

	// Uses Alazar API to convert a return code to text.
	API_EXPORT const char* error_to_text(const RETURN_CODE& return_code) const;

private:
	const u32 m_system_id = 1;
	const u32 m_board_id = 1;
	HANDLE m_board_handle;
	DAQParams m_daq_params;
	u16** m_buffer_array;
	u32 m_bytes_per_buffer;
	bool m_daq_configured;
	bool m_mem_allocated;
	bool m_daq_running;

	void deallocate_memory();
	void on_buffer_receive(u16* const buffer, const size_t& length, const u64 buffers_completed) const;
	void on_buffer_timeout() const;
};