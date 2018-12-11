#!/bin/bash

# Remove the first several characters of root's authorized keys file to allow ssh entry
sed -i 's:^.*\(ssh-rsa\):\1:' /root/.ssh/authorized_keys

#yum -y update
yum -y install vim git emacs

# Add in Lustre repositories
cat >> /etc/yum.repos.d/lustre.repo <<EOF
[lustre-server]
name=CentOS-$releasever - Lustre
baseurl=https://downloads.hpdd.intel.com/public/lustre/latest-feature-release/el7/server/
gpgcheck=0
[e2fsprogs]
name=CentOS-$releasever - Ldiskfs
baseurl=https://downloads.hpdd.intel.com/public/e2fsprogs/latest/el7/
gpgcheck=0
[lustre-client]
name=CentOS-$releasever - Lustre
baseurl=https://downloads.hpdd.intel.com/public/lustre/latest-feature-release/el7/client/
gpgcheck=0
EOF

yum -y upgrade e2fsprogs
yum -y install lustre-tests

# Ethernet for JS
echo "options lnet networks=tcp0(eth0)" >> /etc/modprobe.d/lnet.conf

cat >> /etc/sysconfig/modules/lnet.modules <<EOF
#!/bin/sh
if [ ! -c /dev/lnet ] ; then
    exec /sbin/modprobe lnet >/dev/null 2>&1
fi
EOF
