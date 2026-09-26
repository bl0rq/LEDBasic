#pragma once

namespace Drip {
constexpr char program[] = R"(
param speed number(18.0, 4.0, 80.0, 2.0)
param hue number(200.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  drop = floor(time / speed) % (n + 6)
  for i = 0 to n-1
    src = n - 1 - i
    d = src - drop
    if d < 0
      d = 99
    end
    v = 0
    if d < 5
      v = 255 - d * 45
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
)";
}
