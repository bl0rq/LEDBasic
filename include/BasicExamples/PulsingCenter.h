#pragma once

namespace PulsingCenter {
constexpr char program[] = R"(
param color_hue number(200.0, 0.0, 360.0, 5.0)
param pulse_speed number(50.0, 1.0, 100.0, 1.0)
param pulse_width number(20.0, 1.0, 50.0, 1.0)

setup
  brightness(150)
  clear()
end

loop(time)
  phase = sin(time * pulse_speed / 1000.0) * 0.5 + 0.5
  center = numled() / 2
  max_radius = min(center, pulse_width)
  current_radius = phase * max_radius

  clear()

  for i = 0 to numled() - 1
    distance = abs(i - center)
    if distance <= current_radius
      edge_distance = current_radius - distance
      brightness_factor = edge_distance / max(current_radius, 1)
      brightness_factor = brightness_factor * brightness_factor
      sethsv(i, color_hue, 255, brightness_factor * 255)
    end
  next
end
)";
}
