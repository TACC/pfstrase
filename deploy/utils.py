#!/usr/bin/env python

from __future__ import print_function

import ConfigParser as cp
import logging      as lg
import os           as os
import re           as re
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

#---------------------------------------------------------------------------

user            = os.environ['USER']
hostname        = os.environ['HOSTNAME']

config_parser = cp.RawConfigParser()
config_parser.read("lustre.cfg")

section = "setup"
proj_prefix     = config_parser.get(    str(section), "proj_prefix"     )
mds_prefix      = config_parser.get(    str(section), "mds_prefix"      )
oss_prefix      = config_parser.get(    str(section), "oss_prefix"      )
compute_prefix  = config_parser.get(    str(section), "compute_prefix"  )
volume_prefix   = config_parser.get(    str(section), "volume_prefix"   )
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
network         = config_parser.get( str(section), "network"  )

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
  return state["nova"].servers.ips(server.id)[str(network)][0]['addr']

#---------------------------------------------------------------------------


def create_server_dict(server_list, filter_regex=None):
  server_dict = dict()
  for server in server_list:
    if "master" in server.name: continue
    if proj_prefix not in server.name: continue
    if filter_regex: 
      if re.findall(filter_regex, server.name): continue
    if server.name in server_dict:
      init_log.error("Non-unique server names in use: {0}".format(server.name))
      init_log.error("Server with ID {0} will be overwritten.".format(server_dict[server.name].id))
      init_log.error("Server with ID {0} will replace.".format(server.id))

    server_dict[server.name] = server

  return server_dict
  
#---------------------------------------------------------------------------


def create_ip_dict(state, server_list, filter_regex=None):
  ip_dict = dict()
  for server in server_list:
    if "master" in server.name: continue
    if proj_prefix not in server.name: continue
    if filter_regex: 
      if re.findall(filter_regex, server.name): continue
    if server.name in ip_dict:
      init_log.error("Multiple server IPs in use: {0}".format(server.name))
      init_log.error("Server with IP {0} will be overwritten.".format(ip_dict[server.name]))
      init_log.error("Server with IP {0} will replace.".format(get_ip_address(state, server)))

    ip_dict[server.name] = get_ip_address(state, server)

  return ip_dict

#---------------------------------------------------------------------------


def create_volume_dict(volume_list, filter_regex=None):
  volume_dict = dict()
  for volume in volume_list:
    if "master" in volume.name: continue
    if proj_prefix not in volume.name: continue
    if filter_regex: 
      if re.findall(filter_regex, volume.name): continue
    if volume.name in volume_dict:
      init_log.error("Non-unique volume names in use: {0}".format(volume.name))
      init_log.error("Server with ID {0} will be overwritten.".format(volume_dict[volume.name].id))
      init_log.error("Server with ID {0} will replace.".format(volume.id))

    volume_dict[volume.name] = volume
    
  return volume_dict

#---------------------------------------------------------------------------


def create_server(state, name):
  image  = state["glance"].images.get(image_id=img)
  flavor = state["nova"].flavors.find(name=flav)
  init_log.debug("name:{0}, image:{1}, flavor:{2}, secgroup:{3}, key:{4}, nic:{5}, userdata:{6}".format(name, img, flav, secgroup, key, nic, datafile))
  return state["nova"].servers.create(name, image, flavor, security_groups=[secgroup], key_name=key, nics=[{"net-id":nic}], userdata=open(datafile,"r"))


#---------------------------------------------------------------------------


def destroy_server(state, server):
  init_log.info("Detaching volumes from server {0}".format(server.id))
  detach_all_volumes_from_server(state, server)
  init_log.info("Deleting server {0}".format(server.id))
  rt = state["nova"].servers.delete(server)
  init_log.info("Delete return tuple: {0}".format(rt))
 
#---------------------------------------------------------------------------


def create_volume(state, name, size):
  init_log.debug("name:{0}, size:{1} GiB".format(name, size))
  return state["cinder"].volumes.create(size, name=name)   

#---------------------------------------------------------------------------


def destroy_volume(state, volume):
  init_log.info("Deleting volume {0}".format(volume.id))
  rt = state["cinder"].volumes.delete(volume)
  init_log.info("Delete return tuple: {0}".format(rt))

#---------------------------------------------------------------------------


def attach_volume(state, server, volume):
  init_log.info("Attaching volume {0} to server {1}".format(volume.id, server.id))
  return state["nova"].volumes.create_server_volume(server_id=server.id, volume_id=volume.id)
  # TODO: Need to keep track of device location where mounted -- inside volume instance
  #init_log.info("Volume {0} attached to server {1} at device location {3}".format(volume.id, volume.serverId, volume.device))

#---------------------------------------------------------------------------


def detach_volume(state, server, volume):
  init_log.info("Detaching volume {0} from server {1}".format(volume.id, server.id))
  rt = state["nova"].volumes.delete_server_volume(server.id, volume_id=volume.id)
  init_log.info("Detach return tuple: {0}".format(rt))

#---------------------------------------------------------------------------


def detach_all_volumes_from_server(state, server):
  attached_volume_list = state["nova"].volumes.get_server_volumes(server.id)
  init_log.debug("Attached volume list for server {0}: {1}".format(server.id, attached_volume_list))
  for attached_volume in attached_volume_list:
    detach_volume(state, server, attached_volume)

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
