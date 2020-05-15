#include <iostream>
#include <fstream>
#include <thread>
#include "core0/types.h"
#include "alazar_daq.h"

#define kSamplesPerRecord 256  // Each record, corresponds to a trigger, contains #kSamplesPerTrigger samples.
#define kRecordsPerBuffer 100 // A DAQ buffer contains kRecordsPerBuffer * kSamplesPerRecord #samples.
#define kBuffersPerAcquisition 5 // Each acquisition will contain #kBuffersPerAcquisition * kRecordsPerBuffer records.
#define kAcquisitionMode DAQParams::ACQUISTION_NORMAL // Single or Normal acquisition.


// Global variables.
const u32 samples_per_daq_buffer = kRecordsPerBuffer * kSamplesPerRecord;  // #Samples in a DAQ buffer
const u32 rx_buffer_samples = samples_per_daq_buffer * kBuffersPerAcquisition; // Number of samples in the application RX buffer.
u16* rx_buffer;
DAQParams daq_params;


// Receive buffer call back.
// data_len should be kSamplesInDAQBuffer * 2 since each sample is two bytes.
void on_daq_buffer_receive(u16* const data_ptr, const size_t data_len, const u64 data_index) {
	static size_t daq_buffer_count = 0;
	memcpy(rx_buffer + daq_buffer_count * samples_per_daq_buffer, data_ptr, data_len);
	daq_buffer_count++;
	std::cout << "DAQ: Captured " << daq_buffer_count << " buffers = (" << kRecordsPerBuffer << ") records" << std::endl;
	if (daq_buffer_count == kBuffersPerAcquisition) {
		std::cout << "DAQ: RX buffer full, ";
		if (daq_params.acquisition_mode == DAQParams::ACQUISTION_SINGLE) {
			std::cout << "acquisition ended succssfully. Rerun the application to re-arm the DAQ" << std::endl;
		}
		else {
			std::cout << "acquisition ended succssfully. Normal acquisition mode automatically re-armed the DAQ" << std::endl;
			daq_buffer_count = 0;
		}
	}
}


// Timeout call back.
void on_daq_timeout() {
	std::cout << "DAQ: Acquisition timed out" << std::endl;
}


int main(int argc, char *argv[]) {
	rx_buffer = new u16[rx_buffer_samples];
	DAQ daq;
	daq_params.samples_per_record = kSamplesPerRecord;
	daq_params.records_per_buffer = kRecordsPerBuffer;
	daq_params.buffers_per_acquisition = kBuffersPerAcquisition;
	daq_params.acquisition_mode = kAcquisitionMode;
	daq_params.on_recv = on_daq_buffer_receive;
	daq_params.on_timeout = on_daq_timeout;
	auto daq_cfg_return_code = daq.configure(daq_params);
	if (daq_cfg_return_code == ApiFailed) {
		std::cout << daq.error_to_text(daq_cfg_return_code) << std::endl;
		delete[] rx_buffer;
		return 0;
	}
	std::cout << "DAQ: Waiting for data, timeout is "<< daq_params.acquisition_timeout_ms << "ms..." << std::endl;
	auto test_thread = std::thread{ [&] {daq.capture(); } };
	test_thread.join();
	std::cout << "Exiting...";
	Sleep(2000);
	delete[] rx_buffer;
	return 0;
}