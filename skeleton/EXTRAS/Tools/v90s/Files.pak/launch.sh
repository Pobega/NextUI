#!/bin/sh

cd $(dirname "$0")

HOME="$SDCARD_PATH"
CFG="v90s.cfg"

./NextCommander --config $CFG > "${LOGS_PATH:-/tmp}/commander.txt" 2>&1
