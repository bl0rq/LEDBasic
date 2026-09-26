#pragma once

namespace Ripple {
constexpr char program[] = R"(
param speed number(36.0, 8.0, 160.0, 4.0)
param hue number(190.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  center = (n - 1) / 2
  radius = (time / speed) % (n / 2 + 4)
  for i = 0 to n-1
    d = i - center
    if d < 0
      d = 0 - d
    end
    delta = d - radius
    if delta < 0
      delta = 0 - delta
    end
    v = 0
    if delta < 2.2
      v = 255 - delta * 80
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
)";
}
