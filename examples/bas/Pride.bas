# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(30.0, 6.0, 140.0, 2.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  if n < 1
    n = 1
  end
  for i = 0 to numled()-1
    wave = sin(i * 0.3 + time / 80) * 20
    setpal(i, i * 255 / n + time / speed + wave, 255)
  next
  show()
end
