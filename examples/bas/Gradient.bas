param hue_a number(0.0, 0.0, 360.0, 5.0)
param hue_b number(260.0, 0.0, 360.0, 5.0)
param drift number(80.0, 10.0, 400.0, 10.0)

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
    t = i / (n - 1)
    h = hue_a + (hue_b - hue_a) * t + shift
    sethsv(i, h, 255, 255)
  next
  show()
end
