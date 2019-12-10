from django.db import models
from django.forms import ModelForm
from django.contrib.postgres.fields import JSONField

class Tags(models.Model):
    id         = models.BigIntegerField(primary_key=True)
    hostname   = models.CharField(max_length=128, null = False)     
    obdclass   = models.CharField(max_length=128, null = True)
    nid        = models.CharField(max_length=128, null = True)
    jid        = models.CharField(max_length=128, null = True)
    uid        = models.CharField(max_length=128, null = True)
    target     = models.CharField(max_length=128, null = True)
    client_nid = models.CharField(max_length=128, null = True)
    server_nid = models.CharField(max_length=128, null = True)
    stats_type = models.CharField(max_length=128, null = True)
    event_name = models.CharField(max_length=128, null = True)

    class Meta:
        db_table = "tags"

    def __str__(self):
        return self.hostname

class Stats(models.Model):
    time = models.DateTimeField(primary_key = True, db_index = True)
    tags = models.ForeignKey(Tags, on_delete=models.CASCADE)
    value  = models.FloatField(null=True)

    class Meta:
        db_table = "stats"

