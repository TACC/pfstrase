from django.db import models

class Stats(models.Model):
    time       = models.DateTimeField(null = False)
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
    value  = models.FloatField(null=True)

    class Meta:
        db_table = "stats"

