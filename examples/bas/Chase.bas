# Chase - copied from include/BasicExamples/Chase.h
# Firmware header remains the source of truth for sketches.

param speed number(40.0, 8.0, 200.0, 4.0)
param width number(6.0, 1.0, 24.0, 1.0)
param hue number(15.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  pos = floor(time / speed) % n
  w = floor(width)
  if w < 1
    w = 1
  end
  for i = 0 to n-1
    rel = i - pos
    if rel < 0
      rel = rel + n
    end
    v = 0
    if rel < w
      v = 255
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
