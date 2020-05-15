#include <iostream>
#include <iomanip>
#include <conio.h>
#include "AlazarTech/ATS-SDK/7.1.5/Include/AlazarApi.h"
#include "alazar_daq.h"


DAQParams::DAQParams() :
	records_per_buffer{48},
	buffer_count{16},
	samples_per_record{256},
	buffers_per_acquisition{1},
	acquisition_mode{ACQUISTION_NORMAL},
	acquisition_timeout_ms{10000},
	voltage_range{INPUT_RANGE_PM_2_V},
	channel_mask{CHANNEL_A},
	trigger_delay_sec{0},
	on_recv{nullptr} {
}


DAQ::DAQ() :
	m_daq_running{false},
	m_daq_configured{false},
	m_mem_allocated{false} {
}


DAQ::~DAQ() {
	if (m_daq_running) {
		stop();
	}
	deallocate_memory();
}


RETURN_CODE DAQ::configure(const DAQParams& daq_params) {
	if (m_daq_running) {
		return ApiFailed;
	}
	m_daq_configured = false;
	
	// Get board handle.
	m_board_handle = AlazarGetBoardBySystemID(m_system_id, m_board_id);
	if (!m_board_handle) {
		return ApiFailed;
	}

	//////////////////////////////////
	// Hardware level configuration //
	//////////////////////////////////
	// Specify the sample rate (see sample rate id below).
	const double samples_per_sec = 500000000.0;

	// Select clock parameters as required to generate this sample rate.
	// For example: if samples_per_sec is 100.e6 (100 MS/s), then:
	// - select clock source INTERNAL_CLOCK and sample rate SAMPLE_RATE_100MSPS
	// - select clock source FAST_EXTERNAL_CLOCK, sample rate SAMPLE_RATE_USER_DEF, and connect a 100 MHz signal to the EXT CLK BNC connector.
	auto return_code = AlazarSetCaptureClock(
		m_board_handle,
		INTERNAL_CLOCK,
		SAMPLE_RATE_500MSPS,
		CLOCK_EDGE_RISING,
		0);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select channel A input parameters as required.
	return_code = AlazarInputControlEx(
		m_board_handle,
		CHANNEL_A,
		DC_COUPLING,
		daq_params.voltage_range,
		IMPEDANCE_50_OHM);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select channel A bandwidth limit as required.
	return_code = AlazarSetBWLimit(
		m_board_handle,
		CHANNEL_A,
		0);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select channel B input parameters as required.
	return_code = AlazarInputControlEx(
		m_board_handle,
		CHANNEL_B,
		DC_COUPLING,
		INPUT_RANGE_PM_4_V,
		IMPEDANCE_50_OHM);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select channel B bandwidth limit as required.
	return_code = AlazarSetBWLimit(
		m_board_handle,
		CHANNEL_B,
		0);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select trigger inputs and levels as required.
	auto trigger_level_code = 128 + 127 * 2 / 10;
	return_code = AlazarSetTriggerOperation(
		m_board_handle,
		TRIG_ENGINE_OP_J,
		TRIG_ENGINE_J,
		TRIG_EXTERNAL,
		TRIGGER_SLOPE_POSITIVE,
		trigger_level_code,
		TRIG_ENGINE_K,
		TRIG_DISABLE,
		TRIGGER_SLOPE_POSITIVE,
		128);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Select external trigger parameters as required.
	return_code = AlazarSetExternalTrigger(
		m_board_handle,
		DC_COUPLING,
		ETR_5V);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Set trigger delay as required.
	const u32 trigger_delay_required_alignment = 8;
	u32 trigger_delay_samples = static_cast<u32>(daq_params.trigger_delay_sec * samples_per_sec + 0.5);
	auto trigger_delay_alignment = trigger_delay_samples % trigger_delay_required_alignment;
	if (trigger_delay_alignment != 0) {
		trigger_delay_samples -= trigger_delay_alignment;
	}
	return_code = AlazarSetTriggerDelay(m_board_handle, trigger_delay_samples);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Set trigger timeout as required.
	// NOTE:
	// The board will wait for this amount of time for a trigger event.  If a trigger event
	// does not arrive, then the board will automatically trigger. Set the trigger timeout value to 0 to force the board
	// to wait forever for a trigger event.	
	// IMPORTANT:
	// The trigger timeout value should be set to zero after appropriate trigger parameters have
	// been determined, otherwise the board may trigger if the timeout interval expires before a hardware trigger
	// event arrives.
	f64 trigger_timeout_sec = 0;
	u32 trigger_timeout_clocks = static_cast<u32>(trigger_timeout_sec / 10.e-6 + 0.5);
	return_code = AlazarSetTriggerTimeOut(m_board_handle, trigger_timeout_clocks);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Configure AUX I/O connector as required.
	return_code = AlazarConfigureAuxIO(m_board_handle, AUX_OUT_TRIGGER, 0);
	if (return_code != ApiSuccess) {
		return return_code;
	}


	/////////////////////////////////
	// Data transfer configuration //
	/////////////////////////////////
	// Set the buffer receive callback.
	set_cb_on_buffer_recv(daq_params.on_recv);

	// Calculate the number of enabled channels from the channel mask.
	auto channel_count = 0;
	auto channels_per_board = 2;
	for (auto channel_index = 0; channel_index < channels_per_board; ++channel_index) {
		u32 channel_id = 1U << channel_index;
		if (daq_params.channel_mask & channel_id) {
			channel_count++;
		}
	}

	// Get the sample size in bits, and the on-board memory size in samples per channel/
	u8 bits_per_sample;
	U32 max_samples_per_channel;
	return_code = AlazarGetChannelInfo(m_board_handle, &max_samples_per_channel, &bits_per_sample);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Calculate the size of each DMA buffer in bytes.
	auto bytes_per_sample = static_cast<f64>((bits_per_sample + 7) / 8);
	auto bytes_per_record = static_cast<u32>(bytes_per_sample * daq_params.samples_per_record + 0.5); // 0.5 compensates for f64 to integer conversion 
	m_bytes_per_buffer = bytes_per_record * daq_params.records_per_buffer * channel_count;

	// Allocate memory for DMA buffers.
	if (m_mem_allocated) {
		deallocate_memory();
		m_mem_allocated = false;
	}
	if (!m_mem_allocated) {
		m_buffer_array = new u16*[daq_params.buffer_count];
		if (!m_buffer_array) {
			return ApiFailed;	
		}
		for (auto buffer_index = 0; buffer_index < daq_params.buffer_count; buffer_index++) {
			m_buffer_array[buffer_index] = static_cast<u16*>(VirtualAlloc(nullptr, m_bytes_per_buffer, MEM_COMMIT, PAGE_READWRITE));
			if (m_buffer_array[buffer_index] == nullptr) {
				return ApiFailed;
			}
		}
		m_mem_allocated = true;
	}

	// Configure the record size.
	// There are no pre-trigger samples in NPT mode.
	return_code = AlazarSetRecordSize(m_board_handle, 0, daq_params.samples_per_record);
	if (return_code != ApiSuccess) {
		return return_code;
	}


	// If we got here, everything was configured correctly.
	// Store the current DAQ parameters.
	m_daq_params = daq_params;
	m_daq_configured = true;
	return ApiSuccess;
}


