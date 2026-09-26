#pragma once

namespace Scan {
constexpr char program[] = R"(
param speed number(22.0, 6.0, 100.0, 2.0)
param hue number(0.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  span = n * 2
  p = floor(time / speed) % span
  if p >= n
    p = span - p
  end
  if p >= n
    p = n - 1
  end
  for i = 0 to n-1
    d = i - p
    if d < 0
      d = 0 - d
    end
    v = 0
    if d < 6
      v = 255 - d * 40
    end
    sethsv(i, hue, 255, v)
  next
  show()
end
)";
}
