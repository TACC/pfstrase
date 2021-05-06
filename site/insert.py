#!/usr/bin/env python
import sys, os, json
from datetime import datetime, timezone
import pika 
import psycopg2

conn = psycopg2.connect("dbname=pfstrase_db1 user=postgres")
cur = conn.cursor()

def ingest(channel, method_frame, header_frame, body):
    channel.basic_ack(delivery_tag=method_frame.delivery_tag)

    try:
        message = body.decode()    
    except: 
        print("Unexpected error at decode:", sys.exc_info()[0])
        return
    try: 
        data_json = json.loads(message)
    except: 
        print(sys.exc_info()[0])
        return

    time = datetime.fromtimestamp(data_json["tags"].pop("time"),
                                  tz = timezone.utc)
    tags = data_json["tags"]
    tags["time"]  = time
    data = data_json["data"]
    
    for d in data:
        events = d.pop("stats")                
        d.update(tags)
        keys = list(d.keys()) + ["event_name", "value"]
        cols  = ", ".join(keys)        
        names = ", ".join(["%(" + x + ")s" for x in keys])
        vals = []
        for e,v in events.items():            
            d.update({"event_name" : e, "value" : v})
            vals += [dict(d)]
        cur.executemany("INSERT INTO stats (" + cols + ") values (" + names + ");", vals)
    conn.commit()

parameters = pika.ConnectionParameters("localhost")
connection = pika.BlockingConnection(parameters)
channel = connection.channel()

channel.queue_declare(queue='response', durable = 'True')
channel.basic_consume("response", ingest)
try:
    channel.start_consuming()
except KeyboardInterrupt:
    channel.stop_consuming()

requeued_messages = channel.cancel()
print('Requeued %i messages' % requeued_messages)

connection.close()

cur.close()
conn.close()
