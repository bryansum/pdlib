/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/*  The soundout~ and adc~ routines.
*/

#include "m_pd.h"
#include "s_stuff.h"

/* ----------------------------- soundout~ --------------------------- */
static t_class *soundout_class;

typedef struct _soundout
{
    t_object x_obj;
    t_int x_n; // always 2
    t_int *x_vec; // channel #
    t_float x_f;
} t_soundout;

static void *soundout_new(void)
{
    t_soundout *x = (t_soundout *)pd_new(soundout_class);

    // Add n-1 more signals than we originally had from CLASS_MAINSIGNALIN
    for (int i = 1; i < sys_noutchannels; i++) {
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);        
    }

    x->x_f = 0;
    return (x);
}

static void soundout_dsp(t_soundout *x __attribute__((unused)), t_signal **sp)
{
    t_signal **sp2;
    int channel;

    // for each channel
    for (channel = 0, sp2 = sp; channel < sys_noutchannels; sp2++, channel++) {
        
        if ((*sp2)->s_n != sys_schedblocksize) {
            error("soundout~: bad vector size");
        }
        dsp_add(plus_perform, 
                4, 
                sys_soundout + sys_schedblocksize*channel, 
                (*sp2)->s_vec,
                sys_soundout + sys_schedblocksize*channel,
                sys_schedblocksize);        
    }            
}

static void soundout_free(t_soundout *x __attribute__((unused))) {}

static void soundout_setup(void)
{
    soundout_class = class_new(gensym("soundout~"), (t_newmethod)soundout_new,
        (t_method)soundout_free, sizeof(t_soundout), CLASS_DEFAULT, 0);
    CLASS_MAINSIGNALIN(soundout_class, t_soundout, x_f);
    class_addmethod(soundout_class, (t_method)soundout_dsp, gensym("dsp"), A_CANT, 0);
    class_sethelpsymbol(soundout_class, gensym("adc~_soundout~"));
}

/* ----------------------------- adc~ --------------------------- */
//static t_class *adc_class;
//
//typedef struct _adc
//{
//    t_object x_obj;
//    t_int x_n;
//    t_int *x_vec;
//} t_adc;
//
//static void *adc_new(t_symbol *s, int argc, t_atom *argv)
//{
//    t_adc *x = (t_adc *)pd_new(adc_class);
//    t_atom defarg[2], *ap;
//    int i;
//    if (!argc)
//    {
//        argv = defarg;
//        argc = 2;
//        SETFLOAT(&defarg[0], 1);
//        SETFLOAT(&defarg[1], 2);
//    }
//    x->x_n = argc;
//    x->x_vec = (t_int *)getbytes(argc * sizeof(*x->x_vec));
//    for (i = 0; i < argc; i++)
//        x->x_vec[i] = atom_getintarg(i, argc, argv);
//    for (i = 0; i < argc; i++)
//        outlet_new(&x->x_obj, &s_signal);
//    return (x);
//}
//
//t_int *copy_perform(t_int *w)
//{
//    t_sample *in1 = (t_sample *)(w[1]);
//    t_sample *out = (t_sample *)(w[2]);
//    int n = (int)(w[3]);
//    while (n--) *out++ = *in1++; 
//    return (w+4);
//}
//
//t_int *copy_perf8(t_int *w)
//{
//    t_sample *in1 = (t_sample *)(w[1]);
//    t_sample *out = (t_sample *)(w[2]);
//    int n = (int)(w[3]);
//    
//    for (; n; n -= 8, in1 += 8, out += 8)
//    {
//        t_sample f0 = in1[0];
//        t_sample f1 = in1[1];
//        t_sample f2 = in1[2];
//        t_sample f3 = in1[3];
//        t_sample f4 = in1[4];
//        t_sample f5 = in1[5];
//        t_sample f6 = in1[6];
//        t_sample f7 = in1[7];
//
//        out[0] = f0;
//        out[1] = f1;
//        out[2] = f2;
//        out[3] = f3;
//        out[4] = f4;
//        out[5] = f5;
//        out[6] = f6;
//        out[7] = f7;
//    }
//    return (w+4);
//}
//
//void dsp_add_copy(t_sample *in, t_sample *out, int n)
//{
//    if (n&7)
//        dsp_add(copy_perform, 3, in, out, n);
//    else        
//        dsp_add(copy_perf8, 3, in, out, n);
//}
//
//static void adc_dsp(t_adc *x, t_signal **sp)
//{
//    t_int i, *ip;
//    t_signal **sp2;
//    for (i = x->x_n, ip = x->x_vec, sp2 = sp; i--; ip++, sp2++)
//    {
//        int ch = *ip - 1;
//        if ((*sp2)->s_n != sys_schedblocksize)
//            error("adc~: bad vector size");
//        else if (ch >= 0 && ch < sys_ninchannels)
//            dsp_add_copy(sys_soundin + sys_schedblocksize*ch,
//                (*sp2)->s_vec, sys_schedblocksize);
//        else dsp_add_zero((*sp2)->s_vec, sys_schedblocksize);
//    }    
//}
//
//static void adc_free(t_adc *x)
//{
//    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
//}
//
//static void adc_setup(void)
//{
//    adc_class = class_new(gensym("adc~"), (t_newmethod)adc_new,
//        (t_method)adc_free, sizeof(t_adc), 0, A_GIMME, 0);
//    class_addmethod(adc_class, (t_method)adc_dsp, gensym("dsp"), A_CANT, 0);
//    class_sethelpsymbol(adc_class, gensym("adc~_dac~"));
//}

void d_soundout_setup(void)
{
    soundout_setup();
    //adc_setup();
}

