#!/usr/bin/env python
import os, socket
import json
from hostlist import expand_hostlist
from commands import getoutput
import sys

def print_help():
    print("Usage: map_nids.py [SERVER_HOSTNAME]")
    exit(0)

server_host = None
server_port = None

if len(sys.argv) != 2:
    print_help()

if sys.argv[1] in ['help', '--help', '-h']:
    print_help()

if ":" in sys.argv[1]:
    server_host = sys.argv[1].split(":")[0]
    server_port = int(sys.argv[1].split(":")[1])
else:
    server_host = sys.argv[1]
    server_port = 5672

sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

print("Connecting to " + server_host + ":" + str(server_port) + "...")
try:
    sd.connect((server_host, server_port))
except:
    print("Failed to connect.") 
    exit(1)

rdata = getoutput("squeue -o \"%i %u %j %N\" -t R -a | grep -v NODELIST")
idata = getoutput("sinfo -o \"%N\" | grep -v NODELIST")

if "command not found" in idata:
    print("SLURM not installed.")
    exit(1)

rpcs =[] 
for line in idata.split('\n'):
    hostlist = line    
    for host in expand_hostlist(hostlist):
        rpcs += [{"hostname" : host, "jid" : '-', "uid" : '-'}]

for line in rdata.split('\n'):
    jobid, user, name, hostlist = line.split()[0:4]    
    for host in expand_hostlist(hostlist):
        rpcs += [{"hostname" : host, "jid" : jobid, "uid" : user}]
print(rpcs)
print("Sending JSON to server...")
sd.sendall(json.dumps(rpcs))
print("Done.")

