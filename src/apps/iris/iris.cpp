#define BOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
#include <vector>
#include <mutex>
#include <string>
#include <future>
#include <fstream>
#include "spdlog/spdlog.h"
#include "iris.h"
using ModeMatrixTM = Eigen::Matrix<u16, kDAQSamplesPerRecord, kTMInterferencePatternsPerMode>;
using ModeMatrixIterative = Eigen::Matrix<u16, kDAQSamplesPerRecord, kIterativePhaseStepsPerMode>;
const f32 PI_F32 = 3.1415927f;
const f32 TWOPI_F32 = 2 * PI_F32;


App::App() {	
	// Allocate a DAQ and a GLV.
	m_daq = std::make_unique<DAQ>();
	m_glv = std::make_unique<GLV>();

	// Internal initializations.
	m_cycle_count = 0;
	m_glv_col_period_ns_initial = 20000;
	m_app_running = false;
	m_glv_manual_running = false;
	m_glv_auto_running = false;
	m_calibration_loaded = false;
	m_phase_to_dac = nullptr;

	// Determine how many records the DAQ would return with each buffer.
	m_records_per_buffer_tm = kTMInterferencePatternsPerMode * kModesPerBufferTM;
	m_records_per_buffer_iterative = kIterativePhaseStepsPerMode * kModesPerBufferIterative;

	// Create the input modes.
	m_glv_mode_pixel_ratio = PIXEL_RATIO::ONE_TO_ONE;
	m_input_mode_basis = INPUT_MODE_BASIS::HADAMARD;
	m_phase_steps = PHASE_STEPS::PI_HALF;
	set_num_of_input_modes(kInputModes_initial, true);

	// Set the fixed segment in the preloaded columns and allocate the dynamic column.
	// The fixed segment only affects the TM optimization.
	// Iterative optimization means that the reference is inherently fixed.
	set_tm_fixed_segment(FIXED_SEGMENT::MODE, true, true);
}
 

App::~App() {
	if (m_phase_to_dac) {
		delete[] m_phase_to_dac;
	}
}


