# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param gap number(500.0, 80.0, 2000.0, 20.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  for i = 0 to n-1
    setled(i, 0, 0, 12)
  next
  age = time % gap
  if age < 40
    for i = 0 to n-1
      setled(i, 255, 255, 255)
    next
  end
  if age >= 40
    if age < 70
      for i = 0 to n-1
        if i % 3 == 0
          setled(i, 180, 180, 255)
        end
      next
    end
  end
  show()
end
