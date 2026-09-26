param speed number(60.0, 10.0, 240.0, 5.0)
param scale number(0.25, 0.05, 0.8, 0.05)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    a = sin(i * scale + time / speed)
    b = sin(i * scale * 0.6 - time / speed * 0.7)
    h = (a + b + 2) * 90
    v = 140 + a * 80
    sethsv(i, h, 255, v)
  next
  show()
end