void App::extract_calibration_curve(const CALIBRATION_TYPE calibration_type) {
	if (m_app_running) {
		spdlog::error("APP: Already running");
		return;
	}
	if (calibration_type == CALIBRATION_TYPE::CONSTANT) {
		spdlog::info("APP: --- Extracting the voltage curve for constant columns... ---");
	}
	if (calibration_type == CALIBRATION_TYPE::VOLTAGE_GRATING) {
		spdlog::info("APP: --- Extracting the voltage curve for grating columns with a period of %d pixels... ---", m_glv_mode_pixel_ratio * 2);
	}
	m_app_running = true;

	// DAQ is configured to have all records in one buffer.
	// Each record corresponds to one GLV column.
	DAQParams m_daqparams;
	const auto on_m_daqbuffer_recv = std::bind(&App::on_buffer_receive_extract_calibration_curve, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	const auto on_m_daqtimeout = std::bind(&App::on_daq_timeout, this);
	m_daqparams.records_per_buffer = static_cast<u32>(kGLVDACLevels);
	m_daqparams.samples_per_record = kDAQSamplesPerRecord;
	m_daqparams.buffers_per_acquisition = 1;
	m_daqparams.acquisition_mode = DAQParams::ACQUISTION_SINGLE;
	m_daqparams.trigger_delay_sec = 0;
	m_daqparams.on_recv = on_m_daqbuffer_recv;
	m_daqparams.on_timeout = on_m_daqtimeout;
	
	// GLV configuration.
	GLVParams m_glvparams;
	const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_glvparams.com_port = kGLVComPort;
	m_glvparams.trigger_auto = false;
	m_glvparams.vddah = kGLVvddah;
	m_glvparams.on_recv = on_m_glvserial_recv;

	// Configures the DAQ and the GLV.
	if (!configure(&m_daqparams, &m_glvparams)) {
		spdlog::error("APP: Extracting voltage curve failed");
		m_app_running = false;
		return;
	}

	// Start the DAQ capture.
	std::promise<RETURN_CODE> capture_return_promise;
	std::future<RETURN_CODE> capture_return_future = capture_return_promise.get_future();
	m_daq_thread = std::thread{ [&] {capture_return_promise.set_value_at_thread_exit(m_daq->capture()); } };

	// If calibration type is constant, start GLV USR_TEST5 which cycles through all the GLV amplitudes once.
	if (calibration_type == CALIBRATION_TYPE::CONSTANT) {
		if (!m_glv->run_test(GLV::USR_TEST5)) {
			spdlog::error("APP: Extracting voltage curve failed");
			m_app_running = false;
			return;
		}
	}

	// If calibration type is grating, preload the glv with the gratings and cycle once.
	if (calibration_type == CALIBRATION_TYPE::VOLTAGE_GRATING) {
		if (!m_glv->preload(create_voltage_gratings())) {
			spdlog::error("APP: Extracting voltage curve failed");
			m_app_running = false;
			return;
		}
		if (!m_glv->cycle(0, kGLVMaxAmp)) {
			spdlog::error("APP: Extracting voltage curve failed");
			m_app_running = false;
			return;
		}
	}

	// Clean up.
	capture_return_future.wait();
	auto capture_return_code = capture_return_future.get();
	if (capture_return_code != ApiSuccess) {
		spdlog::error("APP: DAQ failed with status %s", m_daq->error_to_text(capture_return_code));
		spdlog::error("APP: Extracting voltage curve failed");
	}
	if (m_daq_thread.joinable()) {
		m_daq_thread.join();
	}
	m_app_running = false;
}


void App::run_tm_optimization() {
	if (m_app_running) {
		spdlog::error("APP: Already running");
		return;
	}

	// Load phase to DAC calibration for the GLV.
	if (!load_phase_to_dac_calibration_file()) {
		return;
	}

	// One cycle of the optimization requires cycling through #m_buffer_count_per_cycle buffers.
	// This is required for the DAQ buffer receive callback in order to know how many buffer to process per cycle.
	m_buffer_count_per_cycle = m_input_modes / kModesPerBufferTM;

	// DAQ is configured to have #m_records_per_buffer_tm records in one buffer.
	// Each record corresponds to one GLV column.
	DAQParams m_daqparams;
	const auto on_m_daqbuffer_recv = std::bind(&App::on_buffer_receive_run_tm_optimization, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	const auto on_m_daqtimeout = std::bind(&App::on_daq_timeout, this);
	m_daqparams.records_per_buffer = static_cast<u32>(m_records_per_buffer_tm);
	m_daqparams.samples_per_record = kDAQSamplesPerRecord;
	m_daqparams.acquisition_mode = DAQParams::ACQUISTION_NORMAL;
	m_daqparams.trigger_delay_sec = 0;
	m_daqparams.voltage_range = INPUT_RANGE_PM_1_V;
	m_daqparams.on_recv = on_m_daqbuffer_recv;
	m_daqparams.on_timeout = on_m_daqtimeout;

	// GLV configuration.
	GLVParams m_glvparams;
	const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_glvparams.com_port = kGLVComPort;
	m_glvparams.trigger_auto = false;
	m_glvparams.vddah = kGLVvddah;
	m_glvparams.loopcycle_wait_us = kGLVLoopCycleWait_us;
	m_glvparams.col_period_ns = m_glv_col_period_ns_initial;
	m_glvparams.on_recv = on_m_glvserial_recv;

  // Print the configuration and set the running state to true.
	spdlog::info("");
	spdlog::info("APP: --- Running the TM optimization... ---");
	print_configuration(OPTIMIZATION_ALGORITHM::TM);
	auto columns_to_preload = create_preloaded_phase_columns_for_tm_optimization(false);
	m_app_running = true;

	// Configures the DAQ and the GLV.
	if (!configure(&m_daqparams, &m_glvparams)) {
		m_app_running = false;
		spdlog::error("APP: --- Optimization stopped ---");
		return;
	}

	// Preload the fixed columns to the GLV.
	if (!m_glv->preload(convert_phase_to_glv_dac_column(columns_to_preload))) {
		spdlog::error("APP: Failed to preload the GLV");
		spdlog::error("APP: --- Optimization stopped ---");
		m_app_running = false;
		return;
	}
	
	// Start the DAQ capture.
	std::promise<RETURN_CODE> capture_return_promise;
	std::future<RETURN_CODE> capture_return_future = capture_return_promise.get_future();
	m_daq_thread = std::thread{ [&] {capture_return_promise.set_value_at_thread_exit(m_daq->capture()); }};
	Sleep(1000);
#ifdef m_daqWORKAROUND
	m_daqworkaround_first_buffer_flag_ = true;
	m_glv->cycle(0, kModesPerBufferTM * kTMInterferencePatternsPerMode);
#endif

	// Start the GLV loop cycle.
	m_hpc.start();
	m_glv->run_loop_cycle();

	// Clean-up.
	capture_return_future.wait();
	auto capture_return_code = capture_return_future.get();
	if (capture_return_code != ApiSuccess) {
		spdlog::error("APP: DAQ failed with status %s", m_daq->error_to_text(capture_return_code));
	}
	if (m_daq_thread.joinable()) {
		m_daq_thread.join();
	}
	m_glv->stop_loop_cycle();
	m_app_running = false;
	m_algorithm_on_last_run = OPTIMIZATION_ALGORITHM::TM;
	if (capture_return_code == ApiSuccess) {
		spdlog::info("App: --- Optimization stopped ---");
	}
	else {
		spdlog::error("APP: --- Optimization stopped ---");
	}
}


void App::run_iterative_optimization(const bool use_previous_solution) {
	if (m_app_running) {
		spdlog::error("APP: Already running");
		return;
	}

	// Load phase to DAC calibration for the GLV.
	if (!load_phase_to_dac_calibration_file()) {
		return;
	}

	// One cycle of the optimization requires cycling through #m_buffer_count_per_cycle buffers.
	// This is required for the DAQ buffer receive callback in order to know how many buffer to process per cycle.
	m_buffer_count_per_cycle = m_input_modes / kModesPerBufferIterative;

	// DAQ is configured to have #m_records_per_buffer_iterative records in one buffer.
	// Each record corresponds to one GLV column.
	DAQParams m_daqparams;
	const auto on_m_daqbuffer_recv = std::bind(&App::on_buffer_receive_run_iterative_optimization, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	const auto on_m_daqtimeout = std::bind(&App::on_daq_timeout, this);
	m_daqparams.records_per_buffer = static_cast<u32>(m_records_per_buffer_iterative);
	m_daqparams.samples_per_record = kDAQSamplesPerRecord;
	m_daqparams.acquisition_mode = DAQParams::ACQUISTION_NORMAL;
	m_daqparams.trigger_delay_sec = 0;
	m_daqparams.voltage_range = INPUT_RANGE_PM_1_V;
	m_daqparams.on_recv = on_m_daqbuffer_recv;
	m_daqparams.on_timeout = on_m_daqtimeout;

	// GLV configuration.
	GLVParams m_glvparams;
	const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_glvparams.com_port = kGLVComPort;
	m_glvparams.trigger_auto = false;
	m_glvparams.vddah = kGLVvddah;
	m_glvparams.loopcycle_wait_us = kGLVLoopCycleWait_us;
	m_glvparams.col_period_ns = m_glv_col_period_ns_initial;
	m_glvparams.on_recv = on_m_glvserial_recv;

  // Print the configuration and set the running state to true.
	spdlog::info("");
	spdlog::info("APP: --- Running the Iterative optimization... ---");
	print_configuration(OPTIMIZATION_ALGORITHM::ITERATIVE);
	auto columns_to_preload = create_preloaded_phase_columns_for_iterative_optimization(use_previous_solution, false);
	m_app_running = true;

	// Configures the DAQ and the GLV.
	if (!configure(&m_daqparams, &m_glvparams)) {
		m_app_running = false;
		spdlog::error("APP: --- Optimization stopped ---");
		return;
	}

	// Preload the fixed columns to the GLV.
	if (!m_glv->preload(convert_phase_to_glv_dac_column(columns_to_preload))) {
		spdlog::error("APP: Failed to preload the GLV");
		spdlog::error("APP: --- Optimization stopped ---");
		m_app_running = false;
		return;
	}
	
	// Start the DAQ capture.
	std::promise<RETURN_CODE> capture_return_promise;
	std::future<RETURN_CODE> capture_return_future = capture_return_promise.get_future();
	m_daq_thread = std::thread{ [&] {capture_return_promise.set_value_at_thread_exit(m_daq->capture()); }};
	Sleep(1000);
#ifdef m_daqWORKAROUND
	m_daqworkaround_first_buffer_flag_ = true;
	m_glv->cycle(0, kModesPerBufferIterative * kIterativePhaseStepsPerMode);
#endif

	// Start the GLV loop cycle.
	m_hpc.start();
	m_glv->run_loop_cycle();

	// Clean-up.
	capture_return_future.wait();
	auto capture_return_code = capture_return_future.get();
	if (capture_return_code != ApiSuccess) {
		spdlog::error("APP: DAQ failed with status %s", m_daq->error_to_text(capture_return_code));
	}
	if (m_daq_thread.joinable()) {
		m_daq_thread.join();
	}
	m_glv->stop_loop_cycle();
	m_app_running = false;
	m_algorithm_on_last_run = OPTIMIZATION_ALGORITHM::ITERATIVE;
	if (capture_return_code == ApiSuccess) {
		spdlog::info("APP: --- Optimization stopped ---");
	}
	else {
		spdlog::error("APP: --- Optimization stopped ---");
	}
}


void App::cycle_custom_columns(const CYCLE_TYPE cycle_type, const CUSTOM_COLUMN_TYPE custom_column_type, u16* col_index_start, u16* col_index_end, const bool manual_repeat_cycle) {
	if (m_app_running) {
		spdlog::error("APP: Already running");
		return;
	}
	std::string custom_column_type_string;
	if (custom_column_type == CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION) {
		custom_column_type_string = "TM optimization columns";
	}
	if (custom_column_type == CUSTOM_COLUMN_TYPE::ITERATIVE_OPTIMIZATION_USE_PREV_SOLUTION) {
		custom_column_type_string = "iterative optimization columns using previous solution";
	}
	if (custom_column_type == CUSTOM_COLUMN_TYPE::ITERATIVE_OPTIMIZATION_START_BLANK) {
		custom_column_type_string = "iterative optimization columns starting blank";
	}
	if (custom_column_type == CUSTOM_COLUMN_TYPE::VOLTAGE_GRATINGS) {
		custom_column_type_string = "voltage gratings";
	}
	if (custom_column_type == CUSTOM_COLUMN_TYPE::PHASE_GRATINGS) {
		custom_column_type_string = "phase gratings";
	}
	spdlog::info("APP: --- Cycling through %s ---", custom_column_type_string);
	m_app_running = true;

	// GLV configuration.
	GLVParams m_glvparams;
	const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_glvparams.com_port = kGLVComPort;
	if (cycle_type == CYCLE_TYPE::CONTINUOUS) {
		m_glvparams.trigger_auto = true;
	}
	else {
		m_glvparams.trigger_auto = false;
	}
	m_glvparams.vddah = kGLVvddah;
	m_glvparams.on_recv = on_m_glvserial_recv;
	if (!configure(nullptr, &m_glvparams)) {
		m_app_running = false;
		return;
	}

	// For the TM optimization, load phase to DAC calibration for the GLV, create the preloaded phase columns and preload.
	u16 last_column_index;
	if (custom_column_type == CUSTOM_COLUMN_TYPE::TM_OPTIMIZATION) {
		// Load.
		if (!load_phase_to_dac_calibration_file()) {
			m_app_running = false;
			return;
		}
		if (!m_glv->preload(convert_phase_to_glv_dac_column(create_preloaded_phase_columns_for_tm_optimization(true)))) {
			spdlog::error("APP: Failed to preload the GLV");
			m_app_running = false;
			return;
		}
		last_column_index = m_input_modes * kTMInterferencePatternsPerMode - 1;
	}

	// For the Iterative optimization, load phase to DAC calibration for the GLV, create the preloaded phase columns and preload.
	if ((custom_column_type == CUSTOM_COLUMN_TYPE::ITERATIVE_OPTIMIZATION_USE_PREV_SOLUTION)  || (custom_column_type == CUSTOM_COLUMN_TYPE::ITERATIVE_OPTIMIZATION_START_BLANK)){
		// Load.
		if (!load_phase_to_dac_calibration_file()) {
			m_app_running = false;
			return;
		}
		bool use_prev_solution = false;
		if (custom_column_type == CUSTOM_COLUMN_TYPE::ITERATIVE_OPTIMIZATION_USE_PREV_SOLUTION) {
			use_prev_solution = true;
		}
		if (!m_glv->preload(convert_phase_to_glv_dac_column(create_preloaded_phase_columns_for_iterative_optimization(use_prev_solution)))) {
			spdlog::error("APP: Failed to preload the GLV");
			m_app_running = false;
			return;
		}
		last_column_index = m_input_modes * kIterativePhaseStepsPerMode - 1;
	}

	// For the voltage gratings, we only need to preload.
	if (custom_column_type == CUSTOM_COLUMN_TYPE::VOLTAGE_GRATINGS) {
		if (!m_glv->preload(create_voltage_gratings())) {
			spdlog::error("APP: Failed to preload the GLV");
			m_app_running = false;
			return;
		}
		last_column_index = kGLVDACLevels - 1;
	}

	// For the phase gratings, load phase to DAC calibration for the GLV, create the preloaded phase columns and preload.
	if (custom_column_type == CUSTOM_COLUMN_TYPE::PHASE_GRATINGS) {
		if (!load_phase_to_dac_calibration_file()) {
			m_app_running = false;
			return;
		}
		// We create voltage gratings (between 0 and 1023) and then linearly modify it to be gratings between 0 and 2PI.
		auto voltage_gratings = create_voltage_gratings();
		Eigen::MatrixXf phase_gratings = (voltage_gratings.array().cast<f32>() * TWOPI_F32 / kGLVMaxAmp);
		
		// Now modify to the range [-PI, PI].
		phase_gratings = phase_gratings.array() - PI_F32;
		if (!m_glv->preload(convert_phase_to_glv_dac_column(phase_gratings))) {
			spdlog::error("APP: Failed to preload the GLV");
			m_app_running = false;
			return;
		}
		last_column_index = kGLVDACLevels - 1;
	}

	// Trigger GLV cycles in a loop.
	u16 col_index_start_int;
	u16 col_index_end_int;
	
	// Bursty.
	if (cycle_type == CYCLE_TYPE::BURST) {
		if (col_index_start && col_index_end) {
			col_index_start_int = *col_index_start;
			col_index_end_int = *col_index_end;
		}
		else {
			col_index_start_int = 0;
			col_index_end_int = last_column_index;
		}
		m_glv_manual_running = true;
		while (m_glv_manual_running) {
			m_glv->cycle(col_index_start_int, col_index_end_int);
			if (!manual_repeat_cycle) {
				m_glv_manual_running = false;
			}
		}
		m_app_running = false;
	}

	// One by one.
	if (cycle_type == CYCLE_TYPE::ONE_BY_ONE) {
		if (col_index_start && col_index_end) {
			col_index_start_int = *col_index_start;
			col_index_end_int = *col_index_end;
		}
		else {
			col_index_start_int = 0;
			col_index_end_int = last_column_index;
		}
		u16 column_index = col_index_start_int;
		m_glv_manual_running = true;
		while (m_glv_manual_running) {
			m_glv->cycle(column_index, column_index);
			if (column_index == col_index_end_int) {
				if (!manual_repeat_cycle) {
					m_glv_manual_running = false;
				}
				column_index = col_index_start_int;
			}
			else {
				column_index++;			
			}
		}
		m_app_running = false;
	}

	// Continuous.
	if (cycle_type == CYCLE_TYPE::CONTINUOUS) {
		if (col_index_start && col_index_end) {
			col_index_start_int = *col_index_start;
			col_index_end_int = *col_index_end;
		}
		else {
			col_index_start_int = 0;
			col_index_end_int = last_column_index;
		}
		m_glv->cycle(col_index_start_int, col_index_end_int, true);
		m_app_running = false;
		m_glv_auto_running = true;
	}
}


void App::stop(const bool silent) {
	// Stops the GLV on all manual triggered cycle (from cycle_custom_columns).
	if (m_glv_manual_running) {
		m_glv_manual_running = false;
		if (!silent) {
			spdlog::info("APP: Stopped");
		}
		return;
	}

	// Stops the GLV on all "independent cycle" operations such as LOOPLUT or SLM_TEST9, other than LOOPCYCLE.
	if (m_glv_auto_running) {
		m_glv->stop();
		m_glv_auto_running = false;
		if (!silent) {
			spdlog::info("APP: Stopped");
		}
		return;
	}

	// Stops the GLV & DAV on software triggered operations.
	if (m_app_running) {
		if (!silent) {
			spdlog::info("APP: Stopping...\nAPP: Waiting for current operation to terminate...");
		}
		m_daq->stop();
		m_app_running = false;
		if (!silent) {
			spdlog::info("APP: Stopped");
		}
		return;
	}

	// If we got here, nothing was running.
	if (!m_glv_manual_running && !m_app_running && !m_glv_auto_running) {
		if (!silent) {
			spdlog::info("APP: Instance is not running");
		}
	}
}

 
void App::display_solution(const bool ramp_repetitive) {
	if (m_app_running) {
		stop();
		m_glv->stop_loop_cycle();
	}
	else {
		// We probably need to configure the GLV if the app wasn't running.
		GLVParams m_glvparams;
		const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
		m_glvparams.com_port = kGLVComPort;
		m_glvparams.trigger_auto = true;
		m_glvparams.vddah = kGLVvddah;
		m_glvparams.on_recv = on_m_glvserial_recv;
		if (!configure(nullptr, &m_glvparams)) {
			return;
		}
	}

	if (!ramp_repetitive) {
		int focusing_pattern_index;
		if (m_algorithm_on_last_run == OPTIMIZATION_ALGORITHM::TM) {
			focusing_pattern_index = m_input_modes * kTMInterferencePatternsPerMode;
		}
		if (m_algorithm_on_last_run == OPTIMIZATION_ALGORITHM::ITERATIVE) {
			focusing_pattern_index = m_input_modes * kIterativePhaseStepsPerMode;
		}
		m_glv->cycle(focusing_pattern_index, focusing_pattern_index);
		spdlog::info("APP: Displaying focusing column");
	}
	else {
		const auto ramp_steps = 100;
		auto final_phase_ramped_frame = Eigen::MatrixXf{kGLVPixels, ramp_steps};
		for (auto col_index = 0; col_index < ramp_steps; ++col_index) {
			auto add_fraction = (col_index / static_cast<f32>(ramp_steps)) * TWOPI_F32;
			final_phase_ramped_frame.col(col_index) = m_final_phase_column.array() + add_fraction;
			//final_phase_ramped_frame.col(col_index).fill(add_fraction);  // FIXME, for testing purposes, display constant phase columns.
			
			// Original phase values are between [-PI, PI], after ramp, the range becomes [-PI, 3PI].
			// We need to get it back to [-PI, PI]
			for (auto row_index = 0; row_index < kGLVPixels; ++row_index) {
				auto phase_value = final_phase_ramped_frame(row_index, col_index);
				
				// For phase value over PI, wrap.
				// For example:
				// 2.9PI -> 0.9PI
				// 2PI -> 0
				// 1.9PI -> -0.1PI
				// 1.1PI -> -0.9PI				
				if (phase_value > PI_F32) {
					phase_value -= TWOPI_F32;
				}

				final_phase_ramped_frame(row_index, col_index) = phase_value;
			}
		}
		m_glv->preload(convert_phase_to_glv_dac_column(final_phase_ramped_frame));
		m_glv_auto_running = true;
		m_glv->cycle(0, ramp_steps-1, true);
		spdlog::info("APP: Ramping and displaying the focusing column");
	}
}


bool App::is_running() {
	return m_app_running;
}


void App::test_glv(const GLV::GLV_TESTS& test) {
	// GLV configuration.
	GLVParams m_glvparams;
	const auto on_m_glvserial_recv = std::bind(&App::on_glv_serial_receive, this, std::placeholders::_1, std::placeholders::_2);
	m_glvparams.com_port = kGLVComPort;
	m_glvparams.col_period_ns = 2860;
	m_glvparams.trigger_auto = true;
	m_glvparams.vddah = kGLVvddah;
	m_glvparams.on_recv = on_m_glvserial_recv;
	configure(nullptr, &m_glvparams);

	// Run the test.
	if (m_app_running) {
		spdlog::error("APP: Already running");
		return;
	}
	if (!m_glv->run_test(test)) {
		spdlog::error("APP: Failed to run GLV test");
		return;
	}
	m_glv_auto_running = true;
	spdlog::info("APP: Running GLV test");
}


void App::set_glv_column_period(const u32 col_period_ns) {
	m_glv_col_period_ns_initial = col_period_ns;
	if (!is_running()) {
		spdlog::info("APP: Instance is not running");
		return;
	}
	auto success = m_glv->set_column_period(col_period_ns);
	if (success) {
		spdlog::info("APP: Column period was set to %dns", col_period_ns);
	}
	else {
		spdlog::info("APP: Failed to change column period to %dns", col_period_ns);
	}
}


void App::set_tm_fixed_segment(const FIXED_SEGMENT segment, const bool final_reference_is_grating, const bool silent) {
	if (m_app_running) {
		spdlog::error("APP: Running, stop in order to change the configuration");
		return;
	}
	m_fixed_segment = segment;

	// The reference is fixed at 0 during the entire optimization (including the final phase column.)
	// Set the phase of the reference for the final phase column.
	if (segment == FIXED_SEGMENT::REFERENCE_AT_ZERO) {
		m_final_phase_column = Eigen::VectorXf::Ones(kGLVPixels, 1);
		m_final_phase_column = m_final_phase_column.array() * 1;
		if (!silent) {
			spdlog::info("APP: Reference is fixed at phase 0 during measurement");
		}
	}

	// The reference is fixed at PI during the entire optimization (including the final phase column.)
	if (segment == FIXED_SEGMENT::REFERENCE_AT_PI) {
		m_final_phase_column = Eigen::VectorXf::Ones(kGLVPixels, 1);
		m_final_phase_column = m_final_phase_column.array() * PI_F32;
		if (!silent) {
			spdlog::info("APP: Reference is fixed at phase PI during measurement");
		}
	}
	
	// Signal is fixed.
	// For the final phase pattern, we will have the reference at 0, otherwise, the reference changes.
	if (segment == FIXED_SEGMENT::MODE) {
		m_final_phase_column = Eigen::VectorXf::Zero(kGLVPixels, 1);
		if (!silent) {
			spdlog::info("APP: Signal is fixed");
		}

		// If this is true, the reference on the final phase column will be a grating.
		if (final_reference_is_grating) {
			for (auto pixel_index = 0; pixel_index < kGLVPixels; pixel_index += 2) {
				m_final_phase_column(pixel_index) = PI_F32;
			}				
			if (!silent) {			
				spdlog::info("APP: Reference during focusing is a high frequency grating");
			}
		}
		else {
			if (!silent) {
				spdlog::info("APP: Reference during focusing is constant 0");
			}	
		}
	}

}


void App::set_num_of_input_modes(const int input_modes, const bool silent) {
	// Run some validation tests.
	// 1.Number of input modes must be larger than zero.
	// 2.Number of of input modes in a buffer must divide the total number of input modes.
	// 3.Number of input modes must be a power of 2.
	// 4.Number of input modes must be smaller than the number of GLV pixels.
	if (
		  (input_modes <= 0) ||
		  (((input_modes * kTMInterferencePatternsPerMode) % kModesPerBufferTM) != 0) ||
		  (((input_modes * kIterativePhaseStepsPerMode) % kModesPerBufferIterative) != 0) ||
		  ((input_modes & (input_modes - 1)) != 0) 
	) {
		spdlog::error("APP: Number of input modes is illegal");
		return;
	}
	if ((input_modes * m_glv_mode_pixel_ratio) > kGLVPixels) {
		spdlog::error("APP: Could not set the number of input modes to %d, \"GLV to mode\" pixel ratio * #input modes > %d pixels", input_modes, kGLVPixels);
		return;
	}
	if (m_app_running) {
		spdlog::error("APP: Running, stop in order to change the configuration");
		return;
	}

	// For a complete basis, the number of pixels required is equal to the number of modes.
	m_input_modes = input_modes;
	m_pixels_per_mode = m_input_modes * m_glv_mode_pixel_ratio;

	// The pattern includes only the mode (column without reference).
	m_final_cartesian_pattern = Eigen::VectorXcf::Zero(m_pixels_per_mode, 1);

	// In a GLV column, find the pixel index where the mode starts.
	m_mode_start_pixel = (kGLVPixels - m_pixels_per_mode) >> 1;

	// Create the input modes.
	create_input_modes();

	// Do not print message when this is called from ctor.
	if (!silent) {
		spdlog::info("APP: Number of input modes was changed to %d", m_input_modes);
	}
}


void App::set_glv_mode_pixel_ratio(const PIXEL_RATIO ratio) {
	if (m_app_running) {
		spdlog::error("APP: Running, stop in order to change the configuration");
		return;
	}
	if ((m_input_modes * ratio) > kGLVPixels) {
		spdlog::error("APP: Could not set the \"GLV to mode\" pixel ratio to %d:1, \"GLV to mode\" pixel ratio * #input modes > %d pixels", ratio, kGLVPixels);
		return;	
	}
	m_glv_mode_pixel_ratio = ratio;
	set_num_of_input_modes(m_input_modes, true);
	spdlog::info("APP: \"GLV to mode\" pixel ratio was changed to %d:1", m_glv_mode_pixel_ratio);
}


void App::set_tm_optimization_phase_steps(const PHASE_STEPS phase_steps) {
	m_phase_steps = phase_steps;
}


void App::set_basis_type(const INPUT_MODE_BASIS input_mode_basis) {
	if (m_app_running) {
		spdlog::error("APP: Running, stop in order to change the configuration");
		return;
	}
	m_input_mode_basis = input_mode_basis;
	set_num_of_input_modes(m_input_modes, true);
	if (input_mode_basis == HADAMARD) {
		spdlog::info("APP: Input mode basis was set to Hadamard");
	}
	if (input_mode_basis == FOURIER) {
		spdlog::info("APP: Input mode basis was set to Fourier");
	}
}


void App::test_tm_optimization_compute_performance() {	
	// Configure for fixed mode and reference at 0 for the final column.
	set_tm_fixed_segment(FIXED_SEGMENT::MODE, false, true);
	
	// GLV configuration is required (but not actually used), since the glv module is also doing some processing which should be taken into account.
	if (!m_glv->configure(GLVParams{})) {
		return;
	};

	if (!load_phase_to_dac_calibration_file()) {
		return;
	}

	// Simulate the DAQ capture and and callback process.
	// Allocate memory for DMA buffers.
	auto m_daqbuffer_count = 4;
	auto buffer_array = new u16*[m_daqbuffer_count];
	auto bytes_per_record = kDAQSamplesPerRecord * 2;
	auto bytes_per_buffer = bytes_per_record * kModesPerBufferTM * kTMInterferencePatternsPerMode;
	for (auto buffer_index = 0; buffer_index < m_daqbuffer_count; buffer_index++) {
		buffer_array[buffer_index] = static_cast<u16*>(VirtualAlloc(nullptr, bytes_per_buffer, MEM_COMMIT, PAGE_READWRITE));
		if (buffer_array[buffer_index] == nullptr) {
			spdlog::error("Alloc %d bytes failed", bytes_per_buffer);
		}
	}

	// Fill each buffer from synthesized data:
	std::fstream file;
	file.open("matlab/response_synthesizer/synthesis.txt", std::fstream::in);
	if (!file) {
		return;
	}
	std::string line;
	u32 buffer_index = 0;
	u32 sample_index_start = 0;
	u32 intereference_index = 0;
	while (std::getline(file, line)) {
		for (auto sample_index = sample_index_start; sample_index < sample_index_start + kDAQSamplesPerRecord; ++sample_index) {
			buffer_array[buffer_index][sample_index] = static_cast<u16>(std::stof(line));
		}
		sample_index_start += kDAQSamplesPerRecord;
		intereference_index++;
		if (intereference_index == kModesPerBufferTM * kTMInterferencePatternsPerMode) {
			buffer_index++;
			sample_index_start = 0;
			intereference_index = 0;
		}
	}

	// Compute the number of bytes in the buffer.
	auto buffer_length_bytes = kDAQSamplesPerRecord * m_records_per_buffer_tm * 2;

	// One cycle of the optimization requires cycling through #m_buffer_count_per_cycle buffers.
	// This is required for the DAQ buffer receive callback in order to know how many buffer to process per cycle.
	m_buffer_count_per_cycle = m_input_modes / kModesPerBufferTM;

	// Setup and run the test.
	// kTestTrials is #of trials per iteration
	// Start the high performance counter.
	m_app_running = true;
	m_hpc.start();
	while (m_app_running) {
		for (auto ii = 0; ii < m_daqbuffer_count; ++ii) {
			on_buffer_receive_run_tm_optimization(buffer_array[ii], buffer_length_bytes, ii);
		}
	}

	// Clear allocated memory.
	for (auto buffer_index = 0; buffer_index < m_daqbuffer_count; ++buffer_index) {
		if (buffer_array[buffer_index]) {
			VirtualFree(buffer_array[buffer_index], 0, MEM_RELEASE);
		}
	}
	delete[] buffer_array;
}


void App::test_iterative_optimization_compute_performance() {
	// GLV configuration is required (but not actually used), since the glv module is also doing some processing which should be taken into account.
	if (!m_glv->configure(GLVParams{})) {
		return;
	};

	if (!load_phase_to_dac_calibration_file()) {
		return;
	}

	// Unlike the TM case, we need this in order to create the phase steps LUT.
	// Also, simulate a case which we start with a solution vector.
	m_final_phase_column.fill(PI_F32/4);
	create_preloaded_phase_columns_for_iterative_optimization(true, true);

	// Simulate the DAQ capture and and callback process.
	// Allocate memory for DMA buffers.
	auto m_daqbuffer_count = 4;
	auto buffer_array = new u16*[m_daqbuffer_count];
	auto bytes_per_record = kDAQSamplesPerRecord * 2;
	auto bytes_per_buffer = bytes_per_record * kModesPerBufferIterative * kIterativePhaseStepsPerMode;
	for (auto buffer_index = 0; buffer_index < m_daqbuffer_count; buffer_index++) {
		buffer_array[buffer_index] = static_cast<u16*>(VirtualAlloc(nullptr, bytes_per_buffer, MEM_COMMIT, PAGE_READWRITE));
		if (buffer_array[buffer_index] == nullptr) {
			spdlog::error("Alloc %d  bytes failed", bytes_per_buffer);
		}
	}

	// Fill each buffer from synthesized data.
	for (auto buffer_index = 0; buffer_index < m_daqbuffer_count; ++buffer_index) {
		for (auto record_index = 0; record_index < kModesPerBufferIterative * kIterativePhaseStepsPerMode; ++record_index) {
			for (auto sample_index = 0; sample_index < kDAQSamplesPerRecord; ++sample_index) {
				buffer_array[buffer_index][record_index*kDAQSamplesPerRecord + sample_index] = record_index;
			}
		}
	}

	// Compute the number of bytes in the buffer.
	auto buffer_length_bytes = kDAQSamplesPerRecord * m_records_per_buffer_iterative * 2;

	// One cycle of the optimization requires cycling through #m_buffer_count_per_cycle buffers.
	// This is required for the DAQ buffer receive callback in order to know how many buffer to process per cycle.
	m_buffer_count_per_cycle = m_input_modes / kModesPerBufferIterative;

	// Setup and run the test.
	// kTestTrials is #of trials per iteration
	// Start the high performance counter.
	m_app_running = true;
	m_hpc.start();
	while (m_app_running) {
		for (auto ii = 0; ii < m_daqbuffer_count; ++ii) {
			on_buffer_receive_run_iterative_optimization(buffer_array[ii], buffer_length_bytes, ii);
		}
	}

	// Clear allocated memory.
	for (auto buffer_index = 0; buffer_index < m_daqbuffer_count; ++buffer_index) {
		if (buffer_array[buffer_index]) {
			VirtualFree(buffer_array[buffer_index], 0, MEM_RELEASE);
		}
	}
	delete[] buffer_array;
}


bool App::load_phase_to_dac_calibration_file() {
	if (m_phase_to_dac) {
		return true;
	}

	std::fstream file;
	file.open("voltage array.txt", std::fstream::in);
	if (!file) {
		spdlog::info("APP: Failed to load phase to DAC calibration");
		return false;
	}

	// Find the number of lines and allocate the phase to DAC array.
	auto num_lines = 0;
	std::string line;
	while (std::getline(file, line)) {
		++num_lines;
	}
	m_phase_to_dac_size = num_lines;
	m_phase_to_dac = new u16[m_phase_to_dac_size];
	m_phase_index_coeff = m_phase_to_dac_size / (2* PI_F32);

	// Fill in the array.
	file.clear();
	file.seekg(0, std::ios::beg);
	u16 m_phase_to_dacindex = 0;
	while (std::getline(file, line)) {
		m_phase_to_dac[m_phase_to_dacindex++] = std::stoi(line);
	}
	m_calibration_loaded = true;

	return true;
}


bool App::configure(const DAQParams* m_daqparams, const GLVParams* m_glvparams) {
	// Configure the GLV.
	if (m_glvparams) {
		if (!m_glv->configure(*m_glvparams)) {
			spdlog::error("APP: GLV configuration failed");
			return false;
		}
	}

	// Configure the DAQ (dual buffered NPT mode).
	if (m_daqparams) {
		auto m_daqcfg_return_code = m_daq->configure(*m_daqparams);
		bool m_daqok = false;
		if (m_daqcfg_return_code == ApiSuccess) {
			m_daqok = true;
		}
		else {
			spdlog::error("APP: DAQ configuration failed with error code: %s", m_daq->error_to_text(m_daqcfg_return_code));
			return false;
		}
	}

	// Everything is good if we got here.
	return true;
}


void App::on_glv_serial_receive(char* const data_ptr, const size_t data_len) {
	int index = 0;
	static std::string response;
	while (index < data_len) {
		if (data_ptr[index] == '\r') {
			spdlog::info("GLV: %s", response);
			response.clear();
		}
		else {
			response += data_ptr[index];
		}
		index++;  
	}
}


void App::on_buffer_receive_extract_calibration_curve(u16* const data_ptr, const size_t data_len, const u64 data_index) {
	// It is expected that we get kDAQSamplesPerRecord for each GLV DAC level (data_len is in bytes, and each sample is 2bytes).
	auto num_samples = data_len >> 1;
	assert(num_samples == (kDAQSamplesPerRecord * kGLVDACLevels));

	// Open the output calibration file.
	std::ofstream file{"voltage_curve.txt"};

	// Compute the average of each record and dump to file.
	f32 sum = 0;
	for (size_t sample_index = 1; sample_index <= num_samples; ++sample_index) {
		sum += static_cast<f32>(data_ptr[sample_index - 1]);
		if ((sample_index % kDAQSamplesPerRecord) == 0) {
			f32 avg = sum / kDAQSamplesPerRecord;
			if (sample_index == num_samples) {
				file << avg;
			}
			else {
				file << avg << "," << std::endl;
			}
			sum = 0;
		}
	}
	file.close();
	spdlog::info("APP: --- Voltage curve is ready ---");
}


void App::on_buffer_receive_run_tm_optimization(u16* const data_ptr, const size_t data_len, const u64 data_index) {
#ifdef DAQ_WORKAROUND
#ifndef GLV_PROCESSING_EMULATION
	static int buffer_index = 0;
	if (m_daq_workaround_first_buffer_flag) {
		buffer_index = 0;
		m_daq_workaround_first_buffer_flag = false;
		return;
	}

	// Get the index of the buffer, discard first.
	//int buffer_index = (data_index - 1) % m_buffer_count_per_cycle;
#else
	// For the emulation, we should not discard the first buffer.
	int buffer_index = data_index % m_buffer_count_per_cycle;
#endif
#endif

	// Each thread takes care of exactly one input mode.
	#pragma omp parallel
	{
		Eigen::MatrixXcf private_cartesian_pattern = Eigen::VectorXcf::Zero(m_pixels_per_mode, 1);
		#pragma omp for nowait
		for (auto record_index = 0; record_index < m_records_per_buffer_tm; record_index += kTMInterferencePatternsPerMode) {
			// Get the local index of the mode, which is equal to the index of the first record of that mode inside the buffer.
			auto mode_index = record_index / kTMInterferencePatternsPerMode;

			// Convert a buffer segment to a Column-major storage eigen matrix, where each column corresponds to a record (different interference pattern).
			auto start_sample_index = record_index * kDAQSamplesPerRecord;
			auto mode_intensities_per_interference = Eigen::Map<ModeMatrixTM>(data_ptr + start_sample_index);
		
			// Find the mean of each record and store in a row vector with kTMInterferencePatternsPerMode_ entries.
			//auto mode_avg_intensity_per_interference = mode_intensities_per_interference.cast<f32>().colwise().mean();
			auto mode_avg_intensity_per_interference = mode_intensities_per_interference.cast<f32>().block(200, 0, 50, kTMInterferencePatternsPerMode).colwise().mean();

			// Compute the response of the mode from the interference patterns.
			// Note: This equation depends on the interference patterns and would have to change if they change.
			auto I = mode_avg_intensity_per_interference;
			std::complex<f32> mode_response_conj;
			if (m_fixed_segment == FIXED_SEGMENT::MODE) {
				if (m_phase_steps == PHASE_STEPS::PI_HALF) 
					mode_response_conj = std::complex<f32>{I(2)-I(1), -(I(0)-I(1))};			
				else if (m_phase_steps == PHASE_STEPS::PI_QUARTER) {
					auto a = I(0) - I(1);
					auto b = I(2) - I(1);
					mode_response_conj = std::complex<f32>{-a-(1+sqrtf(2))*b, -((1 + sqrtf(2))*a+b)};
				}
				//mode_response_conj = std::complex<f32>{I(0)-I(2), -(I(0)+I(2)-2*I(1))};
				//mode_response_conj = std::complex<f32>{I(2)-I(1), I(0)-I(1)};
			}
			else {
				if( m_phase_steps == PHASE_STEPS::PI_HALF)
					mode_response_conj = std::complex<f32>{I(2)-I(1), (I(0)-I(1))};
				else if (m_phase_steps == PHASE_STEPS::PI_QUARTER) {
					auto a = I(0) - I(1);
					auto b = I(2) - I(1);
					mode_response_conj = std::complex<f32>{-a-(1+sqrtf(2))*b, +((1+sqrtf(2))*a+b)};
				}
			}

			// Find the global mode index.
			auto mode_global_index = kModesPerBufferTM * buffer_index + mode_index;

			// Multiply the mode by the conjugate response in order to align the phase.
			private_cartesian_pattern = m_input_modes_matrix.col(mode_global_index) * (mode_response_conj/std::abs(mode_response_conj));

			// Add up the aligned modes.
			#pragma omp critical  
			{
				m_final_cartesian_pattern += private_cartesian_pattern;
			}
		}
	}

	// Increase the buffer index, which is used in order to compute the index of the mode to be processed.
	buffer_index = (buffer_index + 1) % m_buffer_count_per_cycle;

	// When the buffer_index returns to zero, we finished processing all the modes.
	// Compute element wise phase in the range [-PI, PI] and load to the GLV.
	if (buffer_index == 0) {
		m_final_phase_column.block(m_mode_start_pixel, 0, m_pixels_per_mode, 1) = m_final_cartesian_pattern.imag().binaryExpr(m_final_cartesian_pattern.real(), std::ptr_fun<f32, f32, f32>(atan2f)).array();
		m_final_cartesian_pattern.fill(0);
		m_glv->load_and_resume_cycle(convert_phase_to_glv_dac_column(m_final_phase_column));
		++m_cycle_count;
		
		// Report results.
		if (m_cycle_count == kTestTrials) {
			spdlog::info("APP: Average cycle time is %f usec", m_hpc.stop().get_time_in_usec() / m_cycle_count);
			m_hpc.start();
			m_cycle_count = 0;
		}
	}
}


void App::on_buffer_receive_run_iterative_optimization(u16* const data_ptr, const size_t data_len, const u64 data_index) {
#ifdef DAQ_WORKAROUND
#ifndef GLV_PROCESSING_EMULATION
	static int buffer_index = 0;
	if (m_daq_workaround_first_buffer_flag) {
		buffer_index = 0;
		m_daq_workaround_first_buffer_flag = false;
		return;
	}

	// Get the index of the buffer, discard first.
	//int buffer_index = (data_index - 1) % m_buffer_count_per_cycle;
#else
	// For the emulation, we should not discard the first buffer.
	int buffer_index = data_index % m_buffer_count_per_cycle;
#endif
#endif

	// Each thread takes care of exactly one input mode (the buffer contains multiple modes).
	#pragma omp parallel
	{
		Eigen::MatrixXcf private_cartesian_pattern = Eigen::VectorXcf::Zero(m_pixels_per_mode, 1);
		#pragma omp for nowait
		for (auto mode_index = 0; mode_index < kModesPerBufferIterative; mode_index++) {
			// Convert a buffer segment to a Column-major storage eigen matrix, where each column corresponds to a record (different interference pattern).
			auto start_sample_index = mode_index * kDAQSamplesPerRecord * kIterativePhaseStepsPerMode;
			auto mode_intensities_per_phase_addition = Eigen::Map<ModeMatrixIterative>(data_ptr + start_sample_index);
		
			// Find the mean of each record (i.e. added phase) and store in a row vector with kIterativePhaseStepsPerMode entries.
			//auto mode_avg_intensity_per_phase_addition = mode_intensities_per_phase_addition.cast<f32>().colwise().mean();
			auto mode_avg_intensity_per_phase_addition = mode_intensities_per_phase_addition.cast<f32>().block(200, 0, 50, kIterativePhaseStepsPerMode).colwise().mean();

			// Find the index of the phase step which gives maximum response.
			ModeMatrixIterative::Index max_index;
			mode_avg_intensity_per_phase_addition.maxCoeff(&max_index);

			// Find the global mode index.
			auto mode_global_index = kModesPerBufferIterative * buffer_index + mode_index;

			// Add the phase (in cartesian) that corresponds to the highest response.
			private_cartesian_pattern = m_input_modes_matrix_adjusted.col(mode_global_index).array() * m_iterative_phase_step_in_cartesian[max_index];

			// Add up the aligned modes.
			#pragma omp critical  
			{
				m_final_cartesian_pattern += private_cartesian_pattern;
			}
		}
	}

	// Increase the buffer index, which is used in order to compute the index of the mode to be processed.
	buffer_index = (buffer_index + 1) % m_buffer_count_per_cycle;

	// When the buffer_index returns to zero, we finished processing all the modes.
	// Compute element wise phase in the range [-PI, PI] and load to the GLV.
	if (buffer_index == 0) {
		m_final_phase_column.block(m_mode_start_pixel, 0, m_pixels_per_mode, 1) = m_final_cartesian_pattern.imag().binaryExpr(m_final_cartesian_pattern.real(), std::ptr_fun<f32, f32, f32>(atan2f)).array();
		m_final_cartesian_pattern.fill(0);
		m_glv->load_and_resume_cycle(convert_phase_to_glv_dac_column(m_final_phase_column));
		++m_cycle_count;
		
		// Report results.
		if (m_cycle_count == kTestTrials) {
			spdlog::info("APP: Average cycle time is %f", m_hpc.stop().get_time_in_usec() / m_cycle_count);
			m_hpc.start();
			m_cycle_count = 0;
		}
	}
}


void App::on_daq_timeout() {
}


GLVColVectorXs App::convert_phase_to_glv_dac_column(const Eigen::VectorXf& phase_column) {
	auto dac_column = GLVColVectorXs{kGLVPixels, 1};

	// Atan2 phase is in the range [-PI, PI]. It has two discontinuities which we need to resolve to get an integer index.
	// This is a description of how it is done:
	// We multiply by the m_phase_index_coeff to get a number in the range [-m_phase_to_dac_size/2 : m_phase_to_dac_size/2]
	// and add m_phase_to_dac_size if the number is negative.
	// For example: 
	// m_phase_to_dac_size = 100  -> m_phase_index_coeff = 100/2PI
	// If we have a pixel with phase 3PI/4, the resulting index will be (u16)(3PI/4 * 100/2PI) = 37
	// If we have a pixel with phase PI, the resulting index will be (u16)(PI * 100/2PI) = 50
	// If we have a pixel with phase -PI (discontinuity), the resulting index will be (u16)(-PI * 100/2PI + 100) = 50
	// If we have a pixel with phase -PI/100, the resulting index will be (u16)(-PI/100 * 100/2PI + 100) = 99
	auto phase_column_to_dac_transformed = phase_column.array() * m_phase_index_coeff;
	for (auto dac_row_index = 0; dac_row_index < kGLVPixels; ++dac_row_index) {
		auto m_phase_to_dactransformed = phase_column_to_dac_transformed(dac_row_index);
		u16 dac_value_index;
		if (m_phase_to_dactransformed < 0) {
			dac_value_index = static_cast<u16>(m_phase_to_dactransformed + m_phase_to_dac_size);
		}
		else {
			dac_value_index = static_cast<u16>(m_phase_to_dactransformed);
		}
		dac_column(dac_row_index) = m_phase_to_dac[dac_value_index];
	}
	return dac_column;
}


GLVFrameXs App::convert_phase_to_glv_dac_column(const Eigen::MatrixXf& phase_frame) {
	auto dac_frame = GLVFrameXs{kGLVPixels, phase_frame.cols()};
	for (auto col_index = 0; col_index < phase_frame.cols(); ++col_index) {
		Eigen::VectorXf phase_column = phase_frame.col(col_index);
		dac_frame.col(col_index) = convert_phase_to_glv_dac_column(phase_column);
	}
	return dac_frame;
}


GLVFrameXs App::create_voltage_gratings() {
	GLVFrameXs dac_frame = GLVFrameXs::Zero(kGLVPixels, kGLVDACLevels);
	for (auto col_index = 0; col_index < dac_frame.cols(); ++col_index) {
		auto half_period_cnt = 0;
		bool write = false;
		for (auto row_index = 0; row_index < dac_frame.rows(); ++row_index) {
			// Every half period enable the write flag.
			if (half_period_cnt == m_glv_mode_pixel_ratio) {
				half_period_cnt = 0;
				write = !write;
			}

			// For half a period, write is enabled, so we write the column index.
			if (write) {
				dac_frame(row_index, col_index) = col_index;
			}

			// Increase the half period counter.
			half_period_cnt++;
		}
	}

	return dac_frame;
}


void App::create_input_modes() {
	// First step, create the input mode matrix with a 1:1 to ratio (each basis pixel is equivalent to an input mode pixel.)
	auto m_input_modes_matrixratio_1to1_matrix = Eigen::MatrixXcf{m_input_modes, m_input_modes};

	// Hadamard basis.
	// Online implementation from: https://stackoverflow.com/questions/33080313/the-below-code-generates-the-32x32-hadamard-matrix-but-how-do-i-write-it-recursi
	if (m_input_mode_basis == INPUT_MODE_BASIS::HADAMARD) {
		m_input_modes_matrixratio_1to1_matrix(0, 0) = std::complex<f32>{1.0, 0};
		auto m = 1;
		while (m < m_input_modes) {
			for (auto ii = 0; ii < m; ++ii) {
				for (auto jj = 0; jj < m; ++jj) {
					m_input_modes_matrixratio_1to1_matrix(ii + m, jj) = m_input_modes_matrixratio_1to1_matrix(ii, jj);
					m_input_modes_matrixratio_1to1_matrix(ii, jj + m) = m_input_modes_matrixratio_1to1_matrix(ii, jj);
					m_input_modes_matrixratio_1to1_matrix(ii + m, jj + m) = -m_input_modes_matrixratio_1to1_matrix(ii, jj);
				}
			}
			m = m * 2;
		}
	}
	
	// Fourier basis.
	// Implementation from wikipedia. Each DFT matrix column gives two basis elements
	// Real part corresponds to a cosine wave.
	// Imaginery part corresponds to a sine wave.
	// Convert to cartesian complex representation using by cos(x) + jsin(x)
	if (m_input_mode_basis == INPUT_MODE_BASIS::FOURIER) {
		auto dftmtx = Eigen::MatrixXcf{m_input_modes, m_input_modes};
		auto N = static_cast<f32>(m_input_modes);
		auto w = std::polar(1.0f, -TWOPI_F32 / N);
		for (auto col_index = 0; col_index < (m_input_modes/2); ++col_index) {
			for (auto row_index = 0; row_index < m_input_modes; ++row_index) {
				dftmtx(row_index, col_index) = PI_F32 * pow(w, col_index * row_index);
				auto cos_cartesian_real = cosf(dftmtx(row_index, col_index).real());
				auto cos_cartesian_imag = sinf(dftmtx(row_index, col_index).real());
				auto sin_cartesian_real = cosf(dftmtx(row_index, col_index).imag());
				auto sin_cartesian_imag = sinf(dftmtx(row_index, col_index).imag());
				m_input_modes_matrixratio_1to1_matrix(row_index, 2*col_index) = std::complex<f32>{cos_cartesian_real, cos_cartesian_imag};
				m_input_modes_matrixratio_1to1_matrix(row_index, 2*col_index + 1) = std::complex<f32>{sin_cartesian_real, sin_cartesian_imag};
			}

			// Debugging with array plotter, place break point on auto plot_buffer_f32 = ...
			std::vector<f32> plot_vector(m_input_modes);
			Eigen::VectorXf::Map(plot_vector.data(), m_input_modes) = dftmtx.col(col_index).real();
			auto plot_buffer_f32 = plot_vector.data();
		} 
	}

	// Second step, according to the glv to mode pixel ratio, expand the pixels.
	m_input_modes_matrix = Eigen::MatrixXcf::Zero(m_pixels_per_mode, m_input_modes);
	m_input_modes_matrix_adjusted = Eigen::MatrixXcf::Zero(m_pixels_per_mode, m_input_modes);
	for (auto col_index = 0; col_index < m_input_modes; ++col_index) {
		for (auto row_index = 0; row_index < m_input_modes; ++row_index) {
			for (auto expand_index = 0; expand_index < m_glv_mode_pixel_ratio; ++expand_index) {
				m_input_modes_matrix(m_glv_mode_pixel_ratio * row_index + expand_index, col_index) = m_input_modes_matrixratio_1to1_matrix(row_index, col_index);
			}
		}
	}

	// Create a copy of the input modes matrix which is used by the iterative optimization.
	m_input_modes_matrix_adjusted = m_input_modes_matrix;
}


Eigen::MatrixXf App::create_preloaded_phase_columns_for_tm_optimization(const bool dump_to_file) {
	// The nomenclature scheme for adding the reference is:
	// reference "top"
	// mode
	// reference "bottom"	
	
	// Iterate and create the reference + modes matrix.
	auto ref_modes_cartesian_matrix = Eigen::MatrixXcf{kGLVPixels, m_input_modes * kTMInterferencePatternsPerMode};
	for (auto input_mode_index = 0; input_mode_index < m_input_modes_matrix.cols(); ++input_mode_index) {
		for (auto add_phase_index = 0; add_phase_index < kTMInterferencePatternsPerMode; ++add_phase_index) {
			auto ref_modes_col_index = kTMInterferencePatternsPerMode * input_mode_index + add_phase_index;

			// For the case of fixed reference, fill the entire column (top reference + mode + bottom reference) according to the 
			// phase of the reference. 
			if ((m_fixed_segment == FIXED_SEGMENT::REFERENCE_AT_ZERO) || (m_fixed_segment == FIXED_SEGMENT::REFERENCE_AT_PI)) {
				if (m_fixed_segment == FIXED_SEGMENT::REFERENCE_AT_PI) {
					ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{-1, 0});
				}
				else if (m_fixed_segment == FIXED_SEGMENT::REFERENCE_AT_ZERO) {
					ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{1, 0});
				}

				if (m_phase_steps == PHASE_STEPS::PI_HALF) {
					// Overwrite with the input mode in the middle, shifted by an additional phase.
					switch (add_phase_index) {
					case 0:  // Phase 0
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							m_input_modes_matrix.col(input_mode_index).array() * std::complex<f32>{1, 0};
						break;
					case 1:  // Phase PI/2
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							m_input_modes_matrix.col(input_mode_index).array() * std::complex<f32>{0, 1};
						break;
					case 2:  // Phase PI
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							m_input_modes_matrix.col(input_mode_index).array() * std::complex<f32>{-1, 0};
						break;
					default:
						assert(0);
						break;
					}
				}
				if (m_phase_steps == PHASE_STEPS::PI_QUARTER) {
					// Overwrite with the input mode in the middle, shifted by an additional phase.
					switch (add_phase_index) {
					case 0:  // Phase 0
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							(m_input_modes_matrix.col(input_mode_index).array() + std::complex<f32>{1, -2})  * std::complex<f32>{0, 1};
						break;
					case 1:  // Phase PI/4
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							(m_input_modes_matrix.col(input_mode_index).array() + std::complex<f32>{1, -2})  * std::complex<f32>{-1, 1};
						break;
					case 2:  // Phase PI/2
						ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =
							(m_input_modes_matrix.col(input_mode_index).array() + std::complex<f32>{1, -2})  * std::complex<f32>{-1, 0};
						break;
					default:
						assert(0);
						break;
					}
				}
			}

			// For the case of fixed mode, fill the entire column (top reference + mode + bottom reference) according to the 
			// phase of the reference. 
			if (m_fixed_segment == FIXED_SEGMENT::MODE) {
				switch (add_phase_index) {
				case 0:  // First phase step.
					ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{1, 0});
					break;
				case 1:  // Second phase step.
					if ( m_phase_steps == PHASE_STEPS::PI_HALF )
						ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{0, 1});
					if ( m_phase_steps == PHASE_STEPS::PI_QUARTER )
						ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{1, 1});
					break;
				case 2:  // Third phase step.
					if (m_phase_steps == PHASE_STEPS::PI_HALF )
						ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{-1, 0});
					if (m_phase_steps == PHASE_STEPS::PI_QUARTER)
						ref_modes_cartesian_matrix.col(ref_modes_col_index).fill(std::complex<f32>{0, 1});
					break;
				default:
					assert(0);
					break;
				}
				ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) =	m_input_modes_matrix.col(input_mode_index);
			}

		}
	}

	// Get the phase of the matrix in the range [-PI, PI].
	Eigen::MatrixXf ref_modes_phase_matrix = ref_modes_cartesian_matrix.imag().binaryExpr(ref_modes_cartesian_matrix.real(), std::ptr_fun<f32, f32, f32>(atan2f)).array();

	// Dump to file.
	if (dump_to_file) {
		spdlog::info("APP: Dumping preloaded columns to file\n");
		std::ofstream file{"preloaded_columns.txt"};
		Eigen::IOFormat clean_format(4, 0, ", ", "\n", "[", "]");
		file << ref_modes_phase_matrix.format(clean_format);
		file.close();
	}

	return ref_modes_phase_matrix;
}