RETURN_CODE DAQ::capture() {
	if (!m_daq_configured) {
		return ApiFailed;
	}
	
	// Configure the acquisition mode.
	// For single acquisition, set the records_per_acquisition to buffers_per_acquisition x records_per_buffer.
	// For normal acquisition, set the records_per_acquisition to infinite.
	u32 records_per_acquisition;
	if (m_daq_params.acquisition_mode == DAQParams::ACQUISTION_SINGLE) {
		records_per_acquisition = m_daq_params.buffers_per_acquisition * m_daq_params.records_per_buffer;
	}
	else {
		records_per_acquisition = 0x7FFFFFFF;
	}
	
	// Configure the board to make an infinite NPT AutoDMA acquisition.
	auto adma_flags = ADMA_EXTERNAL_STARTCAPTURE | ADMA_NPT | ADMA_FIFO_ONLY_STREAMING;
	auto return_code = AlazarBeforeAsyncRead(
		m_board_handle,
		m_daq_params.channel_mask,
		-static_cast<long>(0),
		m_daq_params.samples_per_record,
		m_daq_params.records_per_buffer,
		records_per_acquisition,
		adma_flags);
	if (return_code != ApiSuccess) {
		return return_code;
	}
	
	// Add the buffers to a list of buffers available to be filled by the board.    
	for (auto buffer_index = 0; buffer_index < m_daq_params.buffer_count; ++buffer_index) {
		u16 *p_buffer = m_buffer_array[buffer_index];
		return_code = AlazarPostAsyncBuffer(m_board_handle, p_buffer, m_bytes_per_buffer);
		if (return_code != ApiSuccess) {
			return return_code;
		}
	}


	// Arm the board system to wait for a trigger event to begin the acquisition.
	return_code = AlazarStartCapture(m_board_handle);
	if (return_code != ApiSuccess) {
		return return_code;
	}

	// Capture loop.
	u64 buffers_completed = 0;
	m_daq_running = true;
	while (m_daq_running) {
		// Wait for the buffer at the head of the list of available buffers to be filled by the board.
		auto buffer_index = buffers_completed % m_daq_params.buffer_count;
		u16 *p_buffer = m_buffer_array[buffer_index];
		return_code = AlazarWaitAsyncBufferComplete(m_board_handle, p_buffer, m_daq_params.acquisition_timeout_ms);
		if (return_code != ApiSuccess) {
			if (return_code == ApiWaitTimeout) {
				m_daq_running = false;
				AlazarAbortAsyncRead(m_board_handle);
				AlazarAbortCapture(m_board_handle);
				return return_code;
			}
			break;
		}
			
		// The buffer is full and has been removed from the list of buffers available for the board
		buffers_completed++;
						
		// Process sample data in this buffer.
		// While you are processing this buffer, the board is already filling the next
		// available buffer(s).
		// You MUST finish processing this buffer and post it back to the board before
		// the board fills all of its available DMA buffers and on-board memory.
		// Samples are arranged in the buffer as follows: S0A, S0B, ..., S1A, S1B, ...
		// with SXY the sample number X of channel Y.
		// A 12-bit sample code is stored in the most significant bits of in each 16-bit
		// sample value. 
		// Sample codes are unsigned by default. As a result:
		// - a sample code of 0x0000 represents a negative full scale input signal.
		// - a sample code of 0x8000 represents a ~0V signal.
		// - a sample code of 0xFFFF represents a positive full scale input signal.  
		on_buffer_receive(p_buffer, m_bytes_per_buffer, buffers_completed);

		// Add the buffer to the end of the list of available buffers.
		return_code = AlazarPostAsyncBuffer(m_board_handle, p_buffer, m_bytes_per_buffer);
		if (return_code != ApiSuccess) {
			break;
		}

		// For single mode acquisitio, once we received the sufficient number of buffers, we can about the acquisition.
		if ((m_daq_params.acquisition_mode == DAQParams::ACQUISTION_SINGLE) && (buffers_completed == m_daq_params.buffers_per_acquisition)) {
			break;
		}
	}

	// Abort the acquisition.
	m_daq_running = false;
	AlazarAbortAsyncRead(m_board_handle);
	AlazarAbortCapture(m_board_handle);
	return return_code;
}


