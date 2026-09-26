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
  spin = time / speed
  for i = 0 to n-1
    dir = -1
    if i < mid
      dir = 1
    end
    if swap
      dir = 0 - dir
    end
    h = i * 360 / n + spin * dir
    sethsv(i, h, saturation, brightness_level)
  next
  show()
end
