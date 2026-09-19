# Matrix — copied from include/BasicExamples/Matrix.h
# Firmware header remains the source of truth for sketches.

param speed number(3.0, 1.0, 10.0, 1.0)
param density number(30.0, 5.0, 80.0, 5.0)
param tail_length number(4.0, 2.0, 10.0, 1.0)
param spawn_rate number(2000.0, 500.0, 5000.0, 100.0)

setup
  brightness(128)
  clear()
end

loop(time)
  fade = 1 - (speed / (tail_length * 20))
  if fade < 0.5
    fade = 0.5
  end

  for i = 0 to numled()-1
    if random(spawn_rate) < density
      intensity = 100 + random(155)
      setled(i, 0, intensity, 0)
    else
      r = get_led_r(i)
      g = get_led_g(i)
      b = get_led_b(i)
      g = g * fade
      if g < 10
        g = 0
      end
      setled(i, r, g, b)
    end
  next

  show()
end
