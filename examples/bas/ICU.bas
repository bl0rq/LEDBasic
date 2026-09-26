# ICU - copied from include/BasicExamples/ICU.h
# Firmware header remains the source of truth for sketches.

param speed number(26.0, 6.0, 100.0, 2.0)
param hue number(120.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  pos = floor(time / speed) % n
  for i = 0 to n-1
    setled(i, 0, 8, 0)
  next
  sethsv(pos, hue, 255, 255)
  sethsv(n - 1 - pos, 0, 255, 180)
  show()
end
