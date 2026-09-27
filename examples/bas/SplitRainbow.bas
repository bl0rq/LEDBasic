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
  mid = floor(n / 2)
  if mid < 1
    mid = 1
  end
  dir = 1
  if swap
    dir = -1
  end
  spin = time / speed * dir

  if n % 2 == 1
    sethsv(mid, spin, saturation, brightness_level)
  end

  for i = mid to 1 step -1
    h = i * 360 / mid + spin
    left = mid - i
    right = n - 1 - left
    sethsv(left, h, saturation, brightness_level)
    sethsv(right, h, saturation, brightness_level)
  next

  show()
end
