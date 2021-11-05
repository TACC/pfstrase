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
from bokeh.palettes import d3, Category20b, Category20
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, Range1d, Div, RadioButtonGroup, CustomJS, Label
from bokeh.models import HoverTool, PanTool, WheelZoomTool, BoxZoomTool, UndoTool, RedoTool, ResetTool, OpenURL, Title
from bokeh.models.glyphs import Step, Line
from bokeh.transform import factor_cmap, transform, cumsum
from bokeh.models import (BasicTicker, ColorBar, ColumnDataSource,
                          LinearColorMapper, PrintfTickFormatter,)


from pandas import read_sql
import pandas 
pandas.set_option('display.max_rows', None)
from .models import Stats
import time
tz = timezone.get_current_timezone()

import psycopg2

DATABASE = "pfstrase_db"
PORT = "5433"

CONNECTION = "dbname={0} user=postgres port={1}".format(DATABASE, PORT)


conn = psycopg2.connect(CONNECTION)
print(conn.server_version)
with conn.cursor() as cur:

    cur.execute("SELECT pg_size_pretty(pg_database_size(\'{0}\'));".format(DATABASE))
    for x in cur.fetchall():
        print("Database Size:", x[0])

    cur.execute("SELECT chunk_name,before_compression_total_bytes/(1024.0*1024*1024),after_compression_total_bytes/(1024.0*1024*1024) from chunk_compression_stats('host_data');")
    for x in cur.fetchall():
        try: print("{0} Size: before {1:8.1f} GB / after {2:8.1f} GB".format(*x))
        except: pass

    cur.execute("SELECT chunk_name,range_start,range_end FROM timescaledb_information.chunks WHERE hypertable_name = 'host_data';")
    for x in cur.fetchall():
        try:
            print("{0} Range: {1} -> {2}".format(*x))
        except: pass

conn.close()

