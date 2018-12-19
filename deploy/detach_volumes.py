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


def main():
  state = utils.initialize()
  utils.summarize(state)

  all_servers = state["nova"].servers.list()
  server_dict = utils.create_server_dict(all_servers)

  for server_name, server in server_dict.iteritems():
    utils.detach_all_volumes_from_server(state, server)
  

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
