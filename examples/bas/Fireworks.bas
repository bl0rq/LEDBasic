param speed number(400.0, 80.0, 1500.0, 20.0)
param hue number(40.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  for i = 0 to n-1
    r = get_led_r(i) * 0.86
    g = get_led_g(i) * 0.8
    b = get_led_b(i) * 0.75
    setled(i, r, g, b)
  next
  age = time % speed
  if age < 220
    center = floor(n / 2)
    radius = age / 28
    for i = 0 to n-1
      d = i - center
      if d < 0
        d = 0 - d
      end
      delta = d - radius
      if delta < 0
        delta = 0 - delta
      end
      if delta < 1.4
        sethsv(i, hue + age, 255, 255)
      end
    next
  end
  show()
end
