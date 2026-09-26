param speed number(50.0, 10.0, 200.0, 5.0)
param hue number(260.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  center = (n - 1) / 2
  radius = (sin(time / speed) + 1) / 2 * center
  for i = 0 to n-1
    d = i - center
    if d < 0
      d = 0 - d
    end
    v = 0
    if d < radius + 1
      v = 255 - d / (radius + 1) * 180
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
