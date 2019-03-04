function [first, last] = findInfluenceRegion(location, magnitudes, lastBin)
 % find region of influence of this peak 
first = location;
last = location;
        
while(first > 1)
    if (magnitudes(first) > magnitudes(first - 1))
        first = first - 1;
    else
        break;
    end
end
        
while(last < lastBin)
    if (magnitudes(last) > magnitudes(last + 1))
        last = last + 1;
    else
        break;
    end
end

end