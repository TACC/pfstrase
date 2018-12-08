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
