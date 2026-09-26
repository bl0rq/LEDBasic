param speed number(200.0, 40.0, 800.0, 20.0)
param hue number(300.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  for i = 0 to n-1
    setled(i, 4, 0, 8)
  next
  for k = 0 to 4
    pos = floor(n * (k + 0.5) / 5)
    tw = sin(time / speed + k) * 127 + 128
    sethsv(pos, hue, 180, tw)
  next
  show()
end
