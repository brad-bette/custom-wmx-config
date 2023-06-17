Xephyr :30 -screen 1024x768 &
DISPLAY=:30 ./wmx.old
kill -9 $(pgrep Xephyr)
