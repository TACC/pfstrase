from datetime import datetime, timedelta
import pytz, time
from math import pi

from django.shortcuts import render
from django.db.models import Count, F, Value, IntegerField, FloatField, CharField
from django.db.models.functions import Cast, Concat
from django.contrib.postgres.fields.jsonb import KeyTextTransform as ktt
from django.contrib.postgres.fields.jsonb import KeyTransform as kt
from django.contrib.postgres.aggregates import StringAgg
from django.utils import timezone
from django.db.models import Avg,Sum,Max,Min
from django import forms

from bokeh.plotting import figure
from bokeh.embed import components
from bokeh.resources import CDN
from bokeh.layouts import gridplot, column
from bokeh.palettes import d3, Category20b
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, Range1d, Div, RadioButtonGroup, CustomJS
from bokeh.models import HoverTool, PanTool, WheelZoomTool, BoxZoomTool, UndoTool, RedoTool, ResetTool, OpenURL
from bokeh.models.glyphs import Step, Line
from bokeh.transform import factor_cmap, transform, linear_cmap, cumsum
from bokeh.models import (BasicTicker, ColorBar, ColumnDataSource,
                          LinearColorMapper, PrintfTickFormatter,)


from pandas import read_sql
import pandas 
pandas.set_option('display.max_rows', None)
from .models import Stats
import time
tz = timezone.get_current_timezone()

import psycopg2
conn = psycopg2.connect("dbname=pfstrase_db user=postgres port=5433")
cur = conn.cursor(cursor_factory=psycopg2.extras.RealDictCursor)

