/* Copyright (c) 1997-1999 Miller Puckette and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"
#include "m_imp.h"
#include "s_stuff.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef UNISTD
#include <unistd.h>
#endif
#ifdef MSW
#include <io.h>
#include <windows.h>
#include <winbase.h>
#endif
#ifdef _MSC_VER  /* This is only for Microsoft's compiler, not cygwin, e.g. */
#define snprintf sprintf_s
#endif

#include "AudioOutput.h"

char *pd_version;
char pd_compiletime[] = __TIME__;
char pd_compiledate[] = __DATE__;

void pd_init(void);
int sys_startgui(const char *guipath);
int m_mainloop(void);
int m_batchmain(void);
void sys_addhelppath(char *p);

int sys_debuglevel;
int sys_verbose;
int sys_noloadbang;
int sys_nogui;
int sys_hipriority = -1;    /* -1 = don't care; 0 = no; 1 = yes */
int sys_guisetportnumber;   /* if started from the GUI, this is the port # */
int sys_nosleep = 0;  /* skip all "sleep" calls and spin instead */

char *sys_guicmd;
t_symbol *sys_libdir;
t_symbol *sys_guidir;
static t_namelist *sys_openlist;
static t_namelist *sys_messagelist;
int sys_oldtclversion;      /* hack to warn g_rtext.c about old text sel */

/* old s_audio.c globals */
int sys_dacsr = 22050;
int sys_schedblocksize = 256; // block size
int sys_noutchannels = 2;
int sys_ninchannels = 2;
AudioCallbackFn sys_userCallbackFn = NULL;

extern void sched_audio_callbackfn(void);

void audioOutputCallbackFn(const AudioTimeStamp *inTimestamp) {
    
    if (sys_userCallbackFn) {
        (*sys_userCallbackFn)(inTimestamp);
    }
    
    sched_audio_callbackfn();
}

AudioCallbackParams sys_callbackparams = {
    &audioOutputCallbackFn,
    NULL,
    0
};

t_sample *sys_soundout; // global var for samples
t_sample *sys_soundin;

int sys_printtostderr;
int sys_hasstarted = 0;

int sys_nmidiout = -1;
int sys_nmidiin = -1;
int sys_midiindevlist[1] = {1};
int sys_midioutdevlist[1] = {1};

char sys_font[100] = 
#ifdef MSW
    "Courier";
#else
    "Courier";
#endif
char sys_fontweight[] = "bold  "; /* currently only used for iemguis */
//static int sys_main_advance;
//static int sys_main_callback;
//static int sys_listplease;

int sys_externalschedlib;
char sys_externalschedlibname[MAXPDSTRING];
int sys_extraflags;
char sys_extraflagsstring[MAXPDSTRING];
int sys_run_scheduler(const char *externalschedlibname,
    const char *sys_extraflagsstring);
int sys_noautopatch;    /* temporary hack to defeat new 0.42 editing */

typedef struct _fontinfo
{
    int fi_fontsize;
    int fi_maxwidth;
    int fi_maxheight;
    int fi_hostfontsize;
    int fi_width;
    int fi_height;
} t_fontinfo;

    /* these give the nominal point size and maximum height of the characters
    in the six fonts.  */

static t_fontinfo sys_fontlist[] = {
    {8, 6, 10, 0, 0, 0}, {10, 7, 13, 0, 0, 0}, {12, 9, 16, 0, 0, 0},
    {16, 10, 20, 0, 0, 0}, {24, 15, 25, 0, 0, 0}, {36, 25, 45, 0, 0, 0}};
#define NFONT (sizeof(sys_fontlist)/sizeof(*sys_fontlist))

/* here are the actual font size structs on msp's systems:
MSW:
font 8 5 9 8 5 11
font 10 7 13 10 6 13
font 12 9 16 14 8 16
font 16 10 20 16 10 18
font 24 15 25 16 10 18
font 36 25 42 36 22 41

linux:
font 8 5 9 8 5 9
font 10 7 13 12 7 13
font 12 9 16 14 9 15
font 16 10 20 16 10 19
font 24 15 25 24 15 24
font 36 25 42 36 22 41
*/

