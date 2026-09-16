#pragma once

namespace BikeParked {
constexpr char program[] = R"(
param tail_index number(8.0, 0.0, 400.0, 1.0)
param marker_count number(2.0, 1.0, 10.0, 1.0)
param parked_red number(64.0, 10.0, 255.0, 1.0)

setup
  brightness(100)
  clear()
end

loop(time)
  if numled() < 43
    fill(255, 0, 0)
  else
    clear()
    for i = 0 to marker_count - 1
      setled(tail_index + i, parked_red, 0, 0)
    next
  end
end
)";
}
