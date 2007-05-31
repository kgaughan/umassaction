/*
 * volumeinfo.c
 * by Keith Gaughan (http://talideon.com/)
 *
 * A wrapper around libvolume_id for getting information about a volume
 * designated by a given special file.
 *
 * To compile (on FreeBSD), use:
 *
 *     gcc -Wall -Os -DNDEBUG \
 *         -I/usr/local/include/ -L/usr/local/lib/ -lvolume_id
 *         -o volumeinfo volumeinfo.c
 *     strip volumeinfo
 *
 * You will, of course, need libvolume_id.
 *
 * Copyright (c) Keith Gaughan, 2007.
 *
 * I'd like to put this under a BSD-style license, but because libvolume_id
 * is covered by the GPL and not a more reasonable license like the LGPL, I
 * guess this is, by extension, covered by the GPL. So until such time as I
 * know for certain...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307	 USA
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <libvolume_id.h>

struct volume_id* probe_node(const char* node) {
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

int main(int argc, char** argv) {
	struct volume_id* vid;

	if (argc < 2) {
		fprintf(stderr, "usage: volumeinfo special\n");
		return 1;
	}

	vid = probe_node(argv[1]);
	if (!vid) {
		fprintf(stderr, "error: Could not probe device\n");
		return 2;
	}
	if (vid->usage_id != VOLUME_ID_FILESYSTEM) {
		volume_id_close(vid);
		fprintf(stderr, "error: Not a filesystem\n");
		return 3;
	}

	if (strlen(vid->label) > 0) {
		printf("label: %s\n", vid->label);
	}
	if (strlen(vid->uuid) > 0) {
		printf("uuid: %s\n", vid->uuid);
	}

	volume_id_close(vid);

	return 0;
}
