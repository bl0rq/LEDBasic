# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 5.0, 200.0, 5.0)
param saturation number(255.0, 0.0, 255.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  h = (time / speed) % 360
  for i = 0 to numled()-1
    sethsv(i, h, saturation, 255)
  next
  show()
end
