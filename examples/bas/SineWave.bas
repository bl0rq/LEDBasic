# SineWave — copied from include/BasicExamples/SineWave.h
# Firmware header remains the source of truth for sketches.

param speed number(200.0, 50.0, 1000.0, 10.0)
param frequency number(0.39, 0.1, 2.0, 0.05)
param amplitude number(127.0, 50.0, 255.0, 5.0)
param color_mode enum(["Blue-Red", "Green-Red"])

setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    wave = sin(i * frequency + time / speed) * amplitude + 128
    if color_mode == 0
      setled(i, wave, 0, 255 - wave)
    else
      setled(i, 0, wave, 255 - wave)
    end
  next
  show()
end
