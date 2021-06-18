#!/bin/sh

name="cwmp_plugin"

case $1 in
    start|boot)
	cwmp_plugin -D
        ;;
    stop|shutdown)
        if [ -f /var/run/cwmp_plugin.pid ]; then
            kill `cat /var/run/cwmp_plugin.pid`
        fi
        ;;
    debuginfo)
	echo "TODO debuginfo"
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    log)
	echo "TODO log cwmp_plugin"
	;;
    *)
        echo "Usage : $0 [start|boot|stop|debuginfo|log]"
        ;;
esac
