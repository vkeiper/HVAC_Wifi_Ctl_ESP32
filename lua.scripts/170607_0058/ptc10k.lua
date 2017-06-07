local ADCFULLSCALE = 1023


local ptcbits = {993,992,989,987,985,983,980,977,974,971,
    968,965,962,958,954,950,946,942,937,932,
    927,922,917,911,906,900,893,887,880,874,
    866,859,852,844,836,827,819,810,802,792,
    783,774,764,754,744,734,724,713,702,692,
    681,670,659,648,636,625,614,602,591,580,
    568,557,545,534,523,512,500,489,478,467,
    456,446,435,425,414,404,394,384,375,365,
    356,346,337,328,320,311,303,294,286,279,
    271,263,256,249,242,235,229,222,216,210,
    204,198,192,187,182,176,171,166,162,157,
    153,148,144,140,136,132,128,124,121,117,
    114,111,108,105,102,99,96,93,91,88,
    86,83,81,79,77,75,73,71,69,67,
    65,63,62,60,58,57,55,54,52,51,
    50,48,47,46,45,44,42,41,40,39,
}



function ProcessPtc(adc)
    for i=1,160,1 do
        scBits = ptcbits[i]
        if scBits < adc then 
          return i-40
        else
            if i >159 then
                return 155
            end
        end
    end
end