param gap number(180.0, 40.0, 800.0, 20.0)
param hue number(220.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  v = 0
  if (time % gap) < 28
    v = 255
  end
  for i = 0 to numled()-1
    sethsv(i, hue, 255, v)
  next
  show()
end
