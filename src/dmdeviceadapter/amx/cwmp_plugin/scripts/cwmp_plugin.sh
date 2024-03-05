#!/bin/sh

[ -f /etc/environment ] && source /etc/environment
ulimit -c ${ULIMIT_CONFIGURATION:-0}
name="cwmp_plugin"

proxypath="Device.ManagementServer."
realpath="ManagementServer."

case $1 in
    start|boot)
        source /etc/environment
        ubus -t 30 wait_for ProxyManager
        ubus call ProxyManager register "{'proxy' : '$proxypath','real' : '$realpath'}"
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
