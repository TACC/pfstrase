# Generated by Django 3.1.6 on 2021-02-12 14:04

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('pfs_app', '0001_initial'),
    ]

    operations = [
        migrations.AddField(
            model_name='stats',
            name='fid',
            field=models.CharField(max_length=128, null=True),
        ),
    ]
