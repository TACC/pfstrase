#!/bin/bash
#/etc/rc.d/init.d/pfstrase

# Source function library.
. /etc/init.d/functions

CONF_FILE=/etc/pfstrase/pfstrase.conf
PID_FILE=/var/run/pfstrased.pid

start() {
    echo -n "Starting pfstrase"
    /usr/sbin/pfstrased -c $CONF_FILE -d	
    return 0
}

stop() {
    echo -n "Stopping pfstrase"
    cat ${PID_FILE} | /bin/kill -INT `awk '{print $1}'`
    rm -f ${PID_FILE}
    return 0
}

reload() {
    echo -n "Reloading pfstrase config file"
    cat ${PID_FILE} | /bin/kill -HUP `awk '{print $1}'`
    return 0
}

status() {
    if pidof pfstrase > /dev/null; then
	if [ "$1" != "quiet" ] ; then
	        echo "pfstrase is running"
		fi
	RETVAL=1
    else
	if [ "$1" != "quiet" ] ; then 
	    echo "pfstrase is not running"
	    fi
	RETVAL=0   
    fi
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    status)
        ;;
    restart)
        stop
        start
        ;;
    reload)
        ;;
    *)
        echo "Usage: pfstrase {start|stop|status|reload|restart}"
 exit 1
        ;;
esac
exit $?
