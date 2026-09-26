# Fade - copied from include/BasicExamples/Fade.h
# Firmware header remains the source of truth for sketches.

param speed number(80.0, 10.0, 400.0, 10.0)
param hue_a number(0.0, 0.0, 360.0, 5.0)
param hue_b number(220.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  w = (sin(time / speed) + 1) / 2
  h = hue_a + (hue_b - hue_a) * w
  for i = 0 to numled()-1
    sethsv(i, h, 255, 255)
  next
  show()
end
