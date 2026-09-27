# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 10.0, 120.0, 5.0)

setup
  brightness(128)
end

loop(time)
  tick = floor(time / speed)
  for i = 0 to numled()-1
    flick = 160 + (i * 17 + tick) % 95
    setpal(i, flick)
  next
  show()
end
