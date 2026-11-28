# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param rate number(40.0, 5.0, 200.0, 5.0)
param hue number(45.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
  for i = 0 to numled()-1
    if i % 7 == 0
      sethsv(i, hue, 80, 160)
    else
      setled(i, 0, 0, 0)
    end
  next
end

loop(time)
  n = numled()
  for i = 0 to n-1
    r = get_led_r(i) * 0.92
    g = get_led_g(i) * 0.92
    b = get_led_b(i) * 0.92
    setled(i, r, g, b)
  next
  k = floor(time / rate)
  pick = (k * 37 + (k % 17) * 13) % n
  sethsv(pick, hue, 60, 255)
  show()
end
