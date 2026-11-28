# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(50.0, 10.0, 200.0, 5.0)
param hue number(300.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  for i = 0 to n-1
    r = get_led_r(i) * 0.82
    g = get_led_g(i) * 0.82
    b = get_led_b(i) * 0.82
    setled(i, r, g, b)
  next
  pos = (sin(time / speed) + 1) / 2 * (n - 1)
  sethsv(floor(pos), hue, 255, 255)
  show()
end
