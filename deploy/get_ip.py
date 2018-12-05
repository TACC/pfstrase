#!/usr/bin/env python

from __future__ import print_function

import os

from keystoneauth1 import loading
from keystoneauth1 import session
from cinderclient import client as cinderclient
from novaclient import client as novaclient


OS_AUTH_URL             = os.environ['OS_AUTH_URL']
OS_USERNAME             = os.environ['OS_USERNAME']
OS_PASSWORD             = os.environ['OS_PASSWORD']
OS_PROJECT_ID           = os.environ['OS_PROJECT_ID']
OS_USER_DOMAIN_NAME     = os.environ['OS_USER_DOMAIN_NAME']
OS_IDENTITY_API_VERSION = os.environ['OS_IDENTITY_API_VERSION']
OS_NOVA_API_VERSION     = '2'
NETWORK                 = 'cproctor_net'



def create_session():
  loader = loading.get_plugin_loader('password')
  auth = loader.load_from_options(auth_url=OS_AUTH_URL,
                                  username=OS_USERNAME,
                                  password=OS_PASSWORD,
                                  project_id=OS_PROJECT_ID,
                                  user_domain_name=OS_USER_DOMAIN_NAME)
  return session.Session(auth=auth)


def get_ip_address(server):
  return [nova.servers.ips(server.id)[NETWORK][0]['addr']]

def get_volume_id(volume):
  return [volume.id]

def create_ip_dict(server_list):
  ip_dict = dict()
  for server in server_list:
    if server.name in ip_dict:
      ip_dict[server.name].extend(get_ip_address(server))
    else:
      ip_dict[server.name] = get_ip_address(server)

  return ip_dict

def create_volume_dict(volume_list):
  volume_dict = dict()
  for volume in volume_list:
    if volume.name in volume_dict:
      volume_dict[volume.name].extend(get_volume_id(volume))
    else:
      volume_dict[volume.name] = get_volume_id(volume)
    

  return volume_dict

## for server in nova.servers.list(search_opts={'addr': 1}):
##   print(server.name, server.tenant_id, server.flavor)
## 
#cinder = cinderclient.Client(OS_IDENTITY_API_VERSION, session=sess)
#print(cinder.volumes.list())
#print(nova.servers.list())
## server = nova.servers.find(name="mds1")
## #print(type(server.__dict__))
## #print(server.__dict__)
## 
## print(server.list_security_group())
## print(server.interface_list())
## print(server.networks)
## #print(server.ips())
## print(server.list())



if __name__ == "__main__":
  sess = create_session()
  nova = novaclient.Client(OS_NOVA_API_VERSION, session=sess)
  cinder = cinderclient.Client(OS_IDENTITY_API_VERSION, session=sess)
  all_servers = nova.servers.list()
  all_volumes = cinder.volumes.list()
  print(all_servers)
  print(all_volumes)
  #cinder.volumes.create(name="v1", size=10) 
  ip_dict = create_ip_dict(all_servers)
  for name, ip_addr in ip_dict.iteritems():
    print(name, ip_addr)

  volume_dict = create_volume_dict(all_volumes)
  for volume_name, volume_id in volume_dict.iteritems():
    print(volume_name, volume_id)
