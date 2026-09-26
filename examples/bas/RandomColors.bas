# RandomColors - copied from include/BasicExamples/RandomColors.h
# Firmware header remains the source of truth for sketches.

param hold number(600.0, 80.0, 3000.0, 40.0)

setup
  brightness(128)
end

loop(time)
  slot = floor(time / hold)
  h = (slot * 53) % 360
  for i = 0 to numled()-1
    sethsv(i, h, 255, 255)
  next
  show()
end
