param hue number(18.0, 0.0, 60.0, 2.0)

setup
  brightness(128)
end

loop(time)
  tick = floor(time / 40)
  for i = 0 to numled()-1
    flick = 180 + (i * 17 + tick) % 75
    sethsv(i, hue, 255, flick)
  next
  show()
end
