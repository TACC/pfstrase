hostlist="c0,c1,oss0,oss1,mds0"

pdsh -w ${hostlist} systemctl stop pfstrase.service
pdsh -w ${hostlist} rm /etc/systemd/system/pfstrase.service
pdsh -w ${hostlist} rm /run/pfstrased
