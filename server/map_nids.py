#!/bin/env python
import os, socket
import json
import sys

def print_help():
    print("Usage: map_nids.py [SERVER_HOSTNAME] [NID_FILENAME]")
    exit(0)

server_host = None
server_port = None

if len(sys.argv) != 3:
    print_help()

if sys.argv[1] in ['help', '--help', '-h']:
    print_help()    

if ":" in sys.argv[1]:
    server_host = sys.argv[1].split(":")[0]
    server_port = int(sys.argv[1].split(":")[1])
else:
    server_host = sys.argv[1]
    server_port = 5672

nid_file = sys.argv[2]

sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

if not os.path.isfile(nid_file):
   print("File " + nid_file + " not found.")
   exit(1)

print("Connecting to " + server_host + ":" + str(server_port))

try:
    sd.connect((server_host, server_port))
except:
    print("Failed to connect.")
    exit(1)

rpcs = []
print("Reading " + nid_file + "...")
with open(nid_file) as fd:
    for line in fd:
        try:
            nid, fqdn, hn = line.split()
            system = hn.split('.')[1]
        except: pass
        try:
            nid, hn = line.split()
            system = hn.split('.')[1]
        except: continue
        rpcs += [{"hostname" : hn, "nid" : nid, "system" : system}]
#print(rpcs)
if not rpcs:
    print("No NIDS found.")
    exit(1)
else:
    print("Found " + str(len(rpcs)) + " NIDs")
    print("Sending JSON to server...")
    sd.sendall(json.dumps(rpcs))
    print("Done.")    
    
