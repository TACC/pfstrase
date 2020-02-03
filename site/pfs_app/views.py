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
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, Range1d
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

def groupstatby_hostevents(bucket, interval, obd, stats_type, event):
    df = read_sql(
        """select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, event_name, locf(avg(value)) as value from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{3}' and tags.event_name = '{4}' group by hostname, client_nid, event_name, t;""".format(bucket, interval, obd, stats_type, event), conn)
    return df.set_index("t").tz_convert(tz)

def grouplaststatby_hostevents(bucket, interval, obd, stats_type, event_tuple):
    
    df = read_sql(
        """select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, target, event_name, locf(last(value, time)) as value from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{3}' and tags.event_name in {4} group by hostname, target, event_name, t;""".format(bucket, interval, obd, stats_type, event_tuple), conn)
    return df.set_index("t").tz_convert(tz)

def grouprateby_hostclient(bucket, interval, obd, stats_type, event_tuple):
    df= read_sql(
        """select vals.t, vals.hostname, maps.hostname as client_hostname, client_nid, uid, jid, value from \
        (select t, hostname, client_nid, (case when sum(lastval)>=lag(sum(lastval)) over w then sum(lastval)-lag(sum(lastval)) over w when lag(sum(lastval)) over w is NULL then 0 else 0 end) / extract(epoch from t - lag(t) OVER w) as value from \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, client_nid, locf(avg(value)) as lastval from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = '{2}' and tags.stats_type = '{4}' and tags.event_name in {3} group by hostname, client_nid, event_name, t) v \
        group by hostname, client_nid, t window w as (order by hostname, client_nid, t)) vals join \
        (select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, locf(last(nid, time)) as nid, locf(last(uid, time)) as uid, locf(last(jid, time)) as jid from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = 'osc' group by hostname, t) maps on vals.t = maps.t and vals.client_nid = maps.nid;""".format(bucket, interval, obd, event_tuple, stats_type), conn)
    return df.set_index("t").tz_convert(tz)

def gethostnames(interval):
    return read_sql("select distinct hostname from tags join stats on tags_id = id where time > now() - interval '{0}'".format(interval), conn)

def getjids(interval):
    return read_sql("select distinct jid, time from tags join stats on tags_id = id where time > now() - interval '{0}' and tags.obdclass = 'osc' group by jid, time".format(interval), conn)

iops_tuples = { "mds" : ('open', 'close', 'statfs'), "oss" : ('setattr', 'create', 'statfs', 'set_info')}
bw_tuples = { "oss" : ('read_bytes', 'write_bytes')}

