import os, socket
import json

sd = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sd.connect(("login1.stockyard.tacc.utexas.edu", 5672))

rpcs = []
with open("stockyard_nids") as fd:
    for line in fd:
        try:
            nid, fqdn, hn = line.split()
        except: continue    
        rpcs += [{"hostname" : hn, "nid" : nid}]
print(rpcs)
sd.sendall(json.dumps(rpcs))
        
