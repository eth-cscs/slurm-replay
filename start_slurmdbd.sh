#!/bin/bash

VERBOSE="-v"
SLURM_REPLAY_LIB="$1"

SLURM_DIR="/home/$REPLAY_USER/slurmR"
export PATH="$SLURM_DIR/bin:$SLURM_DIR/sbin:$PATH"

# Start sql server if not yet started
if ! pgrep -x "mysqld" > /dev/null
then
echo -n  "Starting mysql... "
if [ ! -f "/home/$REPLAY_USER/var/lib/mysql-bin.index" ]; then
rm -Rf /home/$REPLAY_USER/var/lib/*
rm -Rf /home/$REPLAY_USER/run/mysqld/*
mysql_install_db --user="$REPLAY_USER" --basedir="/usr" --datadir="/home/$REPLAY_USER/var/lib" &> /dev/null
fi
mysqld_safe --datadir="/home/$REPLAY_USER/var/lib" &> /dev/null &
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

# configure slurmdbd
FILE="etc/slurmdbd.conf"
cp conf/slurmdbd.conf $FILE
sed -i -e "s/SlurmUser[[:space:]]*=slurm/SlurmUser=$REPLAY_USER/" $FILE
sed -i -e "s/StorageUser[[:space:]]*=slurm/StorageUser=$REPLAY_USER/" $FILE
sed -i -e "/PidFile[[:space:]]*=/ s/PidFile[[:space:]]*=.*/PidFile=\/home\/$REPLAY_USER\/slurmR\/log\/slurmdbd.pid/" $FILE
sed -i -e "/LogFile[[:space:]]*=/ s/LogFile[[:space:]]*=.*/LogFile=\/home\/$REPLAY_USER\/slurmR\/log\/slurmdbd.log/" $FILE

eval "$SLURM_REPLAY_LIB slurmdbd $VERBOSE"
sleep 1
echo "done."

echo -n  "Populating database... "
sacctmgr -i add cluster Daint >/dev/null

# Command to obtain data:
# mysqldump -u test -p slurmdbd_test acct_table acct_coord_table qos_table tres_table user_table daint_assoc_table > slurmdb_tbl.sql
mysql -u root slurm_acct_db < ../data/slurmdb_tbl_slurm-${SLURM_VERSION}.sql

if [ -f "../data/${REPLAY_USER}_assoc_tbl_slurm-${SLURM_VERSION}.sql" ]; then
    mysql -u root slurm_acct_db < ../data/${REPLAY_USER}_assoc_tbl_slurm-${SLURM_VERSION}.sql
else
    echo -n " that will take several seconds... "
    # add replay user to all accounts
    DAINT_ACCOUNTS=$(mysql -u root slurm_acct_db -Bse "select distinct a.name from acct_table as a join daint_assoc_table as c on a.name = c.acct;")
    sacctmgr -i add user "$REPLAY_USER" Account=$(echo "$DAINT_ACCOUNTS" | tr -s "\n" ",") Cluster=daint >/dev/null
    mysqldump -u "$REPLAY_USER" slurm_acct_db daint_assoc_table > ../data/${REPLAY_USER}_assoc_tbl_slurm-${SLURM_VERSION}.sql
fi
sleep 5
echo "done."