Eigen::MatrixXf App::create_preloaded_phase_columns_for_iterative_optimization(const bool use_prev_solution, const bool dump_to_file) {
	// The nomenclature scheme for adding the reference is:
	// reference "top"
	// mode
	// reference "bottom"	
	// Note that here, the reference has no meaning, pixels that are not part of the mode, are fixed phase 0.
	
	// First, if required, adjust the phase of the input modes by the previous solution, otherwise the adjusted copy is the same as the original.
	// The adjusted input modes are used during the algorithm process.
	if (use_prev_solution) {
		auto m_final_cartesian_patternlocal = Eigen::VectorXcf{m_input_modes_matrix_adjusted.rows(), 1};

		// We don't have the solution in cartesian, only in polar, so transform to cartesian first.
		m_final_cartesian_patternlocal.real() = m_final_phase_column.block(m_mode_start_pixel, 0, m_pixels_per_mode, 1).array().cos();
		m_final_cartesian_patternlocal.imag() = m_final_phase_column.block(m_mode_start_pixel, 0, m_pixels_per_mode, 1).array().sin();
		auto adjustment = m_input_modes_matrix_adjusted.array().colwise() * m_final_cartesian_patternlocal.array();
		m_input_modes_matrix_adjusted = adjustment;
	}

	// Find the phase step size and allocate a cartesian phase step LUT.
	auto iterative_phase_step = TWOPI_F32 / static_cast<f32>(kIterativePhaseStepsPerMode);
	m_iterative_phase_step_in_cartesian = Eigen::VectorXcf{kIterativePhaseStepsPerMode, 1};

	// Iterate and create the reference + modes matrix (still in cartesian coordinates).
	auto ref_modes_cartesian_matrix = Eigen::MatrixXcf{kGLVPixels, m_input_modes * kIterativePhaseStepsPerMode};
	for (auto input_mode_index = 0; input_mode_index < m_input_modes_matrix.cols(); ++input_mode_index) {
		for (auto added_phase_index = 0; added_phase_index < kIterativePhaseStepsPerMode; ++added_phase_index) {
			auto ref_modes_col_index = kIterativePhaseStepsPerMode * input_mode_index + added_phase_index;
			auto added_phase = added_phase_index * iterative_phase_step;
			m_iterative_phase_step_in_cartesian[added_phase_index] = std::complex<f32>{cosf(added_phase), sinf(added_phase)};
			ref_modes_cartesian_matrix.col(ref_modes_col_index) = m_final_phase_column;
			ref_modes_cartesian_matrix.block(m_mode_start_pixel, ref_modes_col_index, m_pixels_per_mode, 1) = m_input_modes_matrix_adjusted.col(input_mode_index).array() * m_iterative_phase_step_in_cartesian[added_phase_index];
		}
	}

	// Get the phase of the matrix in the range [-PI, PI].
	Eigen::MatrixXf ref_modes_phase_matrix = ref_modes_cartesian_matrix.imag().binaryExpr(ref_modes_cartesian_matrix.real(), std::ptr_fun<f32, f32, f32>(atan2f)).array();

	// Dump to file.
	if (dump_to_file) {
		spdlog::info("APP: Dumping preloaded columns to file\n");
		std::ofstream file{"preloaded_columns.txt"};
		Eigen::IOFormat clean_format(4, 0, ", ", "\n", "[", "]");
		file << ref_modes_phase_matrix.format(clean_format);
		file.close();
	}

	return ref_modes_phase_matrix;
}


