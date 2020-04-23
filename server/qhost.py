#!/usr/bin/env python
import os, socket
import json
from hostlist import expand_hostlist
from commands import getoutput

sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sd.connect(("login1.wrangler.tacc.utexas.edu", 5672))

rdata = getoutput("squeue -o \"%i %u %j %N\" -t R -a | grep -v NODELIST")
idata = getoutput("sinfo -o \"%N\" | grep -v NODELIST")

rpcs =[]
for line in idata.split('\n'):
    hostlist = line    
    for host in expand_hostlist(hostlist):
        rpcs += [{"hostname" : host, "jid" : '-', "uid" : '-'}]

for line in rdata.split('\n'):
    jobid, user, name, hostlist = line.split()[0:4]    
    for host in expand_hostlist(hostlist):
        rpcs += [{"hostname" : host, "jid" : jobid, "uid" : user}]


sd.sendall(json.dumps(rpcs))

