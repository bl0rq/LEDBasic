# MovingComets — copied from include/BasicExamples/MovingComets.h
# Firmware header remains the source of truth for sketches.

setup
  brightness(200)
  clear()
  cometPos = 0
  cometSpeed = 0.3
end

loop(time)
  clear()

  // Move comet
  cometPos = cometPos + cometSpeed
  if cometPos >= numled() + 8
    cometPos = -8
  end

  // Draw comet with tail
  for i = 0 to 7
    pos = floor(cometPos) - i
    if pos >= 0 and pos < numled()
      intensity = 255 - i * 30
      setled(pos, intensity, intensity/2, 0)
    end
  next

  show()
end
