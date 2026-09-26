# HalloweenEyes - copied from include/BasicExamples/HalloweenEyes.h
# Firmware header remains the source of truth for sketches.

param speed number(700.0, 100.0, 2000.0, 50.0)
param hue number(20.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  for i = 0 to n-1
    setled(i, 0, 0, 0)
  next
  if (time % speed) < 120
    left = floor(n * 0.3)
    right = floor(n * 0.7)
    sethsv(left, hue, 255, 255)
    if left + 1 < n
      sethsv(left + 1, hue, 255, 255)
    end
    sethsv(right, hue, 255, 255)
    if right + 1 < n
      sethsv(right + 1, hue, 255, 255)
    end
  end
  show()
end
