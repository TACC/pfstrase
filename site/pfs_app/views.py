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
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, Range1d, LinearAxis, DatetimeAxis
from bokeh.models import HoverTool, PanTool, WheelZoomTool, BoxZoomTool, UndoTool, RedoTool, ResetTool
from bokeh.models.glyphs import Step, Line

from pandas import read_sql
import pandas 
pandas.set_option('display.max_rows', None)
from .models import Tags, Stats

tz = timezone.get_current_timezone()

factor = 1.0/(1024*1024)

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

def groupstatby_hostevents(bucket, interval, obd, stats_type, event):
    df = read_sql(
        """select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, event_name, interpolate(avg(value)) as value from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{3}' and tags.event_name = '{4}' group by hostname, client_nid, stats_type, event_name, t;""".format(bucket, interval, obd, stats_type, event), conn)
    return df.set_index("t").tz_convert(tz)

def grouprateby_hostclient(bucket, interval, obd, stats_type, event_tuple):
    #print(read_sql("select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, interpolate(avg(value)) as lastval from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{4}' and tags.event_name in {3} group by hostname, client_nid, event_name, t;".format(bucket, interval, obd, event_tuple, stats_type), conn))
    #print(read_sql("select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, locf(last(nid, time)) as nid, locf(last(uid, time)) as uid, locf(last(jid, time)) as jid from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = 'osc' group by hostname, t".format(bucket, interval, obd, event_tuple, stats_type), conn))
    df= read_sql(
        """select vals.t, vals.hostname, maps.hostname as client_hostname, client_nid, uid, jid, value from \
        (select t, hostname, client_nid, (case when sum(lastval)>=lag(sum(lastval)) over w then sum(lastval)-lag(sum(lastval)) over w when lag(sum(lastval)) over w is NULL then 0 else 0 end) as value from \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, interpolate(avg(value)) as lastval from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{4}' and tags.event_name in {3} group by hostname, client_nid, event_name, t) v \
        group by hostname, client_nid, t window w as (order by hostname, client_nid, t)) vals join \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, locf(last(nid, time)) as nid, locf(last(uid, time)) as uid, locf(last(jid, time)) as jid from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = 'osc' group by hostname, t) maps on vals.t = maps.t and vals.client_nid = maps.nid;""".format(bucket, interval, obd, event_tuple, stats_type), conn)
    return df.set_index("t").tz_convert(tz)

def agrouprateby_hostclient(bucket, interval, obd, stats_type, event_tuple):
    df= read_sql(
        """select vals.t, vals.hostname, maps.hostname as client_hostname, client_nid, uid, jid, value from \
        (select t, hostname, client_nid, (case when sum(lastval)>=lag(sum(lastval)) over w then sum(lastval)-lag(sum(lastval)) over w when lag(sum(lastval)) over w is NULL then 0 else 0 end) as value from \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, interpolate(avg(value)) as lastval from stats join tags on tags_id = id where time > now() - interval '{1}' + interval '15 minutes' and tags.obdclass = '{2}' and tags.stats_type = '{4}' and tags.event_name in {3} group by hostname, client_nid, event_name, t) v \
        group by hostname, client_nid, t window w as (order by hostname, client_nid, t)) vals join \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, last(nid, time) as nid, last(uid, time) as uid, last(jid, time) as jid from stats join tags on tags_id = id where time > now() - interval '{1}' + interval '15 minutes' and tags.obdclass = 'osc' group by hostname, t) maps on vals.t = maps.t and vals.client_nid = maps.nid;""".format(bucket, interval, obd, event_tuple, stats_type), conn)
    return df.set_index("t").tz_convert(tz)


def gethostnames(interval):
    return read_sql("select distinct hostname from tags join stats on tags_id = id where time > now() - interval '{0}'".format(interval), conn)

iops_tuples = { "mds" : ('open', 'close', 'statfs'), "oss" : ('setattr', 'create', 'statfs', 'set_info')}
bw_tuples = { "oss" : ('read_bytes', 'write_bytes')}

