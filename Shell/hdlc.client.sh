#!/bin/sh
# 2014-05-08 15:38
# 2014-05-15 16:24
#
WAN_LOCAL_IP=10.10.10.11
WAN_REMOTE_IP=10.10.10.10
LOCAL_NETMASK=255.255.0.0
ASTERISK=/app/asterisk/sbin/asterisk

${ASTERISK} -rx "module unload chan_dahdi.so"
echo "
fxsls=1-2
fxols = 3
fxols = 4
loadzone = us
defaultzone = us
echocanceller = oslec,1-4

span=3,1,0,esf,b8zs
bchan=5-26
nethdlc=27
hardhdlc=28
echocanceller = oslec,5-26
" > /cfg/etc/dahdi/system.conf
nvram set pri1_linemode=1
/etc/rc.d/rc.dahdi restart

i=0
while [ $i -lt 32 ] ;
do
        svip_ctl clrmode 1 $i > /dev/null
        svip_ctl clrmode 2 $i > /dev/null
        i=`expr $i + 1`
done

sethdlc hdlc0 cisco
ifconfig hdlc0 ${WAN_LOCAL_IP} pointopoint ${WAN_REMOTE_IP}
route del default
route add default gateway ${WAN_REMOTE_IP} dev hdlc0
${ASTERISK} -rx "module load chan_dahdi.so"
