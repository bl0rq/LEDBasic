#pragma once

namespace Twinkle {
constexpr char program[] = R"(
param rate number(40.0, 5.0, 200.0, 5.0)
param hue number(45.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
  for i = 0 to numled()-1
    if i % 7 == 0
      sethsv(i, 45, 80, 160)
    else
      setled(i, 0, 0, 0)
    end
  next
end

loop(time)
  n = numled()
  for i = 0 to n-1
    v = get_led_r(i) * 0.92
    setled(i, v, v * 0.8, v * 0.4)
  next
  pick = floor(time / rate) % n
  sethsv(pick, hue, 60, 255)
  show()
end
)";
}