class RatePlot():
    def __init__(self, bucket, start, end = "clock_timestamp()"):
        self.bucket = bucket
        self.start = start
        self.end  = end
        self.cur = conn.cursor()
        print(read_sql("select version()",conn))

        self.cur.execute("DROP VIEW IF EXISTS server_rates CASCADE;")
                
        query = """CREATE TEMP VIEW server_rates AS select time_bucket_gapfill('{0}', time) as 
        t, hostname, uid, jid, event_name, locf(last(value,time), treat_null_as_missing => true) as value from stats 
        where time > {1} and time <= {2} 
        group by t, hostname, uid, jid, event_name order by hostname, t;""".format(self.bucket, self.start, self.end)

        self.cur.execute(query)        

        ev = time.time()
        df = read_sql("select hostname, event_name, uid, jid from server_rates group by hostname, event_name, uid, jid", conn)
        self.hostnames = sorted(list(set(df["hostname"])))
        self.events = sorted(list(set(df["event_name"])))
        self.uids = sorted(list(set(df["uid"])))
        self.jids = sorted(list(set(df["jid"])))
        print("time to select distinct tags", time.time()-ev)

        print("intialize view")
        #print(self.hostnames)
        #print(self.events)
        #print(self.uids)
        #print(self.jids)

        self.units = {"load" : "#cores", "load_eff" : "#cores", "bytes" : "MB/s", 
                      "read_bytes" : "MB/s", "write_bytes" : "MB/s", "nclients" : "#", "iops" : "#/s"}
        pandas.set_option('display.max_columns', 20)

    def format_plot(self, p):
        p.legend.click_policy="hide"
        p.legend.orientation = "horizontal"
        p.xaxis.axis_label_text_font_size = "12pt"
        p.yaxis.axis_label_text_font_size = "12pt"
        new_legend = p.legend[0]
        p.legend[0] = None
        p.add_layout(new_legend, 'below')

    def plothosteventby_tag(self, tag_name, event, hostname = None, tag = None):        

        if hostname:
            df = read_sql("""select t, hostname, {0}, sum(value) as value from server_rates 
            where event_name = '{1}' and hostname = '{2}' 
            group by t, hostname, {0} order by hostname, {0}, t""".format(tag_name, event, hostname), conn)
            hosts = [hostname]
        else:
            df = read_sql("""select t, hostname, {0}, sum(value) as value from server_rates 
            where event_name = '{1}' 
            group by t, hostname, {0} order by hostname, {0}, t""".format(tag_name, event), conn)        
            hosts = self.hostnames
        
        df["t"].dt.tz_convert(tz).dt.tz_localize(None)        
        ts = sorted(list(set(df["t"])))
        dt = (ts[1] - ts[0]).total_seconds()

        if tag:            
            df.loc[(df[tag_name] != tag), tag_name] = '*'
            df = df.groupby(["t", "hostname", tag_name]).sum().reset_index()
            print(df)
        tags = df[tag_name].unique()
        color = list(Category20b[min(20, max(3, len(tags)))])
        cmap = {}
        for i, t in enumerate(tags):
            try: cmap[t] = color[i]
            except: cmap[t] = "#7f7f7f"

        try: 
            units = self.units[event]
        except:
            units = "#/s"
        plots = []

        for h in hosts:   
            data = df[df.hostname == h]
            source = ColumnDataSource(data = data.pivot(index="t", columns = tag_name, values = "value"))
            ylimit = (1.1*data["value"]).max()
            plot = figure(title = h + ": " + event + " by " + tag_name, plot_width=1200, plot_height=300, 
                          x_axis_type = "datetime", 
                          y_range = Range1d(0, ylimit), y_axis_label = units, 
                          tools = "pan,wheel_zoom,save,box_zoom,reset",tooltips="$name: @$name")
            plot.vbar_stack(tags, x= "t", width=dt*1000, alpha=0.5, color = list(cmap.values()), source = source)

            plots += [plot]
        return gridplot(plots, ncols = 1)

    def piechartsby_tag(self, tag, event, hostname = None):
        
        if hostname:
            df = read_sql("""select distinct on ({0}) t, hostname, {0}, sum(value) as value
            from server_rates where hostname = '{2}' and event_name = '{1}' 
            group by t, hostname, {0} order by {0}, hostname, t desc""".format(tag,event,hostname),conn)
            hosts = [hostname]
        else:
            df = read_sql("""select distinct on ({0}, hostname) t, hostname, {0}, sum(value) as value
            from server_rates where event_name = '{1}' 
            group by t, hostname, {0} order by {0}, hostname, t desc""".format(tag,event),conn)
            hosts = self.hostnames

        df = df.set_index("t").tz_convert(tz).tz_localize(None)            

        plots = []
        tags  = df[tag].unique()
        color = list(Category20b[min(20, max(3, len(tags)))])
        cmap = {}
        for i, t in enumerate(tags):
            try: cmap[t] = color[i]
            except: cmap[t] = "#7f7f7f"
        df.insert(1, "color", list(df[tag]))
        df = df.replace({"color" : cmap})

        df_total = df.groupby([df.index, "hostname"]).sum().reset_index(level = "hostname")

        total_colors = ["#75968f", "#a5bab7", "#c9d9d3", "#e2e2e2", "#dfccce", "#ddb7b1", "#cc7878", "#933b41", "#550b1d"]
        mapper = LinearColorMapper(palette=total_colors, low=df_total["value"].min(), high=df_total["value"].max())
        TOOLS = "hover,save,box_zoom,reset,wheel_zoom"

        for h in hosts:
            data = df[df.hostname == h].copy()
            total = df_total[df_total.hostname == h].copy()            
            data["angle"] = data["value"] / total["value"] * 2*pi            

            p = figure(plot_height=150, plot_width = 150, 
                       title= h.split('.')[0], 
                       tools=TOOLS, 
                       tooltips='@'+tag+ ': @value',
                       x_range=(-0.5, 0.5), y_range=(-0.5, 0.5))
            
            p.annular_wedge(x=0, y=0.0, inner_radius=0.1, outer_radius=0.4,
                            start_angle=cumsum('angle', include_zero=True), end_angle=cumsum('angle'),
                            line_color="white", fill_color="color", source=data)            
            
            p.wedge(x=0, y=0.0, radius=0.1,
                    start_angle=0, end_angle=2*pi,
                    line_color=None, fill_color= {'field' : "value", 'transform' : mapper}, source=total)
            
            p.axis.visible=False
            p.grid.grid_line_color = None
            plots += [p]

        title = Div(text="<b>" + str(data.index[-1]) + ":</b> " + event + " by " + tag, sizing_mode="stretch_width", 
                    style={'font-size': '100%', 'color': 'black'})
        
        return column(title, gridplot(plots, ncols = int(1 + len(hosts)**0.5)))

    def ploteventby_host(self, event_name):
        df = read_sql("""select t, hostname, sum(value) as value from server_rates 
        where event_name = '{0}' group by t, hostname order by hostname""".format(event_name), conn)
        ts = sorted(list(set(df["t"])))
        df = df.set_index("t").tz_convert(tz).tz_localize(None)        
        dt = (ts[1] - ts[0]).total_seconds()

        hosts = self.hostnames[::-1]
        colors = ["#75968f", "#a5bab7", "#c9d9d3", "#e2e2e2", "#dfccce", "#ddb7b1", "#cc7878", "#933b41", "#550b1d"]
        mapper = LinearColorMapper(palette=colors, low=df["value"].min(), high=df["value"].max())

        TOOLS = "save,pan,box_zoom,reset,wheel_zoom"
        p = figure(plot_width=20*len(ts), plot_height=max(200, 20*len(hosts)), title="Total " + event_name + " per server",
                   x_axis_type = 'datetime', 
                   y_range = hosts,
                   x_axis_location="above", toolbar_location='below', 
                   tools=TOOLS)
        p.add_tools(HoverTool(tooltips=[('host', '@hostname'), ("time", "@t{%Y-%m-%d %H:%M:%S}"), 
                                        (event_name, '@value')], formatters = {"@t" : "datetime"}))
        
        p.rect(x = "t", y = "hostname", width = dt*1000, height=1, source = df, 
               fill_color = {'field' : 'value', 'transform' : mapper}, line_color="white")

        color_bar = ColorBar(color_mapper=mapper, location=(0, 0),
                             ticker=BasicTicker(desired_num_ticks=len(colors)),
                             formatter=PrintfTickFormatter(format="%.2f"))
        p.add_layout(color_bar, 'right')
        p.grid.grid_line_color = None        
        p.axis.axis_line_color = None
        p.axis.major_tick_line_color = None
        p.axis.major_label_text_font_size = "10px"
        p.axis.major_label_standoff = 1
        p.xaxis.major_label_orientation = 1.0
        p.x_range.range_padding = 0
        return p

