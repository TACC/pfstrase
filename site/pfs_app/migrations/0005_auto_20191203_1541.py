# Generated by Django 2.2.5 on 2019-12-03 15:41

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('pfs_app', '0004_auto_20191203_1537'),
    ]

    operations = [
        migrations.AlterField(
            model_name='lod',
            name='activeobd',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='blocksize',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='filesfree',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='filestotal',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='kbytesavail',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='kbytesfree',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='kbytestotal',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='lmv_failout',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='numobd',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='stripecount',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='lod',
            name='tripetype',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='close',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='getattr',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='getxattr',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='mkdir',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='mknod',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='open',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='setattr',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='mdt',
            name='statfs',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='create',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='get_info',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='read_bytes',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='set_info',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='statfs',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='obdfilter',
            name='write_bytes',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='sysinfo',
            name='bufferram',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='sysinfo',
            name='freeram',
            field=models.BigIntegerField(null=True),
        ),
        migrations.AlterField(
            model_name='sysinfo',
            name='totalram',
            field=models.BigIntegerField(null=True),
        ),
    ]