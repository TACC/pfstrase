# Generated by Django 2.1.5 on 2019-04-10 16:18

from django.db import migrations


class Migration(migrations.Migration):

    dependencies = [
        ('pfs_app', '0005_auto_20190410_1552'),
    ]

    operations = [
        migrations.RenameField(
            model_name='exports',
            old_name='hostname',
            new_name='host',
        ),
        migrations.RenameField(
            model_name='exports',
            old_name='measurename',
            new_name='measure',
        ),
        migrations.RenameField(
            model_name='hosts',
            old_name='devicename',
            new_name='device',
        ),
        migrations.RenameField(
            model_name='hosts',
            old_name='hostname',
            new_name='host',
        ),
        migrations.RenameField(
            model_name='hosts',
            old_name='measurename',
            new_name='measure',
        ),
        migrations.RenameField(
            model_name='hosts',
            old_name='typename',
            new_name='stat',
        ),
    ]
