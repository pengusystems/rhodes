add_library(serialport SHARED
	serialport.cpp
	serialport.h
)
target_include_directories(serialport
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/..
	${CMAKE_CURRENT_SOURCE_DIR}/../../../ext/
	${CMAKE_CURRENT_SOURCE_DIR}/../../../ext/asio/include
)
target_compile_definitions(serialport PRIVATE
	DLL_EXPORT
)
find_package(Threads)
target_link_libraries(serialport
	PRIVATE ${CMAKE_THREAD_LIBS_INIT}
)
set_target_properties(serialport PROPERTIES
	CXX_STANDARD 14
)
