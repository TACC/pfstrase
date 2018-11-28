#!/bin/bash

# Remove the first several characters of root's authorized keys file to allow ssh entry
sed -i 's:^.*\(ssh-rsa\):\1:' /root/.ssh/authorized_keys

yum -y update
yum -y install vim git emacs
