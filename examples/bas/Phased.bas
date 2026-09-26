# Phased - copied from include/BasicExamples/Phased.h
# Firmware header remains the source of truth for sketches.

param speed number(35.0, 8.0, 160.0, 4.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    a = sin(i * 0.25 + time / speed) * 127 + 128
    b = sin(i * 0.25 + time / speed + 1.6) * 127 + 128
    setled(i, a * 0.3, b, a)
  next
  show()
end
