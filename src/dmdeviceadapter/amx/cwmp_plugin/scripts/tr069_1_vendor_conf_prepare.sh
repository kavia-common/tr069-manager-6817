

#!/bin/sh

#tr69 will look for a file with the name (/tmp/SERIALNIMBER_vendor_conf.odl)
# $1 will contain the file full name

#delete old
[ -f $1] && rm $1;

#send the cwmp_plugin.odl file as a dummy test
cp /etc/config/cwmp_plugin/odl/cwmp_plugin.odl $1;
