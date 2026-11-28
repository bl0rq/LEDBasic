# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(28.0, 6.0, 120.0, 2.0)
param hue number(190.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  pos = floor(time / speed) % n
  for i = 0 to n-1
    tail = pos - i
    if tail < 0
      tail = tail + n
    end
    v = 0
    if tail < 10
      v = 255 - tail * 22
    end
    sethsv(i, hue, 220, v)
  next
  show()
end
