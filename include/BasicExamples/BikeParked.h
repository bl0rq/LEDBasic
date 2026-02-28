#pragma once

namespace BikeParked {
constexpr char program[] = R"(
setup
  brightness(100)
  clear()
end

loop(time)
  if numled() < 43
    fill(255, 0, 0)
  end

  setled(8, 64, 0, 0)
  setled(9, 64, 0, 0)
end
)";
}
