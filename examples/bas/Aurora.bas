# Aurora - copied from include/BasicExamples/Aurora.h
# Firmware header remains the source of truth for sketches.

param speed number(70.0, 15.0, 250.0, 5.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    curtain = sin(i * 0.12 + time / speed) * sin(i * 0.05 - time / speed * 0.4)
    v = 50 + (curtain + 1) * 90
    setled(i, 8, v, 28 + curtain * 24)
  next
  show()
end
