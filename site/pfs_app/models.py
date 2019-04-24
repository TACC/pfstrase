from django.db import models
from django.forms import ModelForm
from django.contrib.postgres.fields import JSONField

class hosts(models.Model):
    time = models.DateTimeField(primary_key = True)
    host = models.CharField(max_length=128, null = False)
    obdclass = models.CharField(max_length=16, null = True)
    nid = models.CharField(max_length=128, null = True)
    jid = models.CharField(max_length=128, null = True)
    user = models.CharField(max_length=128, null = True)
    
    stats = JSONField(null = True)

    def __str__(self):
        return str(self.time) + ' ' + self.host
