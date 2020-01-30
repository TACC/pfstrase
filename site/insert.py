#!/usr/bin/env python
import sys, os, json
from datetime import datetime, timezone
import pika 
import psycopg2


conn = psycopg2.connect("dbname=pfstrase_db user=postgres")
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
    data = data_json["data"]
    print(tags)
    for d in data:
        events = d.pop("stats")                
        d.update(tags)
        for e,v in events.items():
            d.update({"event_name" : e})
            keys = list(d.keys())
            vals = ", ".join(["%(" + x + ")s" for x in keys])
            cur.execute("INSERT INTO tags (%s) values (%s) returning id;" % (", ".join(keys), vals), d)
            cur.execute("INSERT INTO stats (time, tags_id, value) values (%s, %s, %s);", (time, cur.fetchone()[0], v))  
    conn.commit()

parameters = pika.ConnectionParameters("tacc-stats03")
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
