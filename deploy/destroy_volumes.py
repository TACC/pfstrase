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

  all_volumes = state["cinder"].volumes.list()
  volume_dict = utils.create_volume_dict(all_volumes, filter_regex=None)

  for volume_name, volume in volume_dict.iteritems():
    utils.destroy_volume(state, volume)
  
  utils.init_log.info("10 second destroy cooldown...")
  time.sleep(10)

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
