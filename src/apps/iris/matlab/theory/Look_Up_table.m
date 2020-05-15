clear all ; clc ; close all;
% This script calculats the GLV lookup table 
% Define the phase interval between 0-2pi
phase_intervals_vector = (0:100)*2*pi/100;
% Simulate the GLV and calculate the theoretical difraction efficiency
[ Order0 , Order1] = Calc_diffraction_from_GLV (phase_intervals_vector);
% nomelize curve
Order1_Norm = (Order1 - min(Order1)) / max(Order1 - min(Order1));
% plot
figure ; 
plot(phase_intervals_vector , Order1_Norm ,'r','linewidth' ,3);
title('difraction efficiency normelized'); xlabel('\phi'); ylabel('Amp');
hold on;
%% import experimental curve 
% here I select the zero order inverted and normelized and span equaly the
% GLV voltages
Experimental_curve = Order0;
Experimental_curve = abs((Experimental_curve - min(Experimental_curve)) / max(Experimental_curve - min(Experimental_curve))-1);
Experimental_curve_norm = (Experimental_curve - min(Experimental_curve)) / max(Experimental_curve - min(Experimental_curve));
voltages_vector = linspace (0 , 1023 , length(Order0));

plot(phase_intervals_vector , Experimental_curve_norm ,'k','linewidth' ,3);
legend ('Simulated first order', 'experimental first order');

%% Look up
% define the interval in the lookup
delta = 0.05; % determined experimentally
Experimental_pi_index = find (Experimental_curve_norm == max(Experimental_curve_norm));
% Voltage_index = zeros (length (phase_intervals_vector) , 20);
for i = 1 : length (phase_intervals_vector)
    if (phase_intervals_vector(i) <pi) % going up
    Voltage_index =  find(Experimental_curve_norm(1:Experimental_pi_index) > (Order1_Norm (i)-delta) & Experimental_curve_norm(1:Experimental_pi_index) < (Order1_Norm (i)+delta) )
    else %going down
    Voltage_index = Experimental_pi_index + find(Experimental_curve_norm(Experimental_pi_index+1:end) > (Order1_Norm (i)-delta) & Experimental_curve_norm(Experimental_pi_index+1:end) < (Order1_Norm (i)+delta) )
    end
    % now we select the index from the range
    Voltage_index_archive(i) = floor(mean(Voltage_index));
    % now we select the coresponding GLV voltage to the index
    Voltage_archive(i) = voltages_vector (Voltage_index_archive(i));
end

figure()
plotyy(phase_intervals_vector , Voltage_index_archive, phase_intervals_vector , Voltage_archive );
title('phase index mapping'); xlabel('\phi'); ylabel('index  0-100');

%% Creating the array for high speed
Phase_index_Array = floor (phase_intervals_vector*100)+1;
for i = 1 : length (phase_intervals_vector)
    Voltage_Array (Phase_index_Array(i)) = Voltage_archive(i);
end
figure()
bar(Voltage_Array ,'LineWidth',3 );
title('Vector for high speed mapping'); xlabel('index that corespond to \phi *100'); ylabel('GLV voltage');

