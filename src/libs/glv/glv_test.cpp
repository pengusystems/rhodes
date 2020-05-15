#include <iostream>
#include "core0/types.h"
#include "glv.h"


// GLV Serial receive call back.
void on_glv_serial_receive(char* const data_ptr, const size_t data_len) {
	int index = 0;
	while (index < data_len) {
		if (data_ptr[index] == '\r') {
			printf("\n");
		}
		else {
			printf("%c", data_ptr[index]);
		}
		index++;  
	}
}


const int kVddah = 324; 
int main(int argc, char *argv[]) {
  GLV glv;
	GLVParams glv_params;
	glv_params.com_port = std::string{"COM3"};
	glv_params.vddah = kVddah;
	glv_params.on_recv = on_glv_serial_receive;
	glv_params.trigger_auto = true;
	if (!glv.configure(glv_params)) {
		std::cout << "TEST: GLV configuration failed" << std::endl;
		Sleep(30000);
		return 0;
	}
	auto test_index = GLV::USR_TEST4;
	if (!glv.run_test(test_index)) {
		std::cout << "TEST: GLV test did not run, possibly in emulation mode " << std::endl;
	}
	Sleep(1000000);
	return 0;
}