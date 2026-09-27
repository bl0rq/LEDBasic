# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(40.0, 10.0, 160.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  for i = 0 to n-1
    r = get_led_r(i) * 0.8
    g = get_led_g(i) * 0.8
    b = get_led_b(i) * 0.8
    setled(i, r, g, b)
  next
  span = n - 1
  a = (sin(time / speed) + 1) / 2 * span
  bpos = (sin(time / speed * 1.7 + 2) + 1) / 2 * span
  c = (sin(time / speed * 0.6 + 4) + 1) / 2 * span
  sethsv(floor(a), 0, 255, 255)
  sethsv(floor(bpos), 140, 255, 255)
  sethsv(floor(c), 240, 255, 255)
  show()
end
