#!/usr/bin/env python

from __future__ import print_function

# System
import time
import sys
import subprocess as sp

# Local
import utils as utils

KNOWN_HOSTS     = "/root/.ssh/known_hosts"
ETC_HOSTS       = "/etc/hosts"
ANSIBLE_INV     = "/etc/ansible/hosts"
INVENTORY_BEGIN = "# Inventory Begin"
INVENTORY_END   = "# Inventory End"

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


def update_known_hosts_file(ip_dict):
  for host, ip in ip_dict.iteritems():
    c1 = [ "ssh-keygen", "-R", str(host) ]
    c2 = [ "ssh-keygen", "-R", str(ip)   ]
    c3 = [ "ssh-keyscan", str(ip) + "," + str(host) ]
    utils.init_log.debug("Command to be run:")
    utils.init_log.debug(" ".join(c1))
    utils.init_log.debug("Command to be run:")
    utils.init_log.debug(" ".join(c2))
    utils.init_log.debug("Command to be run:")
    utils.init_log.debug(" ".join(c3))
    p1 = sp.Popen(c1, stdout=sp.PIPE, stderr=sp.STDOUT)
    p2 = sp.Popen(c2, stdout=sp.PIPE, stderr=sp.STDOUT)
    with open(KNOWN_HOSTS, "a") as khf:
      p3 = sp.Popen(c3, stdout=khf) # Do not wish for stderr
    s1 = p1.communicate()[0]
    s2 = p2.communicate()[0]
    s3 = p3.communicate()[0]
    utils.init_log.debug(s1)
    utils.init_log.debug(s2)
    utils.init_log.debug(s3)

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


def update_ansible_inventory_file(ip_dict):

  template = open(ANSIBLE_INV, "r").read().split("\n")
  utils.init_log.debug("Old {0} file:".format(ANSIBLE_INV))
  for line in template:
    utils.init_log.debug(line)
  updated = ""
  host_str = create_ansible_hosts_file_str(ip_dict)
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

  utils.init_log.debug("New {0} file:".format(ANSIBLE_INV))
  for line in updated.split("\n"):
    utils.init_log.debug(line)

  with open(ANSIBLE_INV, "w") as f:
    f.write(updated)

  utils.init_log.info("New {0} file written.".format(ANSIBLE_INV))

#---------------------------------------------------------------------------


def create_ansible_hosts_file_str(ip_dict):
  host_str    = INVENTORY_BEGIN + "\n"
  mds_str     = "[" + utils.mds_prefix     + "]\n"
  oss_str     = "[" + utils.oss_prefix     + "]\n"
  compute_str = "[" + utils.compute_prefix + "]\n"
  for host, ip in ip_dict.iteritems():
    if utils.proj_prefix + utils.mds_prefix     in host: mds_str     += str(host) + " ansible_ssh_host=" + str(ip) + "\n"
    if utils.proj_prefix + utils.oss_prefix     in host: oss_str     += str(host) + " ansible_ssh_host=" + str(ip) + "\n"
    if utils.proj_prefix + utils.compute_prefix in host: compute_str += str(host) + " ansible_ssh_host=" + str(ip) + "\n"

  host_str += mds_str
  host_str += oss_str
  host_str += compute_str
  host_str += INVENTORY_END + "\n"
  
  return host_str

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
  update_known_hosts_file(ip_dict)
  update_ansible_inventory_file(ip_dict)

  utils.summarize(state)
  
  return 0

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


if __name__ == "__main__":
  sys.exit(main())
