close all;

% Implementation of harmonizer using 
% peak-translation method described by
% Jean Laroche and Mark Dolson in following paper:
% "NEW PHASE-VOCODER TECHNIQUES FOR PITCH-SHIFTING, HARMONIZING AND OTHER EXOTIC EFFECTS"
% http://www.ee.columbia.edu/~dpwe/papers/LaroD99-pvoc.pdf

[input, Fs] = audioread('sax.wav');
y = input(:, 1)';

% configurable parameters
winPeriod = 0.020;
hopRate = 50;
fftSize = 4096;
maxPeaks = 50;
fTol = 3;
harmony = [0 7 13];

winSize = floor(Fs * winPeriod);
hopSize = floor(winSize * hopRate / 100);

% 1. Split signal in frames
Y = [];
nextStart = 1;
stop = 0;
while stop ~= 1
    if nextStart + winSize - 1 <= length(y)
        frame = y(nextStart:nextStart + winSize - 1);
    else
        frame = y(nextStart:length(y));
        frame = [frame zeros(1, winSize - length(frame))];
        stop = 1;
    end
    nextStart = nextStart + hopSize;
    Y = [Y frame'];
end

% 2. Processing
O = zeros(size(Y, 1), size(Y, 2)); % output frames
prevPhases = ones(length(harmony), maxPeaks); % phase accumulator
prevBinFreqs = zeros(length(harmony), maxPeaks); % holds bin frequency
for fI = 1:size(Y, 2)
    % apply windowing function
    frame = Y(:, fI) .* hamming(winSize);
    
    % take fourier transform
    ft = fft(frame, fftSize);
    lHalf = ft(1:length(ft) / 2 + 1);
    
    % ---------------------------------------------------------------------
    
    % calculate magnitudes
    mags = abs(lHalf);
    [pks, locs] = findpeaks(mags, 'SortStr', 'Descend');
    
    % constrain max peaks
    if length(locs) > maxPeaks
        pks = pks(1:maxPeaks);
        locs = locs(1:maxPeaks);
    end
        
    newHalf = zeros(1, length(lHalf))';
    
    for hI=1:length(harmony)
 
        keep = [];
        keepBins = [];

        % loop each peak
        for pI=1:length(locs)
            % calculate bin transposition factor for this peak
            newBin = round((locs(pI) - 1) * 2^(harmony(hI)/12));
            t = newBin - (locs(pI) - 1);

            % try to find matching fWanted from previous frame
            [m, l] = min(abs(prevBinFreqs(hI, :) - newBin));
            if (m <= fTol && m >= -fTol)
                % match with prev frequency
                keep = [keep prevPhases(hI, l)];
                keepBins = [keepBins prevBinFreqs(hI, l)];
            else
                % not match
                keep = [keep 1];
                keepBins = [keepBins newBin];
            end

            % phase rotation
            dw = 2*pi*t/fftSize;
            keep(end) = keep(end)*exp(1j*dw*hopSize);
            
            % isolate region of influence
            [first, last] = findInfluenceRegion(locs(pI), mags, floor(length(lHalf) / 2));

            % translate peak
            if last + t <= length(newHalf) && first + t >= 1 
                region = lHalf(first:last);
                newHalf(first + t:last + t) = newHalf(first + t:last + t) + 1/length(harmony) * region * keep(end);
            end
        end

        % replace not matched phase continuations with new ones
        prevPhases(hI, :) = [keep ones(1, maxPeaks - length(keep))];
        prevBinFreqs(hI, :) = [keepBins ones(1, maxPeaks - length(keepBins))];        
    end
    
    % ---------------------------------------------------------------------
    % reconstruct signal
    reflected = [newHalf' conj(fliplr(newHalf(2:end-1)'))]';
    reconstructed = real(ifft(reflected));
    O(:, fI) = reconstructed(1:winSize);
end

synth = zeros(1, length(input) + winSize);
% overlap add
startIndex = 1;
for fi=1:size(O, 2)
    w = O(:, fi);
    synth(startIndex:startIndex + winSize - 1) = synth(startIndex:startIndex + winSize - 1) + w';
    startIndex = startIndex + hopSize;
end

audiowrite('out.wav', synth, Fs);