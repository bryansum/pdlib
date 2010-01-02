/*
 *  nonblocking_server_example.c
 *
 *  This code demonstrates two methods of monitoring both an lo_server
 *  and other I/O from a single thread.
 *
 *  Copyright (C) 2004 Steve Harris, Uwe Koloska
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>

#include "lo/lo.h"

const char kInternalPort[] = "7770";
const char kExternalPort[] = "7771";
const char kPDPort[] = "7772";
const char kEventPort[] = "7773";

lo_server gInternalServer;
lo_server gExternalServer;
lo_address gPDAddr;
lo_address gEventAddr;


int done = 0;

void error(int num, const char *m, const char *path);

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

int foo_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);

int pd_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);

int event_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);


int main()
{
    int lo_fd;
    fd_set rfds;
    int retval;

    /* start a new server listening on port kRouterPort */
    gInternalServer = lo_server_new(kInternalPort, error);

    gExtneralServer = lo_server_new(kExternalPort, error);

    gPDAddr = lo_address_new(NULL, kPDPort);
    gEventAddr = lo_address_new(NULL, kEventPort);


    /* add method that will match any path and args */
    lo_server_add_method(server, NULL, NULL, generic_handler, NULL);


    /* add method that will match the path /quit with no args */
    lo_server_add_method(server, "/quit", "", quit_handler, NULL);

    lo_server_add_method(server, "/instrument/*", "", pd_handler, NULL);

    /* get the file descriptor of the server socket, if supported */
    lo_fd = lo_server_get_socket_fd(server);

        do {

            FD_ZERO(&rfds);
#ifndef WIN32
            FD_SET(0, &rfds);  /* stdin */
#endif
            FD_SET(lo_fd, &rfds);

            retval = select(lo_fd + 1, &rfds, NULL, NULL, NULL); /* no timeout */

            if (retval == -1) {

                printf("select() error\n");
                exit(1);

            } else if (retval > 0) {

                if (FD_ISSET(lo_fd, &rfds)) {

                    lo_server_recv_noblock(server, 0);

                }
            }

        } while (!done);

 
    
    return 0;
}

void error(int num, const char *msg, const char *path)
{
    printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data)
{
    int i;

    printf("path: <%s>\n", path);
    for (i=0; i<argc; i++) {
	printf("arg %d '%c' ", i, types[i]);
	lo_arg_pp(types[i], argv[i]);
	printf("\n");
    }
    printf("\n");
    fflush(stdout);

    return 1;
}

int pd_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    lo_send(kPDAddr, path,  NULL);

    return 0;
}

int pd_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    lo_send(kEventAddr, path,  NULL);

    return 0;
}

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data)
{
    done = 1;
    printf("quiting\n\n");

    return 0;
}


/* vi:set ts=8 sts=4 sw=4: */
