#!/bin/sh
set -eu
[ "$(id -u)" -eq 0 ] || { echo 'Root required'; exit 1; }
BACKLIGHT=/sys/class/backlight/backlight
validate_brightness() {
  case "$1" in ''|*[!0-9]*) return 1;; esac
  [ -r "$BACKLIGHT/max_brightness" ] && [ -w "$BACKLIGHT/brightness" ] || return 1
  maximum=$(cat "$BACKLIGHT/max_brightness")
  case "$maximum" in ''|*[!0-9]*) return 1;; esac
  [ "$1" -ge 1 ] && [ "$1" -le "$maximum" ]
}
case "${1:-}" in
 volume)
  [ "$#" -eq 2 ] || exit 2
  case "$2" in ''|*[!0-9]*) exit 2;; esac
  [ "$2" -le 100 ] || exit 2
  [ -r /etc/cherrypi-autostart.conf ] || exit 1
  /root/audio-tools/amixer -c 0 sset 'Headphone' "$2%" unmute
  tmp=/etc/cherrypi-autostart.conf.new.$$
  trap 'rm -f "$tmp"' EXIT
  awk -v volume="$2" '/^VOLUME=/ { if (!found) print "VOLUME=" volume; found=1; next } { print } END { if (!found) print "VOLUME=" volume }' /etc/cherrypi-autostart.conf > "$tmp"
  chmod 644 "$tmp"
  mv "$tmp" /etc/cherrypi-autostart.conf
  sync
  ;;
 brightness)
  [ "$#" -eq 2 ] || exit 2
  validate_brightness "$2" || { echo 'Brightness must be 1..max_brightness'; exit 2; }
  printf '%s\n' "$2" > "$BACKLIGHT/brightness"
  ;;
 settings)
  [ "$#" -eq 3 ] || exit 2
  case "$2" in ''|*[!0-9]*) exit 2;; esac
  [ "$2" -le 100 ] || exit 2
  [ -r /etc/cherrypi-autostart.conf ] || exit 1
  if [ "$3" = 0 ] && [ ! -e "$BACKLIGHT/max_brightness" ]; then
    save_brightness=0
  else
    validate_brightness "$3" || exit 2
    save_brightness=1
  fi
  /root/audio-tools/amixer -c 0 sset 'Headphone' "$2%" unmute
  if [ "$save_brightness" -eq 1 ]; then printf '%s\n' "$3" > "$BACKLIGHT/brightness"; fi
  tmp=/etc/cherrypi-autostart.conf.new.$$
  trap 'rm -f "$tmp"' EXIT
  awk -v volume="$2" -v brightness="$3" -v save_brightness="$save_brightness" '
    /^VOLUME=/ { if (!vfound) print "VOLUME=" volume; vfound=1; next }
    /^BRIGHTNESS=/ { if (save_brightness) { if (!bfound) print "BRIGHTNESS=" brightness; bfound=1; next } }
    { print }
    END { if (!vfound) print "VOLUME=" volume; if (save_brightness && !bfound) print "BRIGHTNESS=" brightness }
  ' /etc/cherrypi-autostart.conf > "$tmp"
  chmod 644 "$tmp"
  mv "$tmp" /etc/cherrypi-autostart.conf
  sync
  ;;
 request-poweroff)
  [ "$#" -eq 1 ] || exit 2
  [ -f /run/cherrypi-autostart.ready ] && [ -r /run/cherrypi-autostart.pid ] || { echo 'Autostart supervisor is not ready'; exit 1; }
  pid=$(cat /run/cherrypi-autostart.pid)
  case "$pid" in ''|*[!0-9]*) exit 1;; esac
  kill -0 "$pid" || exit 1
  : > /run/cherrypi-poweroff-request
  ;;
 *) echo 'Usage: cherrypi-device-control volume 0..100 | brightness 1..max | settings VOLUME BRIGHTNESS | request-poweroff'; exit 2;;
esac
