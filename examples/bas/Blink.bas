# Inspired by a WLED effect of the same name. The idea is theirs; this sketch is ours.

param speed number(500.0, 80.0, 2000.0, 20.0)
param hue number(0.0, 0.0, 360.0, 5.0)
param duty number(50.0, 10.0, 90.0, 5.0)

setup
  brightness(128)
end

loop(time)
  phase = (time % speed) * 100 / speed
  v = 0
  if phase < duty
    v = 255
  end
  for i = 0 to numled()-1
    sethsv(i, hue, 255, v)
  next
  show()
end
