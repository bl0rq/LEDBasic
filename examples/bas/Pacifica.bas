# Pacifica - copied from include/BasicExamples/Pacifica.h
# Firmware header remains the source of truth for sketches.

param speed number(50.0, 10.0, 200.0, 5.0)

setup
  brightness(128)
end

loop(time)
  for i = 0 to numled()-1
    wave = sin(i * 0.18 + time / speed) * 0.5 + sin(i * 0.07 - time / speed * 0.6) * 0.5
    deep = 40 + (wave + 1) * 30
    foam = 0
    if wave > 0.55
      foam = (wave - 0.55) * 500
    end
    setled(i, foam * 0.35, 30 + wave * 25 + foam * 0.6, deep + foam)
  next
  show()
end
