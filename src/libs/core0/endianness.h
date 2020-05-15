#pragma once
#ifndef _ENDIANNESS_H_
#define _ENDIANNESS_H_

#include <cstring>
#include "types.h"

#ifdef _MSC_VER
#include <stdlib.h>
#define bswap_32(x) _byteswap_ulong(x)
#define bswap_64(x) _byteswap_uint64(x)
#else
#include <byteswap.h>
#endif

namespace core0 {
	inline u32 bswap(u32 x) { return bswap_32(x); };
	inline u64 bswap(u64 x) { return bswap_64(x); };

	template <typename T>
	void swap_endianness(T* data, size_t len) {
		for (size_t ii = 0; ii < len; ii++) {
			auto start_byte_addr = data + ii;
			T value = bswap(*reinterpret_cast<T*>(start_byte_addr));
			memcpy(start_byte_addr, &value, sizeof(T));
		}
	};
}

#endif
