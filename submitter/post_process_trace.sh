#!/bin/bash

TRACE_NAME=$1

passwd_header="root:x:0:0::/root:/bin/bash
bin:x:1:1::/:/sbin/nologin
daemon:x:2:2::/:/sbin/nologin
mail:x:8:12::/var/spool/mail:/sbin/nologin
ftp:x:14:11::/srv/ftp:/sbin/nologin
http:x:33:33::/srv/http:/sbin/nologin
nobody:x:65534:65534:Nobody:/:/sbin/nologin
dbus:x:81:81:System Message Bus:/:/sbin/nologin
systemd-journal-remote:x:982:982:systemd Journal Remote:/:/sbin/nologin
systemd-network:x:981:981:systemd Network Management:/:/sbin/nologin
systemd-resolve:x:980:980:systemd Resolver:/:/sbin/nologin
systemd-coredump:x:979:979:systemd Core Dumper:/:/sbin/nologin
uuidd:x:68:68::/:/sbin/nologin"

group_header="root:x:0:root
sys:x:3:bin
mem:x:8:
ftp:x:11:
mail:x:12:
log:x:19:
smmsp:x:25:
proc:x:26:
games:x:50:
lock:x:54:
network:x:90:
floppy:x:94:
scanner:x:96:
power:x:98:
adm:x:999:daemon
wheel:x:998:
kmem:x:997:
tty:x:5:
utmp:x:996:
audio:x:995:
disk:x:994:
input:x:993:
kvm:x:992:
lp:x:991:
optical:x:990:
render:x:989:
storage:x:988:
uucp:x:987:
video:x:986:
users:x:985:
systemd-journal:x:984:
rfkill:x:983:
bin:x:1:daemon
daemon:x:2:bin
http:x:33:
nobody:x:65534:
dbus:x:81:
systemd-journal-remote:x:982:
systemd-network:x:981:
systemd-resolve:x:980:
systemd-coredump:x:979:
uuidd:x:68:"

echo "$group_header" > tmp
cat tmp ${TRACE_NAME}_group | sort | uniq > ${TRACE_NAME}_etc_group
rm tmp ${TRACE_NAME}_group

echo "$passwd_header" > tmp
cat tmp ${TRACE_NAME}_passwd | sort | uniq > ${TRACE_NAME}_etc_passwd
rm tmp ${TRACE_NAME}_passwd

