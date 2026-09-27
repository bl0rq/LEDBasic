# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(30.0, 8.0, 160.0, 4.0)
param zones number(4.0, 2.0, 8.0, 1.0)

setup
  brightness(128)
end

loop(time)
  z = floor(zones)
  if z < 1
    z = 1
  end
  width = numled() / z
  if width < 1
    width = 1
  end
  shift = time / speed
  for i = 0 to numled()-1
    band = floor((i + shift) / width)
    h = (band * 360 / z) % 360
    sethsv(i, h, 255, 255)
  next
  show()
end