static t_fontinfo *sys_findfont(int fontsize)
{
    unsigned int i;
    t_fontinfo *fi;
    for (i = 0, fi = sys_fontlist; i < (NFONT-1); i++, fi++)
        if (fontsize < fi[1].fi_fontsize) return (fi);
    return (sys_fontlist + (NFONT-1));
}

int sys_nearestfontsize(int fontsize)
{
    return (sys_findfont(fontsize)->fi_fontsize);
}

int sys_hostfontsize(int fontsize)
{
    return (sys_findfont(fontsize)->fi_hostfontsize);
}

int sys_fontwidth(int fontsize)
{
    return (sys_findfont(fontsize)->fi_width);
}

int sys_fontheight(int fontsize)
{
    return (sys_findfont(fontsize)->fi_height);
}

int sys_defaultfont;
#ifdef MSW
#define DEFAULTFONT 12
#else
#define DEFAULTFONT 10
#endif

int openit(const char *dirname, const char *filename)
{
    char dirbuf[MAXPDSTRING], *nameptr;
    int fd = open_via_path(dirname, filename, "", dirbuf, &nameptr,
        MAXPDSTRING, 0);
    if (fd >= 0)
    {
        close (fd);
        glob_evalfile(0, gensym(nameptr), gensym(dirbuf));
        return 0;
    }
    else {
        error("%s: can't open", filename);
        return -1;
    }
}

/* this is called from the gui process.  The first argument is the cwd, and
succeeding args give the widths and heights of known fonts.  We wait until 
these are known to open files and send messages specified on the command line.
We ask the GUI to specify the "cwd" in case we don't have a local OS to get it
from; for instance we could be some kind of RT embedded system.  However, to
really make this make sense we would have to implement
open(), read(), etc, calls to be served somehow from the GUI too. */

void glob_initfromgui(void *dummy, t_symbol *s, int argc, t_atom *argv)
{
    char *cwd = atom_getsymbolarg(0, argc, argv)->s_name;
    t_namelist *nl;
    unsigned int i;
    int j;
    int nhostfont = (argc-2)/3;
    sys_oldtclversion = atom_getfloatarg(1, argc, argv);
    if (argc != 2 + 3 * nhostfont) bug("glob_initfromgui");
    for (i = 0; i < NFONT; i++)
    {
        int best = 0;
        int wantheight = sys_fontlist[i].fi_maxheight;
        int wantwidth = sys_fontlist[i].fi_maxwidth;
        for (j = 1; j < nhostfont; j++)
        {
            if (atom_getintarg(3 * j + 4, argc, argv) <= wantheight &&
                atom_getintarg(3 * j + 3, argc, argv) <= wantwidth)
                    best = j;
        }
            /* best is now the host font index for the desired font index i. */
        sys_fontlist[i].fi_hostfontsize =
            atom_getintarg(3 * best + 2, argc, argv);
        sys_fontlist[i].fi_width = atom_getintarg(3 * best + 3, argc, argv);
        sys_fontlist[i].fi_height = atom_getintarg(3 * best + 4, argc, argv);
    }
#if 0
    for (i = 0; i < 6; i++)
        fprintf(stderr, "font (%d %d %d) -> (%d %d %d)\n",
            sys_fontlist[i].fi_fontsize,
            sys_fontlist[i].fi_maxwidth,
            sys_fontlist[i].fi_maxheight,
            sys_fontlist[i].fi_hostfontsize,
            sys_fontlist[i].fi_width,
            sys_fontlist[i].fi_height);
#endif
        /* load dynamic libraries specified with "-lib" args */
    for  (nl = sys_externlist; nl; nl = nl->nl_next)
        if (!sys_load_lib(0, nl->nl_string))
            post("%s: can't load library", nl->nl_string);
        /* open patches specifies with "-open" args */
    for  (nl = sys_openlist; nl; nl = nl->nl_next)
        openit(cwd, nl->nl_string);
    namelist_free(sys_openlist);
    sys_openlist = 0;
        /* send messages specified with "-send" args */
    for  (nl = sys_messagelist; nl; nl = nl->nl_next)
    {
        t_binbuf *b = binbuf_new();
        binbuf_text(b, nl->nl_string, strlen(nl->nl_string));
        binbuf_eval(b, 0, 0, 0);
        binbuf_free(b);
    }
    namelist_free(sys_messagelist);
    sys_messagelist = 0;
}

