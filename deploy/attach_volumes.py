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


def create_server_volume_name_mapping(server_dict, volume_dict):
  server_volume_dict = dict()
  for server_name, server in server_dict.iteritems():
    server_volume_dict[server_name] = {"server":server, "volume_dict":{key:value for key,value in volume_dict.items() if server_name in key}}
    utils.init_log.debug("Server Name: {0}; Volume Mapping: {1}".format(server_name, server_volume_dict[server_name]))
  
  return server_volume_dict

#---------------------------------------------------------------------------


def main():
  state = utils.initialize()
  utils.summarize(state)

  all_servers = state["nova"].servers.list()
  all_volumes = state["cinder"].volumes.list()
  
  server_dict = utils.create_server_dict(all_servers)
  volume_dict = utils.create_volume_dict(all_volumes)
  server_volume_dict = create_server_volume_name_mapping(server_dict, volume_dict)

  for server_name, mapping_dict  in server_volume_dict.iteritems():
    for volume_name, volume in mapping_dict["volume_dict"].iteritems():
      utils.attach_volume(state, mapping_dict["server"], volume)
  

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
