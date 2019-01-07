#!/bin/sh

##########################################################
# Input Parameters
##########################################################

# For complete parameters list see:
# https://github.com/jacksonliam/mjpg-streamer/tree/master/mjpg-streamer-experimental/plugins/input_raspicam

WIDTH=640
HEIGHT=480

# Leave empty for default (5)
FPS=10

# leave empty for default (85)
QUALITY=50

# Examples:
#   -ex night -vf"
RASPICAM_OPTIONS=


##########################################################
# Output parameters
##########################################################
TCP_PORT=8082

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


input_cmd="input_raspicam.so -x $WIDTH -y $HEIGHT"
test "$FPS" && input_cmd="$input_cmd -fps $FPS"
test "$QUALITY" && input_cmd="$input_cmd -quality $QUALITY"
test "$RASPICAM_OPTIONS" && input_cmd="$input_cmd $RASPICAM_OPTIONS"

output_cmd="output_http.so"
test "$TCP_PORT" && output_cmd="$output_cmd -p $TCP_PORT"
test "$WWW_DIR"  && output_cmd="$output_cmd -w $WWW_DIR"
test "$LISTEN_ADDR"  && output_cmd="$output_cmd -l $LISTEN_ADDR"

exec mjpg_streamer -o "$output_cmd" -i "$input_cmd"