P = RatePlot("1m", "clock_timestamp() - interval '15m'")    

class ChoiceForm(forms.Form):
    TAGCHOICES = [("jid", "jid"),("uid", "uid")]
    EVENTCHOICES = [(e, e) for e in P.events]
    tag = forms.ChoiceField(choices=TAGCHOICES, initial = "jid", required = True)
    event = forms.ChoiceField(choices=EVENTCHOICES, initial = "load_eff", required = True)

    TIMECHOICES = [("15m", "15m"),("1h", "1h"),("1d", "1d")]
    BUCKETCHOICES = [("30s", "30s"),("1m", "1m"),("1h", "1h")]
    time_interval = forms.ChoiceField(choices=TIMECHOICES, initial = "15m", required = True)
    time_bucket = forms.ChoiceField(choices=BUCKETCHOICES, initial = "1m", required = True)

def history(request, hostname):
    field = {}
    if request.method == 'POST':
        tag = request.POST["tag"]
        event = request.POST["event"]
        time_interval = request.POST["time_interval"]
        time_bucket = request.POST["time_bucket"]
        P = RatePlot(time_bucket, "clock_timestamp() - interval '{0}'".format(time_interval))    
        field["choice"] = ChoiceForm(request.POST)
    else:
        tag = "jid"
        event = "nclients"
        time_interval = "15m"
        time_bucket = "1m"
        P = RatePlot(time_bucket, "clock_timestamp() - interval '{0}'".format(time_interval))    
        field["choice"] = ChoiceForm()

    field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag, event, hostname = hostname))
    field["hm_script"], field["hm_div"] = components(P.plothosteventby_tag(tag, event, hostname = hostname))

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hostnames"] = P.hostnames
    field["hostname"] = hostname
    return render(request, "pfs_app/history.html", field)

def tag_detail(request):
    print(request)

    tag = request.GET["tag"]

    field = {}

    tag_name = "jid"
    event = "nclients"
    time_bucket = "30m"
        
    ts = read_sql("""select min(time), max(time) from stats where jid = '{0}'""".format(tag), conn)
    
    tmin = "\'" + str(ts["min"].dt.tz_convert(tz).dt.tz_localize(None).values[0]) + "\'"
    tmax = "\'" + str(ts["max"].dt.tz_convert(tz).dt.tz_localize(None).values[0]) + "\'"

    P = RatePlot(time_bucket, tmin, tmax)
    
    field["choice"] = ChoiceForm()        

    #field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag_name, event))
    field["hm_script"], field["hm_div"] = components(P.plothosteventby_tag(tag_name, event, tag = tag))

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hostnames"] = P.hostnames

    return render(request, "pfs_app/tag_detail.html", field)

def home(request):
    field = {}
    if request.method == 'POST':
        tag = request.POST["tag"]
        event = request.POST["event"]
        time_interval = request.POST["time_interval"]
        time_bucket = request.POST["time_bucket"]
        P = RatePlot(time_bucket, "clock_timestamp() - interval '{0}'".format(time_interval))
        field["choice"] = ChoiceForm(request.POST)
    else:
        tag = "jid"
        event = "load_eff"
        time_interval = "15m"
        time_bucket = "1m"
        P = RatePlot(time_bucket, "clock_timestamp() - interval '{0}'".format(time_interval))
        field["choice"] = ChoiceForm()
        
    q = time.time()
    field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag, event))
    print("time to build pie chart", time.time()-q)
    q = time.time()
    field["hm_script"], field["hm_div"] = components(P.ploteventby_host(event))
    print("time to build heat map", time.time()-q)

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hostnames"] = P.hostnames
    
    return render(request, "pfs_app/home.html", field)


