# BikeParked — copied from include/BasicExamples/BikeParked.h
# Firmware header remains the source of truth for sketches.

param tail_index number(8.0, 0.0, 400.0, 1.0)
param marker_count number(2.0, 1.0, 10.0, 1.0)
param parked_red number(64.0, 10.0, 255.0, 1.0)

setup
  brightness(100)
  clear()
end

loop(time)
  n = numled()
  if n < 43
    fill(255, 0, 0)
  else
    clear()
    count = marker_count
    if count > n
      count = n
    end
    start = tail_index
    max_start = n - count
    if start > max_start
      start = max_start
    end
    if start < 0
      start = 0
    end
    for i = 0 to count - 1
      setled(start + i, parked_red, 0, 0)
    next
  end
end