class TimePlot():
    def __init__(self, bucket, interval):
        self.bucket = bucket
        self.interval = interval
        self.hostnames = gethostnames(interval)["hostname"]
        self.hc = {}
        colors = d3["Category10"][10]*5#[20]
        for i, h in enumerate(self.hostnames):
            print(h,colors[i%20])
            self.hc[h] = colors[i%20]

    def ploteventby_host(self, obdclass, stats_type, event, scale = 1.0):
        df = groupstatby_hostevents(self.bucket, self.interval, obdclass, stats_type, event)    
        df = df[df.event_name == event]
        hostnames = list(df.hostname.unique())
        
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), ("host", "@hostname"), (event, "@values")],
                          formatters = {"time" : "datetime"}, line_policy = "nearest")                
        plot = figure(title = event + " " + obdclass, plot_width=1200, plot_height=200, x_axis_type = "datetime", 
                      y_range = Range1d(0, (1.1*scale*df["value"]).max()), y_axis_label = event, 
                      tools = "pan,wheel_zoom,save,box_zoom,reset", toolbar_location = "above")
        plot.tools.append(hover)
        for h in hostnames:
            selectby_hostname = df[df.hostname == h]
            source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                       "hostname": selectby_hostname["hostname"], 
                                       "values" : scale*selectby_hostname["value"]})
            plot.line(source = source, x = "time", y = "values", color = self.hc[h], line_width = 4, legend_label = h)
        plot.legend.click_policy="hide"
        plot.legend.orientation = "horizontal"
        #plot.legend.location = "top_left"
        new_legend = plot.legend[0]
        plot.legend[0] = None
        plot.add_layout(new_legend, 'below')

        return plot 

    def plotrateby_host(self, obdclass, stats_type, events, tag):

        event_tuple = events[obdclass]
        if "statfs" in event_tuple:
            label = "iops"
            scale = 1.0
        if "read_bytes" in event_tuple:
            label = "MB/s"
            scale = 1.0/(1024*1024)

        df = grouprateby_hostclient(self.bucket, self.interval, obdclass, stats_type, event_tuple)
        server_hostnames = list(df.hostname.unique())

        client_tags = list(df[tag].unique())

        groupby_hostname_tag = df.groupby([df.index, "hostname", tag]).sum().reset_index(level = "hostname").reset_index(level = tag)
        groupby_hostname = groupby_hostname_tag.groupby([groupby_hostname_tag.index, "hostname"]).sum().reset_index(level = "hostname")
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), (tag, "@"+tag), (label, "@values")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    
        plots = []

        for h in server_hostnames:
            selectby_hostname = groupby_hostname[groupby_hostname.hostname == h]

            plot = figure(title =  h + " " + label + " by " + tag, plot_width=1200, plot_height=300, x_axis_type = "datetime", 
                          y_range = Range1d(0, (1.1*scale*selectby_hostname["value"]).max()), y_axis_label = label, 
                          tools = "pan,wheel_zoom,save,box_zoom,reset", toolbar_location = "above")
            plot.tools.append(hover)
            source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                       tag : ['sum']*len(selectby_hostname.index), 
                                       "hostname": selectby_hostname["hostname"], 
                                       "values" : scale*selectby_hostname["value"]})
            plot.line(source = source, x = "time", y = "values", line_color = self.hc[h], 
                      line_width = 4, line_alpha = 0.5, legend_label = h)
            for n in client_tags:
                selectby_hostname_client = groupby_hostname_tag[(groupby_hostname_tag["hostname"] == h) & (groupby_hostname_tag[tag] == n)]
                source = ColumnDataSource({"time" : selectby_hostname_client.index.tz_localize(None),
                                           tag : selectby_hostname_client[tag], 
                                           "hostname": [h]*len(selectby_hostname_client.index),
                                           "values" : scale*selectby_hostname_client["value"]})
                try: c = self.hc[n]
                except: c = d3["Category10"][10][client_tags.index(n)%10]
                plot.triangle(source = source, x = "time", y = "values", color = c, legend_label = n)
            plot.legend.click_policy="hide"
            plot.legend.orientation = "horizontal"
            #plot.legend.location = "top_left"
            new_legend = plot.legend[0]
            plot.legend[0] = None
            plot.add_layout(new_legend, 'below')
            
            plots += [plot]
        return gridplot(plots, ncols = 1)


    def plotlatestvalueby_host(self):            
        scale = 1/1000.0
        plots = []

        df = grouplaststatby_hostevents(self.bucket, self.interval, "mds", "mdt", ('filesfree', 'filestotal'))
        df = df.append(grouplaststatby_hostevents(self.bucket, self.interval, "oss", "ost", ('filesfree', 'filestotal')))
        df = df.groupby(["target", "event_name"]).last().reset_index(level = "target").reset_index(level = "event_name")

        targets = list(df.target.unique())
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), ("Used Inodes [K]", "@used")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    

        plot = figure(plot_width=1200, plot_height=300, x_range = targets,
                      y_range = Range1d(0, (1.1*scale*df["value"]).max()), y_axis_label = "Inode Usage [K]", 
                      toolbar_location = "above", tools = "pan,wheel_zoom,save,box_zoom,reset")
        plot.tools.append(hover)
        df_total = df[df["event_name"] == "filestotal"]
        df_free = df[df["event_name"] == "filesfree"]
        source = ColumnDataSource({"targets" : targets, "total" : scale*df_total["value"], 
                                   "used" : scale*(df_total["value"].to_numpy()-df_free["value"].to_numpy())})
        plot.vbar(x = "targets", top = "total", source = source, width = 0.8, alpha = 0.5, legend_label = "Total Files")    
        plot.vbar(x = "targets", top = "used", source = source, width = 0.8, color = "red", legend_label = "Used Files")
        plots += [plot]

        scale = 1.0/1024
        df = grouplaststatby_hostevents(self.bucket, self.interval, "mds", "mdt", ('kbytesfree', 'kbytestotal'))
        df = df.append(grouplaststatby_hostevents(self.bucket, self.interval, "oss", "ost", ('kbytesfree', 'kbytestotal')))
        df = df.groupby(["target", "event_name"]).last().reset_index(level = "target").reset_index(level = "event_name")
        targets = list(df.target.unique())
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), ("Used MB", "@used")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    

        plot = figure(plot_width=1200, plot_height=300, x_range = targets,
                      y_range = Range1d(0, (1.1*scale*df["value"]).max()), y_axis_label = "Disk Usage [MB]", 
                      toolbar_location = "above", tools = "pan,wheel_zoom,save,box_zoom,reset")
        plot.tools.append(hover)
        df_total = df[df["event_name"] == "kbytestotal"]
        df_free = df[df["event_name"] == "kbytesfree"]
        source = ColumnDataSource({"targets" : targets, "total" : scale*df_total["value"], 
                                   "used" : scale*(df_total["value"].to_numpy()-df_free["value"].to_numpy())})
        plot.vbar(x = "targets", top = "total", source = source, width = 0.8, alpha = 0.5, legend_label = "Total MB")    
        plot.vbar(x = "targets", top = "used", source = source, width = 0.8, color = "red", legend_label = "Used MB")
        plots += [plot]

        return gridplot(plots, ncols = 1)


def home(request):
    field = {}

    bucket = "1 minutes"
    interval = '12 hours'

    P = TimePlot(bucket, interval)

    field["ost_script"], field["ost_div"] = components(P.plotlatestvalueby_host())
    field["mds_freeram_script"], field["mds_freeram_div"] = components(P.ploteventby_host("mds", "sysinfo", "loadavg1m"))
    field["oss_freeram_script"], field["oss_freeram_div"] = components(P.ploteventby_host("oss", "sysinfo", "loadavg1m"))
            
    field["mds_script"], field["mds_div"] = components(P.plotrateby_host("mds", "mds", iops_tuples, "client_hostname"))
    field["oss_script"], field["oss_div"] = components(P.plotrateby_host("oss", "oss", bw_tuples, "client_hostname"))

    field["jid_mds_script"], field["jid_mds_div"] = components(P.plotrateby_host("mds", "mds", iops_tuples, "jid"))
    field["jid_oss_script"], field["jid_oss_div"] = components(P.plotrateby_host("oss", "oss", bw_tuples, "jid"))

    field["uid_mds_script"], field["uid_mds_div"] = components(P.plotrateby_host("mds", "mds", iops_tuples, "uid"))
    field["uid_oss_script"], field["uid_oss_div"] = components(P.plotrateby_host("oss", "oss", bw_tuples, "uid"))

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    
    return render(request, "pfs_app/home.html", field)


