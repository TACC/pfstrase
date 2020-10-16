#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>
#include "collect.h"
#include "lfs_utils.h"

static void ib_sw_collect_dev(json_object *stats, char *hca_name, int hca_port)
{
  struct ibmad_port *mad_port = NULL;
  int mad_timeout = 15;
  int mad_classes[] = { IB_SMI_DIRECT_CLASS, IB_PERFORMANCE_CLASS, };

  mad_port = mad_rpc_open_port(hca_name, hca_port, mad_classes, 2);
  if (mad_port == NULL) {
    fprintf(stderr, "cannot open MAD port for HCA `%s' port %d\n", hca_name, hca_port);
    goto out;
  }

  /* For reasons we don't understand, PMA queries can only be LID
     addressed.  But we don't know the LID of the switch to which the
     HCA is connected, so we send a SMP on the directed route 0,1 and
     ask the port to identify itself. */

  ib_portid_t sw_port_id = {
    .drpath = {
      .cnt = 1,
      .p = { 0, 1, },
    },
  };

  uint8_t sw_info[64];
  memset(sw_info, 0, sizeof(sw_info));
  if (smp_query_via(sw_info, &sw_port_id, IB_ATTR_PORT_INFO, 0, mad_timeout, mad_port) == NULL) {
    fprintf(stderr, "cannot query port info: %m\n");
    goto out;
  }

  int sw_lid, sw_port;
  mad_decode_field(sw_info, IB_PORT_LID_F, &sw_lid);
  mad_decode_field(sw_info, IB_PORT_LOCAL_PORT_F, &sw_port);
  sw_port_id.lid = sw_lid;

  uint8_t sw_pma[1024];
  memset(sw_pma, 0, sizeof(sw_pma));
  if (pma_query_via(sw_pma, &sw_port_id, sw_port, mad_timeout, IB_GSI_PORT_COUNTERS_EXT, mad_port) == NULL) {
    fprintf(stderr, "cannot query performance counters of switch LID %d, port %d: %m\n", sw_lid, sw_port);
    goto out;
  }

  uint64_t sw_rx_bytes, sw_rx_packets, sw_tx_bytes, sw_tx_packets;
  mad_decode_field(sw_pma, IB_PC_EXT_RCV_BYTES_F, &sw_rx_bytes);
  mad_decode_field(sw_pma, IB_PC_EXT_RCV_PKTS_F,  &sw_rx_packets);
  mad_decode_field(sw_pma, IB_PC_EXT_XMT_BYTES_F, &sw_tx_bytes);
  mad_decode_field(sw_pma, IB_PC_EXT_XMT_PKTS_F,  &sw_tx_packets);

  /* The transposition of tx and rx is intentional: the switch port
     receives what we send, and conversely. */
  json_object_object_add(stats, "rx_bytes", json_object_new_int64(sw_tx_bytes)); 
  json_object_object_add(stats, "rx_packets", json_object_new_int64(sw_tx_packets));
  json_object_object_add(stats, "tx_bytes",   json_object_new_int64(sw_rx_bytes));
  json_object_object_add(stats, "tx_packets", json_object_new_int64(sw_rx_packets));

 out:
  if (mad_port != NULL)
    mad_rpc_close_port(mad_port);
}

void collect_ib_sw(json_object *jarray)
{
  const char *ib_dir_path = "/sys/class/infiniband";
  DIR *ib_dir = NULL;

  ib_dir = opendir(ib_dir_path);
  if (ib_dir == NULL) {
    fprintf(stderr, "cannot open `%s': %m\n", ib_dir_path);
    goto out;
  }
  struct dirent *hca_ent;
  while ((hca_ent = readdir(ib_dir)) != NULL) {

    char *hca = hca_ent->d_name;
    char ports_path[80];
    DIR *ports_dir = NULL;

    if (hca[0] == '.')
      goto next_hca;

    snprintf(ports_path, sizeof(ports_path), "%s/%s/ports", ib_dir_path, hca);
    ports_dir = opendir(ports_path);
    if (ports_dir == NULL) {
      fprintf(stderr, "cannot open `%s': %m\n", ports_path);
      goto next_hca;
    }

    struct dirent *port_ent;
    while ((port_ent = readdir(ports_dir)) != NULL) {
      int port = atoi(port_ent->d_name);
      if (port <= 0)
        continue;

      /* Check that port is active. .../HCA/ports/PORT/state should read "4: ACTIVE." */
      int state = -1;
      char state_path[80];
      snprintf(state_path, sizeof(state_path), "/sys/class/infiniband/%s/ports/%d/state", hca, port);
      if (pscanf(state_path, "%d", &state) != 1) {
        fprintf(stderr, "cannot read state of IB HCA `%s' port %d: %m\n", hca, port);
        continue;
      }
      if (state != 4) {
        continue;
      }

      /* Create dev name (HCA/PORT) and get stats for dev. */
      char dev[80];
      snprintf(dev, sizeof(dev), "%s/%d", hca, port);

      json_object *tags_json = json_object_new_object();
      json_object_object_add(tags_json, "stats_type", json_object_new_string("ib_sw"));
      json_object_object_add(tags_json, "device", json_object_new_string(dev));

      json_object *stats_json = json_object_new_object();
      ib_sw_collect_dev(stats_json, hca, port);

      if (json_object_object_length(stats_json) > 0) {
	json_object_object_add(tags_json, "stats", stats_json);
	json_object_array_add(jarray, tags_json);
      }
      else {
	json_object_put(stats_json);
	json_object_put(tags_json);
      }
    }

  next_hca:
    if (ports_dir != NULL)
      closedir(ports_dir);
  }

 out:
  if (ib_dir != NULL)
    closedir(ib_dir);
}
