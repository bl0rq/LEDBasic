#pragma once

namespace Sparkle {
constexpr char program[] = R"(
param hue number(0.0, 0.0, 360.0, 5.0)
param bg number(20.0, 0.0, 80.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  for i = 0 to n-1
    setled(i, bg, bg, bg)
  next
  a = floor(time * 3) % n
  b = floor(time * 5 + 7) % n
  sethsv(a, hue, 40, 255)
  setled(b, 255, 255, 255)
  show()
end
)";
}
