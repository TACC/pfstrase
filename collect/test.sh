pkill -9 -f map_server
make
echo "---------------------------------------"
./map_server &

echo '{"hostname" : "c1", "jid" : "101", "uid" : "test1", "nid" : "192.168.0.1@o2ib"}' | nc c1 8213
echo '{"hostname" : "c1", "jid" : "101", "uid" : "test2", "nid" : "192.168.0.1@o2ib"}' | nc c1 8213
echo '{"hostname" : "c0", "jid" : "102", "uid" : "test2", "nid" : "192.168.0.2@o2ib"}' | nc c1 8213
ssh -t mds0 'systemctl restart pfstrase'
ssh -t mds0 'systemctl restart pfstrase'
ssh -t oss0 'systemctl restart pfstrase'
ssh -t oss1 'systemctl restart pfstrase'

#pkill -9 -f map_server