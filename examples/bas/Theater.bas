param speed number(90.0, 20.0, 400.0, 10.0)
param gap number(3.0, 2.0, 8.0, 1.0)
param hue number(280.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  march = floor(time / speed)
  g = floor(gap)
  if g < 2
    g = 2
  end
  for i = 0 to numled()-1
    on = 0
    if (i + march) % g == 0
      on = 255
    end
    sethsv(i, hue, 255, on)
  next
  show()
end
