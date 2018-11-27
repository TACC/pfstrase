#!/bin/bash

keyname="lustre-key"

if [ ! -f /root/.ssh/id_rsa ]; then
  openstack keypair create "${keyname}" > /root/.ssh/id_rsa
  chmod 0600 /root/.ssh/id_rsa
else
  echo "A private RSA key already exists! Aborting."
  exit -1
fi

if [ ! -f /root/.ssh/id_rsa.pub ]; then
  openstack keypair show "${keyname}" --public-key >> /root/.ssh/id_rsa.pub
  openstack keypair show "${keyname}" --public-key >> /root/.ssh/authorized_keys
else
  echo "A public RSA key already exists! Aborting."
  exit -1
fi  
