# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 8.0, 180.0, 4.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  if n < 1
    n = 1
  end
  for i = 0 to numled()-1
    wave = sin(i * 0.2 + time / speed) * 24
    setpal(i, i * 255 / n + time / speed + wave, 220)
  next
  show()
end
