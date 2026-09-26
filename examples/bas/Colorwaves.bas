param speed number(40.0, 8.0, 180.0, 4.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 8 + sin(i * 0.2 + time / speed) * 40 + time / 15) % 360
    sethsv(i, h, 255, 220)
  next
  show()
end
