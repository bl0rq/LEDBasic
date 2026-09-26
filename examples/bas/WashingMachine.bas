param speed number(28.0, 6.0, 120.0, 2.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  span = n * 2
  p = floor(time / speed) % span
  if p >= n
    p = span - p
  end
  h = (time / 20) % 360
  door = n / 5
  if door < 2
    door = 2
  end
  for i = 0 to n-1
    d = i - p
    if d < 0
      d = 0 - d
    end
    v = 30
    if d < door
      v = 255 - d * 8
    end
    sethsv(i, h, 220, v)
  next
  show()
end
