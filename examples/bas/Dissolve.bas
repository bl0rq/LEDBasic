# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(80.0, 20.0, 400.0, 10.0)
param hue number(280.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  slot = floor(time / speed)
  for i = 0 to numled()-1
    if (i * 3 + slot) % 5 < 2
      sethsv(i, hue, 255, 255)
    else
      sethsv(i, (hue + 180) % 360, 255, 80)
    end
  next
  show()
end
