import os, socket
import json


with open("wrangler_nids") as fd:
    for line in fd:
        try:
            nid, fqdn, hn = line.split()
        except: continue
        
        rpc = json.dumps({"hostname" : hn, "nid" : nid})
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

            

        
