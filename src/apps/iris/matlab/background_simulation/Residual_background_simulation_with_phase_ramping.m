clear all; clc; %close all;
load('voltage array.mat');
Element_size = 3.75e-6 ; %m
GLV_grid = 0 : Element_size/10 : 2*Element_size*1088 ;
GLV_periodic_grid = pi*GLV_grid / Element_size;
Grid = GLV_periodic_grid;

% Grid = -100*pi() :0.1: 100*pi(); 
Steps = 30;
for ii =  0: Steps
    
Amp = ones (size(Grid)); % uniform amplitude
Phase_Gap = pi/1.8;
phi = ii*2*pi/Steps + Phase_Gap;%pi();
phi_archive(ii+1) = phi;

phase = (1+sign(sin(Grid)+0.9))/2*phi; %  phase of GLV
Modulation_index = find(sign(sin(Grid)+0.9)==-1);


phase(Modulation_index) = 0;
    

% H_phase(Modulation_index) = 0;
% if (ii == 0)
    figure(99)
    plot(GLV_grid*10^6/2, phase,'b','linewidth' ,2);
    hold on
%     plot(Grid, phase ,'r', 'linewidth' ,3);
    xlabel ('Grid [\mum]'); ylabel('phase [rad]');
    title('phase grating');
%     xlim([0,100]);
    drawnow
% end
% defining the transmision from teh grating

 t_a = Amp.* exp(i*(phase));
% t_a = Amp.* exp(i*(phase+H_phase));
y = fftshift(fft(t_a));
MAG = abs(y);
PHASE = angle(y); 
L = length (y);
%displaying only the possitive, single side FFT, (instead of FFT shift);
m = MAG;%(1:L/2+1);
p = PHASE;%(1:L/2+1);
% sampling time: 
T = (Grid(end) - Grid(1));
% calibration the frequencies axis: 
Fs = (-1*length(m)/2: length(m)/2-1)* 1/T; % in Hertz
f = Fs; %1/m
%% plot results
if 1
    figure(2);
    ha(1) = subplot(2,1,1);
    plot(f,m) ;
    title('Amplitude Spectrum'); xlabel('f (1/m)'); ylabel('|P1(f)|');
    ha(2) =subplot(2,1,2);
    plot(f,p);
    title('Phase');xlabel('f (1/m)');ylabel('\phi[RAD]');
    linkaxes(ha, 'x'); % Link all axes in x 
end
% calculating difraction efficency
Order0(ii+1) = max(MAG(1.084e4:1.09e4))^2;
Order1(ii+1) = max(MAG(1.19e4:1.2e4)).^2;

end
figure()
plot(phi_archive , Order0 , 'b' , 'LineWidth',3);
hold on
plot(phi_archive , Order1 , 'r' , 'LineWidth',3);
title(['difraction efficiency \phi Hadamart = ',num2str(phi)]); xlabel('\phi'); ylabel('Amp');
legend('Zero order', 'First order');

% phi_archive(phi_archive > 2*pi) = phi_archive(phi_archive > 2*pi) - 2*pi;
Phase_for_calibration = round(phi_archive*100);
Phase_for_calibration(find(Phase_for_calibration==0))=1;
% wrapping : 
Phase_for_calibration(find(Phase_for_calibration>628)) = Phase_for_calibration(find(Phase_for_calibration>628))-628;
for i = 1:length(Phase_for_calibration)
    GLV_voltage(i) = Voltage_Array(Phase_for_calibration(i));
end

[~, wrap_index] = max(GLV_voltage);
Order0_modified = [Order0(wrap_index+1:end) Order0(1:wrap_index)];
Order1_modified = [Order1(wrap_index+1:end) Order1(1:wrap_index)];
figure()
plot(GLV_voltage , Order0_modified , '*b' , 'LineWidth',3);
hold on
plot(GLV_voltage , Order1_modified , '*r' , 'LineWidth',3);
title(['difraction efficiency \phi Hadamart = ',num2str(phi)]); xlabel('GLV voltage'); ylabel('Amp');
legend('Zero order', 'First order');
