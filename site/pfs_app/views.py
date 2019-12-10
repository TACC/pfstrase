from datetime import datetime, timedelta
import pytz, time
import requests, json

from django.shortcuts import render, render_to_response
from django.db.models import Count, F, Value, IntegerField, FloatField, CharField
from django.db.models.functions import Cast, Concat
from django.contrib.postgres.fields.jsonb import KeyTextTransform as ktt
from django.contrib.postgres.fields.jsonb import KeyTransform as kt
from django.contrib.postgres.aggregates import StringAgg
from django.utils import timezone
from django.db.models import Avg,Sum,Max,Min

from bokeh.plotting import figure
from bokeh.embed import components
from bokeh.resources import CDN
from bokeh.layouts import gridplot
from bokeh.palettes import d3
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, LinearAxis, DatetimeAxis
from bokeh.models import HoverTool, PanTool, WheelZoomTool, BoxZoomTool, UndoTool, RedoTool, ResetTool
from bokeh.models.glyphs import Step, Line

from pandas import read_sql
import pandas 
pandas.set_option('display.max_rows', None)
from .models import Tags, Stats

tz = timezone.get_current_timezone()

factor = 1.0/(1024*1024)
colors = d3["Category20"][20]

import psycopg2
conn = psycopg2.connect("dbname=pfstrase_db user=postgres")
cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

def add_axes(plot, label):
    xaxis = DatetimeAxis()
    yaxis = LinearAxis()      
    yaxis.axis_label = label
    plot.add_layout(xaxis, 'below')        
    plot.add_layout(yaxis, 'left')
    plot.add_layout(Grid(dimension=0, ticker=xaxis.ticker))
    plot.add_layout(Grid(dimension=1, ticker=yaxis.ticker))
    return plot

def bucketize(bucket, interval, obd, event_tuple):
    return read_sql("select t, hostname, sum(lastval) as values from (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, last(value, time) as lastval, locf(last(value,time)) from stats join tags on tags_id = id where tags.obdclass = '{2}' and tags.event_name  in {3} and time > now() - interval '{1}' group by hostname, client_nid, event_name, t) lv group by hostname, t;".format(bucket, interval, obd, event_tuple), conn)

def bucketize_by_client(bucket, interval, hostname, event_tuple):
    return read_sql("select t, client_nid, sum(lastval) as values from (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, client_nid, last(value, time) as lastval, locf(last(value,time)) from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.hostname = '{2}' and tags.event_name in {3} group by client_nid, event_name, t) lv group by client_nid, t;".format(bucket, interval, hostname, event_tuple), conn, parse_dates = ["t"])

def groupby_hostclient(bucket, interval, obd, event_tuple):
    return read_sql("select t, hostname, client_nid, sum(lastval) as values from (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, last(value, time) as lastval, locf(last(value,time)) from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.event_name in {3} group by hostname, client_nid, event_name, t) lv group by hostname, client_nid, t;".format(bucket, interval, obd, event_tuple), conn)

def bucket_client_tags(bucket, interval):
    return read_sql("select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, last(nid, time) as nid, last(jid, time) as jid, locf(last(jid, time)) from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = 'osc' group by hostname, t;".format(bucket, interval), conn)

iops_tuples = { "mds" : ('open', 'close'), "oss" : ('setattr', 'create', 'statfs', 'set_info')}

def iopsbyhost_plot(obdclass):

    bucket = "15 minutes"
    interval = '2 hours'
    event_tuple = iops_tuples[obdclass]

    client_tags = bucket_client_tags(bucket, interval)
    client_hostnames = list(client_tags.hostname.unique())

    df = groupby_hostclient(bucket, interval, obdclass, event_tuple).set_index("t").tz_convert(tz)

    server_hostnames = list(df.hostname.unique())
    plot = Plot(plot_width=1200, plot_height=400, x_range = DataRange1d(), y_range = DataRange1d())
    hc = {}
    for i, h in enumerate(server_hostnames + client_hostnames):
        hc[h] = colors[i%20]

    groupby_hostname = df.groupby([df.index, "hostname"]).sum().reset_index(level = "hostname")

    for h in server_hostnames:
        selectby_hostname = groupby_hostname[groupby_hostname.hostname == h]
        source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                   "jid" : ['-']*len(selectby_hostname.index), 
                                   "hostname": [h]*len(selectby_hostname.index), 
                                   "values" : selectby_hostname["values"]})
        plot.add_glyph(source, Line(x = "time", y = "values", line_color = hc[h]))

        for n in df.client_nid.unique():
            selectby_hostname_nid = df.loc[(df.client_nid == n) & (df.hostname == h)] 
            selectby_nid = client_tags.loc[client_tags.nid == n]
            source = ColumnDataSource({"time" : selectby_hostname_nid.index.tz_localize(None),
                                       "jid" : selectby_nid["jid"], 
                                       "hostname": selectby_nid["hostname"], 
                                       "values" : selectby_hostname_nid["values"]})
            plot.add_glyph(source, Line(x = "time", y = "values", 
                                        line_color = hc[selectby_nid["hostname"].head(1).tolist()[0]]))

    hover = HoverTool(tooltips = [("val", "@values"), ("jid", "@jid"), 
                                  ("time", "@time{%Y-%m-%d %H:%M:%S}"), 
                                  ("host", "@hostname")], 
                      formatters = {"time" : "datetime"}, line_policy = "nearest")        
    plot.add_tools(hover, PanTool(), WheelZoomTool(), BoxZoomTool(), 
                   UndoTool(), RedoTool(), ResetTool())

    plot = add_axes(plot, "IOPS")
    return plot

def home(request):
    field = {}

    default_time = pytz.utc.localize(datetime.now() - timedelta(hours = 1))
    stats = Stats.objects.filter(time__gt = default_time).order_by("-time")

    sysinfo = stats.filter(tags__obdclass = "mds", tags__stats_type = "sysinfo")
    mdt = stats.filter(tags__stats_type = "mdt")
    osc = stats.filter(tags__stats_type = "osc")
    

    hostnames = Tags.objects.filter(obdclass = "mds").values_list("hostname", flat = True).distinct()
    field["mds"] = []
    for h in hostnames:
        h_sysinfo = sysinfo.filter(tags__hostname = h)
        time = h_sysinfo.filter(tags__event_name = "freeram").values_list("time", flat=True)

        freeram = h_sysinfo.filter(tags__event_name = "freeram").values_list("value", flat=True)
        totalram = h_sysinfo.filter(tags__event_name = "totalram").values_list("value", flat=True)
        load1m = h_sysinfo.filter(tags__event_name = "loadavg1m").values_list("value", flat=True)
        load5m = h_sysinfo.filter(tags__event_name = "loadavg5m").values_list("value", flat=True)
        load15m = h_sysinfo.filter(tags__event_name = "loadavg15m").values_list("value", flat=True)
        percentram = 100*(totalram[0]-freeram[0])/totalram[0]

        h_mdt = mdt.filter(tags__hostname = h)
        client_iops = h_mdt.filter(tags__event_name__in = ["open", "close"]).values("time", "tags__client_nid").annotate(iops = Sum("value"))

        total_iops  = h_mdt.filter(tags__event_name__in = ["open", "close"]).values("time").annotate(iops = Sum("value"))
        field["mds"] += [(time[0], h, load1m[0], load5m[0], load15m[0], percentram, total_iops[0]["iops"])]
        
        
    
    script,div = components(iopsbyhost_plot("mds"))
    field["script"] = script
    field["div"] = div
    field["resources"] = CDN.render()

    field["datetime"] = timezone.now()
    
    return render(request, "pfs_app/home.html", field)


