#pragma once
#include <thread>
#include <string>
#include "eigen/Eigen/Dense"
#include "core0/types.h"
#include "core2/hpc.h"
#include "alazar_daq/alazar_daq.h"
#include "glv/glv.h"

// Application defaults.
const int kInputModes_initial = 256;
const int kGLVvddah = 340;
const u32 kGLVLoopCycleWait_us = 50000;
const std::string kGLVComPort = "COM3";
const int kTMInterferencePatternsPerMode = 3;
const int kIterativePhaseStepsPerMode = 16;

// Probably don't need to touch these.
const int kDAQSamplesPerRecord = 256;
const int kModesPerBufferTM = 64;  // Each buffer in the TM optimization will contain data for X records, where X = kModesPerBufferTM * kInterferencePatternsPerMode
                                   // The following must be an integer: (kInputModes * kInterferencePatternsPerMode) / kModesPerBufferTM
const int kModesPerBufferIterative = 16; // Each buffer in the iterative optimization will contain data for X records, where X = kModesPerBufferIterative * kIterativePhasesPerMode
                                         // The following must be an integer: (kInputModes * kIterativePhasesPerMode) / kModesPerBufferIterative
const int kTestTrials = 50;  // Report results every kTestTrials cycles.
#define DAQ_WORKAROUND


class App {
public:
	App();
	~App();

	enum OPTIMIZATION_ALGORITHM {
		TM = 0,
		ITERATIVE
	};

	enum FIXED_SEGMENT {
		REFERENCE_AT_ZERO = 0,
		REFERENCE_AT_PI,
		MODE
	};

	enum CYCLE_TYPE {
		BURST = 0,   // Command the GLV to cycle all the patterns in a burst and then hold.
		ONE_BY_ONE,  // Command the GLV to cycle through the patterns one at a time.
		CONTINUOUS   // Command the GLV to cycle all the pattern in a burst repetitively, without holding in between.
	};

	enum PIXEL_RATIO {
		ONE_TO_ONE = 1,
		TWO_TO_ONE = 2,
		THREE_TO_ONE = 3,
		FOUR_TO_ONE = 4
	};

	enum CALIBRATION_TYPE {
		CONSTANT = 0,
		VOLTAGE_GRATING
	};

	enum PHASE_STEPS {
		PI_HALF = 0,
		PI_QUARTER
	};

	enum CUSTOM_COLUMN_TYPE {
		TM_OPTIMIZATION = 0,
		ITERATIVE_OPTIMIZATION_USE_PREV_SOLUTION,
		ITERATIVE_OPTIMIZATION_START_BLANK,
		VOLTAGE_GRATINGS,
		PHASE_GRATINGS
	};

	enum INPUT_MODE_BASIS {
		HADAMARD = 0,
		FOURIER
	};

	// Extracts a voltage curve (file), with a DAQ voltage level for each GLV DAC level.
	void extract_calibration_curve(const CALIBRATION_TYPE calibration_type);
	
	// Runs the TM optimization.
	void run_tm_optimization();

	// Runs the iterative optimization.
	void run_iterative_optimization(const bool use_previous_solution);

	// Cycles through a set of custom columns used in either in the experiment or for calibration.
	// manual_repeat_cycle is only effective for CYCLE_TYPE::BURST & CYCLE_TYPE::ONE_BY_ONE. CYCLE_TYPE::CONTINUOUS is natively repetitive.
	void cycle_custom_columns(const CYCLE_TYPE cycle_type, const CUSTOM_COLUMN_TYPE custom_column_type, u16* col_index_start = nullptr, u16* col_index_end = nullptr, const bool manual_repeat_cycle = true);

	// Stops the current operation.
	void stop(const bool silent = false);

	// Displays the solution, which was measured during the experiment, on the GLV.
	void display_solution(const bool ramp_repetitive = false);

	// Returns true if the app is running.
	bool is_running();

	// Runs a test on the GLV.
	void test_glv(const GLV::GLV_TESTS& test);

