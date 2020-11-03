#pragma once
#include <functional>
#include <string>
#include <memory>
#include "core0/types.h"
#include "core0/api_export.h"

// Call back on receive type.
using cb_on_serial_recv = std::function<void(u8* const data_ptr, const size_t data_len)>;

// A self explanatory class to manage a serial port.
class serialport {
public:
	API_EXPORT serialport();
	API_EXPORT ~serialport();

	// Disable copy constructors.
	serialport(const serialport&p) = delete;
	serialport&operator=(const serialport&p) = delete;

	// Opens the serial port and starts listening on it.
	// Under Linux com_port_name would usually look like: "/dev/ttyS0"
	// where 0 can be replaced with different serial port nodes.
	// If permission is denied: sudo chmod o+rw /dev/ttyS0
	API_EXPORT bool start(const char *com_port_name, const int baud_rate=9600);

	// Stops listening on the serial port and closes it.
	API_EXPORT void stop();

	// Sets a call back handler to be called when data is received.
	API_EXPORT void set_cb_on_recv(cb_on_serial_recv on_recv);

	// Sends data over the serial port.
	API_EXPORT size_t send(const std::string& buf);
	API_EXPORT size_t send(const char* buf, const size_t& size);
	API_EXPORT size_t send(const u8* buf, const size_t& size);

private:
	struct impl;
	std::unique_ptr<impl> m_pimpl;
};
