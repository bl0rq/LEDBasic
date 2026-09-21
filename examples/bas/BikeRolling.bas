# BikeRolling — copied from include/BasicExamples/BikeRolling.h
# Firmware header remains the source of truth for sketches.

param speed number(40.0, 5.0, 200.0, 1.0)
param head_brightness number(255.0, 50.0, 255.0, 5.0)
param tail_brightness number(200.0, 50.0, 255.0, 5.0)

setup
  brightness(180)
  clear()
end

loop(time)
  clear()
  n = numled()

  // Rear taillight
  setled(0, tail_brightness, 0, 0)
  if n > 1
    setled(1, tail_brightness / 3, 0, 0)
  end

  // Front headlight
  setled(n - 1, head_brightness, head_brightness, head_brightness)
  if n > 2
    setled(n - 2, head_brightness / 2, head_brightness / 2, 0)
  end

  // Moving amber highlight along the frame
  span = max(1, n - 4)
  pos = floor((time / speed) % span) + 2
  for i = 0 to 3
    p = pos - i
    if p >= 2 and p < n - 2
      k = 255 - i * 50
      setled(p, k, k / 2, 0)
    end
  next
  show()
end
