#ifdef _WIN32
	#ifndef _WIN32_WINNT
		#define _WIN32_WINNT 0xA00
	#endif
#endif
#ifndef ASIO_STANDALONE
	#define ASIO_STANDALONE
#endif

#include <mutex>
#include <thread>
#include <memory>
#include "asio/include/asio.hpp"
#include "asio/include/asio/serial_port.hpp"
#include "SerialPort.h"


struct SerialPort::impl {
	impl();
	~impl();
	void async_read_some();
	void on_receive(const std::error_code ec, size_t bytes_transferred);
	const int m_read_buf_size = 256;
	asio::io_service m_io_service;
	std::thread m_io_service_thread;
	using serial_port_ptr = std::shared_ptr<asio::serial_port>;
	serial_port_ptr m_port;
	std::mutex m_mutex;
	char *m_read_buf_raw;
	cb_on_serial_recv m_on_recv;
};


SerialPort::impl::impl() {
	m_read_buf_raw = new char[m_read_buf_size];
	m_on_recv = nullptr;
}


SerialPort::impl::~impl() {
	delete[] m_read_buf_raw;
}


SerialPort::SerialPort() {
	m_pimpl = std::make_unique<impl>();
}


SerialPort::~SerialPort() {
	stop();
	if (m_pimpl->m_io_service_thread.joinable()) {
		m_pimpl->m_io_service_thread.join();
	}
}


bool SerialPort::start(const char* com_port_name, const int baud_rate) {
	std::error_code ec;

	if (m_pimpl->m_port) {
		return false;
	}

	m_pimpl->m_port = impl::serial_port_ptr(new asio::serial_port(m_pimpl->m_io_service));
	m_pimpl->m_port->open(com_port_name, ec);
	if (ec) {
		return false;
	}

	// Option settings...
	m_pimpl->m_port->set_option(asio::serial_port_base::baud_rate(baud_rate));
	m_pimpl->m_port->set_option(asio::serial_port_base::character_size(8));
	m_pimpl->m_port->set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
	m_pimpl->m_port->set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
	m_pimpl->m_port->set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

	// First time is required to bind the handler.
	m_pimpl->async_read_some();

	// IO service must be started after the read.
	m_pimpl->m_io_service_thread = std::thread([&]{m_pimpl->m_io_service.run(); });

	return true;
}


void SerialPort::stop() {
	std::lock_guard<std::mutex> lock(m_pimpl->m_mutex);

	if (m_pimpl->m_port) {
		if (m_pimpl->m_port->is_open()) {
			m_pimpl->m_port->cancel();
			m_pimpl->m_port->close();
			m_pimpl->m_port.reset();
		}
	}
	m_pimpl->m_io_service.stop();
	m_pimpl->m_io_service.reset();
}


void SerialPort::set_cb_on_recv(cb_on_serial_recv on_recv) {
	m_pimpl->m_on_recv = on_recv;
}


size_t SerialPort::send(const std::string &buf) {
	return send(buf.c_str(), buf.size());
}


size_t SerialPort::send(const char* buf, const size_t& size) {
	std::error_code ec;

	if (!m_pimpl->m_port) {
		return -1;
	}
	if (size == 0) {
		return 0;
	}

	return m_pimpl->m_port->write_some(asio::buffer(buf, size), ec);
}


void SerialPort::impl::async_read_some() {
	if (m_port.get() == NULL || !m_port->is_open()) {
		return;
	}

	m_port->async_read_some(
		asio::buffer(m_read_buf_raw, m_read_buf_size),
		std::bind(
			&SerialPort::impl::on_receive,
			this,
			std::placeholders::_1,
			std::placeholders::_2));
}


void SerialPort::impl::on_receive(const std::error_code ec, size_t bytes_transferred) {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_port.get() == NULL || !m_port->is_open()) {
		return;
	}

	if (m_on_recv) {
		m_on_recv(m_read_buf_raw, bytes_transferred);
	}

	async_read_some();
}