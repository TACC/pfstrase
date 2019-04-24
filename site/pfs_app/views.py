from datetime import datetime, timedelta
import pytz, time
import requests, json

from django.shortcuts import render
from django.db.models import Count, F, Value, IntegerField, FloatField, CharField
from django.db.models.functions import Cast, Concat
from django.contrib.postgres.fields.jsonb import KeyTextTransform as ktt
from django.contrib.postgres.fields.jsonb import KeyTransform as kt
from django.contrib.postgres.aggregates import StringAgg

from bokeh.embed import components
from bokeh.layouts import gridplot
from bokeh.plotting import figure
from bokeh.palettes import d3
from bokeh.models import ColumnDataSource, Plot, Grid, DataRange1d, LinearAxis, DatetimeAxis
from bokeh.models import HoverTool, PanTool, WheelZoomTool, BoxZoomTool, UndoTool, RedoTool, ResetTool
from bokeh.models.glyphs import Step, Line

from .models import hosts

tz = pytz.timezone('US/Central')

def add_axes(plot, label):
    xaxis = DatetimeAxis()
    yaxis = LinearAxis()      
    yaxis.axis_label = label
    plot.add_layout(xaxis, 'below')        
    plot.add_layout(yaxis, 'left')
    plot.add_layout(Grid(dimension=0, ticker=xaxis.ticker))
    plot.add_layout(Grid(dimension=1, ticker=yaxis.ticker))
    return plot

factor = 1.0/(1024*1024)
def ost_plot():
    begin = time.time()
    start = (datetime.now() - timedelta(hours = 12)).strftime('%Y-%m-%dT%H:%M:%S.000Z')
    end   = datetime.now().strftime('%Y-%m-%dT%H:%M:%S.000Z')

    try:
        response = requests.get("http://localhost:9090/api/v1/query_range?query=rate(osc_total{ost=~\"scratch.*\"}[1m])&start=" + start + "&end=" + end + "&step=10s").json()["data"]["result"]
    except:
        response = []
    colors = d3["Category20"][20]
    plots = []

    plot = Plot(plot_width=1600, plot_height=600, x_range = DataRange1d(), y_range = DataRange1d())

    rend = []
    for i, recs in enumerate(response):
        ost = recs["metric"]["ost"]
        
        ts = list(map(lambda x: datetime.fromtimestamp(x[0], tz = tz), recs["values"]))
        vs = list(map(lambda x: float(x[1])*factor, recs["values"]))
        dt = list(map(lambda x: x.strftime("%Y-%m-%d %H:%M:%S"), ts))

        source = ColumnDataSource({"DateTime" : ts, "values" : vs, "ost" : len(vs)*[ost], "time_fmt" : dt})
        #rend += [plot.add_glyph(source, Step(x = "x",y = "y", mode = "after", line_color = colors[i%20]))]
        rend += [plot.add_glyph(source, Line(x = "DateTime", y = "values", line_color = colors[i%20]))]
    print("time", time.time() - begin)
    hover = HoverTool(tooltips = [("val", "@values"), ("time", "@time_fmt"), ("ost", "@ost")], 
                      line_policy = "nearest", renderers = rend)        
    plot.add_tools(hover, PanTool(), WheelZoomTool(), BoxZoomTool(), UndoTool(), RedoTool(), ResetTool())

    plot = add_axes(plot, "MB/s")

    return plot
    #plots += [add_axes(plot, "MB/s")]
    #return gridplot(plots, ncols = len(plots)//4 + 1, toolbar_options = {"logo" : None})

def home(request):
    field = {}

    mdc = hosts.objects.filter(obdclass = "osc").order_by("host", "-time").distinct("host")
    mds = hosts.objects.filter(obdclass = "mds").order_by("host", "-time").distinct("host")
    mds = mds.annotate(freeram = Cast(ktt("freeram", ktt('sysinfo', 'stats')), FloatField()))
    mds = mds.annotate(totalram = Cast(ktt("totalram", ktt('sysinfo', 'stats')), FloatField()))
    mds = mds.annotate(percentram = 100*(F('totalram')-F('freeram'))/F('totalram'))    

    field["mds"] = []
    for m in mds:
        ops = 0
        mdt_ops = {}
        jid_ops = {}
        user_ops = {}
        for mdt, exports in m.stats["mdt"].items():
            mdt_ops.setdefault(mdt, 0.0)
            for c in mdc:
                jid_ops.setdefault(c.jid, 0.0)
                user_ops.setdefault(c.user, 0.0)
                stats = exports[c.nid]
                try:
                    stats = (stats.get("open", 0) + stats.get("close", 0))
                    ops += stats
                    mdt_ops[mdt] += stats
                    jid_ops[c.jid] += stats                    
                    user_ops[c.user] += stats                    
                except: pass
        field["mds"] += [(m, ops, mdt_ops, jid_ops, user_ops)]

        
    osc = hosts.objects.filter(obdclass = "osc").order_by("host", "-time").distinct("host")
    oss = hosts.objects.filter(obdclass = "oss").order_by("host", "-time").distinct("host")
    oss = oss.annotate(freeram = Cast(ktt("freeram", ktt('sysinfo', 'stats')), FloatField()))
    oss = oss.annotate(totalram = Cast(ktt("totalram", ktt('sysinfo', 'stats')), FloatField()))
    oss = oss.annotate(percentram = 100*(F('totalram')-F('freeram'))/F('totalram'))    

    field["oss"] = [] 
    
    for o in oss:
        ops = 0
        ost_ops = {}
        jid_ops = {}
        user_ops = {}
        for ost, exports in o.stats["obdfilter"].items():
            ost_ops.setdefault(ost, 0.0)
            for c in osc:
                jid_ops.setdefault(c.jid, 0.0)
                user_ops.setdefault(c.user, 0.0)
                stats = exports[c.nid]
                try:
                    stats = (stats.get("read_bytes", 0) + stats.get("write_bytes", 0))/2**20
                    ops += stats
                    ost_ops[ost] += stats
                    jid_ops[c.jid] += stats                    
                    user_ops[c.user] += stats                    
                except: pass
        field["oss"] += [(o, ops, ost_ops, jid_ops, user_ops)]


    """
    script,div = components(nid_jid_plot())
    field["script"] = script
    field["div"] = div
    """
    return render(request, "pfs_app/home.html", field)


