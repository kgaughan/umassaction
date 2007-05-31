/*
 * umassaction.c
 * by Keith Gaughan (http://talideon.com/)
 *
 * Copyright (c) Keith Gaughan, 2007. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.

 */

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <cam/cam.h>
#include <cam/scsi/scsi_pass.h>
#include <camlib.h>
#include <libvolume_id.h>

#ifndef FALSE
#define FALSE 0
#define TRUE (!(FALSE))
#endif

#define SIZEOF_ARRAY(a) (sizeof(a) / sizeof((a)[0]))

#define UMASS_DEV_NAME  "umass-sim"
#define SCSI_HDD_PERIPH "da"

/*
 * Takes the unit number of a umass device and figures out which SCSI device
 * it maps to.
 *
 * This does is derived from the getdevtree() function in the FreeBSD
 * camcontrol tool by Kenneth D. Merry.
 */
static int
map_umass_device_to_peripheral(int unit_num) {
	struct ccb_dev_match cdm;
	int fd;
	int bufsize;
	size_t i;
	struct bus_match_result* bus;
	struct periph_match_result* periph;
	int bus_matched = FALSE;
	int periph_unit_num = -1;

	fd = open(XPT_DEVICE, O_RDWR);
	if (fd == -1) {
		warn("couldn't open %s", XPT_DEVICE);
		return -1;
	}

	memset(&cdm, 0, sizeof(struct ccb_dev_match));
	cdm.ccb_h.path_id    = CAM_XPT_PATH_ID;
	cdm.ccb_h.target_id  = CAM_TARGET_WILDCARD;
	cdm.ccb_h.target_lun = CAM_LUN_WILDCARD;
	cdm.ccb_h.func_code  = XPT_DEV_MATCH;

	bufsize = sizeof(struct dev_match_result) * 100;
	cdm.match_buf_len = bufsize;
	cdm.matches = (struct dev_match_result*) malloc(bufsize);
	if (cdm.matches == NULL) {
		warnx("can't malloc memory for matches");
		close(fd);
		return -1;
	}
	cdm.num_matches = 0;
	cdm.num_patterns = 0;
	cdm.pattern_buf_len = 0;

	/* We do the ioctl multiple times if necessary, in case there are more
	   than 100 nodes in the EDT. */
	do {
		if (ioctl(fd, CAMIOCOMMAND, &cdm) == -1) {
			warn("error sending CAMIOCOMMAND ioctl");
			break;
		}
		if (cdm.ccb_h.status != CAM_REQ_CMP ||
				(cdm.status != CAM_DEV_MATCH_LAST &&
				 cdm.status != CAM_DEV_MATCH_MORE)) {
			warnx("got CAM error %#x, CDM error %d",
					cdm.ccb_h.status, cdm.status);
			break;
		}

		for (i = 0; i < cdm.num_matches; i++) {
			if (cdm.matches[i].type == DEV_MATCH_BUS) {
				if (bus_matched) {
					warnx("umass%d has no peripheral", unit_num);
					break;
				}

				bus = &cdm.matches[i].result.bus_result;
				if (strcmp(bus->dev_name, UMASS_DEV_NAME) == 0 &&
						bus->unit_number == unit_num) {
					bus_matched = TRUE;
				}
			} else if (bus_matched && cdm.matches[i].type == DEV_MATCH_PERIPH) {
				periph = &cdm.matches[i].result.periph_result;
				if (strcmp(periph->periph_name, SCSI_HDD_PERIPH) == 0) {
					periph_unit_num = periph->unit_number;
					break;
				}
			}
		}
	} while (cdm.ccb_h.status == CAM_REQ_CMP &&
			cdm.status == CAM_DEV_MATCH_MORE);

	close(fd);

	return periph_unit_num;
}

/*
 * Get's volume information associated with a drive node.
 */
static struct volume_id*
probe_node(const char* node) {
	struct volume_id* vid;
	off_t media_size;

	assert(node != NULL);

	vid = volume_id_open_node(node);
	if (vid) {
		ioctl(vid->fd, DIOCGMEDIASIZE, &media_size);
		if (volume_id_probe_all(vid, 0, media_size) != 0) {
			volume_id_close(vid);
			vid = NULL;
		}
	}

	return vid;
}

static void on_attach(int unit_num, int device_type) {
	puts("attach");
	exit(0);
}

static void
on_detach(int unit_num, int device_type) {
	puts("detach");
	exit(0);
}

static struct {
	char* cmd;
	void (*fn)(int, int);
} handlers[] = {
	{ .cmd = "attach", .fn = on_attach },
	{ .cmd = "detach", .fn = on_detach }
};

int
main(int argc, char** argv) {
	/* struct volume_id* vid; */
	size_t i;

	if (argc == 4) {
		/* Parse the device node name. */
		/* Parse the device type. */
		for (i = 0; i < SIZEOF_ARRAY(handlers); i++) {
			if (strcmp(handlers[i].cmd, argv[1]) == 0) {
				handlers[i].fn(0, 0);
				return 0;
			}
		}

		fprintf(stderr, "No such command: %s\n", argv[1]);
	}

	fprintf(stderr, "usage: umassaction {attach|detach} device type\n");
	return 1;

	/*
	 * To do:
	 *
	 *  * Given a device file, check for child slices and partitions, mounting
	 *    each leaf child.
	 *
	 *  * Record information (for cleanup purposes) about each device plugged
	 *    in, specifically their device number, type, and mount point.
	 *
	 *  * Load mappings between libvolume_id filesystem names to native
	 *    filesystem names.
	 *
	 *  * Load 

	/*
	vid = probe_node(argv[1]);
	if (!vid) {
		fprintf(stderr, "error: Could not probe device\n");
		return 2;
	}

	write_pair("label", vid->label);
	write_pair("uuid", vid->uuid);
	write_pair("usage", vid->usage);
	write_pair("type", vid->type);
	write_pair("type-version", vid->type_version);

	volume_id_close(vid);
	*/
}