class TimePlot():
    def __init__(self, bucket, interval):
        self.bucket = bucket
        self.interval = interval
        self.hostnames = gethostnames(interval)["hostname"]
        self.hc = {}
        colors = d3["Category20"][20]
        for i, h in enumerate(self.hostnames):
            self.hc[h] = colors[i%20]

    def ploteventby_host(self, obdclass, stats_type, event, scale = 1.0):
        df = groupstatby_hostevents(self.bucket, self.interval, obdclass, stats_type, event)    
        hostnames = list(df.hostname.unique())
        df = df[df.event_name == event]
        hover = HoverTool(tooltips = [(event, "@values"), 
                                      ("time", "@time{%Y-%m-%d %H:%M:%S}"), 
                                      ("host", "@hostname")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")        
        plot = Plot(plot_width=1200, plot_height=200, x_range = DataRange1d(), 
                    y_range = Range1d(0, (1.1*scale*df["value"]).max()))
        for h in hostnames:
            selectby_hostname = df[df.hostname == h]
            source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                       "hostname": selectby_hostname["hostname"], 
                                       "values" : scale*selectby_hostname["value"]})
            plot.add_glyph(source, Line(x = "time", y = "values", line_color = self.hc[h], line_width=2))
        plot.add_tools(hover, PanTool(), WheelZoomTool(), BoxZoomTool(), ResetTool())        
        plot = add_axes(plot, event)
        return plot 

    def plotrateby_host(self, obdclass, stats_type, events):

        event_tuple = events[obdclass]
        if "statfs" in event_tuple:
            label = "iops"
            scale = 1.0
        if "read_bytes" in event_tuple:
            label = "MB/s"
            scale = 1.0/(1024*1024)

        df = grouprateby_hostclient(self.bucket, self.interval, obdclass, stats_type, event_tuple)
        server_hostnames = list(df.hostname.unique())

        #tag = "client_hostname"
        tag = "jid"
        client_tags = list(df[tag].unique())

        groupby_hostname_tag = df.groupby([df.index, "hostname", tag]).sum().reset_index(level = "hostname").reset_index(level = tag)
        groupby_hostname = groupby_hostname_tag.groupby([groupby_hostname_tag.index, "hostname"]).sum().reset_index(level = "hostname")
        hover = HoverTool(tooltips = [("val", "@values"), (tag, "@"+tag), 
                                      ("time", "@time{%Y-%m-%d %H:%M:%S}"), 
                                      ("server", "@hostname")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")        
        plots = []
        for h in server_hostnames:
            selectby_hostname = groupby_hostname[groupby_hostname.hostname == h]
            plot = Plot(plot_width=1200, plot_height=200, x_range = DataRange1d(), 
                        y_range = Range1d(0, (1.1*scale*selectby_hostname["value"].max())))
            source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                       tag : ['total']*len(selectby_hostname.index), 
                                       "hostname": selectby_hostname["hostname"], 
                                       "values" : scale*selectby_hostname["value"]})
            plot.add_glyph(source, Line(x = "time", y = "values", line_color = self.hc[h], line_width = 2, line_alpha = 0.5))

            for n in client_tags:
                selectby_hostname_client = groupby_hostname_tag[(groupby_hostname_tag["hostname"] == h) & (groupby_hostname_tag[tag] == n)]
                source = ColumnDataSource({"time" : selectby_hostname_client.index.tz_localize(None),
                                           tag : selectby_hostname_client[tag], 
                                           "hostname": [h]*len(selectby_hostname_client.index),
                                           "values" : scale*selectby_hostname_client["value"]})
                try: c = self.hc[n]
                except: c = d3["Category20"][20][int(n)%20]
                plot.add_glyph(source, Line(x = "time", y = "values", line_color = c))
            plot.add_tools(hover, PanTool(), WheelZoomTool(), BoxZoomTool(), ResetTool())
            plots += [add_axes(plot, h + " " + label)]

        return gridplot(plots, ncols = 1)

def home(request):
    field = {}

    bucket = "1 minutes"
    interval = '1 hours'

    P = TimePlot(bucket, interval)
    field["mds_freeram_script"], field["mds_freeram_div"] = components(P.ploteventby_host("mds", "sysinfo", "loadavg1m"))
    field["oss_freeram_script"], field["oss_freeram_div"] = components(P.ploteventby_host("oss", "sysinfo", "loadavg1m"))
            
    field["mds_script"], field["mds_div"] = components(P.plotrateby_host("mds", "mdt", iops_tuples))
    field["oss_script"], field["oss_div"] = components(P.plotrateby_host("oss", "obdfilter", bw_tuples))
    field["resources"] = CDN.render()

    field["datetime"] = timezone.now()
    
    return render(request, "pfs_app/home.html", field)


