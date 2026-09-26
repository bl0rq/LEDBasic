#pragma once

namespace TVSimulator {
constexpr char program[] = R"(
param speed number(120.0, 30.0, 500.0, 10.0)

setup
  brightness(128)
end

loop(time)
  slot = floor(time / speed)
  for i = 0 to numled()-1
    h = (i * 19 + slot * 47) % 360
    v = 80 + (i * 13 + slot) % 160
    sethsv(i, h, 180, v)
  next
  show()
end
)";
}
