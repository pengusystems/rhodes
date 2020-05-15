#include <thread>
#include <conio.h>
#include "spdlog/spdlog.h"
#include "iris.h"


void help() {
			spdlog::info("App: Command legend:");
			spdlog::info("General:");
			spdlog::info("  's' - Stops the app");
			spdlog::info("  'h' - Prints this help menu");
			spdlog::info("  'q' - Exits the application\n");
			spdlog::info("Calibration:");
			spdlog::info("  'c' - Extracts a voltage curve for a set of grating columns");
			spdlog::info("  'e' - Extracts a voltage curve for a set of constant columns");
			spdlog::info("  '+' - Ramps calibrated phase grating on the GLV");
			spdlog::info("  'g' - Configures the GLV to constantly cycle through all DAC amplitudes (GLV is flat)");
			spdlog::info("  'a' - Configures the GLV to constantly cycle through all voltage gratings in a continuous mode");
			spdlog::info("  'b' - Configures the GLV to constantly cycle through all voltage gratings in a burst mode");
			spdlog::info("  'n' - Configures the GLV to constantly cycle through all voltage gratings one by one\n");
			spdlog::info("TM optimization:");
			spdlog::info("  'r' - Runs the optimization using the TM algorithm");
			spdlog::info("  'w' - Toggle between phase steps of PI/2 to PI/4");
			spdlog::info("  '0' - Fixes the reference for the preloaded columns to phase 0 (mode changes)");
			spdlog::info("  'p' - Fixes the reference for the preloaded columns to phase PI (mode changes)");
			spdlog::info("  'z' - Fixes the mode for the preloaded columns (reference changes), final reference phase is constant");
			spdlog::info("  'i' - Fixes the mode for the preloaded columns (reference changes), final reference phase is grating");
			spdlog::info("  'o' - Alternate between a set of adjacent preloaded columns from the TM optimization one by one");
			spdlog::info("  'v' - Alternate between a set of adjacent preloaded columns from the TM optimization in continuous mode");
			spdlog::info("  '1' - Toggle between two fixed preloaded columns from the TM optimization");
			spdlog::info("  'd' - Configures the GLV to constantly cycle through the TM optimization preloaded columns in a burst mode");
			spdlog::info("  'f' - Configures the GLV to constantly cycle through the TM optimization preloaded columns one by one\n");
			spdlog::info("Iterative optimization:");
			spdlog::info("  'm' - Runs the optimization using the iterative algorithm starting from a null solution");
			spdlog::info("  ',' - Runs the optimization using the iterative algorithm starting from the previous solution\n");
			spdlog::info("Shared optimization options:");
			spdlog::info("  'u' - Displays the solution, which was measured during the optimization, on the GLV");
			spdlog::info("  ';' - Displays and ramps the solution, which was measured during the optimization, on the GLV");
			spdlog::info("  '-' - Set the number of input modes to 64");
			spdlog::info("  '*' - Set the number of input modes to 128");
			spdlog::info("  'j' - Set the number of input modes to 256");
			spdlog::info("  'k' - Set the number of input modes to 512");
			spdlog::info("  'l' - Set the number of input modes to 1024");
			spdlog::info("  't' - Set the \"GLV pixel to mode pixel\" ratio to 1:1");
			spdlog::info("  'x' - Set the \"GLV pixel to mode pixel\" ratio to 2:1");
			spdlog::info("  '=' - Set the \"GLV pixel to mode pixel\" ratio to 3:1");
			spdlog::info("  'y' - Set the \"GLV pixel to mode pixel\" ratio to 4:1");
			spdlog::info("  '[' - Sets the input mode basis to Hadamard");
			spdlog::info("  ']' - Sets the input mode basis to Fourier\n");
			spdlog::info("GLV options:");
			spdlog::info("  '2' - Sets GLV column time to 2.86us");
			spdlog::info("  '3' - Sets GLV column time to 3us");
			spdlog::info("  '5' - Sets GLV column time to 5us");
			spdlog::info("  '6' - Sets GLV column time to 10us");
			spdlog::info("  '7' - Sets GLV column time to 15us");
			spdlog::info("  '8' - Sets GLV column time to 20us");
			spdlog::info("  '9' - Sets GLV column time to 25us\n");
}



