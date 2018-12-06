#!/usr/bin/env python

from __future__ import print_function

import ConfigParser as cp
import logging      as lg
import os           as os
import sys          as sys

#---------------------------------------------------------------------------

from keystoneauth1 import loading
from keystoneauth1 import session
from novaclient import client as novaclient
from glanceclient import client as glanceclient
from cinderclient import client as cinderclient

#---------------------------------------------------------------------------

OS_AUTH_URL             = os.environ['OS_AUTH_URL']
OS_USERNAME             = os.environ['OS_USERNAME']
OS_PASSWORD             = os.environ['OS_PASSWORD']
OS_PROJECT_ID           = os.environ['OS_PROJECT_ID']
OS_USER_DOMAIN_NAME     = os.environ['OS_USER_DOMAIN_NAME']
OS_IDENTITY_API_VERSION = os.environ['OS_IDENTITY_API_VERSION']
OS_NOVA_API_VERSION     = '2'
OS_GLANCE_API_VERSION   = '2'
NETWORK                 = 'cproctor_net'

#---------------------------------------------------------------------------

user            = os.environ['USER']
hostname        = os.environ['HOSTNAME']

config_parser = cp.RawConfigParser()
config_parser.read("lustre.cfg")

section = "setup"
mds_count       = config_parser.getint( str(section), "mds_count"       )
oss_count       = config_parser.getint( str(section), "oss_count"       )
compute_count   = config_parser.getint( str(section), "compute_count"   )
volumes_per_mds = config_parser.getint( str(section), "volumes_per_mds" )
volumes_per_oss = config_parser.getint( str(section), "volumes_per_oss" )
mds_volume_size = config_parser.getint( str(section), "mds_volume_size" )
oss_volume_size = config_parser.getint( str(section), "oss_volume_size" )

section = "openstack"
nic             = config_parser.get( str(section), "nic"      )
key             = config_parser.get( str(section), "key"      )
img             = config_parser.get( str(section), "img"      )
flav            = config_parser.get( str(section), "flav"     )
secgroup        = config_parser.get( str(section), "secgroup" )
datafile        = config_parser.get( str(section), "datafile" )

section = "logging"
default_log_filepath = config_parser.get(    str(section), "default_log_filepath" )
default_log_level    = config_parser.getint( str(section), "default_log_level"    )
  
#---------------------------------------------------------------------------

def setup_logging( name,
                   log_filepath    = default_log_filepath    ,\
                   log_level       = default_log_level       ):

  formatter = lg.Formatter("LUSTRE %(levelname)s {0}: ".format(name) + str(hostname) + ": " + str(user) + ": " + \
                           "%(asctime)s: %(filename)s;%(funcName)s();%(lineno)d: %(message)s")

  logger = lg.getLogger(name)
  logger.setLevel(log_level)

  file_handler = lg.FileHandler(log_filepath, mode="w", encoding="utf8")        
  file_handler.setFormatter(formatter)

  stream_handler = lg.StreamHandler(stream=sys.stdout)
  stream_handler.setFormatter(formatter)

  logger.addHandler(file_handler)
  logger.addHandler(stream_handler)

  return logger


########################################
########################################
init_log = setup_logging("INIT")
########################################
########################################

#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------


def create_session():
  init_log.debug("Creating OpenStack session")
  loader = loading.get_plugin_loader('password')
  auth = loader.load_from_options(auth_url=OS_AUTH_URL,
                                  username=OS_USERNAME,
                                  password=OS_PASSWORD,
                                  project_id=OS_PROJECT_ID,
                                  user_domain_name=OS_USER_DOMAIN_NAME)
  return session.Session(auth=auth)

#---------------------------------------------------------------------------


def get_ip_address(state, server):
  print(state["nova"].servers.ips(server.id))
  return [state["nova"].servers.ips(server.id)[str(NETWORK)][0]['addr']]

#---------------------------------------------------------------------------


def get_volume_id(volume):
  return [volume.id]

#---------------------------------------------------------------------------


def create_ip_dict(state, server_list):
  ip_dict = dict()
  for server in server_list:
    if server.name in ip_dict:
      ip_dict[server.name].extend(get_ip_address(state, server))
    else:
      ip_dict[server.name] = get_ip_address(state, server)

  return ip_dict

#---------------------------------------------------------------------------


def create_volume_dict(volume_list):
  volume_dict = dict()
  for volume in volume_list:
    if volume.name in volume_dict:
      volume_dict[volume.name].extend(get_volume_id(volume))
    else:
      volume_dict[volume.name] = get_volume_id(volume)
    
  return volume_dict

#---------------------------------------------------------------------------


def create_vm(state, name):
  image  = state["glance"].images.get(image_id=img)
  flavor = state["nova"].flavors.find(name=flav)
  return state["nova"].servers.create(name, image, flavor, security_groups=[secgroup], key_name=key, nics=[{"net-id":nic}], userdata=open(datafile,"r"))
 

#---------------------------------------------------------------------------


def initialize():
  sess    = create_session()
  nova    = novaclient.Client(OS_NOVA_API_VERSION, session=sess)
  glance  = glanceclient.Client(OS_GLANCE_API_VERSION, session=sess)
  cinder  = cinderclient.Client(OS_IDENTITY_API_VERSION, session=sess)
  return { "sess":sess, "nova":nova, "glance":glance, "cinder":cinder }
  
#---------------------------------------------------------------------------


def summarize(state):
  all_servers = state["nova"].servers.list()
  all_images  = state["glance"].images.list()
  all_volumes = state["cinder"].volumes.list()

  init_log.info("SUMMARY:")
  ip_dict = create_ip_dict(state, all_servers)
  init_log.info(str(len(ip_dict)) + " SERVERS:")
  for name, ip_addr in ip_dict.iteritems():
    init_log.info(str(name) + " " + str(ip_addr))

  volume_dict = create_volume_dict(all_volumes)
  init_log.info(str(len(volume_dict)) + " VOLUMES:")
  for volume_name, volume_id in volume_dict.iteritems():
    init_log.info(str(volume_name) + " " + str(volume_id))


#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
#---------------------------------------------------------------------------