	// Sets the GLV column period (ns).
	void set_glv_column_period(const u32 col_period_ns);

	// Sets the fixed segment on the preloaded columns for the TM optimization. 
	void set_tm_fixed_segment(const FIXED_SEGMENT segment, const bool final_reference_is_grating = false, const bool silent = false);

	// Sets the number of input modes.
	void set_num_of_input_modes(const int input_modes, const bool silent = false);

	// Sets the number of GLV pixels used for one mode pixel.
	void set_glv_mode_pixel_ratio(const PIXEL_RATIO ratio);

	// Sets the phase steps for the TM optimization.
	void set_tm_optimization_phase_steps(const PHASE_STEPS phase_steps);

	// Sets the input mode basis type.
	void set_basis_type(const INPUT_MODE_BASIS input_mode_basis);

	// Benchmarks the processing datapath of the application.
	// Instead of triggering and acquiring real data, we process dummy data as fast as possible and then
	// send the final result to the GLV module which just sinks the data instead of sending it to the hardware. 
	// This indicates an upper bound for how fast the application will run without the supporting hardware.
	// Note: Requires the definition of the macro GLV_PROCESSING_EMULATION in glv.h to include the processing in
	//       the GLV module.
	void test_tm_optimization_compute_performance();
	void test_iterative_optimization_compute_performance();


private:
	std::unique_ptr<DAQ> m_daq;  
	std::unique_ptr<GLV> m_glv;
	int m_input_modes;
	int m_glv_mode_pixel_ratio;
	int m_pixels_per_mode;
	int m_records_per_buffer_tm;
	int m_records_per_buffer_iterative;
	int m_buffer_count_per_cycle;
	size_t m_cycle_count;
	bool m_app_running;
	bool m_glv_auto_running;
	bool m_glv_manual_running;
	bool m_calibration_loaded;
	std::thread m_daq_thread;
	HPC m_hpc;
	int m_mode_start_pixel;
	u32 m_glv_col_period_ns_initial;
	f32 m_phase_index_coeff;
	u16* m_phase_to_dac;
	u16 m_phase_to_dac_size;
	FIXED_SEGMENT m_fixed_segment;
	INPUT_MODE_BASIS m_input_mode_basis;
	Eigen::MatrixXcf m_input_modes_matrix;
	Eigen::MatrixXcf m_input_modes_matrix_adjusted;
	Eigen::VectorXcf m_final_cartesian_pattern;
	Eigen::VectorXf m_final_phase_column;
	PHASE_STEPS m_phase_steps;
	Eigen::VectorXcf m_iterative_phase_step_in_cartesian;
	OPTIMIZATION_ALGORITHM m_algorithm_on_last_run;
	bool m_daq_workaround_first_buffer_flag;

	// Private methods.
	bool load_phase_to_dac_calibration_file();
	bool configure(const DAQParams* daq_params, const GLVParams* glv_params);
	void on_glv_serial_receive(char* const data_ptr, const size_t data_len);
	void on_buffer_receive_extract_calibration_curve(u16* const data_ptr, const size_t data_len, const u64 data_index);
	void on_buffer_receive_run_tm_optimization(u16* const data_ptr, const size_t data_len, const u64 data_index);
	void on_buffer_receive_run_iterative_optimization(u16* const data_ptr, const size_t data_len, const u64 data_index);
	void on_daq_timeout();
	GLVColVectorXs convert_phase_to_glv_dac_column(const Eigen::VectorXf& phase_column);
	GLVFrameXs convert_phase_to_glv_dac_column(const Eigen::MatrixXf& phase_frame);
	GLVFrameXs create_voltage_gratings();
	void create_input_modes();
	Eigen::MatrixXf create_preloaded_phase_columns_for_tm_optimization(const bool dump_to_file = false);
	Eigen::MatrixXf create_preloaded_phase_columns_for_iterative_optimization(const bool use_prev_solution, const bool dump_to_file = false);
	void print_configuration(const OPTIMIZATION_ALGORITHM algorithm);
}; 