# Wipe - copied from include/BasicExamples/Wipe.h
# Firmware header remains the source of truth for sketches.

param speed number(30.0, 5.0, 150.0, 5.0)
param hue number(140.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  head = floor(time / speed) % (numled() + 8)
  for i = 0 to numled()-1
    if i <= head
      sethsv(i, hue, 255, 255)
    else
      sethsv(i, hue, 255, 0)
    end
  next
  show()
end
