# Spots - copied from include/BasicExamples/Spots.h
# Firmware header remains the source of truth for sketches.

param spread number(7.0, 3.0, 16.0, 1.0)
param width number(2.0, 1.0, 5.0, 1.0)
param hue number(50.0, 0.0, 360.0, 5.0)
param speed number(60.0, 15.0, 300.0, 5.0)

setup
  brightness(128)
end

loop(time)
  g = floor(spread)
  if g < 2
    g = 2
  end
  w = floor(width)
  off = floor(time / speed) % g
  for i = 0 to numled()-1
    slot = (i + off) % g
    v = 0
    if slot < w
      v = 255
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
