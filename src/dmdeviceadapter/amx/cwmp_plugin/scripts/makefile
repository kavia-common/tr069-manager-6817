all:
	m4 -D START_ORDER=$(CONFIG_SAH_AMX_TR069_MANAGER_ORDER) -D STOP_ORDER=$(CONFIG_SAH_AMX_TR069_MANAGER_ORDER) cwmp_plugin.sh.m4 > cwmp_plugin.sh

clean:
	rm -rf cwmp_plugin.sh

#
# All these targets must be run regardless of the presence of a file
# named after them.
#
.PHONY: all clean