int main(int argc, char *argv[]) {
	// Custom pattern.
	spdlog::set_pattern("%v");

	// disable QuickEdit mode in output console.
	HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
	DWORD prev_mode;
	GetConsoleMode(hInput, &prev_mode);
	SetConsoleMode(hInput, prev_mode & ~ENABLE_QUICK_EDIT_MODE);
	
	// Resources.
	App app;
	std::thread work_thread;
	u16 col_exp_index_start = 0;
	u16 col_exp_index_end = 2;
	u16 col_exp_index_grating1 = 1;
	u16 col_exp_index_grating2 = 0;
	bool toggle_1 = false;
	bool toggle_2 = false;
#ifdef GLV_PROCESSING_EMULATION
	// If the macro GLV_PROCESSING_EMULATION is defined in glv.h then we can benchmark the complete processing data path.
	work_thread = std::thread{[&] {app.test_tm_optimization_compute_performance(); }};
	//work_thread = std::thread{ [&] {app.test_iterative_optimization_compute_performance(); } };
#else
	
	// Program loop.
	help();
	bool quit = false;
	while (!quit) {
		auto key = _getch();
		switch (key) {
			case 'e':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.extract_calibration_curve(App::CALIBRATION_TYPE::CONSTANT); } };
				break;
			case 'c':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.extract_calibration_curve(App::CALIBRATION_TYPE::VOLTAGE_GRATING); } };
				break;
			case 'r':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.run_tm_optimization(); } };
				break;
			case 'm':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{[&] {app.run_iterative_optimization(false); }};
				break;
			case ',':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{[&] {app.run_iterative_optimization(true); }};
				break;
			case 's':
				app.stop();
				break;
			case 'u':
				app.display_solution();
				break;
			case ';':
				app.display_solution(true);
				break;
			case '+':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::CONTINUOUS, App::CUSTOM_COLUMN_TYPE::PHASE_GRATINGS); } };
				break;
			case 'd':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::BURST, App::CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION); } };
				break;
			case 'f':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::ONE_BY_ONE, App::CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION); } };
				break;
			case 'a':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::CONTINUOUS, App::CUSTOM_COLUMN_TYPE::VOLTAGE_GRATINGS); } };
				break;
			case 'b':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::BURST, App::CUSTOM_COLUMN_TYPE::VOLTAGE_GRATINGS); } };
				break;
			case 'n':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::ONE_BY_ONE, App::CUSTOM_COLUMN_TYPE::VOLTAGE_GRATINGS); } };
				break;
			case '0':
				app.set_tm_fixed_segment(App::FIXED_SEGMENT::REFERENCE_AT_ZERO);
				break;
			case 'p':
				app.set_tm_fixed_segment(App::FIXED_SEGMENT::REFERENCE_AT_PI);
				break;
			case 'z':
				app.set_tm_fixed_segment(App::FIXED_SEGMENT::MODE);
				break;
			case 'i':
				app.set_tm_fixed_segment(App::FIXED_SEGMENT::MODE, true);
				break;
			case 'o':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::ONE_BY_ONE, App::CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION, &col_exp_index_start, &col_exp_index_end); } };
				break;
			case 'v':
				if (app.is_running()) {
					spdlog::error("APP: Already running...");
					break;
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::CONTINUOUS, App::CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION, &col_exp_index_start, &col_exp_index_end); } };
				break;
			case 'w':
				App::PHASE_STEPS phase_step;
				if (toggle_2) {
					phase_step = App::PHASE_STEPS::PI_HALF;
					spdlog::info("APP: Phase step is PI/2");
				}
				else {
					phase_step = App::PHASE_STEPS::PI_QUARTER;
					spdlog::info("APP: Phase step is PI/4");
				}
				app.set_tm_optimization_phase_steps(phase_step);
				toggle_2 = !toggle_2;
				break;
			case '1':
				if (app.is_running()) {
					app.stop(true);
				}
				if (work_thread.joinable()) {
					work_thread.join();
				}				
				u16 col_exp_index_grating;
				if (toggle_1) {
					col_exp_index_grating = col_exp_index_grating1;
				}
				else {
					col_exp_index_grating = col_exp_index_grating2;				
				}
				toggle_1 = !toggle_1;
				spdlog::info("APP: Displaying preloaded column %s", std::to_string(col_exp_index_grating));
				work_thread = std::thread{ [&] {app.cycle_custom_columns(App::CYCLE_TYPE::ONE_BY_ONE, App::CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION, &col_exp_index_grating, &col_exp_index_grating, false); } };
				break;
			case 'g':
				app.test_glv(GLV::SLM_TEST9);
				break;
			case '-':
				app.set_num_of_input_modes(64);
				break;
			case '*':
				app.set_num_of_input_modes(128);
				break;
			case 'j':
				app.set_num_of_input_modes(256);
				break;
			case 'k':
				app.set_num_of_input_modes(512);
				break; 
			case 'l':
				app.set_num_of_input_modes(1024);
				break;
			case 't':
				app.set_glv_mode_pixel_ratio(App::PIXEL_RATIO::ONE_TO_ONE);
				break;
			case 'x':
				app.set_glv_mode_pixel_ratio(App::PIXEL_RATIO::TWO_TO_ONE);
				break;
			case '=':
				app.set_glv_mode_pixel_ratio(App::PIXEL_RATIO::THREE_TO_ONE);
				break;
			case 'y':
				app.set_glv_mode_pixel_ratio(App::PIXEL_RATIO::FOUR_TO_ONE);
				break;
			case '2':
				app.set_glv_column_period(2860);
				break;
			case '3':
				app.set_glv_column_period(3000);
				break;
			case '5':
				app.set_glv_column_period(5000);
				break;
			case '6':
				app.set_glv_column_period(10000);
				break;
			case '7':
				app.set_glv_column_period(15000);
				break;
			case '8':
				app.set_glv_column_period(20000);
				break;
			case '9':
				app.set_glv_column_period(25000);
				break;
			case '[':
				app.set_basis_type(App::INPUT_MODE_BASIS::HADAMARD);
				break;
			case ']':
				app.set_basis_type(App::INPUT_MODE_BASIS::FOURIER);
				break;
			case 'h':
				spdlog::info("");
				help();
				break;
			case 'q':
				app.stop(true);
				quit = true;
				break;
			default:
				break;
		}
	}
#endif
	if (work_thread.joinable()) {
		work_thread.join();
	}
	return 0;
}
