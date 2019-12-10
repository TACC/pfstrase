#!/usr/bin/env python
import sys, os, json
from datetime import datetime, timezone
os.environ['DJANGO_SETTINGS_MODULE']='pfstrase.settings'
import django
from django.db.models import Max
django.setup()
from pfs_app.models import Tags, Sysinfo, Obdfilter, Mdt, Lod
import pika 

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
    for d in data: 
        events = d.pop("stats")                
        d.update(tags)
        print(time,d)
        alltags = Tags.objects.create(**d)
        print(events)
        if d["stats_type"] == "sysinfo":            
            Sysinfo.objects.create(time = time, tags = alltags, **events) 
        if d["stats_type"] == "obdfilter":
            Obdfilter.objects.create(time = time, tags = alltags, **events) 
        if d["stats_type"] == "mdt":
            Mdt.objects.create(time = time, tags = alltags, **events) 
        if d["stats_type"] == "lod":
            Lod.objects.create(time = time, tags = alltags, **events) 



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
