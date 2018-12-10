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


def deploy(state, meta_dict, server_count, volume_count, server_prefix, volume_prefix, size):
  for server_index in range(server_count):
    for volume_index in range(volume_count):
      name = str(utils.proj_prefix) + str(server_prefix) + str(server_index) + "-" + str(volume_prefix) + str(volume_index)
      utils.init_log.debug("creating volume with name {0}".format(name))
      volume = utils.create_volume(state, name=name, size=size)
      meta_dict[name] = volume

  return meta_dict

#---------------------------------------------------------------------------


def deploy_lustre_volumes(state):
  mds_volume_dict = dict()
  oss_volume_dict = dict()

  mds_volume_count = utils.mds_count * utils.volumes_per_mds
  oss_volume_count = utils.oss_count * utils.volumes_per_oss

  utils.init_log.debug("creating {0} mds volumes".format(mds_volume_count))
  mds_volume_dict = deploy(state, mds_volume_dict, utils.mds_count, utils.volumes_per_mds, utils.mds_prefix, utils.volume_prefix, utils.mds_volume_size)
  utils.init_log.debug("creating {0} oss volumes".format(oss_volume_count))
  oss_volume_dict = deploy(state, oss_volume_dict, utils.oss_count, utils.volumes_per_oss, utils.oss_prefix, utils.volume_prefix, utils.oss_volume_size)

  return { utils.mds_prefix:mds_volume_dict, utils.oss_prefix:oss_volume_dict }

#---------------------------------------------------------------------------


def volume_creation_summary(created):
  utils.init_log.info(str(len(created[utils.mds_prefix])) + " CREATED MDS VOLUMES")
  utils.init_log.info(str(len(created[utils.oss_prefix])) + " CREATED OSS VOLUMES")

#---------------------------------------------------------------------------


def main():
  state = utils.initialize()
  utils.summarize(state)

  created = deploy_lustre_volumes(state)

  utils.init_log.info("10 second deployment cooldown...")
  time.sleep(10)

  volume_creation_summary(created)

  utils.summarize(state)

  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
