#pragma once

namespace Railway {
constexpr char program[] = R"(
param speed number(20.0, 4.0, 80.0, 2.0)

setup
  brightness(128)
end

loop(time)
  n = numled()
  shift = floor(time / speed) % n
  b = n - 1 - shift
  for i = 0 to n-1
    setled(i, 18, 18, 28)
  next
  sethsv(shift, 40, 255, 255)
  if shift > 0
    sethsv(shift - 1, 40, 200, 120)
  end
  sethsv(b, 200, 255, 255)
  if b > 0
    sethsv(b - 1, 200, 180, 80)
  end
  show()
end
)";
}
