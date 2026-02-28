#pragma once

namespace PulsingCenter {
constexpr char program[] = R"(
# Pulsing center LED effect
# Parameters: color_hue (0-360), pulse_speed (1-100), pulse_width (1-50)

setup
  brightness(150)
  clear()
end

loop(time)
  
  # Calculate pulse phase (0-1) using sine wave
  phase = sin(time * speed / 1000.0) * 0.5 + 0.5
  
  # Calculate center position
  center = numled() / 2
  
  # Calculate current pulse radius
  max_radius = min(center, width)
  current_radius = phase * max_radius
  
  # Clear all LEDs first
  clear()
  
  # Draw the pulse from center outward
  for i = 0 to numled() - 1
    # Calculate distance from center
    distance = abs(i - center)
    
    # Check if this LED should be lit
    if distance <= current_radius
      # Calculate brightness based on distance from pulse edge
      edge_distance = current_radius - distance
      brightness_factor = edge_distance / max(current_radius, 1)
      
      # Smooth falloff using squared function
      brightness_factor = brightness_factor * brightness_factor
      
      # Set LED with calculated brightness
      saturation = 255
      value = brightness_factor * 255
      sethsv(i, hue, saturation, value)
    end
  next
end
)";
}
