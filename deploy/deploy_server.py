#!/usr/bin/env python

from __future__ import print_function

# System
import time
import sys

# Local
import utils as utils

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


def deploy(state, meta_dict, count, prefix, input_name):
  for index in range(count):
    name = str(utils.proj_prefix) + str(prefix) + str(index)
    if name != input_name: continue
    utils.init_log.debug("creating VM with name {0}".format(name))
    server = utils.create_server(state, name=name)
    meta_dict[name] = server

  return meta_dict

#---------------------------------------------------------------------------


def deploy_lustre_vms(state, input_name):
  mds_dict     = dict()
  oss_dict     = dict()
  compute_dict = dict()

  utils.init_log.debug("creating {0} mds servers".format(utils.mds_count))
  mds_dict     = deploy(state, mds_dict, utils.mds_count, utils.mds_prefix, input_name)
  utils.init_log.debug("creating {0} oss servers".format(utils.oss_count))
  oss_dict     = deploy(state, oss_dict, utils.oss_count, utils.oss_prefix, input_name)
  utils.init_log.debug("creating {0} compute servers".format(utils.compute_count))
  compute_dict = deploy(state, compute_dict, utils.compute_count, utils.compute_prefix, input_name)

  return { utils.mds_prefix:mds_dict, utils.oss_prefix:oss_dict, utils.compute_prefix:compute_dict }

#---------------------------------------------------------------------------


def vm_creation_summary(created):
  utils.init_log.info(str(len(created[utils.mds_prefix]))     + " CREATED MDS SERVERS")
  utils.init_log.info(str(len(created[utils.oss_prefix]))     + " CREATED OSS SERVERS")
  utils.init_log.info(str(len(created[utils.compute_prefix])) + " CREATED COMPUTE CLIENTS")

#---------------------------------------------------------------------------


def main():

  if len(sys.argv) <= 1:
    print("Input server name to deploy")
    return -1

  input_name = sys.argv[1]

  state = utils.initialize()
  utils.summarize(state)

  created = deploy_lustre_vms(state, input_name)

  utils.init_log.info("10 second deployment cooldown...")
  time.sleep(10)

  vm_creation_summary(created)

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