void sys_send_msg(const char *msg)
{
    t_binbuf *b = binbuf_new();
    binbuf_text(b, (char*) msg, strlen(msg));
    binbuf_eval(b, 0, 0, 0);
    binbuf_free(b);
}

static void sys_afterargparse(void);

static void pd_makeversion(void)
{
    char foo[100];
    sprintf(foo,  "Pd version %d.%d-%d%s\n",PD_MAJOR_VERSION,
        PD_MINOR_VERSION,PD_BUGFIX_VERSION,PD_TEST_VERSION);
    pd_version = malloc(strlen(foo)+1);
    strcpy(pd_version, foo);
}

/* this is called from main() in s_entry.c */
int sys_main(const char *libdir, 
             const char *externs, 
             const char *openfiles,
             const char *searchpath,
             int soundRate,
             int blockSize,
             int nOutChannels,
             int nInChannels,
             AudioCallbackFn callback)
{
    sys_userCallbackFn = callback;
    
    sys_externalschedlib = 0;
    sys_extraflags = 0;

    sys_soundout = malloc(sizeof(t_sample) * sys_schedblocksize * sys_noutchannels);
    memset(sys_soundout, 0, sizeof(t_sample) * sys_schedblocksize * sys_noutchannels);

    sys_soundin = malloc(sizeof(t_sample) * sys_schedblocksize * sys_ninchannels);
    memset(sys_soundin, 0, sizeof(t_sample) * sys_schedblocksize * sys_ninchannels);

    sys_dacsr = soundRate;
    sys_schedblocksize = blockSize;
    sys_noutchannels = nOutChannels;
    sys_ninchannels = nInChannels;
    
    // pure data defaults
    
    pd_init(); /* start the message system */
    sys_libdir = gensym(libdir); // Find lib dir
    sys_guidir = gensym(libdir); // should be libdir + "/bin" usually, but it doesn't really matter in our case
    
    sys_searchpath = namelist_append_files(sys_searchpath, searchpath);
    sys_openlist = namelist_append_files(sys_openlist, openfiles); // open files
    sys_externlist = namelist_append_files(sys_externlist, externs); // open externs at start up
    sys_printtostderr = sys_nogui = 1;
    
    if (!sys_defaultfont)
        sys_defaultfont = DEFAULTFONT;
    

    sys_afterargparse();                    /* post-argparse settings */
        /* build version string from defines in m_pd.h */
    pd_makeversion();
    if (sys_startgui(libdir))       /* start the gui */
        return(1);

    // bryansum: initialize coreaudio stuff
    int status;
    if ((status = AudioOutputInitialize(&sys_callbackparams))) {
        return status;
    }
    
    // bryansum
    // Run the either m_pollingscheduler, which actually computes ticks,
    // or m_callbackscheduler, which waits for the hardware to callback for DSP ticks.
    // CoreAudio on the iPhone uses the callback mechanism. 
    sched_set_using_audio(SCHED_AUDIO_CALLBACK);
    
    sys_hasstarted = 1;
    
    return (m_mainloop());
}

    /* stuff to do, once, after calling sys_argparse() -- which may itself
    be called more than once (first from "settings, second from .pdrc, then
    from command-line arguments */
static void sys_afterargparse(void)
{
    char sbuf[MAXPDSTRING];
            /* add "extra" library to path */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/extra");
    sys_setextrapath(sbuf);
            /* add "doc/5.reference" library to helppath */
    strncpy(sbuf, sys_libdir->s_name, MAXPDSTRING-30);
    sbuf[MAXPDSTRING-30] = 0;
    strcat(sbuf, "/doc/5.reference");
    sys_helppath = namelist_append_files(sys_helppath, sbuf);
}