void DAQ::stop() {
	m_daq_running = false;
}


void DAQ::set_cb_on_buffer_recv(cb_on_buffer_recv on_recv) {
	m_daq_params.on_recv = on_recv;
}


const char* DAQ::error_to_text(const RETURN_CODE& return_code) const {
	return AlazarErrorToText(return_code);
}


void DAQ::deallocate_memory() {
	if (m_mem_allocated) {
		for (auto buffer_index = 0; buffer_index < m_daq_params.buffer_count; ++buffer_index) {
			if (m_buffer_array[buffer_index]) {
				VirtualFree(m_buffer_array[buffer_index], 0, MEM_RELEASE);
			}
		}
		delete[] m_buffer_array;
	}
}


void DAQ::on_buffer_receive(u16* const buffer, const size_t& length, const u64 buffers_completed) const {
	m_daq_params.on_recv(buffer, length, buffers_completed);
}


void DAQ::on_buffer_timeout() const {
	U32 num_triggers;
	const u32 password = 0x32145876;
	auto return_code = AlazarReadRegister(m_board_handle, 12, &num_triggers, password);
	if (return_code != ApiSuccess) {
		std::cout << "DAQ: Failed to get number of triggers" << std::endl;
	}
	else {
		std::cout << "DAQ: Triggered " << num_triggers << " times" << std::endl;
	}
	if (m_daq_params.on_timeout) {
		m_daq_params.on_timeout();
	}
}
