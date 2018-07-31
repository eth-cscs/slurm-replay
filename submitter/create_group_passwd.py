#!/usr/bin/env python


import argparse
import csv


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Recreate group and passwd file from list of user, group, uid and gid.')
    parser.add_argument('-passwd', help='file containing tuple of username:uid:gid', required=True, default=False)
    parser.add_argument('-group', help='file containing tuple of groupname:gid', required=True, default=False)
    parser.add_argument('-name', help='prefix for output.', required=True, default=False)
    cmdline = parser.parse_args()

    passwd=dict()
    group=dict()
    unknown_counter = 0
    unknown=dict()

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
            if username == '(null)':
                if uid not in unknown:
                   username = 'unknown'+str(unknown_counter)
                   unknown_counter+=1
                   unknown[uid]=username
                else:
                    username = unknown[uid]
            if (username, uid) not in passwd[gid]:
               passwd[gid].append((username, uid))



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

