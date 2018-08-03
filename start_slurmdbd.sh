#!/bin/bash

VERBOSE="-v"
SLURM_REPLAY_LIB="$1"

SLURM_DIR="/$REPLAY_USER/slurmR"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"

# Start sql server if not yet started
if ! pgrep -x "mysqld" > /dev/null
then
echo -n  "Starting mysql... "
if [ ! -f "/$REPLAY_USER/var/lib/mysql-bin.index" ]; then
rm -Rf /$REPLAY_USER/var/lib/*
rm -Rf /$REPLAY_USER/run/mysqld/*
mysql_install_db --user="$REPLAY_USER" --basedir="/usr" --datadir="/$REPLAY_USER/var/lib" &> /dev/null
fi
mysqld_safe --datadir="/$REPLAY_USER/var/lib" &> /dev/null &
sleep 5
echo "done."
fi

# Configure the database
CREATE_DATABASE="
DROP USER IF EXISTS '$REPLAY_USER'@'localhost';
CREATE USER '$REPLAY_USER'@'localhost' IDENTIFIED BY '';
GRANT ALL ON slurm_acct_db.* TO '$REPLAY_USER'@'localhost';
DROP DATABASE IF EXISTS slurm_acct_db;
CREATE DATABASE slurm_acct_db;
"

mysql -u root -Bse "$CREATE_DATABASE"

echo -n  "Starting slurmdbd... "

killall -q -9 slurmdbd

eval "$SLURM_REPLAY_LIB slurmdbd $VERBOSE"
sleep 1
echo "done."

echo -n  "Populating database... "
sacctmgr -i add cluster Daint >/dev/null

# Command to obtain data:
# mysqldump -u <user> -p <password> acct_table acct_coord_table qos_table tres_table user_table <cluster>_assoc_table > slurmdb_tbl_slurm-17.02.9.sql
mysql -u root slurm_acct_db < ../data/slurmdb_tbl_slurm-${SLURM_VERSION}.sql

sleep 5
echo "done."
