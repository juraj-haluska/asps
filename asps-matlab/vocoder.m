close all;

[input, Fs] = audioread('sax.wav');
input = input(:, 1)';

winSize = 1024;
hopSize = 512;
fftSize = 4096;

% harmony in semitones
harmony = [12];
gain = [0.8 0.8 0.8];

% frequency difference toleration
ftol = 20;

% number peaks used for synthesis (the N-most prominent peaks are selected)
peaksCount = 50;

%% Split signal into frames
framesCount = floor((length(input) - hopSize) / (winSize - hopSize));
frames = zeros(framesCount, winSize);
for fi = 1:framesCount
    frameStart = (fi - 1) * winSize + 1 - (fi - 1) * hopSize;
    frameEnd = frameStart + winSize - 1;
    frames(fi, :) = input(frameStart:frameEnd);
end

synth = zeros(1, length(input));

prevFreqs = zeros(1, peaksCount);
prevPhases = zeros(length(harmony), peaksCount);

%% Process frames
for fi = 1:framesCount
    wb = waitbar(fi/framesCount);
    
    frame = frames(fi, :);
    frame = frame .* hanning(winSize)';
    
    ft = 2/winSize*fft(frame, fftSize);
    
    % filter noise and low peaks from spectrum
    [pks, ~] = findpeaks(abs(ft(1:fftSize / 2 + 1)), 'SortStr', 'Descend');
    if length(pks) <= peaksCount
        continue;
    end
    th = min(pks(1:peaksCount));
    for fqI=1:fftSize
        if abs(ft(fqI)) < th
            ft(fqI) = 0;
        end
    end
    
    % isolate the most significant peaks
    [~, locs] = findpeaks(abs(ft(1:fftSize / 2 + 1)), 'SortStr', 'Descend');

    freqs = (locs - 1) * Fs/fftSize;
    mags = abs(ft(locs));
    phases = angle(ft(locs));
        
    % synthesis
    slen = winSize;
    
    buff = zeros(1, slen);
    t = linspace(0, slen/Fs - 1/Fs, slen);
    
    pps = zeros(length(harmony), peaksCount);
    fqs = zeros(1, peaksCount);
    
    for s=1:slen               
       for fqI=1:length(freqs)  
           for hI=1:length(harmony)
               f = freqs(fqI);
               m = mags(fqI);
               p = phases(fqI);

               pp = 0;

               % find if this frequency was present in previous frame
               for pfqI=1:peaksCount
                   if prevFreqs(pfqI) <= f + ftol && prevFreqs(pfqI) >= f - ftol
                       pp = prevPhases(hI, pfqI);
                       break; 
                   end
               end

               tf = 2^(harmony(hI)/12);
               
               buff(s) = buff(s) + gain(hI) * m * cos(2*pi*f*tf*t(s) + p + pp);

               pp = pp + (tf-1)*hopSize*2*pi*f/Fs;
               pps(hI, fqI) = pp;
               fqs(fqI) = f;
           end
       end
    end
    
    prevFreqs = fqs;
    prevPhases = pps;
    
    buff = buff .* hanning(winSize)';
    
    frameStart = (fi - 1) * winSize + 1 - (fi - 1) * hopSize;
    frameEnd = frameStart + winSize - 1;
        
    synth(frameStart:frameEnd) = synth(frameStart:frameEnd) + buff;
end

close(wb);

audiowrite('out.wav', synth, Fs);