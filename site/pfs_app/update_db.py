import sys, os, json
from datetime import datetime
os.environ['PYTHONPATH']='/home/rtevans/pfs/pfstrase'
print(os.environ['PYTHONPATH'])
import pfstrase
os.environ['DJANGO_SETTINGS_MODULE']='pfstrase.settings'
import django
from django.db.models import Max
django.setup()
from pfs_app.models import hosts
import pika 

def ingest(channel, method_frame, header_frame, body):
    channel.basic_ack(delivery_tag=method_frame.delivery_tag)

    try:
        message = body.decode()    
    except: 
        print("Unexpected error at decode:", sys.exc_info()[0])
        return
    try: 
        data = json.loads(message)
    except: 
        print(sys.exc_info()[0])
        return
    data["time"] = datetime.utcfromtimestamp(data["time"])

    #hosts.objects.all().delete()
    print(data)
    hosts.objects.create(**data)
    
parameters = pika.ConnectionParameters("tacc-stats03")
connection = pika.BlockingConnection(parameters)
channel = connection.channel()

channel.queue_declare(queue='response', durable = 'True')
channel.basic_consume(ingest, "response")
try:
    channel.start_consuming()
except KeyboardInterrupt:
    channel.stop_consuming()

requeued_messages = channel.cancel()
print('Requeued %i messages' % requeued_messages)

connection.close()
