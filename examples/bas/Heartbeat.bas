# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(900.0, 300.0, 1800.0, 50.0)
param hue number(0.0, 0.0, 360.0, 5.0)

setup
  brightness(128)
end

loop(time)
  beat = time % speed
  v = 20
  if beat < 70
    v = 255
  end
  if beat > 140
    if beat < 210
      v = 200
    end
  end
  for i = 0 to numled()-1
    sethsv(i, hue, 255, v)
  next
  show()
end
