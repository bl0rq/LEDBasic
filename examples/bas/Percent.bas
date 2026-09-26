param level number(65.0, 0.0, 100.0, 5.0)
param hue number(130.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  cut = numled() * level / 100
  for i = 0 to numled()-1
    if i < cut
      sethsv(i, hue, 255, 255)
    else
      sethsv(i, hue, 40, 18)
    end
  next
  show()
end
