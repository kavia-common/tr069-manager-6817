#DEFAULT VALUES FOR RASPPI
export DEVICECATEGORY=""
export MANUFACTURER="defaultVal_manufacturer"
export MANUFACTUREROUI="000111"
export CID="000111"
export PEN="000111"


#VALUES USING BASH
export UPTIME=$(awk '{ printf("%i", $1) }' /proc/uptime)
export MODELNAME=$(awk '{ for (x=1; x<=NF-1; x++) printf("%s ", $x) }' /proc/device-tree/model)
export MODELNUMBER=$(awk '{ print $NF }' /proc/device-tree/model)
export DESCRIPTION=$(awk '$1 ~ /DISTRIB_DESCRIPTION/' /etc/openwrt_release | cut -d "'" -f2 | cut -d "'" -f1)
export PRODUCTCLASS=$(cat /proc/device-tree/model)
export SERIALNUMBER=$(cat /proc/device-tree/serial-number)
export HARDWAREVERSION=$(awk '$1 ~ /DISTRIB_TARGET/' /etc/openwrt_release | cut -d "'" -f2 | cut -d "'" -f1)
export SOFTWAREVERSION=$(grep -oE 'Linux version [0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}' /proc/version | awk '{ print $NF }')
export TOTALMEMORY=$(awk '$1 ~ /MemTotal/{ print $2 }' /proc/meminfo)
export FREEMEMORY=$(awk '$1 ~ /MemFree/{ print $2 }' /proc/meminfo)
