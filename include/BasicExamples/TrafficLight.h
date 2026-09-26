#pragma once

namespace TrafficLight {
constexpr char program[] = R"(
param speed number(900.0, 200.0, 3000.0, 50.0)

setup
  brightness(128)
end

loop(time)
  phase = floor(time / speed) % 3
  r = 0
  g = 0
  b = 0
  if phase == 0
    r = 255
  end
  if phase == 1
    r = 255
    g = 160
  end
  if phase == 2
    g = 255
  end
  for i = 0 to numled()-1
    setled(i, r, g, b)
  next
  show()
end
)";
}