class RatePlot():
    def __init__(self, bucket, start, end = "now()"):
        self.bucket = bucket
        self.start = start
        self.end  = end
        self.conn = psycopg2.connect(CONNECTION)
        self.cur = self.conn.cursor()

        s = time.time()

        query = """DROP VIEW IF EXISTS server_rates CASCADE; CREATE TEMP VIEW server_rates AS select time_bucket_gapfill('{0}', time) as 
        t, host, system, uid, jid, event, coalesce(avg(value),0) as value from host_data
        where time > {1} and time <= {2} 
        group by t, host, system, uid, jid, event order by host, t;""".format(self.bucket, self.start, self.end)           
        self.cur.execute(query)        

        print("bucket time: {0:.1f}".format(time.time() - s))

        ev = time.time()
        df = read_sql("select distinct on (system) system from host_data where time > {0} and time <= {1} order by system, time desc".format(self.start, self.end), self.conn)
        self.systems = sorted(list(set(df["system"])))
        df = read_sql("select distinct on (host) host from host_data where time > {0} and time <= {1} order by host, time desc".format(self.start, self.end), self.conn)
        self.hosts = sorted(list(set(df["host"])))
        df = read_sql("select distinct on (event) event from host_data where time > {0} and time <= {1} order by event, time desc".format(self.start, self.end), self.conn)
        self.events = sorted(list(set(df["event"])))

        print("time to select distinct tags", time.time()-ev)

        print("intialize view")
        print(self.systems)
        print(self.hosts)
        print(self.events)

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

    def plothosteventby_tag(self, tag_name, event, host = None, tag = None):        

        s = time.time()

        df = read_sql("""select time_bucket('{0}', time) as t, host, system, {1}, sum(value) as value from host_data 
        where time > {2} and time <={3}  and event = '{4}' and host = '{5}' group by t, host, system, {1} order by host, system, {1}, t
        """.format(self.bucket, tag_name, self.start, self.end, event, host), self.conn)

        hosts = [host]

        print("stack chart query: {0:.1f}".format(time.time() - s))
        
        df["t"].dt.tz_convert(tz).dt.tz_localize(None)        
        ts = sorted(list(set(df["t"])))
        dt = (ts[1] - ts[0]).total_seconds()

        if tag:            
            df.loc[(df[tag_name] != tag), tag_name] = '*'
            df = df.groupby(["t", "host", "system", tag_name]).sum().reset_index()
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
        
        for s in self.systems:   
            system_data = df[df.system == s]
            for h in hosts:   
                data = system_data[system_data.host == h]
                if len(data) == 0: continue
                source = ColumnDataSource(data = data.pivot(index="t", columns = tag_name, values = "value"))
                ylimit = (1.1*data["value"]).max()
                plot = figure(title = s + " " + h + ": " + event + " by " + tag_name, plot_width=1200, plot_height=300, 
                              x_axis_type = "datetime", 
                              y_range = Range1d(0, ylimit), y_axis_label = units, 
                              tools = "pan,wheel_zoom,save,box_zoom,reset",tooltips="$name: @$name")
                plot.vbar_stack(tags, x= "t", width=dt*1000, alpha=0.5, color = list(cmap.values()), source = source)
                plots += [plot]

        return gridplot(plots, ncols = 1)

    def piechartsby_tag(self, tag, event, host = None):
        s = time.time()

        df = read_sql("""select {0}, host, system, time, value from host_data where time > {2} and time <= {3}
        and event = '{1}' and (host, time) in (select distinct on (host) host, time from host_data where
        time > {2} and time <= {3} and event = 'nclients' and value > 0 order by host, time desc)
        """.format(tag, event, self.start, self.end),self.conn)           

        hosts = self.hosts
        if host: hosts = [host]

        print("pie chart query: {0:.1f}".format(time.time() - s))

        df = df.set_index("time").tz_convert(tz).tz_localize(None)            
    
        plots = []
        tags  = df[tag].unique()
        color = list(Category20[20])

        cmap = {}
        for i, t in enumerate(tags):
            cmap[t] = color[i%20]

        df.insert(1, "color", list(df[tag]))
        df = df.replace({"color" : cmap})

        df_total = df.groupby([df.index, "host"]).sum().reset_index(level = "host")
        TOOLS = "hover,save,box_zoom,reset,wheel_zoom"

        for h in hosts:
            data = df[df.host == h].copy()
            if len(data) == 0: continue
            total = df_total[df_total.host == h].copy()            
            data["angle"] = data["value"] / total["value"] * 2*pi            

            p = figure(plot_height=280, plot_width = 280, 
                       tools=TOOLS, 
                       tooltips='system: @system ' + '@'+tag+ ': @value',
                       x_range=(-1.4, 1.4))#, y_range=(-0.6, 0.6))

            p.add_layout(Title(text=str(data.index[-1]), text_font_style="italic"), 'below')
            p.add_layout(Title(text=h.split('.')[0], text_font_size="10pt"), 'above')

            p.annular_wedge(x=0, y=0.0, inner_radius=0.4, outer_radius=1.0,
                            start_angle=cumsum('angle', include_zero=True), end_angle=cumsum('angle'),
                            line_color="white", fill_color="color", source=data)                        

            p.add_layout(Label(x=0,y=0,text_baseline="middle", text_align="center", text = "{0:.1f}".format(total["value"].values[0])))
            p.axis.visible=False
            p.grid.grid_line_color = None
            plots += [p]

        if len(df) > 0:
            title = Div(text=event + " by " + tag, sizing_mode="stretch_width", 
                        style={'font-size': '200%', 'color': 'black'})
        else:
            title = Div(text="event" + " by " + tag + " N/A", sizing_mode="stretch_width", 
                        style={'font-size': '200%', 'color': 'black'})
        return column(title, gridplot(plots, ncols = int(1 + len(hosts)**0.5)))

    def ploteventby_host(self, event):

        df = read_sql("""select time_bucket('{0}', time) as t, host, sum(value) as value from host_data 
        where time > {2} and time <= {3} and event = '{1}' group by t, host order by host
        """.format(self.bucket, event, self.start, self.end), self.conn)


        ts = sorted(list(set(df["t"])))
        df = df.set_index("t").tz_convert(tz).tz_localize(None)        
        dt = (ts[1] - ts[0]).total_seconds()

        hosts = self.hosts[::-1]
        colors = ["#75968f", "#a5bab7", "#c9d9d3", "#e2e2e2", "#dfccce", "#ddb7b1", "#cc7878", "#933b41", "#550b1d"]
        mapper = LinearColorMapper(palette=colors, low=df["value"].min(), high=df["value"].max())

        TOOLS = "save,pan,box_zoom,reset,wheel_zoom"
        p = figure(plot_width=20*len(ts), plot_height=max(200, 20*len(hosts)), title="Total " + event + " per server",
                   x_axis_type = 'datetime', 
                   y_range = hosts,
                   x_axis_location="above", toolbar_location='below', 
                   tools=TOOLS)
        p.add_tools(HoverTool(tooltips=[('host', '@host'), ("time", "@t{%Y-%m-%d %H:%M:%S}"), 
                                        (event, '@value')], formatters = {"@t" : "datetime"}))
        
        p.rect(x = "t", y = "host", width = dt*1000, height=1, source = df, 
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

P = RatePlot("1m", "now() - interval '15m'")    

class ChoiceForm(forms.Form):
    TAGCHOICES = [("jid", "jid"),("uid", "uid")]
    EVENTCHOICES = [(e, e) for e in P.events]
    tag = forms.ChoiceField(choices=TAGCHOICES, initial = "jid", required = True)
    event = forms.ChoiceField(choices=EVENTCHOICES, initial = "load_eff", required = True)

    TIMECHOICES = [("15m", "15m"),("1h", "1h"),("1d", "1d")]
    BUCKETCHOICES = [("30s", "30s"),("1m", "1m"),("1h", "1h")]
    time_interval = forms.ChoiceField(choices=TIMECHOICES, initial = "15m", required = True)
    time_bucket = forms.ChoiceField(choices=BUCKETCHOICES, initial = "1m", required = True)

def history(request, host):
    print(host)
    field = {}
    if request.method == 'POST':
        tag = request.POST["tag"]
        event = request.POST["event"]
        time_interval = request.POST["time_interval"]
        time_bucket = request.POST["time_bucket"]
        P = RatePlot(time_bucket, "now() - interval '{0}'".format(time_interval))    
        field["choice"] = ChoiceForm(request.POST)
    else:
        tag = "jid"
        event = "load_eff"
        time_interval = "15m"
        time_bucket = "1m"
        P = RatePlot(time_bucket, "now() - interval '{0}'".format(time_interval))    
        field["choice"] = ChoiceForm()

    field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag, event, host = host))
    #try:
    field["hm_script"], field["hm_div"] = components(P.plothosteventby_tag(tag, event, host = host))
    #except:
    #    pass
    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hosts"] = P.hosts
    field["host"] = host
    return render(request, "pfs_app/history.html", field)

