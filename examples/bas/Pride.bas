param speed number(30.0, 6.0, 140.0, 2.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    h = (i * 12 + time / speed + sin(i * 0.3 + time / 80) * 30) % 360
    sethsv(i, h, 255, 255)
  next
  show()
end
