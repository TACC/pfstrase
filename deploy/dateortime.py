#!/usr/bin/env python

## \file experiments/python_util/dateortime.py
# \author W. Cyrus Proctor
# \date Thursday November 21 9:49:17
# \note Copywrite (C) 2013 W. Cyrus Proctor

import time
import datetime
import os
import sys
import functools



def datetime_to_string():
  t = datetime.datetime.now()

  dtstr = str(t.year) + '-' + \
    str(t.month).zfill(2) + '-' + \
    str(t.day).zfill(2) + '_' + \
    str(t.hour).zfill(2) + '-' + \
    str(t.minute).zfill(2) + '-' + \
    str(t.second).zfill(2)

  return dtstr



def timer(denovo_inst):
  def actualDecorator(test_func):
    @functools.wraps(test_func)
    def wrapper(*args, **kwargs):
      time1 = time.time()
      ret = test_func(*args,**kwargs)
      time2 = time.time()
      if denovo_inst.node() == 0:
        print '%s function took %0.3f s' % (test_func.__name__, (time2-time1))
      return ret 
    return wrapper
  return actualDecorator



def timing(f):
  def wrap(*args,**kwargs):
    time1 = time.time()
    ret = f(*args,**kwargs)
    time2 = time.time()
    print '%s function took %0.3f s' % (f.__name__, (time2-time1))
    return ret
  return wrap


def dump_args(func):
    "This decorator dumps out the arguments passed to a function before calling it"
    argnames = func.func_code.co_varnames[:func.func_code.co_argcount]
    fname = func.func_name
    def echo_func(*args,**kwargs):
        print fname, "(", ', '.join(
            '%s=%r' % entry
            for entry in zip(argnames,args[:len(argnames)])+[("args",list(args[len(argnames):]))]+[("kwargs",kwargs)]) +")"
    return echo_func
