# Fire - copied from include/BasicExamples/Fire.h
# Firmware header remains the source of truth for sketches.

param cooling number(40.0, 5.0, 80.0, 5.0)
param spark number(200.0, 40.0, 255.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  keep = 1 - cooling / 255
  for i = n - 1 to 1 step -1
    heat = get_led_r(i - 1) * keep
    setled(i, heat, heat * 0.25, 0)
  next
  base = spark
  if (floor(time / 50) % 3) == 0
    base = spark * 0.45
  end
  setled(0, base, base * 0.3, 0)
  show()
end
