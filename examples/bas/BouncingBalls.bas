# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param gravity number(0.18, 0.04, 0.6, 0.02)
param bounce number(0.86, 0.4, 1.0, 0.02)

setup
  brightness(128)
  y0 = 8
  v0 = 0
  y1 = 24
  v1 = 0.4
  y2 = 4
  v2 = 1.2
end

loop(time)
  n = numled() - 1
  launch = sqrt(2 * gravity * (n + 1)) * 0.85
  if launch < 0.8
    launch = 0.8
  end
  v0 = v0 - gravity
  y0 = y0 + v0
  if y0 < 0
    y0 = 0
    v0 = 0 - v0 * bounce
    if v0 < 0.35
      v0 = launch
    end
  end
  v1 = v1 - gravity
  y1 = y1 + v1
  if y1 < 0
    y1 = 0
    v1 = 0 - v1 * bounce
    if v1 < 0.35
      v1 = launch * 0.9
    end
  end
  v2 = v2 - gravity
  y2 = y2 + v2
  if y2 < 0
    y2 = 0
    v2 = 0 - v2 * bounce
    if v2 < 0.35
      v2 = launch * 1.05
    end
  end
  if y0 > n
    y0 = n
    if v0 > 0
      v0 = 0 - v0 * bounce
    end
  end
  if y1 > n
    y1 = n
    if v1 > 0
      v1 = 0 - v1 * bounce
    end
  end
  if y2 > n
    y2 = n
    if v2 > 0
      v2 = 0 - v2 * bounce
    end
  end
  clear()
  setled(floor(y0), 255, 40, 40)
  setled(floor(y1), 40, 220, 80)
  setled(floor(y2), 80, 80, 255)
  show()
end
