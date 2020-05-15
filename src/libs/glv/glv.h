#pragma once
#include<windows.h>
#include <memory>
#include <thread>
#include <mutex>
#include "CyAPI/inc/CyAPI.h"
#include "eigen/Eigen/Dense"
#include "core0/types.h"
#include "core0/api_export.h"
#include "serialport/serialport.h"

#define kGLVPixels 1088
#define kGLVMinAmp 0
#define kGLVMaxAmp 1023
#define kGLVDACLevels 1024
//#define GLV_NO_LOOPCYCLE
//#define GLV_PROCESSING_EMULATION


// Type aliases to a GLV column and GLV frame.
using GLVColVectorXs = Eigen::Matrix<u16, -1, 1>;
using GLVFrameXs = Eigen::Matrix<u16, -1, -1>;


// DAQ configuration parameters.
struct GLVParams {
	API_EXPORT GLVParams();
	bool trigger_auto;
	int vddah;
	u32 col_period_ns;
	u32 loopcycle_wait_us;
	std::string com_port;
	cb_on_serial_recv on_recv;
};


class GLV {
public:
	API_EXPORT GLV();
	API_EXPORT ~GLV();

	// GLV tests.
	enum GLV_TESTS {
		SLM_TEST0 = 0,
		SLM_TEST1,
		SLM_TEST2,
		SLM_TEST3,
		SLM_TEST4,
		SLM_TEST9,
		USR_TEST0, // Uses USB, fill with kGLVMinAmp's and display.
		USR_TEST1, // Uses USB, fill with kGLVMaxAmp's and display.
		USR_TEST2, // Constantly switches SLM_TEST1 with two different amplitudes.
		USR_TEST3, // Uses USB, Constantly switches between two custom LUT columns, which are first sent over the USB.
		USR_TEST4, // Uses USB, Constantly cycle all DAC amplitudes from 0 to 1023.
		USR_TEST5  // Uses USB, Cycle all DAC amplitudes from 0 to 1023 once.
	};

	// Configures the GLV hardware for the experiment.
	API_EXPORT bool configure(const GLVParams& glv_params);

	// Explicitly set the column period (ns) on the GLV.
	API_EXPORT bool set_column_period(const u32 col_period_ns);

	// Preload a series of dac columns (frame) to the GLV.
	// Each column should be shaped as a col vector in the input matrix.
	API_EXPORT bool preload(const GLVFrameXs& dac_frame);

	// Cycles through the contents of the PLUT between start and end.
	API_EXPORT bool cycle(const u16 column_start, const u16 column_end, const bool repeat = false);

	// Starts projecting the pre-loaded frame and wait for a variable column, once received, repeat.
	API_EXPORT bool run_loop_cycle();

	// Stops the loop cycle, see comment run_loop_cycle().
	API_EXPORT bool stop_loop_cycle();

	// Stops the GLV from any constant, repeatative display.
	API_EXPORT bool stop();

	// Send a warm reset command to the GLV.
	API_EXPORT bool reset();

	// Runs the specified GLV test.
	API_EXPORT bool run_test(const GLV_TESTS& test_index);

	// Requests the GLV status over the COM.
	API_EXPORT bool get_status();

	// Calls the GLV boot routine.
	// The boot routine runs automatically on power, it is usually not necessary to call this method.
	API_EXPORT bool boot();
	
	// Loads one dymanic column to the GLV.
	API_EXPORT bool load_and_resume_cycle(const GLVColVectorXs& dac_column);

private:
	bool m_glv_hw_configured;
	bool m_test_running;
	bool m_mem_allocated;
	bool m_loopcycle_running;
	bool m_glv_responsive;
	SerialPort m_uart;
	cb_on_serial_recv m_on_glv_serial_recv;
	GLVParams m_glv_params;
	u16* m_glv_buffer;
	size_t m_preload_column_count;
	std::unique_ptr<CCyUSBDevice> m_usb_device;
	std::thread m_test_thread;
	std::mutex m_uart_mtx;

	bool usb_load_to_glv(const GLVColVectorXs& dac_column);
	bool uart_send_to_glv(const std::string& command, const u32 post_sleep_time = 1000);
	bool configure_over_uart();
	void convert_to_raw_buffer(const GLVColVectorXs& dac_column);
	u16 byte_swap(const u16 dac_value) const;
	bool open_fx3();
	void on_uart_receive(char* const data_ptr, const size_t data_len);
};