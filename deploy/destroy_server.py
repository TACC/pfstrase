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

  if len(sys.argv) <= 1:
    print("Input server name to destroy")
    return -1

  input_name = sys.argv[1]

  state = utils.initialize()
  utils.summarize(state)

  all_servers = state["nova"].servers.list()
  server_dict = utils.create_server_dict(all_servers, filter_regex=None)

  if input_name in server_dict:
    utils.destroy_server(state, server_dict[input_name])
  
  utils.init_log.info("3 second destroy cooldown...")
  time.sleep(3)


  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