def tag_detail(request):
    print(request)

    tag = request.GET["tag"]

    field = {}

    tag_name = "jid"
    event = "nclients"
    time_bucket = "30m"
    conn = psycopg2.connect(CONNECTION)
    ts = read_sql("""select min(time), max(time) from host_data where jid = '{0}'""".format(tag), conn)
    conn.close()
    print(ts)
    tmin = "\'" + str(ts["min"].dt.tz_convert(tz).dt.tz_localize(None).values[0]) + "\'"
    tmax = "\'" + str(ts["max"].dt.tz_convert(tz).dt.tz_localize(None).values[0]) + "\'"

    P = RatePlot(time_bucket, tmin, tmax)
    
    field["choice"] = ChoiceForm()        

    #field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag_name, event))
    field["hm_script"], field["hm_div"] = components(P.plothosteventby_tag(tag_name, event, tag = tag))

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hosts"] = P.hosts

    return render(request, "pfs_app/tag_detail.html", field)

def home(request):
    field = {}
    if request.method == 'POST':
        tag = request.POST["tag"]
        event = request.POST["event"]
        time_interval = request.POST["time_interval"]
        time_bucket = request.POST["time_bucket"]
        P = RatePlot(time_bucket, "now() - interval '{0}'".format(time_interval))
        field["choice"] = ChoiceForm(request.POST)
    else:
        tag = "jid"
        event = "load_eff"
        time_interval = "15m"
        time_bucket = "1m"
        P = RatePlot(time_bucket, "now() - interval '{0}'".format(time_interval))
        field["choice"] = ChoiceForm()
        
    q = time.time()
    field["pie_script"], field["pie_div"] = components(P.piechartsby_tag(tag, event))
    print("time to build pie chart", time.time()-q)
    q = time.time()

    field["hm_script"], field["hm_div"] = components(P.ploteventby_host(event))

    print("time to build heat map", time.time()-q)

    field["resources"] = CDN.render()
    field["datetime"] = timezone.now()
    field["hosts"] = P.hosts
    
    return render(request, "pfs_app/home.html", field)


