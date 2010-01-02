/* x_net_udpreceive.c 20060424. Martin Peach did it based on x_net.c. x_net.c header follows: */
/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "s_stuff.h"

#ifdef MSW
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#endif


/* ----------------------------- udpreceive ------------------------- */

static t_class *udpreceive_class;

#define MAX_UDP_RECEIVE 65536L // longer than data in maximum UDP packet

typedef struct _udpreceive
{
    t_object  x_obj;
    t_outlet  *x_msgout;
    t_outlet  *x_addrout;
    int       x_connectsocket;
    t_atom    x_addrbytes[5];
    t_atom    x_msgoutbuf[MAX_UDP_RECEIVE];
    char      x_msginbuf[MAX_UDP_RECEIVE];
} t_udpreceive;

void udpreceive_setup(void);
static void udpreceive_free(t_udpreceive *x);
static void *udpreceive_new(t_floatarg fportno);
static void udpreceive_read(t_udpreceive *x, int sockfd);

static void udpreceive_read(t_udpreceive *x, int sockfd)
{
    int                 i, read = 0;
    struct sockaddr_in  from;
    socklen_t           fromlen = sizeof(from);
    long                addr;
    unsigned short      port;

    read = recvfrom(sockfd, x->x_msginbuf, MAX_UDP_RECEIVE, 0, (struct sockaddr *)&from, &fromlen);
#ifdef OSC_DEBUG
    post("udpreceive_read: read %lu x->x_connectsocket = %d",
        read, x->x_connectsocket);
#endif
    /* get the sender's ip */
    addr = ntohl(from.sin_addr.s_addr);
    port = ntohs(from.sin_port);

    x->x_addrbytes[0].a_w.w_float = (addr & 0xFF000000)>>24;
    x->x_addrbytes[1].a_w.w_float = (addr & 0x0FF0000)>>16;
    x->x_addrbytes[2].a_w.w_float = (addr & 0x0FF00)>>8;
    x->x_addrbytes[3].a_w.w_float = (addr & 0x0FF);
    x->x_addrbytes[4].a_w.w_float = port;
    outlet_list(x->x_addrout, &s_list, 5L, x->x_addrbytes);

    if (read < 0)
    {
        sys_sockerror("udpreceive_read");
        sys_closesocket(x->x_connectsocket);
        return;
    }
    if (read > 0)
    {
        for (i = 0; i < read; ++i)
        {
            /* convert the bytes in the buffer to floats in a list */
            x->x_msgoutbuf[i].a_w.w_float = (float)(unsigned char)x->x_msginbuf[i];
        }
        /* send the list out the outlet */
        if (read > 1) outlet_list(x->x_msgout, &s_list, read, x->x_msgoutbuf);
        else outlet_float(x->x_msgout, x->x_msgoutbuf[0].a_w.w_float);
    }
}

static void *udpreceive_new(t_floatarg fportno)
{
    t_udpreceive       *x;
    struct sockaddr_in server;
    int                sockfd, portno = fportno;
    int                intarg, i;

    /* create a socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef OSC_DEBUG
    post("udpreceive_new: socket %d port %d", sockfd, portno);
#endif
    if (sockfd < 0)
    {
        sys_sockerror("udpreceive: socket");
        return (0);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;

    /* enable delivery of all multicast or broadcast (but not unicast)
    * UDP datagrams to all sockets bound to the same port */
    intarg = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
        (char *)&intarg, sizeof(intarg)) < 0)
        post("udpreceive: setsockopt (SO_REUSEADDR) failed");

    /* assign server port number */
    server.sin_port = htons((u_short)portno);

    /* name the socket */
    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        sys_sockerror("udpreceive: bind");
        sys_closesocket(sockfd);
        return (0);
    }
    x = (t_udpreceive *)pd_new(udpreceive_class);
    x->x_msgout = outlet_new(&x->x_obj, &s_anything);
    x->x_addrout = outlet_new(&x->x_obj, &s_list);
    x->x_connectsocket = sockfd;

    /* convert the bytes in the buffer to floats in a list */
    for (i = 0; i < MAX_UDP_RECEIVE; ++i)
    {
        x->x_msgoutbuf[i].a_type = A_FLOAT;
        x->x_msgoutbuf[i].a_w.w_float = 0;
    }
    for (i = 0; i < 5; ++i)
    {
        x->x_addrbytes[i].a_type = A_FLOAT;
        x->x_addrbytes[i].a_w.w_float = 0;
    }
    sys_addpollfn(x->x_connectsocket, (t_fdpollfn)udpreceive_read, x);
    return (x);
}

static void udpreceive_free(t_udpreceive *x)
{
    if (x->x_connectsocket >= 0)
    {
        sys_rmpollfn(x->x_connectsocket);
        sys_closesocket(x->x_connectsocket);
    }
}

void udpreceive_setup(void)
{
    udpreceive_class = class_new(gensym("udpreceive"),
        (t_newmethod)udpreceive_new, (t_method)udpreceive_free,
        sizeof(t_udpreceive), CLASS_NOINLET, A_DEFFLOAT, 0);
}

/* end udpreceive.c */
