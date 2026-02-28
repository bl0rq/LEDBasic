#pragma once

namespace DoubleRainbow {
constexpr char program[] = R"(
param speed number(30.0, 10.0, 100.0, 1.0)
param hue_spread number(180.0, 90.0, 360.0, 10.0)
param saturation number(255.0, 100.0, 255.0, 5.0)
param brightness_level number(255.0, 50.0, 255.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  for i = 0 to numled()-1
    if i % 2 == 0
      // Even LEDs - forward rainbow
      h = (i * hue_spread / numled() + time / speed) % 360
    else
      // Odd LEDs - reverse rainbow
      h = (360 - (i * hue_spread / numled() + time / speed)) % 360
    end
    sethsv(i, h, saturation, brightness_level)
  next
  show()
end
)";
}
