#!/usr/bin/env python


import os, socket
import json
from hostlist import expand_hostlist
from commands import getoutput

data = getoutput("squeue -o \"%i %u %j %N\" -t R -a | grep -v NODELIST")
recs = []
for line in data.split('\n'):
    
    jobid, user, name, hostlist = line.split()[0:4]    
    for host in expand_hostlist(hostlist):
        rpc = json.dumps({"hostname" : host, "jid" : jobid, "uid" : user})
        print(rpc)
        MSGLEN=len(rpc)
        totalsent = 0
        sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sd.connect(("login1.wrangler.tacc.utexas.edu", 5672))
        while totalsent < MSGLEN:
            sent = sd.send(rpc[totalsent:])
            if sent == 0:
                raise RuntimeError("socket connection broken")
            totalsent = totalsent + sent

            
