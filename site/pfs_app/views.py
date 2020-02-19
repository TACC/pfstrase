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
import time
tz = timezone.get_current_timezone()

import psycopg2
conn = psycopg2.connect("dbname=pfstrase_db user=postgres")
cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

class TimePlot():
    def __init__(self, bucket, interval):
        self.bucket = bucket
        self.interval = interval
        self.cur = conn.cursor()

        self.cur.execute("DROP VIEW IF EXISTS server_bucket_stats CASCADE;")
        self.cur.execute("DROP VIEW IF EXISTS client_bucket_tags CASCADE;")
        self.cur.execute("CREATE TEMP VIEW server_bucket_stats AS select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, obdclass, hostname, target, stats_type, event_name, client_nid, locf(avg(value)) as bucket_value from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass in ('mds', 'oss') group by obdclass, hostname, target, stats_type, client_nid, event_name, t;".format(self.bucket, self.interval))
        self.cur.execute("CREATE TEMP VIEW client_bucket_tags AS select time_bucket_gapfill('{0}', time, now() - interval '{1}', now()) as t, hostname, locf(last(nid, time)) as nid, locf(last(uid, time)) as uid, locf(last(jid, time)) as jid from stats join tags on tags_id = id where time > now() - interval '{1}' and tags.obdclass = 'osc' group by hostname, t;".format(self.bucket, self.interval), conn)

        self.hostnames = read_sql("select distinct hostname from tags join stats on tags_id = id where time > now() - interval '{0}'".format(interval), conn)["hostname"]

        self.hc = {}
        colors = d3["Category10"][10]*5#[20]
        for i, h in enumerate(self.hostnames):
            self.hc[h] = colors[i%20]

    pandas.set_option('display.max_columns', 20)

    def grouprateby_tags(self, obd, stats_type, event_tuple):
        s = time.time()
        self.cur.execute("CREATE TEMP VIEW rate AS SELECT t, hostname, client_nid, (sum(bucket_value) - lag(sum(bucket_value)) OVER w) / extract(epoch from t - lag(t) OVER w) as value FROM server_bucket_stats where obdclass = '{0}' and stats_type = '{1}' and event_name in {2} group by hostname, client_nid, t window w as (order by hostname, client_nid, t);".format(obd, stats_type, event_tuple))
        self.cur.execute("CREATE TEMP VIEW tagged_rate AS select rate.t, rate.value, rate.hostname, rate.client_nid, client_bucket_tags.hostname as client_hostname, uid, jid from rate join client_bucket_tags on rate.t = client_bucket_tags.t and rate.client_nid = client_bucket_tags.nid;")
        df = read_sql("SELECT * FROM tagged_rate", conn)
        self.cur.execute("DROP VIEW rate CASCADE;")
        print("grouprateby_tags", time.time() - s)
        return df.set_index("t").tz_convert(tz)

    def grouprateby_host(self, obd, stats_type, event_tuple):
        s = time.time()
        self.cur.execute("CREATE TEMP VIEW rate AS SELECT t, hostname, (sum(bucket_value) - lag(sum(bucket_value)) OVER w) / extract(epoch from t - lag(t) OVER w) as value FROM server_bucket_stats where obdclass = '{0}' and stats_type = '{1}' and event_name in {2} group by hostname, t window w as (order by hostname, t);".format(obd, stats_type, event_tuple))
        df = read_sql("SELECT * FROM rate", conn)
        self.cur.execute("DROP VIEW rate CASCADE;")
        print("grouprateby_host", time.time() - s)
        return df.set_index("t").tz_convert(tz)

    def groupvalueby_host(self, obd, stats_type, event_tuple):
        s = time.time()
        self.cur.execute("CREATE TEMP VIEW values AS SELECT t, hostname, sum(bucket_value) as value FROM server_bucket_stats where obdclass = '{0}' and stats_type = '{1}' and event_name in {2} group by hostname, t window w as (order by hostname, t);".format(obd, stats_type, event_tuple))
        df = read_sql("SELECT * FROM values", conn)
        self.cur.execute("DROP VIEW values CASCADE;")
        print("groupvalueby_host", time.time() - s)
        return df.set_index("t").tz_convert(tz)

    def grouplastvalueby_target_events(self, obd, stats_type, event_tuple):
        s = time.time()
        self.cur.execute("CREATE TEMP VIEW lastvalues AS SELECT DISTINCT ON (target, event_name) t, hostname, target, event_name, bucket_value AS value FROM server_bucket_stats WHERE obdclass in {0} and stats_type in {1} and event_name in {2} ORDER BY target, event_name, t DESC;".format(obd, stats_type, event_tuple))
        df = read_sql("SELECT * FROM lastvalues", conn)
        self.cur.execute("DROP VIEW lastvalues CASCADE;")
        print("grouplastvalueby_target_events", time.time() - s)
        return df.set_index("t").tz_convert(tz)

    def format_plot(self, p):
        p.legend.click_policy="hide"
        p.legend.orientation = "horizontal"
        p.xaxis.axis_label_text_font_size = "12pt"
        p.yaxis.axis_label_text_font_size = "12pt"
        new_legend = p.legend[0]
        p.legend[0] = None
        p.add_layout(new_legend, 'below')

    def plotrateby_tag(self, obdclass, stats_type, events):        
        if "statfs" in events:
            label = "iops"
            scale = 1.0
        if "read_bytes" in events:
            label = "MB/s"
            scale = 1.0/(1024*1024)
        
        df = self.grouprateby_tags(obdclass, stats_type, events)
        server_hostnames = sorted(list(df.hostname.unique()))

        plots = []
        for tag in ["client_hostname", "jid", "uid"]:
            tags = list(df[tag].unique())
            groupby_hostname_tag = df.groupby([df.index, "hostname", tag]).sum().reset_index(level = "hostname").reset_index(level = tag)
            groupby_hostname = groupby_hostname_tag.groupby([groupby_hostname_tag.index, "hostname"]).sum().reset_index(level = "hostname")
            hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), (tag, "@"+tag), (label, "@values")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    
            for h in server_hostnames:
                selectby_hostname = groupby_hostname[groupby_hostname.hostname == h]

                plot = figure(title =  h + " " + label + " by " + tag, 
                              plot_width=1200, plot_height=300, x_axis_type = "datetime", 
                              y_range = Range1d(0, (1.1*scale*selectby_hostname["value"]).max()), y_axis_label = label, 
                              tools = "pan,wheel_zoom,save,box_zoom,reset", toolbar_location = "above")
                plot.tools.append(hover)
                source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                           tag : ['sum']*len(selectby_hostname.index), 
                                           "hostname": selectby_hostname["hostname"], 
                                           "values" : scale*selectby_hostname["value"]})
                plot.line(source = source, x = "time", y = "values", line_color = self.hc[h], 
                          line_width = 4, line_alpha = 0.5, legend_label = h)
                for n in tags:                
                    selectby_hostname_client = groupby_hostname_tag[(groupby_hostname_tag["hostname"] == h) & (groupby_hostname_tag[tag] == n)]
                    #selectby_hostname_client.at[0, "value"] = 0.0
                    source = ColumnDataSource({"time" : selectby_hostname_client.index.tz_localize(None),
                                               tag : selectby_hostname_client[tag], 
                                               "hostname": [h]*len(selectby_hostname_client.index),
                                               "values" : scale*selectby_hostname_client["value"]})
                    try: c = self.hc[n]
                    except: c = d3["Category10"][10][tags.index(n)%10]
                    plot.triangle(source = source, x = "time", y = "values", color = c, legend_label = n)
                self.format_plot(plot)
                plots += [plot]
        return gridplot(plots, ncols = 1)

    def plotrateby_host(self, obdclass, stats_type, events):        
        if "system" in events:
            label = "Cores used"
            scale = 1.0/100.
        df = self.grouprateby_host(obdclass, stats_type, events)
        hostnames = list(df.hostname.unique())
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), (label, "@values")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    
        plot = figure(title =  label, plot_width=1200, plot_height=300, x_axis_type = "datetime", 
                      y_range = Range1d(0, (1.1*scale*df["value"]).max()), y_axis_label = label, 
                      tools = "pan,wheel_zoom,save,box_zoom,reset", toolbar_location = "above")
        plot.tools.append(hover)        
        for h in hostnames:
            selectby_hostname = df[df.hostname == h]
            source = ColumnDataSource({"time" : selectby_hostname.index.tz_localize(None), 
                                       "hostname": selectby_hostname["hostname"], 
                                       "values" : scale*selectby_hostname["value"]})
            plot.line(source = source, x = "time", y = "values", line_color = self.hc[h], 
                      line_width = 4, line_alpha = 0.5, legend_label = h)
        self.format_plot(plot)
        return plot


    def plotlatestvalueby_host(self):            
        scale = 1/1000.0
        plots = []

        df = self.grouplastvalueby_target_events(("mds", "oss"), ("mdt", "ost"), 
                                                 ('filesfree', 'filestotal', 'kbytesfree', 'kbytestotal'))
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
                                   "time" : df_free.index.tz_localize(None),
                                   "used" : scale*(df_total["value"].to_numpy()-df_free["value"].to_numpy())})
        plot.vbar(x = "targets", top = "total", source = source, width = 0.8, alpha = 0.5, legend_label = "Total Files")    
        plot.vbar(x = "targets", top = "used", source = source, width = 0.8, color = "red", legend_label = "Used Files")
        plots += [plot]

        scale = 1.0/(1024*1024)
        hover = HoverTool(tooltips = [("time", "@time{%Y-%m-%d %H:%M:%S}"), ("Used GB", "@used")], 
                          formatters = {"time" : "datetime"}, line_policy = "nearest")    

        plot = figure(plot_width=1200, plot_height=300, x_range = targets,
                      y_range = Range1d(0, (1.1*scale*df["value"]).max()), y_axis_label = "Disk Usage [GB]", 
                      toolbar_location = "above", tools = "pan,wheel_zoom,save,box_zoom,reset")
        plot.tools.append(hover)
        df_total = df[df["event_name"] == "kbytestotal"]
        df_free = df[df["event_name"] == "kbytesfree"]
        source = ColumnDataSource({"targets" : targets, "total" : scale*df_total["value"], 
                                   "time" : df_free.index.tz_localize(None),
                                   "used" : scale*(df_total["value"].to_numpy()-df_free["value"].to_numpy())})
        plot.vbar(x = "targets", top = "total", source = source, width = 0.8, alpha = 0.5, legend_label = "Total GB")    
        plot.vbar(x = "targets", top = "used", source = source, width = 0.8, color = "red", legend_label = "Used GB")
        plots += [plot]

        return gridplot(plots, ncols = 1)


def home(request):
    field = {}

    bucket = "60 seconds"
    interval = '2 hours'

    P = TimePlot(bucket, interval)

    field["ost_script"], field["ost_div"] = components(P.plotlatestvalueby_host())

    iops_tuples = ('open', 'close', 'setattr', 'create', 'statfs', 'set_info')
    bw_tuples = ('read_bytes', 'write_bytes')
    cpu_tuples = ('system', 'user', "iowait", "nice", "irq", "softirq")

    field["mds_freeram_script"], field["mds_freeram_div"] = components(P.plotrateby_host("mds", "cpu", cpu_tuples))
    field["oss_freeram_script"], field["oss_freeram_div"] = components(P.plotrateby_host("oss", "cpu", cpu_tuples))
            
    field["mds_script"], field["mds_div"] = components(P.plotrateby_tag("mds", "mds", iops_tuples))
    field["oss_script"], field["oss_div"] = components(P.plotrateby_tag("oss", "oss", bw_tuples))


    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    
    return render(request, "pfs_app/home.html", field)


