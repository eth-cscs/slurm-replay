#!/bin/bash

VERBOSE="-v"

# replay user should be set as SlurmUser, SlurmdUser and AccountingStorageUser in the slurm.conf file
REPLAY_USER=slurm

SLURM_DIR="/home/slurm/slurmR"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"

# Start sql server if not yet started
if ! pgrep -x "mysqld" > /dev/null
then
echo -n  "Starting mysql... "
sudo mysqld_safe -u mysql &> /dev/null &
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

rm -Rf $SLURM_DIR/log/*
mkdir $SLURM_DIR/log/state
mkdir $SLURM_DIR/log/slurmd
mkdir $SLURM_DIR/log/archive

cp conf/slurmdbd.conf etc
slurmdbd $VERBOSE
sleep 1
echo "done."

echo -n  "Populating database... "
sacctmgr -i add cluster Daint >/dev/null

# Command to obtain data:
# mysqldump -u test -p slurmdbd_test acct_table acct_coord_table qos_table tres_table user_table daint_assoc_table > slurmdb_tbl.sql
mysql -u root slurm_acct_db < ../data/slurmdb_tbl.sql

if [ -f "../data/userreplay_assoc_tbl.sql" ]; then
    mysql -u root slurm_acct_db < ../data/userreplay_assoc_tbl.sql
else
    echo -n " it will take several seconds... "
    # add replay user to all accounts
    DAINT_ACCOUNTS=$(mysql -u root slurm_acct_db -Bse "select distinct a.name from acct_table as a join daint_assoc_table as c on a.name = c.acct;")
    sacctmgr -i add user "$REPLAY_USER" Account=$(echo "$DAINT_ACCOUNTS" | tr -s "\n" ",") Cluster=daint >/dev/null
    mysqldump -u "$REPLAY_USER" slurm_acct_db daint_assoc_table > ../data/userreplay_assoc_tbl.sql
fi
sleep 5
echo "done."
