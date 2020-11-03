#include <iostream>
#include <windows.h>
#include "serialport.h"


int main(int argc, char *argv[]) {
	serialport port;
	port.start("COM4", 115200);
	while(1) {
		Sleep(0);
		port.send("Eyal");
		std::cout << "test" << std::endl;
	}
	return 0;
}
