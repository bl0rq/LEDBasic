# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 8.0, 200.0, 4.0)
param width number(12.0, 4.0, 40.0, 1.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  edge = width + (time / speed) % (n + width)
  for i = 0 to n-1
    d = edge - i
    if d < 0
      d = 0
    end
    if d > width
      d = width
    end
    v = d / width * 255
    h = 15 + v / 8
    sethsv(i, h, 240, v)
  next
  show()
end
