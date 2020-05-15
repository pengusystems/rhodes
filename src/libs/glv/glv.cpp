#include <windows.h>
#include "glv.h"
const USHORT kVendorID = 0x0D2B;
const USHORT kProductID = 0x0102;
const int kGLVBytesPerTransfer = 4096;
const int kGLVColTrigWidth_ns = 200;
const int kGLVColTrigDelay_ns = 0;


GLVParams::GLVParams() :
	vddah{324},
	trigger_auto{false},
	col_period_ns{6000},
	com_port{"COM3"},
	on_recv{nullptr} {
}


GLV::GLV() :
	m_test_running{false},
	m_glv_hw_configured{false},
	m_mem_allocated{false},
	m_loopcycle_running{false},
	m_glv_responsive{false},
	m_preload_column_count{0} {	
	m_usb_device = std::make_unique<CCyUSBDevice>();
	const auto on_uart_recv = std::bind(&GLV::on_uart_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_uart.set_cb_on_recv(on_uart_recv);
}


GLV::~GLV() {
	if (m_glv_buffer) {
		VirtualFree(m_glv_buffer, 0, MEM_RELEASE);
	}
	if (m_test_running) {
		m_test_running = false;
		if (m_test_thread.joinable()) {
			m_test_thread.join();
		}
	}
}


bool GLV::configure(const GLVParams& glv_params) {
	// Store the current GLV parameters.
	m_glv_params = glv_params;

	// Set the internal uart receive call back.
	m_on_glv_serial_recv = glv_params.on_recv;
	
	// Allocate a GLV buffer for sending data over the USB.
	if (!m_mem_allocated) {
		m_glv_buffer = static_cast<u16*>(VirtualAlloc(nullptr, kGLVBytesPerTransfer, MEM_COMMIT, PAGE_READWRITE));
		if (m_glv_buffer == nullptr) {
			return false;
		}
		m_mem_allocated = true;
	}

#ifdef GLV_PROCESSING_EMULATION
	return true;
#endif

	// Open the USB3 and the UART interfaces.
	bool uart_ok = false;
	bool usb_ok = false;
	if (!m_glv_hw_configured) {
		usb_ok = open_fx3();
		uart_ok = m_uart.start(m_glv_params.com_port.c_str(), 115200);
		Sleep(500);
		m_glv_hw_configured = usb_ok && uart_ok;
		if (!m_glv_hw_configured) {
			return false;
		}
	}
	else {
		uart_ok = true;
		usb_ok = true;
	}

	auto software_config_ok = configure_over_uart();
	return m_glv_hw_configured && software_config_ok;
}


bool GLV::set_column_period(const u32 col_period_ns) {
	return uart_send_to_glv("COLTIME " + std::to_string(col_period_ns));
}


bool GLV::preload(const GLVFrameXs& dac_frame) {
	// Verify column size.
	assert(dac_frame.rows() == kGLVPixels);
	
	// Configure the GLV to accept data over USB.
	m_preload_column_count = dac_frame.cols();
	uart_send_to_glv("USB 0 0 " + std::to_string(m_preload_column_count));
	
	// Iterate through the columns and load to the GLV.
	for (auto col_index = 0; col_index < m_preload_column_count; ++col_index) {
		if (!usb_load_to_glv(dac_frame.col(col_index))) {
			return false;
		}
	}

	// Need to wait a bit, otherwise if we trigger a cycle command too early we get unexpected behaviour.
	Sleep(3000);

	return true;
}


bool GLV::cycle(const u16 column_start, const u16 column_end, const bool repeat) {
	if (repeat) {
		return uart_send_to_glv("LOOPLUT " + std::to_string(column_start) + " " + std::to_string(column_end) + " 0");
	}
	else {
		bool uart_send_ok = uart_send_to_glv("GOLUT " + std::to_string(column_start) + " " + std::to_string(column_end));
		return uart_send_ok & uart_send_to_glv("SOFTTRIGGER F1");
	}
}


bool GLV::run_loop_cycle() {
#ifdef GLV_NO_LOOPCYCLE
	// Configure GLV to cycle through all the preloaded PLUTs (loaded from index #0 to index #preload_column_count_-1)
	uart_send_to_glv("GOLUT 0 " + std::to_string(preload_column_count_ - 1));
	return true;
#else
	// Specialized GLV command to:
	// 1.Run a set of preloaded columns between index #0 and index #preload_column_count_-1
	// 2.Wait for a column to be sent over the USB (without a UART command)
	// 3.Once arrived, store in #preload_column_count_ (the third parameter) and display
	// 4.Repeat (go back to step 1), optionally wait here.
	// Note: the last two numbers are wait time (us) in step 4 and whether or not to trigger for step 3 (correspondingly)
	m_loopcycle_running = true;
	uart_send_to_glv("LOOPCYCLE 0 " + std::to_string(m_preload_column_count - 1) + " " + std::to_string(m_preload_column_count) + " " + std::to_string(m_glv_params.loopcycle_wait_us) + " 0");
	return true;
#endif
}


bool GLV::stop_loop_cycle() {
	// Stop the loop cycle command.
	if (m_loopcycle_running) {
		return uart_send_to_glv("LOOPSTOP");
		m_loopcycle_running = false;
	}
	return true;
}


bool GLV::stop() {
	// Stop any constant display.
	return uart_send_to_glv("/");
}


bool GLV::reset() {
	m_usb_device->Close();
	uart_send_to_glv("RESET");
	Sleep(35000);
	m_uart.stop();
	m_glv_hw_configured = false;
	return true;
}


bool GLV::run_test(const GLV_TESTS& test_index) {
#ifdef GLV_PROCESSING_EMULATION
	return false;
#endif
	if (!m_glv_hw_configured) {
		return false;
	}

	switch(test_index) {
		case SLM_TEST1: {
			auto uart_send_ok = uart_send_to_glv("TEST1 " + std::to_string(800));
			return uart_send_ok;
		}

		case SLM_TEST4: {
			auto uart_send_ok = uart_send_to_glv("TEST4 0 " + std::to_string(kGLVMaxAmp));
			return uart_send_ok;
		}

		case SLM_TEST9: {
			auto uart_send_ok = uart_send_to_glv("TEST9");
			return uart_send_ok;
		}

		case USR_TEST0: {
			auto column_zero = GLVColVectorXs::Zero(kGLVPixels, 1);
			auto uart_send_ok = uart_send_to_glv("USB 0 0 1");
			auto usb_load_ok = usb_load_to_glv(column_zero);
			uart_send_ok = uart_send_to_glv("GOLUT 0 0");
			return usb_load_ok && uart_send_ok;
		}

		case USR_TEST1: {
			auto column_one = GLVColVectorXs::Ones(kGLVPixels, 1) * kGLVMaxAmp;
			auto uart_send_ok = uart_send_to_glv("USB 0 0 1");
			auto usb_load_ok = usb_load_to_glv(column_one);
			uart_send_ok = uart_send_to_glv("GOLUT 0 0");
			return usb_load_ok && uart_send_ok;
		}

		case USR_TEST2: {
			m_test_running = true;
			m_test_thread = std::thread{ [&] {
				while (m_test_running) {
					uart_send_to_glv("TEST1 " + std::to_string(0));
					uart_send_to_glv("TEST1 " + std::to_string(730));
				}
			} };
			return true;
		}

		case USR_TEST3: {
			m_test_thread = std::thread{ [&] {
				GLVColVectorXs column_zero = GLVColVectorXs::Zero(kGLVPixels, 1);
				GLVColVectorXs column_one = GLVColVectorXs::Ones(kGLVPixels, 1) * 730;
				uart_send_to_glv("USB 0 0 4");
				usb_load_to_glv(column_zero);
				usb_load_to_glv(column_zero);
				usb_load_to_glv(column_one);
				usb_load_to_glv(column_one);
				Sleep(1000);
				m_test_running = true;
				while (m_test_running) {
					// 1st half of the cycle.
					uart_send_to_glv("GOLUT 0 0");	
					
					// 2nd half of the cycle.
					uart_send_to_glv("GOLUT 2 2");			
				}
			} };
			return true;
		}

		case USR_TEST4: {
			bool usb_load_ok = false;
			auto uart_send_ok = uart_send_to_glv("USB 0 0 1024");
			for (auto amp = 0; amp <= kGLVMaxAmp; ++amp) {
				auto column = GLVColVectorXs::Ones(kGLVPixels, 1) * amp;
				usb_load_ok = usb_load_to_glv(column);
			}
			Sleep(1000);
			uart_send_ok = uart_send_to_glv("LOOPLUT 0 1023 0");
			return usb_load_ok && uart_send_ok;
		}

		case USR_TEST5: {
			bool usb_load_ok = false;
			auto uart_send_ok = uart_send_to_glv("USB 0 0 1024");
			for (auto amp = 0; amp <= kGLVMaxAmp; ++amp) {
				auto column = GLVColVectorXs::Ones(kGLVPixels, 1) * amp;
				usb_load_ok = usb_load_to_glv(column);
			}
			Sleep(2000);
			uart_send_ok = uart_send_to_glv("GOLUT 0 1023");
			uart_send_ok = uart_send_to_glv("SOFTTRIGGER F1");
			return usb_load_ok && uart_send_ok;
		}

		default: {
			return false;
		}
	}
}


bool GLV::get_status() {
	return
		uart_send_to_glv("STATUS") &&
		uart_send_to_glv("PSOC") &&
		uart_send_to_glv("COLSTATUS") &&
		uart_send_to_glv("FRAMESTATUS") &&
		uart_send_to_glv("READADC") &&
		uart_send_to_glv("STATUS");
}


bool GLV::boot() {
	// If sleep time is too short, GLV behaves weird on subsequent commands.
	return uart_send_to_glv("BOOTUP", 8000);
}


bool GLV::load_and_resume_cycle(const GLVColVectorXs& dac_column) {
	// Verify column size.
	assert(dac_column.rows() == kGLVPixels);

	// If GLV does not have loop cycle command, manually configure it to accept data over USB.
#ifdef GLV_NO_LOOPCYCLE
	// Configure GLV for receiving only one LUT over USB indexed at #preload_column_count_.
	uart_send_to_glv("USB 0 "+ std::to_string(preload_column_count_) + " 1");
#endif

  // Send the new column o the USB.
	auto transfer_success = usb_load_to_glv(dac_column);

	// If GLV does not have loop cycle command, manually configure it to display the given dac_column and then 
	// cycle again through the preloaded columns.
#ifdef GLV_NO_LOOPCYCLE
	// Configure GLV to display only one PLUT starting with the index #preload_column_count_ and ending with the same index.
	uart_send_to_glv("GOLUT " + std::to_string(preload_column_count_) + " " + std::to_string(preload_column_count_));

	// Configure GLV to cycle through all the preloaded PLUTs (loaded from index #0 to index #preload_column_count_-1).
	uart_send_to_glv("GOLUT 0 " + std::to_string(preload_column_count_ - 1));
#endif

	return transfer_success;
}


bool GLV::usb_load_to_glv(const GLVColVectorXs& dac_column) {
	// Process the eigen vector to a raw buffer.
	convert_to_raw_buffer(dac_column);

	// Send over USB.
#ifdef GLV_PROCESSING_EMULATION
	// Debugging with array plotter, place break point on dummy = m_glv_buffer[0];
	u16 plot_buffer_u16[kGLVPixels];
	GLVColVectorXs::Map(plot_buffer_u16, dac_column.rows()) = dac_column;
	volatile auto dummy = m_glv_buffer[0];
	return true;
#endif
	auto ep_bulk_out = m_usb_device->BulkOutEndPt;
	auto length = static_cast<LONG>(kGLVBytesPerTransfer);
	auto success = ep_bulk_out->XferData(reinterpret_cast<PUCHAR>(m_glv_buffer), length);
	if (!success) {
		return false;
	}

	return true;
}


bool GLV::uart_send_to_glv(const std::string& command, const u32 post_sleep_time) {
#ifdef GLV_PROCESSING_EMULATION
	return true;
#endif
	std::unique_lock<std::mutex>(uart_mtx);
	auto success = m_uart.send(command + '\r');
	if ((success == 0xffffffffffffffff) || (success == 0)) {
		return false;
	}
	else {
		Sleep(post_sleep_time);
		return true;
	}
}


bool GLV::configure_over_uart() {
	// Configure Vddah.
	// If GLV is not responding, it is possible that a loopcycle is running. In that case, the GLV will not respond
	// until it receives the RESET or LOOPSTOP commands. 
	uart_send_to_glv("VDDAH " + std::to_string(m_glv_params.vddah));
	if (!m_glv_responsive) {
		uart_send_to_glv("LOOPSTOP");
	}

	// Set the column trigger source to gated source.
	// Framecontrol has to be on for the gated operation to work.
	if (m_glv_params.trigger_auto) {
		uart_send_to_glv("FRAMECONTROL OFF");
		uart_send_to_glv("TRIGCSOURCE POS NOG");
	}
	else {
		uart_send_to_glv("FRAMETIME 10000000");
		uart_send_to_glv("FRAMECONTROL ON");
		uart_send_to_glv("FRAMESOURCE SWS");
		uart_send_to_glv("TRIGCSOURCE POS GAT");
	}

	// Trigger width and delay.
	uart_send_to_glv("TRIGCPWTIME " + std::to_string(kGLVColTrigWidth_ns));
	uart_send_to_glv("COLTRIGDELAY " + std::to_string(kGLVColTrigDelay_ns));

	// Set the column period.
	uart_send_to_glv("COLTIME " + std::to_string(m_glv_params.col_period_ns));

	return true;
}


void GLV::convert_to_raw_buffer(const GLVColVectorXs& dac_column) {
	// Interleave the pixels into the following scheme:
	// 0, 1, 2, 3, 4, 5, 6, 7, 8, ... , 1086, 1087
	//                         ||
	//                         \/
	// 0, 1, 544, 545, 2, 3, 546, 547, ... , 542, 543, 1086, 1087
	// Also swap the bytes on each u16 dac value.
	const int kGLVPixelsHalf = kGLVPixels >> 1;
	const int kGLVPixelsHalfPlusOne = kGLVPixelsHalf + 1;
	const int kGLVPixelsHalfMinusOne = kGLVPixelsHalf - 1;
	for (auto sample_index = 0; sample_index < kGLVPixelsHalfMinusOne; sample_index += 2) {
		auto il_index = sample_index << 1;
		m_glv_buffer[il_index    ] = byte_swap(dac_column[sample_index]);
		m_glv_buffer[il_index + 1] = byte_swap(dac_column[sample_index + 1]);
		m_glv_buffer[il_index + 2] = byte_swap(dac_column[kGLVPixelsHalf + sample_index]);
		m_glv_buffer[il_index + 3] = byte_swap(dac_column[kGLVPixelsHalfPlusOne + sample_index]);
	}
}


u16 GLV::byte_swap(const u16 dac_value) const {
	return ((dac_value & 0x00FF) << 8) | ((dac_value & 0xFF00) >> 8);
}


bool GLV::open_fx3() {
	// Find the correct usb device.
	const UCHAR n = m_usb_device->DeviceCount(); 
	for (UCHAR i = 0; i < n; ++i) {
		m_usb_device->Open(i);
		if ((m_usb_device->VendorID == kVendorID) && (m_usb_device->ProductID == kProductID)) {
			m_usb_device->Reset();
			Sleep(500);
			return true;
		}
		else {
			m_usb_device->Close();
		}
	}
	return false;
}


void GLV::on_uart_receive(char* const data_ptr, const size_t data_len) {
	m_glv_responsive = true;
	m_on_glv_serial_recv(data_ptr, data_len);
}