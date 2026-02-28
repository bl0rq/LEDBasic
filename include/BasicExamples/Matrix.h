#pragma once

namespace Matrix {
constexpr char program[] = R"(
param speed number(3.0, 1.0, 10.0, 1.0)
param density number(30.0, 5.0, 80.0, 5.0)
param tail_length number(4.0, 2.0, 10.0, 1.0)
param spawn_rate number(2000.0, 500.0, 5000.0, 100.0)

setup
  brightness(128)
  clear()
end

loop(time)
  // Simple Matrix rain effect
  for i = 0 to numled()-1
    // Random chance to spawn a new "drop"
    if random(spawn_rate) < density
      // Set LED to green with random intensity
      intensity = 100 + random(155)
      setled(i, 0, intensity, 0)
    else
      // Fade existing LEDs
      r = get_led_r(i)
      g = get_led_g(i)
      b = get_led_b(i)
      
      // Fade green component
      g = g * 0.9
      if g < 10
        g = 0
      end
      
      setled(i, r, g, b)
    end
  next
  
  show()
end
)";
}
