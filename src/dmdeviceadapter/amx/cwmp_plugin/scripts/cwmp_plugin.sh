#!/bin/sh

. /usr/lib/amx/scripts/amx_init_functions.sh

name="cwmp_plugin"
datamodel_root="ManagementServer"

case $1 in
    boot)
        export LD_LIBRARY_PATH=/opt/prplos/usr/lib
        process_boot ${name} -D
        ;;
    start)
        export LD_LIBRARY_PATH=/opt/prplos/usr/lib
        process_start ${name} -D
        ;;
    stop)
        process_stop ${name}
        ;;
    shutdown)
        process_shutdown ${name}
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    debuginfo)
        process_debug_info ${datamodel_root}
        ;;
    *)
        echo "Usage : $0 [start|boot|stop|shutdown|debuginfo|restart]"
        ;;
esac
