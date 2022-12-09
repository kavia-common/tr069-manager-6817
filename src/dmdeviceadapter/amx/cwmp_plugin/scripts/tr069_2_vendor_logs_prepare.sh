

#!/bin/sh

#tr69 will look for a file with the name (/tmp/SERIALNIMBER_vendor_logs.log)
#by default cwmpd will send the /var/log/messages


# $1 will contain the file full name
[ -f $1] && rm $1;

#send the syslog file
cp /var/log/messages $1;
