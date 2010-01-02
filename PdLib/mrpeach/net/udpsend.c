/* udpsend.c 20060424. Martin Peach did it based on x_net.c. x_net.c header follows: */
/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* network */

#include "m_pd.h"
#include "s_stuff.h"

#include <stdio.h>
#include <string.h>
#ifdef MSW
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#endif

static t_class *udpsend_class;

typedef struct _udpsend
{
    t_object x_obj;
    int      x_fd;
} t_udpsend;

void udpsend_setup(void);
static void udpsend_free(t_udpsend *x);
static void udpsend_send(t_udpsend *x, t_symbol *s, int argc, t_atom *argv);
static void udpsend_disconnect(t_udpsend *x);
static void udpsend_connect(t_udpsend *x, t_symbol *hostname, t_floatarg fportno);
static void *udpsend_new(void);

static void *udpsend_new(void)
{
    t_udpsend *x = (t_udpsend *)pd_new(udpsend_class);
    outlet_new(&x->x_obj, &s_float);
    x->x_fd = -1;
    return (x);
}

static void udpsend_connect(t_udpsend *x, t_symbol *hostname,
    t_floatarg fportno)
{
    struct sockaddr_in  server;
    struct hostent      *hp;
    int                 sockfd;
    int                 portno = fportno;
    int                 broadcast = 1;/* nonzero is true */

    if (x->x_fd >= 0)
    {
        error("udpsend: already connected");
        return;
    }

    /* create a socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef OSC_DEBUG
    fprintf(stderr, "udpsend_connect: send socket %d\n", sockfd);
#endif
    if (sockfd < 0)
    {
        sys_sockerror("udpsend: socket");
        return;
    }
/* Based on zmoelnig's patch 2221504:
Enable sending of broadcast messages (if hostname is a broadcast address)*/
#ifdef SO_BROADCAST
    if( 0 != setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const void *)&broadcast, sizeof(broadcast)))
    {
        pd_error(x, "couldn't switch to broadcast mode");
    }
#endif /* SO_BROADCAST */

    /* connect socket using hostname provided in command line */
    server.sin_family = AF_INET;
    hp = gethostbyname(hostname->s_name);
    if (hp == 0)
    {
	    post("udpsend: bad host?\n");
        return;
    }
    memcpy((char *)&server.sin_addr, (char *)hp->h_addr, hp->h_length);

    /* assign client port number */
    server.sin_port = htons((u_short)portno);

    post("udpsend: connecting to port %d", portno);
    /* try to connect. */
    if (connect(sockfd, (struct sockaddr *) &server, sizeof (server)) < 0)
    {
        sys_sockerror("udpsend: connecting stream socket");
        sys_closesocket(sockfd);
        return;
    }
    x->x_fd = sockfd;
    outlet_float(x->x_obj.ob_outlet, 1);
}

static void udpsend_disconnect(t_udpsend *x)
{
    if (x->x_fd >= 0)
    {
        sys_closesocket(x->x_fd);
        x->x_fd = -1;
        outlet_float(x->x_obj.ob_outlet, 0);
    }
}

static void udpsend_send(t_udpsend *x, t_symbol *s, int argc, t_atom *argv)
{
#define BYTE_BUF_LEN 65536 // arbitrary maximum similar to max IP packet size
    static char    byte_buf[BYTE_BUF_LEN];
    int            d;
    int            i, j;
    unsigned char  c;
    float          f, e;
    char           *bp;
    int            length, sent;
    int            result;
    static double  lastwarntime;
    static double  pleasewarn;
    double         timebefore;
    double         timeafter;
    int            late;
    char           fpath[FILENAME_MAX];
    FILE           *fptr;

#ifdef OSC_DEBUG
    post("s: %s", s->s_name);
    post("argc: %d", argc);
#endif
    for (i = j = 0; i < argc; ++i)
    {
        if (argv[i].a_type == A_FLOAT)
        {
            f = argv[i].a_w.w_float;
            d = (int)f;
            e = f - d;
            if (e != 0)
            {
                error("udpsend_send: item %d (%f) is not an integer", i, f);
                return;
            }
	        c = (unsigned char)d;
	        if (c != d)
            {
                error("udpsend_send: item %d (%f) is not between 0 and 255", i, f);
                return;
            }
#ifdef OSC_DEBUG
	        post("udpsend_send: argv[%d]: %d", i, c);
#endif
	        byte_buf[j++] = c;
        }
        else if (argv[i].a_type == A_SYMBOL)
        {

            atom_string(&argv[i], fpath, FILENAME_MAX);
#ifdef OSC_DEBUG
            post ("udpsend fname: %s", fpath);
#endif
            fptr = fopen(fpath, "rb");
            if (fptr == NULL)
            {
                post("udpsend: unable to open \"%s\"", fpath);
                return;
            }
            rewind(fptr);
#ifdef OSC_DEBUG
            post("udpsend: d is %d", d);
#endif
            while ((d = fgetc(fptr)) != EOF)
            {
                byte_buf[j++] = (char)(d & 0x0FF);
#ifdef OSC_DEBUG
                post("udpsend: byte_buf[%d] = %d", j-1, byte_buf[j-1]);
#endif
                if (j >= BYTE_BUF_LEN)
                {
                    post ("udpsend: file too long, truncating at %lu", BYTE_BUF_LEN);
                    break;
                }
            }
            fclose(fptr);
            fptr = NULL;
            post("udpsend: read \"%s\" length %d byte%s", fpath, j, ((d==1)?"":"s"));
        }
        else
        {
            error("udpsend_send: item %d is not a float or a file name", i);
            return;
        }
    }

    length = j;
    if ((x->x_fd >= 0) && (length > 0))
    {
        for (bp = byte_buf, sent = 0; sent < length;)
        {
            timebefore = sys_getrealtime();
            result = send(x->x_fd, byte_buf, length-sent, 0);
            timeafter = sys_getrealtime();
            late = (timeafter - timebefore > 0.005);
            if (late || pleasewarn)
            {
                if (timeafter > lastwarntime + 2)
                {
                    post("udpsend blocked %d msec",
                        (int)(1000 * ((timeafter - timebefore) + pleasewarn)));
                    pleasewarn = 0;
                    lastwarntime = timeafter;
                }
                else if (late) pleasewarn += timeafter - timebefore;
            }
            if (result <= 0)
            {
                sys_sockerror("udpsend");
                udpsend_disconnect(x);
                break;
            }
            else
            {
                sent += result;
                bp += result;
	        }
        }
    }
    else error("udpsend: not connected");
}

static void udpsend_free(t_udpsend *x)
{
    udpsend_disconnect(x);
}

void udpsend_setup(void)
{
    udpsend_class = class_new(gensym("udpsend"), (t_newmethod)udpsend_new,
        (t_method)udpsend_free,
        sizeof(t_udpsend), 0, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_connect,
        gensym("connect"), A_SYMBOL, A_FLOAT, 0);
    class_addmethod(udpsend_class, (t_method)udpsend_disconnect,
        gensym("disconnect"), 0);
    class_addmethod(udpsend_class, (t_method)udpsend_send, gensym("send"),
        A_GIMME, 0);
    class_addlist(udpsend_class, (t_method)udpsend_send);
}

/* end udpsend.c*/

