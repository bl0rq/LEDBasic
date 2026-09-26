# Candle - copied from include/BasicExamples/Candle.h
# Firmware header remains the source of truth for sketches.

param hue number(25.0, 0.0, 50.0, 1.0)

setup
  brightness(128)
end

loop(time)
  flick = 170 + (floor(time / 35) * 13) % 80
  for i = 0 to numled()-1
    sethsv(i, hue, 255, flick)
  next
  show()
end
