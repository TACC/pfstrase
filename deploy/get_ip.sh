#!/bin/bash

get_ip_address () {
  if [ $# -ne 1 ]; then
   echo "Please provide the unique VM name to be queried"
   exit -1
  fi
  
  openstack server list --name "${1}" -f value -c Networks | awk -F "=" '{print $2}' | cut -f1 -d","
}
