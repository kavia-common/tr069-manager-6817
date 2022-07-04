#!/bin/sh

ulimit -c unlimited
name="cwmp_plugin"

case $1 in
    start|boot)
        source /etc/environment
        LD_LIBRARY_PATH=/opt/prplos/usr/lib cwmp_plugin -D
        ;;
    stop|shutdown)
        if [ -f /var/run/cwmp_plugin.pid ]; then
            kill `cat /var/run/cwmp_plugin.pid`
        fi
        ;;
    debuginfo)
	ubus-cli "ManagementServer.?"
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    log)
	echo "TODO log cwmp_plugin"
	;;
    *)
        echo "Usage : $0 [start|boot|stop|debuginfo|log]"
        ;;
esac
