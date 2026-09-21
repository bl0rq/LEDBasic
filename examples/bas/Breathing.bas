# Breathing — copied from include/BasicExamples/Breathing.h
# Firmware header remains the source of truth for sketches.

param speed number(1000.0, 200.0, 5000.0, 100.0)
param intensity number(127.0, 50.0, 255.0, 5.0)
param color_scheme enum(["Blue-Red", "Green-Red", "Yellow"])

setup
  brightness(128)
  clear()
end

loop(time)
  breath = (sin(time / speed) + 1) * intensity
  if color_scheme == 0
    fill(breath, 0, 255 - breath)
  else
    if color_scheme == 1
      fill(0, breath, 255 - breath)
    else
      fill(breath, breath, 0)
    end
  end
  show()
end
