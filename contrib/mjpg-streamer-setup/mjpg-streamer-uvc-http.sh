#!/bin/sh

##########################################################
# Input Parameters
##########################################################

# It's safer (and more convenient) to specifiy the USB ID
# of the UVC camera. Easily detected by 'lsusb'.
# This way, if there are multiple cameras, /dev/videoX can change
# without affecting the script.
VIDEO_USB_ID=usb:1908:3272

# Empty to use default resolution
RESOLUTION=640x480

# Empty for no FPS limit
FPS=5

# Empty for no min-size bad-frame detection
FRAME_MINSIZE=10000 #bytes

# Empty to send all frames
FRAME_EVERY=3

##########################################################
# Output parameters
##########################################################

TCP_PORT=8081

# Leave empty for default (0.0.0.0), or set something like 127.0.0.1
LISTEN_ADDR=

# Empty for no website
WWW_DIR=/usr/local/share/mjpg-streamer/www/

##########################################################
# Don't change anything below this point
##########################################################

die()
{
  base=$(basename "$0")
  echo "$base: error: $*" >&2
  exit 1
}


get_dev_usb_info()
{
  test -e "$1" \
    || { echo "file '$1' not found" >&2 ; return 1 ; }
  expr /foo/bar : '^[-_a-z0-9A-Z/]*$' >/dev/null \
    || { echo "file '$1' contains forbidden characters" >&2 ; return 1 ; }
  # Generally, eval is dangerous: http://mywiki.wooledge.org/BashFAQ/048
  # but we sanitize the input, above.
  ( eval $(udevadm info -x -q property -n "$1") ;
    echo $ID_BUS:$ID_VENDOR_ID:$ID_MODEL_ID )
}

get_dev_for_usb_id()
{
  __device=
  for __d in /dev/video* ; do
    __dev_usb_id=$(get_dev_usb_info "$__d") || continue
    if test "$1" = "$__dev_usb_id" ; then
      echo "$__d"
      return 0
    fi
  done
  return 1
}


device=$(get_dev_for_usb_id "$VIDEO_USB_ID") \
    || die "USB Camera device (e.g. /dev/video0) not found, " \
           "looking for USD ID '$VIDEO_USB_ID'"


input_cmd="input_uvc.so -d $device"
test "$RESOLUTION" && input_cmd="$input_cmd -r $RESOLUTION"
test "$FRAME_MINSIZE" && input_cmd="$input_cmd -m $FRAME_MINSIZE"
test "$FPS" && input_cmd="$input_cmd -f $FPS"
test "$FRAME_EVERY" && input_cmd="$input_cmd -e $FRAME_EVERY"


output_cmd="output_http.so"
test "$TCP_PORT" && output_cmd="$output_cmd -p $TCP_PORT"
test "$WWW_DIR"  && output_cmd="$output_cmd -w $WWW_DIR"
test "$LISTEN_ADDR"  && output_cmd="$output_cmd -l $LISTEN_ADDR"

exec mjpg_streamer -o "$output_cmd" -i "$input_cmd"

