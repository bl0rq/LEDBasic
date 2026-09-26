# TwoDots - copied from include/BasicExamples/TwoDots.h
# Firmware header remains the source of truth for sketches.

param speed number(24.0, 6.0, 100.0, 2.0)
param hue number(180.0, 0.0, 360.0, 5.0)
param hue2 number(330.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  span = n * 2
  p = floor(time / speed) % span
  if p >= n
    p = span - p
  end
  q = n - 1 - p
  clear()
  sethsv(floor(p), hue, 255, 255)
  sethsv(floor(q), hue2, 255, 255)
  show()
end
