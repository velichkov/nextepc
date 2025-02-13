/*
 * Copyright (C) 2019 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FD_LIB_H
#define FD_LIB_H

#include "ogs-core.h"

#include "freeDiameter/freeDiameter-host.h"
#include "freeDiameter/libfdcore.h"

#include "base/context.h"

#include "fd-message.h"
#include "fd-logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/* This is default diameter configuration if there is no config file 
 * The Configuration : No TLS, Only TCP */
typedef struct fd_config_s {
    /* Diameter Identity of the local peer (FQDN -- ASCII) */
    const char *cnf_diamid; 
    /* Diameter realm of the local peer, default to realm part of cnf_diamid */
    const char *cnf_diamrlm; 
    /* IP address of the local peer */
    const char *cnf_addr;

    /* the local port for legacy Diameter (default: 3868) in host byte order */
    uint16_t cnf_port;
    /* the local port for Diameter/TLS (default: 5658) in host byte order */
    uint16_t cnf_port_tls;

	struct {
		unsigned no_sctp: 1;	/* disable the use of SCTP */
	} cnf_flags;

#define MAX_NUM_OF_FD_EXTENSION 32
    struct {
        const char *module;
        const char *conf;
    } ext[MAX_NUM_OF_FD_EXTENSION];
    int num_of_ext;

#define MAX_NUM_OF_FD_CONN 16
    /* (supposedly) UTF-8, \0 terminated. 
     * The Diameter Identity of the remote peer. */
    struct {
        const char *identity; 
        const char *addr; /* IP address of the remote peer */
        uint16_t port; /* port to connect to. 0: default. */
    } conn[MAX_NUM_OF_FD_CONN];
    int num_of_conn;
} fd_config_t;

int fd_init(int mode, const char *conffile, fd_config_t *fd_config);
void fd_final(void);

int fd_config_init(fd_config_t *fd_config);

int fd_avp_search_avp ( struct avp * groupedavp, 
        struct dict_object * what, struct avp ** avp );

#ifdef __cplusplus
}
#endif

#endif /* FD_LIB_H */
