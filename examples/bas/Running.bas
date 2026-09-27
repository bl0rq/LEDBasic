# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 8.0, 200.0, 4.0)
param width number(0.35, 0.1, 1.2, 0.05)
param hue number(210.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    v = sin(i * width + time / speed) * 127 + 128
    sethsv(i, hue, 255, v)
  next
  show()
end
