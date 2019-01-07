#!/bin/sh

##########################################################
# Input Parameters
##########################################################

# It's safer (and more convenient) to specifiy the USB ID
# of the UVC camera. Easily detected by 'lsusb'.
# This way, if there are multiple cameras, /dev/videoX can change
# without affecting the script.

#VIDEO_USB_ID=usb:046d:0870 ## Logitech, Inc. QuickCam Express
VIDEO_USB_ID=usb:046d:08f6 ## Logitech, Inc. QuickCam Messenger Plus


# Empty to use default resolution
#RESOLUTION=352x292

# Empty for no FPS limit
FPS=5

# Format - leave empty for MJPG, use "-g" for GRGR/BGBG, use "-y" for YUV
FORMAT="-g"

##########################################################
# Output parameters
##########################################################

TCP_PORT=8083

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


params="-d $device"
test "$RESOLUTION" && params="$params -r $RESOLUTION"
test "$FPS" && params="$params -f $FPS"
test "$TCP_PORT" && params="$params -p $TCP_PORT"
test "$FORMAT" && params="$params $FORMAT"

exec uvc_stream $params

