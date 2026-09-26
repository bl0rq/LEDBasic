param cooling number(40.0, 5.0, 80.0, 5.0)
param spark number(200.0, 40.0, 255.0, 5.0)

setup
  brightness(128)
  dim heat(numled())
  clear()
end

loop(time)
  n = numled()
  keep = 1 - cooling / 255
  for i = n - 1 to 1 step -1
    heat[i] = heat[i - 1] * keep
  next
  base = spark
  if (floor(time / 50) % 3) == 0
    base = spark * 0.45
  end
  heat[0] = base
  for i = 0 to n-1
    setpal(i, heat[i])
  next
  show()
end
