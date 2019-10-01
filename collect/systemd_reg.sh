make

hostlist="c0,c1,oss0,oss1,mds0"


pdsh -w ${hostlist} systemctl stop pfstrase
pdsh -w ${hostlist} rm /run/pfstrased

pdcp -w ${hostlist} pfstrased /run
pdcp -w ${hostlist} pfstrase.service /etc/systemd/system/
pdsh -w ${hostlist} systemctl daemon-reload
pdsh -w ${hostlist} systemctl start pfstrase

