#!/usr/bin/env python

from __future__ import print_function

# System
import time
import sys

# Local
import utils as utils

ETC_HOSTS       = "/etc/hosts"
INVENTORY_BEGIN = "# Inventory Begin"
INVENTORY_END   = "# Inventory End"

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


def update_etc_hosts_file(ip_dict):
  template = open(ETC_HOSTS, "r").read().split("\n")
  utils.init_log.debug("Old {0} file:".format(ETC_HOSTS))
  for line in template:
    utils.init_log.debug(line)
  updated  = ""
  host_str = create_hosts_file_str(ip_dict)
  index = 0
  inv_present = False
  while index < len(template):
    line = template[index]
    if INVENTORY_BEGIN not in line: updated += line + "\n"
    else:
      inv_present = True
      for inv_line in template[index:]:
        index += 1
        if INVENTORY_END not in inv_line: continue
        else:
          updated += host_str
          break
    index += 1

  if not inv_present:
    updated += host_str

  utils.init_log.debug("New {0} file:".format(ETC_HOSTS))
  for line in updated.split("\n"):
    utils.init_log.debug(line)

  with open(ETC_HOSTS, "w") as f:
    f.write(updated)

  utils.init_log.info("New {0} file written.".format(ETC_HOSTS))


#---------------------------------------------------------------------------


def create_hosts_file_str(ip_dict):
  host_str = INVENTORY_BEGIN + "\n"
  for host, ip in ip_dict.iteritems():
    host_str += "{0} {1}\n".format(ip, host)
  host_str += INVENTORY_END + "\n"

  return host_str

#---------------------------------------------------------------------------


def main():
  state = utils.initialize()
  utils.summarize(state)

  all_servers = state["nova"].servers.list()

  ip_dict = utils.create_ip_dict(state, all_servers)
  update_etc_hosts_file(ip_dict)

  utils.init_log.info("10 second deployment cooldown...")
  time.sleep(10)

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
