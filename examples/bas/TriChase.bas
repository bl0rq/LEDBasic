# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(35.0, 8.0, 160.0, 4.0)
param size number(4.0, 1.0, 12.0, 1.0)

setup
  brightness(128)
end

loop(time)
  cell = floor(size)
  if cell < 1
    cell = 1
  end
  shift = floor(time / speed)
  for i = 0 to numled()-1
    band = floor((i + shift) / cell) % 3
    if band == 0
      setled(i, 255, 0, 0)
    end
    if band == 1
      setled(i, 0, 180, 0)
    end
    if band == 2
      setled(i, 0, 40, 255)
    end
  next
  show()
end
