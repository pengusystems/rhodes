function [ Order0 , Order1] = Calc_diffraction_from_GLV (phase_intervals_vector)

% phase_intervals_vector is an input vector from 0-2pi 

%% creating the GLV grating
Element_size = 4.25e-6 ; %m
GLV_grid = 0 : Element_size/10 : 2*Element_size*1088 ;
GLV_periodic_grid = pi*GLV_grid / Element_size;
Grid = GLV_periodic_grid;
% Grid = -100*pi() :0.1: 100*pi(); 

%% calculating the difraction efficency for 0-2pi phases
for ii =  1: length(phase_intervals_vector)
    
    Amp = ones (size(Grid)); % uniform amplitude
    phi = phase_intervals_vector(ii);
    phi_archive(ii) = phi;

    phase = (1+sign(sin(Grid)))/2*phi; %  phase of GLV
    Modulation_index = find(sign(sin(Grid))==-1);
    % The difraction element is defined as:
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
    figure(2);
    ha(1) = subplot(2,1,1);
    plot(f,m) ;
    title('Amplitude Spectrum'); xlabel('f (1/m)'); ylabel('|P1(f)|');
    ha(2) =subplot(2,1,2);
    plot(f,p);
    title('Phase');xlabel('f (1/m)');ylabel('\phi[RAD]');
    linkaxes(ha, 'x'); % Link all axes in x 

    % calculating difraction efficency
    Order0(ii) = max(MAG(1.084e4:1.09e4));
    Order1 (ii) = max(MAG(1.19e4:1.2e4));

end

% display the difraction efficienncy : 
hold off
figure()
plot(phi_archive , Order0 , 'b' , 'LineWidth',3);
hold on
plot(phi_archive , Order1 , 'r' , 'LineWidth',3);
title('difraction efficiency simulation'); xlabel('\phi'); ylabel('Amp');
legend('Zero order', 'First order');

end

