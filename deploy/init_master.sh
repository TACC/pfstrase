#!/bin/bash

yum -y update
yum -y install centos-release-openstack-rocky
yum -y install python2-openstackclient python2-openstacksdk
yum -y install vim git emacs

