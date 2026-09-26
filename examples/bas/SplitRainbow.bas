param speed number(20.0, 5.0, 100.0, 1.0)
param saturation number(255.0, 0.0, 255.0, 5.0)
param brightness_level number(255.0, 10.0, 255.0, 5.0)
param swap boolean(false)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  mid = n / 2
  if mid < 1
    mid = 1
  end
  spin = time / speed
  leftDir = 1
  rightDir = -1
  if swap
    leftDir = -1
    rightDir = 1
  end
  for d = 0 to mid
    h = d * 360 / mid
    left = mid - 1 - d
    right = mid + d
    if left >= 0
      sethsv(left, h + spin * leftDir, saturation, brightness_level)
    end
    if right < n
      sethsv(right, h + spin * rightDir, saturation, brightness_level)
    end
  next
  show()
end
