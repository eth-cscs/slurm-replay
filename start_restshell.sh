#!/bin/bash

TOKEN=${RESTSHELL_TOKEN}
HOST_PORT=${RESTSHELL_PORT}
if [ -z "$TOKEN" ]; then
   TOKEN=scops
fi
if [ -z "$HOST_PORT" ]; then
   HOST_PORT=8081
fi

LOG_DIR="/replayuser/var/log/restshell"
HOST_IP=$(hostname -I | awk '{print $1}')
echo -n "Starting rest-shell..."
TOKEN=$TOKEN rest-shell --server ${HOST_IP}:${HOST_PORT} > $LOG_DIR/output.log 2> $LOG_DIR/error.log &
echo " TOKEN=$TOKEN rest-shell ${HOST_IP}:${HOST_PORT} ... done"
