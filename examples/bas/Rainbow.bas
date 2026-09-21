# Rainbow — copied from include/BasicExamples/Rainbow.h
# Firmware header remains the source of truth for sketches.

param speed number(20.0, 5.0, 100.0, 1.0)
param saturation number(255.0, 0.0, 255.0, 5.0)
param brightness_level number(255.0, 10.0, 255.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 360 / numled() + time / speed) % 360
    sethsv(i, h, saturation, brightness_level)
  next
  show()
end
