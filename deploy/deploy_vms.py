#!/usr/bin/env python

from __future__ import print_function

import time

import utils as utils

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------

def deploy(state, meta_dict, count, prefix):
  for index in range(count):
    name = str(prefix) + str(index)
    server = utils.create_vm(state, name=name)
    meta_dict[name] = server

  return meta_dict


if __name__ == "__main__":
  state = utils.initialize()
  utils.summarize(state)

  mds_dict     = dict()
  oss_dict     = dict()
  compute_dict = dict()

#  mds_dict     = deploy(state, mds_dict, utils.mds_count, "mds")
  oss_dict     = deploy(state, oss_dict, utils.oss_count, "oss")
#  compute_dict = deploy(state, compute_dict, utils.compute_count, "c")

  utils.init_log.info("10 second deployment cooldown...")
  time.sleep(10)
  utils.summarize(state)

  print(mds_dict)
  print(oss_dict)
  print(compute_dict)
