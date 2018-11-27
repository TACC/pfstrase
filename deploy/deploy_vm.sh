#!/bin/bash
#
# W. Cyrus Proctor
# 2018-11-26

if [ $# -ne 1 ]; then
 echo "Please provide the unique VM name to be created"
 exit -1
fi

# From `openstack network show cproctor_net`
nic="e8d17d08-4787-4336-b454-8e29f8ecfa62"
# From `openstack keypair list`
key="lustre-key"
# From `openstack image list`
img="28d78e82-05b4-4325-990c-6dffaf907453"
# From `openstack flavor list`
flav="m1.small"
# From `openstack security group list`
secgroup="cproctor_lustre"
# To allow root remote login
datafile="./remove_authkey_limitation.sh"

openstack server create --flavor "${flav}" --image "${img}" --nic net-id="${nic}" --security-group "${secgroup}" --key-name "${key}" --user-data "${datafile}" "${1}"
