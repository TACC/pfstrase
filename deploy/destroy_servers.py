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
  server_dict = utils.create_server_dict(all_servers, filter_regex=None)

  for server_name, server in server_dict.iteritems():
    utils.destroy_server(state, server)
  
  utils.init_log.info("10 second destroy cooldown...")
  time.sleep(10)


  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