void App::print_configuration(const OPTIMIZATION_ALGORITHM algorithm) {
	spdlog::info("APP: \"GLV pixel to mode pixel\" ratio is %d:1", m_glv_mode_pixel_ratio);
	spdlog::info("APP: Using %d input modes", m_input_modes);
	std::string m_input_mode_basisstring;
	if (m_input_mode_basis == INPUT_MODE_BASIS::HADAMARD) {
		m_input_mode_basisstring = "HADAMARD";
	}
	else {
		m_input_mode_basisstring = "FOURIER";
	}
	spdlog::info("APP: Input mode basis is %s", m_input_mode_basisstring);
	if (algorithm == OPTIMIZATION_ALGORITHM::TM) {
		switch (m_fixed_segment) {
		case FIXED_SEGMENT::REFERENCE_AT_ZERO:
			spdlog::info("APP: Reference is fixed to phase 0 (mode changes)");
			break;
		case FIXED_SEGMENT::REFERENCE_AT_PI:
			spdlog::info("APP: Reference is fixed to phase PI (mode changes)");
			break;
		case FIXED_SEGMENT::MODE:
			spdlog::info("APP: Mode is fixed (reference changes)");
			std::string reference_during_focus_string;
			if (m_final_phase_column(0) == PI_F32) {
				reference_during_focus_string = "is a high frequency grating";
			}
			else {
				reference_during_focus_string = "has a phase of 0";
			}
			spdlog::info("APP: Reference on focusing pattern %s", reference_during_focus_string);
			break;
		}
	}
	if (algorithm == OPTIMIZATION_ALGORITHM::ITERATIVE) {
		spdlog::info("APP: Using %d phase steps per mode", kIterativePhaseStepsPerMode);
	}
	spdlog::info("");
}