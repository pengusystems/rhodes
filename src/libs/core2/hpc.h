#pragma once
#include <windows.h>
#include "../Core0/types.h"

class HPC {
public:
	bool start();
	HPC& stop();
	f64 get_time_in_usec() const;
private:
	LARGE_INTEGER m_li_start_tick_count, m_li_end_tick_count;
	f64 m_cnt_per_sec;
};
