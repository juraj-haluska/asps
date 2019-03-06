function [first, last] = findInfluenceHalf(location, magnitudes, lastBin)
 % find region of influence of this peak 
first = location;
last = location;

leftPeak = 1;
rightPeak = lastBin;

for i=1:location - 2
    middle = magnitudes(location - i);
    right = magnitudes(location - i + 1);
    left = magnitudes(location - i - 1); 
    if (middle > right && middle > left)
        leftPeak = location - i;
        break
    end
end
        
for i=location + 1:lastBin - 1
    middle = magnitudes(i);
    right = magnitudes(i + 1);
    left = magnitudes(i - 1); 
    if (middle > right && middle > left)
        rightPeak = i;
        break
    end
end

first = leftPeak + ceil((location - leftPeak) / 2);
last = location + floor((rightPeak - location) / 2);

end