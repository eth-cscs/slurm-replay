#!/usr/bin/env python


import argparse
import csv


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Recreate group and passwd file from list of user, group, uid and gid.')
    parser.add_argument('-passwd', help='file containing tuple of username:uid:gid', required=True, default=False)
    parser.add_argument('-group', help='file containing tuple of groupname:gid', required=True, default=False)
    parser.add_argument('-name', help='prefix for output.', required=True, default=False)
    parser.add_argument('-replayuser', help='replay user in the from username:uid', default='replayuser')
    cmdline = parser.parse_args()

    passwd=dict()
    group=dict()
    all_uids=[]
    all_gids=[]
    all_usernames=[]

    with open(cmdline.group) as csvfile:
        spamreader = csv.reader(csvfile, delimiter=':')
        for row in spamreader:
            groupname, gid = row
            if gid not in group:
               group[gid]=groupname

    with open(cmdline.passwd) as csvfile:
        spamreader = csv.reader(csvfile, delimiter=':')
        for row in spamreader:
            username, uid, gid = row
            if not gid in passwd:
                passwd[gid]=list()
                all_gids.append(int(gid))
            if (username, uid) not in passwd[gid]:
               passwd[gid].append((username, uid))
               all_uids.append(int(uid))
               all_usernames.append(username)

    with open(cmdline.name+'_etc_group','w') as f:
        for gid, name in group.items():
            if len(passwd[gid]) > 0:
               f.write(name+':x:'+gid+':'+','.join([u[0] for u in passwd[gid]])+'\n')
            else:
               f.write(name+':x:'+gid+':\n')

    with open(cmdline.name+'_etc_passwd','w') as f:
        for gid, luser in passwd.items():
            for l in luser:
                name, uid = l
                f.write(name+':*:'+uid+':'+gid+':x:/users/'+name+':/usr/local/bin/bash\n')

    # check for the replayuser
    uid = 1000
    gid = 1000
    replayuser = cmdline.replayuser
    if replayuser not in all_usernames:
       if uid in all_uids:
           print("Warning - uid=1000 already in use but not by the replay user: %s",replayuser)
       if gid in all_gids:
           print("Warning - gid=1000 already in use but not by the replay user group")
       with open(cmdline.name+'_etc_passwd','a') as f:
          f.write(replayuser+':*:'+str(uid)+':'+str(gid)+':x:/users/'+replayuser+':/usr/local/bin/bash\n')
       with open(cmdline.name+'_etc_group','a') as f:
          f.write('replaygrp:x:'+str(gid)+':'+replayuser+'\n')

