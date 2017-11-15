#!/bin/bash

VERBOSE="-v"

# replay user should be set as SlurmUser, SlurmdUser and AccountingStorageUser in the slurm.conf file
REPLAY_USER=maximem

SLURM_DIR="/home/maximem/dev/github/slurm_simulator/slurm_newsim"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"

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

rm -Rf $SLURM_DIR/tmp/*
mkdir $SLURM_DIR/tmp/state
mkdir $SLURM_DIR/tmp/slurmd
mkdir $SLURM_DIR/tmp/archive

slurmdbd $VERBOSE
sleep 1
echo "done."

echo -n  "Populating database... "
sacctmgr -i add cluster Daint >/dev/null

# accounts
#while IFS= read -r line; do
#    acc="$(echo $line | cut -d '|' -f1)"
#    descr="$(echo $line | cut -d '|' -f2)"
#    org="$(echo $line | cut -d '|' -f3)"
#    sacctmgr -i add account "$acc" Cluster=daint Description="$descr" Organization="$org" >/dev/null
#done < "account.dat"


# Command to obtain data:
# mysqldump -u test -p slurmdbd_test acct_table acct_coord_table qos_table tres_table user_table daint_assoc_table daint_resv_table > slurmdb_tbl.sql
mysql -u root slurm_acct_db < slurmdb_tbl.sql

if [ -f "userreplay_assoc_tbl.sql" ]; then
    mysql -u root slurm_acct_db < userreplay_assoc_tbl.sql
else
    echo -n " it will take several seconds... "
    # add replay user to all accounts
    DAINT_ACCOUNTS=$(mysql -u root slurm_acct_db -Bse "select distinct a.name from acct_table as a join daint_assoc_table as c on a.name = c.acct;")
    sacctmgr -i add user "$REPLAY_USER" Account=$(echo "$DAINT_ACCOUNTS" | tr -s "\n" ",") Cluster=daint >/dev/null
    mysqldump -u maximem slurm_acct_db daint_assoc_table > userreplay_assoc_tbl.sql
fi
sleep 5
echo "done."
