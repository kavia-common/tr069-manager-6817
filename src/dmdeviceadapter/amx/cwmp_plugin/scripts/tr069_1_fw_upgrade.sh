#!/bin/sh

#By default cwmpd will download th image into /tmp/image_name
#then call this script with image path as argument , to change this
#behavior you will need to override this script to do some specific

#the argument $1 will contain the image full name
sysupgrade -v $1
