# Glitter - copied from include/BasicExamples/Glitter.h
# Firmware header remains the source of truth for sketches.

param hue number(200.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  for i = 0 to n-1
    sethsv(i, hue, 255, 140)
  next
  tick = floor(time / 30)
  setled((tick * 13) % n, 255, 255, 255)
  setled((tick * 29 + 4) % n, 255, 255, 255)
  show()
end
