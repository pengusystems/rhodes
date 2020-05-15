clear all; clc; close all;
% This script calculats the GLV lookup table 

% hold on;
%% import experimental curve 
% here I select the zero order inverted and normelized and span equaly the
% GLV voltages
load('Calib_data_460nm_2.mat');
% Voltage = 0:(length(Calib_data_1)-1);
Experimental_curve = Calib_data_460nm_2;
%smothing the curves

Experimental_curve = moving(Experimental_curve,10);
Experimental_curve(990:end) = Calib_data_460nm_2(990:end);
Experimental_curve = moving(Experimental_curve,2);

%Normelizing
Experimental_curve = (Experimental_curve - min(Experimental_curve)) / max(Experimental_curve - min(Experimental_curve));

Start_index = find(Experimental_curve == max(Experimental_curve));
Start_index = 270;
End_index = length(Experimental_curve);
Experimental_curve_norm = Experimental_curve(Start_index:End_index);

voltages_vector = linspace (0 , 1023 , length(Calib_data_460nm_2));
voltages_vector = voltages_vector(Start_index:End_index);

figure(99);
plot(voltages_vector , Experimental_curve_norm ,'k','linewidth' ,3);
hold on;

L = length (Experimental_curve_norm);
% Define the phase interval between 0-2pi ( its length is 2 p
phase_intervals_vector = (0:L-1)*2*pi/L;
% Simulate the GLV and calculate the theoretical difraction efficiency
[Order0 , Order1] = Calc_diffraction_from_GLV (phase_intervals_vector);
% nomelize curve
Order0_Norm = (Order0 - min(Order0)) / max(Order0 - min(Order0)); 
figure (99)
plot(voltages_vector , Order0_Norm ,'r','linewidth' ,3);
title('diffraction efficiency normelized'); xlabel('\phi'); ylabel('Amp');
legend ('experimental Zero order','Simulated Zero order');
hold off;
%% Look up
% define the interval in the lookup

Experimental_pi_index = find (Experimental_curve_norm == min(Experimental_curve_norm));
Simulation_pi_index = find (Order0_Norm == min(Order0_Norm));
% define the maximum value for modulation we can reach
End_look_up_value = max(Experimental_curve_norm(find(Experimental_curve_norm== max(Experimental_curve_norm (Experimental_pi_index+1:end)))))
% find which phase value does it corespond to based on simulations
End_look_up_index = Simulation_pi_index + find(Order0_Norm(Simulation_pi_index+1:end) > (End_look_up_value-delta) & Order0_Norm(Simulation_pi_index+1:end) < (End_look_up_value+delta) )
% if there is more than one , we just select the last
End_look_up_index = End_look_up_index(end);
% Voltage_index = zeros (length (phase_intervals_vector) , 20);
%% LOOK UP
delta = 0.002; % determined amperically
for i = 1 : length (phase_intervals_vector)
    if(i==90)
       delta = 0.0055; 
    end
    if (phase_intervals_vector(i) <pi) % going down in the search
    Voltage_index =  find(Experimental_curve_norm(1:Experimental_pi_index) > (Order0_Norm (i)-delta) & Experimental_curve_norm(1:Experimental_pi_index) < (Order0_Norm (i)+delta) )
    else %going up in the search
        if (i<=End_look_up_index)
        Voltage_index = Experimental_pi_index + find(Experimental_curve_norm(Experimental_pi_index+1:end) > (Order0_Norm (i)-delta) & Experimental_curve_norm(Experimental_pi_index+1:end) < (Order0_Norm (i)+delta) )
       % Now I take care of unmodulated range:
        elseif (i<=floor(End_look_up_index+(L-End_look_up_index)/2))
        Voltage_index = Voltage_index_archive(i-1);
        else
        Voltage_index = Voltage_index_archive(1);
        end
    end
    % now we select the index from the range
    Voltage_index_archive(i) = floor(mean(Voltage_index));
    % now we select the coresponding GLV voltage to the index
    Voltage_archive(i) = voltages_vector (Voltage_index_archive(i));
end

figure()
plotyy(phase_intervals_vector , Voltage_archive , phase_intervals_vector ,Voltage_index_archive );
title('phase index mapping'); xlabel('\phi'); ylabel('GLV digital voltage value');

%% Creating the array for high speed ( its length would be 2*pi*100)
Phase_index_Array = floor (phase_intervals_vector*100)+1;
for i = 1 : length (phase_intervals_vector)
    Voltage_Array (Phase_index_Array(i)) = Voltage_archive(i);
end
figure()
bar(Voltage_Array ,'LineWidth',3 );
title('Vector for high speed mapping'); xlabel('index that corespond to \phi *100'); ylabel('GLV voltage');

