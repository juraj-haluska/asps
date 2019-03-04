close all;

[input, Fs] = audioread('sax.wav');
y = input(:, 1)';

% configurable parameters
winPeriod = 0.020;
hopRate = 50;
fftSize = 4096;
maxPeaks = 20;
fTol = 25;
harmony = [0 4 7 11 14];

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
O = zeros(size(Y, 1), size(Y, 2));
zu = ones(length(harmony), maxPeaks); % phase accumulator
zuf = zeros(length(harmony), maxPeaks); % holds bin frequency
for fI = 1:size(Y, 2)
    % apply windowing function
    frame = Y(:, fI) .* hamming(winSize);
    
    % take fourier transformis
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
    
    halves = zeros(length(lHalf), length(harmony));
        
    for hI=1:length(harmony)
        % keep will indicate which phases were matched
        % newZu is 
        keep = [];
        newZu = [];
        newZuf = [];

        % loop each peak
        newHalf = zeros(1, length(lHalf))';
        for pI=1:length(locs)
            % estimate peaks frequency
            fHz = (locs(pI) - 1) * Fs / fftSize;

            % calculate bin transposition factor for this peak
            fWanted = fHz * 2^(harmony(hI)/12);
            t = round((fWanted - fHz) / (Fs/fftSize));

            % try to find matching fWanted from previous frame
            dw = 2*pi*t/fftSize; % delta
            [m, l] = min(abs(zuf(hI, :) - fWanted));
            z = 1;
            if (m <= fTol && m >= -fTol)
                % match - rotate phase
                zu(hI, l) = zu(hI, l)*exp(1j*dw*hopSize);
                z = zu(hI, l);
                keep = [keep l];
            else
                % not match
                z = z*exp(1j*dw*hopSize);
                newZu = [newZu z];
                newZuf = [newZuf fWanted];
            end

            % isolate region of influence
            [first, last] = findInfluenceRegion(locs(pI), mags, floor(length(lHalf) / 2));

            % translate peak
            if last + t <= length(newHalf) && first + t >= 1 
                region = lHalf(first:last);
                newHalf(first + t:last + t) = newHalf(first + t:last + t) + region * z;
            end
        end

        % replace not matched phase continuations with new ones
        tempZu = [];
        tempZuf = [];
        for kI=1:length(keep)
            tempZu = [tempZu zu(hI, keep(kI))];
            tempZuf = [tempZuf zuf(hI, keep(kI))];
        end
        for nI=1:length(newZu)
            tempZu = [tempZu newZu(nI)];
            tempZuf = [tempZuf newZuf(nI)];       
        end

        zu(hI, :) = tempZu;
        zuf(hI, :) = tempZuf;
    
        halves(:, hI) = newHalf;
        
    end
    
    % ---------------------------------------------------------------------
    % reconstruct signal
    mixed = 0.5*sum(halves, 2);
    reflected = [mixed' conj(fliplr(mixed(2:end-1)'))]';
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