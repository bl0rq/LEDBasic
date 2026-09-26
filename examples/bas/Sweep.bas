param speed number(25.0, 5.0, 120.0, 5.0)

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
  for i = 0 to n-1
    d = i - p
    if d < 0
      d = 0 - d
    end
    pos = i * 255 / n
    if d < 4
      setpal(i, pos, 255)
    else
      setpal(i, pos, 40)
    end
  next
  show()
end
