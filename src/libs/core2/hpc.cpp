#include "hpc.h"


bool HPC::start() {
	LARGE_INTEGER li_cnt_per_sec;
	if (!QueryPerformanceFrequency(&li_cnt_per_sec)) {
		return false;
	}
	m_cnt_per_sec = static_cast<f64>(li_cnt_per_sec.QuadPart);
	return QueryPerformanceCounter(&m_li_start_tick_count);
}


HPC& HPC::stop() {
	QueryPerformanceCounter(&m_li_end_tick_count);
	return *this;
}


f64 HPC::get_time_in_usec() const {
	return 1e6 * (m_li_end_tick_count.QuadPart - m_li_start_tick_count.QuadPart) / m_cnt_per_sec;
}
