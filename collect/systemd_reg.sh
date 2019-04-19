make

hostlist="c0,c1,oss0,oss1,mds0"

pdcp -w ${hostlist} pfstrased /run
pdcp -w ${hostlist} pfstrase.service /etc/systemd/system/

pdsh -w ${hostlist} systemctl daemon-reload
pdsh -w ${hostlist} systemctl restart pfstrase

