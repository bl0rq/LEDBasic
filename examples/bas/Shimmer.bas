param hue number(45.0, 0.0, 360.0, 5.0)
param speed number(30.0, 5.0, 120.0, 5.0)

setup
  brightness(128)
end

loop(time)
  tick = floor(time / speed)
  for i = 0 to numled()-1
    grain = 160 + (i * 13 + tick) % 90
    sethsv(i, hue, 200, grain)
  next
  show()
end
