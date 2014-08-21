#!/bin/bash
#
#

#EXT_IF=en0
#EXT_IP=10.0.0.200
#CTUNNEL_ROUTES=10.0.0.0/24,10.0.5.0/24,10.0.6.0/24,10.1.0.0/24,10.0.1.0/24
#CTUNNEL_DEV=ppp0

function forward_linux
{
    EXT_IF=eth0
    IFS=','
    EXT_IP=$(/sbin/ifconfig eth0 | grep 'inet addr' | awk -F: '{ print $2 }' | awk '{ print $1 }')
    for i in ${CTUNNEL_ROUTES}
    do
        iptables -t nat -A POSTROUTING -s ${i} -o ${EXT_IF} -j SNAT --to ${EXT_IP} 
    done
    unset IFS
}

function forward_darwin
{
    EXT_IF=en0
    # Turn off scopedroute (bug in Lion and on), turn on IP Forwarding
    # /Library/Preferences/SystemConfiguration/com.apple.Boot.plist
    #  <dict>
    #   <key>Kernel Flags</key>
    #   <string>net.inet.ip.scopedroute=0</string>
    #  </dict>
    # net.inet.ip.scopedroute=0
    # net.inet.ip.forwarding=1
    PFCTL=/sbin/pfctl
    TMP_CONF=/tmp/pf.conf.$(date +%s)

    IFS=','
    for i in ${CTUNNEL_ROUTES}
    do
      echo "nat on ${EXT_IF} from ${i} to any -> (${EXT_IF})" >> ${TMP_CONF}
    done
    echo "nat on ${EXT_IF} from ${CTUNNEL_GATEWAY} to any -> (${EXT_IF})" >> ${TMP_CONF}
    echo "pass on { lo0, ${CTUNNEL_DEV} }" >> ${TMP_CONF}
    ${PFCTL} -v -e -f ${TMP_CONF} -E
    rm -f ${TMP_CONF}
    unset IFS
    return 0
}

env | grep CTUN

case "$(uname -s)" in
   "Linux") echo "test";;
   "Darwin") forward_darwin;;
esac

exit 0
