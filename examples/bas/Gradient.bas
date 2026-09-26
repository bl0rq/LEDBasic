# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param drift number(80.0, 10.0, 400.0, 10.0)
param brightness_level number(255.0, 10.0, 255.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  if n < 2
    n = 2
  end
  shift = time / drift
  for i = 0 to numled()-1
    setpal(i, i * 255 / (n - 1) + shift, brightness_level)
  next
  show()
end
