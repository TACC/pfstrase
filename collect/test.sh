#pkill -9 -f pfstrase_server
#make
#echo "---------------------------------------"
#./pfstrase_server &

echo '{"hostname" : "c1", "jid" : "101", "uid" : "test1", "nid" : "192.168.0.1@o2ib"}' | nc localhost 5672
echo '{"hostname" : "c1", "jid" : "101", "uid" : "test2", "nid" : "192.168.0.1@o2ib"}' | nc localhost 5672
echo '{"hostname" : "c0", "jid" : "102", "uid" : "test2", "nid" : "192.168.0.2@o2ib"}' | nc localhost 5672
#ssh -tq mds0 'systemctl restart pfstrase'
#ssh -tq mds0 'systemctl restart pfstrase'
#ssh -tq oss0 'systemctl restart pfstrase'
#ssh -tq oss1 'systemctl restart pfstrase'

#pkill -9 -f pfstrase_server
