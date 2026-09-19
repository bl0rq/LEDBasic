# BackgroundStars — copied from include/BasicExamples/BackgroundStars.h
# Firmware header remains the source of truth for sketches.

setup
  brightness(100)
  clear()
end

loop(time)
  // Slowly twinkling background stars
  for i = 0 to numled()-1
    if random(1000) < 3
      twinkle = random(150) + 50
      setled(i, twinkle, twinkle, 255)
    else
      // Fade existing stars
      fadeAmount = random(10) + 5
      // Simplified fade - just dim a bit
      if random(100) < 20
        setled(i, 0, 0, 0)
      end
    end
  next
  show()
end
