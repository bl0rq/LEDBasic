# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(30.0, 6.0, 160.0, 2.0)
param width number(12.0, 3.0, 40.0, 1.0)
param hue number(20.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  w = width
  if w < 2
    w = 2
  end
  shift = time / speed
  for i = 0 to numled()-1
    saw = (i + shift) % w
    v = saw / w * 255
    sethsv(i, hue, 255, v)
  next
  show()
end
