%% User parameters
clear all;
close all;
filename = 'scope_22_1.csv';
index_start = 670;
index_end = 1164;

%% Oscope file formatting.
first_numeric_row = 2;
time_col = 1;
signal_col = 2;

% Get the relevant time series and find the period T and sampling frequency Fs.
time_series_original = csvread(filename, first_numeric_row);
time_series = time_series_original(index_start:index_end, :);
T = time_series(2, time_col) - time_series(1, time_col);
Fs = 1/T;
signal = time_series(:, signal_col);
time = time_series(:, time_col);

%% Compute PSD. Signal is real valued. We only need half of the specturm, and 
% to conserve the energy, need to multiply all data points, but the DC by 2.
window = hamming(length(signal));
%window = rectwin(length(signal))
[psd, freq] = periodogram(signal, window, length(signal), Fs);

% Manual PSD computation.
% N = length(signal);
% spectrum = fft(signal);
% spectrum = spectrum(1:floor(N/2)+1);
% psd = (1/(Fs*N)) * abs(spectrum).^2;
% psd(2:end-1) = 2*psd(2:end-1);
% freq = 0:Fs/length(signal):Fs/2;

%% Display results.
h.mainfig = figure();
h.tabgroup = uitabgroup(h.mainfig);
ntabs = 3;
for ii = 1:ntabs
    h.tab(ii) = uitab(h.tabgroup, 'Title', sprintf('Tab_%i', ii));
end

% Original time series.
h.axes(1) = axes('Parent', h.tab(1));
h.tab(1).Title = 'Original';
h.axes(1).Units = 'normalized';
h.axes(1).Position = [0 0 1 1];
plot(h.axes(1), time_series_original(:, signal_col));
original_length = length(time_series_original(:,1));
dim = [index_start/original_length, 0.01, (index_end - index_start)/original_length, 0.95];
annotation(h.tab(1), 'rectangle', dim);
grid on;

% Clipped time series.
h.axes(2) = axes('Parent', h.tab(2));
h.tab(2).Title = 'Time series clipped';
plot(h.axes(2), time, signal);
grid on;
title(h.tab(2).Title);
xlabel('t(s)');
ylabel('Voltage(V)');

% PSD.
h.axes(3) = axes('Parent', h.tab(3));
h.tab(3).Title = 'PSD';
plot(h.axes(3), freq, 10*log10(psd));
grid on;
title(h.tab(3).Title);
xlabel('Frequency(Hz)');
ylabel('Power/Frequency(dB/Hz)');

% Save.
savefig(h.mainfig, 'vibration_analysis.fig');