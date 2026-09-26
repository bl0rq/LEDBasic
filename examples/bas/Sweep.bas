param speed number(25.0, 5.0, 120.0, 5.0)
param hue number(200.0, 0.0, 360.0, 5.0)
param hue2 number(30.0, 0.0, 360.0, 5.0)

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
    if d < 4
      sethsv(i, hue, 255, 255)
    else
      sethsv(i, hue2, 200, 40)
    end
  next
  show()
end
