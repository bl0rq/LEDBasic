# BPM - copied from include/BasicExamples/BPM.h
# Firmware header remains the source of truth for sketches.

param tempo number(480.0, 200.0, 1000.0, 20.0)

setup
  brightness(128)
end

loop(time)
  beat = time % tempo
  pulse = 50
  if beat < 60
    pulse = 255
  end
  for i = 0 to numled()-1
    h = (i * 20 + floor(time / tempo) * 40) % 360
    sethsv(i, h, 255, pulse)
  next
  show()
end
