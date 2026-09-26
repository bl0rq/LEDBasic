param speed number(70.0, 20.0, 300.0, 10.0)
param hue number(40.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
  clear()
end

loop(time)
  n = numled()
  for i = 0 to n-1
    r = get_led_r(i) * 0.75
    setled(i, r, r * 0.4, 0)
  next
  k = floor(time / speed)
  sethsv((k * 17) % n, hue, 255, 255)
  sethsv((k * 29 + 5) % n, hue, 200, 180)
  show()
end
