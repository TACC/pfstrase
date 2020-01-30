#!/usr/bin/env python
import sys, os, json
from datetime import datetime, timezone
os.environ['DJANGO_SETTINGS_MODULE']='pfstrase.settings'
import django
from django.db.models import Max
django.setup()
from pfs_app.models import Tags, Stats


stats = Stats.objects.filter(time__gt = "2019-12-04", tags__hostname = "mds0", tags__event_name__in = ["open", "close"])
for s in stats:
    print(s.time, s.tags.hostname, s.tags.event_name, s.value)

#t = Tags.objects.filter(hostname = "mds0")
#print(t)
