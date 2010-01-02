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
#include <pthread.h>

#include "lo/lo.h"

//The external port is the port the router is listening on for messages from outside the iPhone
const char kExternalPort[] = "7770";
//The internal port receives messages from sockets inside the iphone
const char kInternalPort[] = "7771";
//PD port is the port we are sending data to the pd controller 
const char kPDPort[] = "7772";
//Outgoing port is the port we are sending data to OSCGroupClient that sends data out
//to the server and other iPhones
const char kOutgoingPort[] = "7773";

lo_server_thread gInternalServer;
lo_server_thread gExternalServer;
lo_address gPDAddr;
lo_address gOutgoingAddr;

pthread_mutex_t pd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t outgoing_mutex = PTHREAD_MUTEX_INITIALIZER;


int done = 0;

void error(int num, const char *m, const char *path);

int generic_handler(const char *path, const char *types, lo_arg **argv,
		    int argc, void *data, void *user_data);

int foo_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 void *data, void *user_data);

int internal_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message message, void *user_data);

int external_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message message, void *user_data);


int main()
{

    /* Setting up our listening ports*/
    gInternalServer = lo_server_thread_new(kInternalPort, error);
    gExternalServer = lo_server_thread_new(kExternalPort, error);

    /*Setting up our addresses we are sending to */
    gPDAddr = lo_address_new(NULL, kPDPort);
    gOutgoingAddr = lo_address_new(NULL, kOutgoingPort);


    lo_server_thread_add_method(gExternalServer, "/instrument/*", "", external_handler, NULL);
    lo_server_thread_add_method(gInternalServer, "/instrument/*", "", internal_handler, NULL);

    lo_send(gOutgoingAddr, "/a/b/c/d", NULL);

    lo_server_thread_start(gInternalServer);
    lo_server_thread_start(gExternalServer);

    while(!done){


    }
 
    
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

int internal_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message message, void *user_data)
{
    pthread_mutex_lock(&pd_mutex);
    lo_send_message(gPDAddr, path,  message);
    pthread_mutex_unlock(&pd_mutex);


    pthread_mutex_lock(&outgoing_mutex);
    lo_send_message(gOutgoingAddr, path,  message);
    pthread_mutex_unlock(&outgoing_mutex);

    return 0;
}

int external_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message message, void *user_data)
{
    pthread_mutex_lock(&pd_mutex);
    lo_send_message(gPDAddr, path,  message);
    pthread_mutex_unlock(&pd_mutex);

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
