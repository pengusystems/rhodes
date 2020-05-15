% Parameters.
num_modes = 256;
%mode_basis = 'FOURIER';  % HADAMARD or FOURIER
mode_basis = 'HADAMARD';  % HADAMARD or FOURIER
num_intereference_per_mode = 3;
glv_pixels = 1088;
num_patterns = num_modes * num_intereference_per_mode;
phase_reconstructor = @(x) x(:,3)-x(:,2) - 1i*(x(:,1)-x(:,2));
synthesize_new = 0;

% Synthesis.
% Arbitrarily generate a number for each projected pattern
if (synthesize_new)
    intensity_response = randi([0, 2^16 - 1], num_patterns, 1);
    format long;
    save('synthesis.txt', 'intensity_response', '-ASCII');
else
    intensity_response = load('synthesis.txt');    
end

% Create the input modes.
if (strcmp(mode_basis, 'HADAMARD'))
    modes = hadamard(num_modes);
elseif (strcmp(mode_basis, 'FOURIER'))
    dft_mtx = pi * dftmtx(num_modes);
    modes_cos = cos(real(dft_mtx(:, 1:num_modes/2))) + 1i*sin(real(dft_mtx(:, 1:num_modes/2)));
    modes_sin = cos(imag(dft_mtx(:, 1:num_modes/2))) + 1i*sin(imag(dft_mtx(:, 1:num_modes/2)));
    modes = reshape([modes_cos ; modes_sin], size(modes_cos,1), []);    
else
    error('Unknown mode basis');
end

% Analyze response for each mode using the phase reconstruction equation.
intensity_response_reshpaed = reshape(intensity_response, num_intereference_per_mode, num_modes).';
mode_response_conj = phase_reconstructor(intensity_response_reshpaed).';
mode_response_conj = mode_response_conj ./ abs(mode_response_conj);

% Compute the final pattern in the range [-PI,PI] and then [0, 2PI].
mode_response_conj_rep = repmat(mode_response_conj, num_modes, 1);
modes_adjusted = mode_response_conj_rep .* modes;
final_cartesian_pattern = sum(modes_adjusted, 2);
final_phase_pattern = atan2(imag(final_cartesian_pattern), real(final_cartesian_pattern));
final_phase_0_to_2pi = wrapTo2Pi(final_phase_pattern);

% Convert to DAC values.
% The scheme adjusts the index such that phase '0' corresponds to index 1
% and phase 2pi corresponds to index 628.
lut = load('..\..\voltage array.txt');
lut_index_vector = floor(final_phase_0_to_2pi * 100);
lut_index_vector(lut_index_vector == floor(2*pi*100)) = length(lut) - 1;
final_dac_pattern = lut(lut_index_vector+1);
dac_column = ones(glv_pixels, 1) * lut(1);
start_pixel = (glv_pixels - num_modes) / 2 + 1;
dac_column(start_pixel:start_pixel + num_modes - 1) = final_dac_pattern;
plot(dac_column);
title('GLV final pattern');
xlabel('Pixel');
ylabel('DAC value');