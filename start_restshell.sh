#!/bin/bash

TOKEN="${RESTSHELL_TOKEN}"

if [ -z "$TOKEN" ]; then
   TOKEN=scops
fi
if [ -z "$RESTSHELL_PORT" ]; then
   RESTSHELL_PORT=8081
fi

LOG_DIR="/${REPLAY_USER}/var/log/restshell"
HOST_IP=$(hostname -I | awk '{print $1}')
echo -n "Starting rest-shell..."
TOKEN=$TOKEN rest-shell --server ${HOST_IP}:${RESTSHELL_PORT} > $LOG_DIR/output.log 2> $LOG_DIR/error.log &
echo " TOKEN=$TOKEN rest-shell ${HOST_IP}:${RESTSHELL_PORT} ... done"
