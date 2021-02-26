from django.db import models

class Stats(models.Model):
    time       = models.DateTimeField(null = False)
    hostname   = models.CharField(max_length=128, null = False)     
    fid        = models.CharField(max_length=128, null = True)
    jid        = models.CharField(max_length=128, null = True)
    uid        = models.CharField(max_length=128, null = True)
    client     = models.CharField(max_length=128, null = True)
    event_name = models.CharField(max_length=128, null = True)
    value  = models.FloatField(null=True)

    class Meta:
        db_table = "stats"

