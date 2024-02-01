/*
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
/*#include "../include/asoundlib.h"*/
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>

#define PHL

/*
#define ONBOARD
*/

#ifdef ONBOARD
char *pdevice = "hw:CARD=wm8904audio,DEV=0";
char *cdevice = "hw:CARD=wm8904audio,DEV=0";
int channels = 4;
#endif

#ifdef PHL
char *pdevice = "hw:CARD=boneaudioext,DEV=0";
char *cdevice = "hw:CARD=boneaudioext,DEV=0";
int channels = 6;
#endif

/*char *pdevice = "hw:0,0";
char *cdevice = "hw:0,0";*/ 

/*char *pdevice = "sysdefault:CARD=wm8904audio";
char *cdevice = "sysdefault:CARD=wm8904audio";*/ 

/*char *pdevice = "hw:CARD=wm8904audioa,DEV=0";
char *cdevice = "hw:CARD=wm8904audioa,DEV=0";*/


/*char *pdevice = "merge";
char *cdevice = "merge";*/ 

snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
int rate = 16000;
int buffer_size = 0;        /* auto */
int period_size = 0;        /* auto */
int latency_min = 32;       /* in frames / 2 */
int latency_max = 2048;     /* in frames / 2 */
int loop_sec = 5;      /* seconds */
int block = 0;          /* block mode */
int use_poll = 1;
int resample = 1;
unsigned long loop_limit;
int length[512];
int len_index = 0;
int save_index = 0;
int mask = 0x000001FF;
snd_output_t *output = NULL;
unsigned int temp;

struct timespec c_start, c_end, p_start, p_end;
int * r_time;
int rtime_index = 0;
int * a_time;
int atime_index = 0;
int time_mask = 0x1FF;
int r_flag = 0;
int * w_time;
int wtime_index = 0;
int w_flag = 0;
int * r_avail;
int ravail_index = 0;
int ra_flag = 0;
int * w_avail1;
int wavail1_index = 0;
int wa1_flag = 0;
int * w_avail2;
int wavail2_index = 0;
int wa2_flag = 0;
int * w_delay;
int wdelay_index = 0;
int wd_flag = 0;
int avail_mask = 0x1FF;
int flag_limit = 64;

FILE *fptr;
 
int setparams_stream(snd_pcm_t *handle,
             snd_pcm_hw_params_t *params,
             const char *id)
{
    int err;
    unsigned int rrate;
 
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for %s PCM: no configurations available: %s\n", snd_strerror(err), id);
        return err;
    }
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resample setup failed for %s (val %i): %s\n", id, resample, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for %s: %s\n", id, snd_strerror(err));
        return err;
    }

    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%i) not available for %s: %s\n", channels, id, snd_strerror(err));
        return err;
    }
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %iHz not available for %s: %s\n", rate, id, snd_strerror(err));
        return err;
    }
    if ((int)rrate != rate) {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    return 0;
}


int set_bufsize(snd_pcm_t *handle,
              snd_pcm_hw_params_t *params,
              snd_pcm_uframes_t bufsize)
{
    int err;
    snd_pcm_uframes_t periodsize;
 
    periodsize = bufsize / 2;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &bufsize);
    if (err < 0) {
        printf("Unable to set buffer size %li: %s\n", bufsize ,  snd_strerror(err));
        return err;
    }

    err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
    if (err < 0) {
        printf("Unable to set period size %li:  %s\n", periodsize,  snd_strerror(err));
        return err;
    }
    return 0;
}
 
 
int setparams_bufsize(snd_pcm_t *handle,
              snd_pcm_hw_params_t *params,
              snd_pcm_hw_params_t *tparams,
              snd_pcm_uframes_t bufsize,
              const char *id)
{
    int err;
    snd_pcm_uframes_t periodsize;
 
    snd_pcm_hw_params_copy(params, tparams);
    periodsize = bufsize * 2;
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &periodsize);
    if (err < 0) {
        printf("Unable to set buffer size %li for %s: %s\n", bufsize * 2, id, snd_strerror(err));
        return err;
    }
    if (period_size > 0)
        periodsize = period_size;
    else
        periodsize /= 2;
    err = snd_pcm_hw_params_set_period_size_near(handle, params, &periodsize, 0);
    if (err < 0) {
        printf("Unable to set period size %li for %s: %s\n", periodsize, id, snd_strerror(err));
        return err;
    }
    return 0;
}
 
int setparams_set(snd_pcm_t *handle,
          snd_pcm_hw_params_t *params,
          snd_pcm_sw_params_t *swparams,
          const char *id)
{
    int err;
    snd_pcm_uframes_t val;

    printf("In setparams_set.\n"); 
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 0x7fffffff);
    if (err < 0) {
        printf("Unable to set start threshold mode for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    if (!block)
        val = 4;
    else
        snd_pcm_hw_params_get_period_size(params, &val, NULL);
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, val);
    if (err < 0) {
        printf("Unable to set avail min for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for %s: %s\n", id, snd_strerror(err));
        return err;
    }
    return 0;
}
 
int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle, int *bufsize)
{
    int err, last_bufsize = *bufsize;
    snd_pcm_hw_params_t *pt_params, *ct_params; /* templates with rate, format and channels */
    snd_pcm_hw_params_t *p_params, *c_params;
    snd_pcm_sw_params_t *p_swparams, *c_swparams;
    snd_pcm_uframes_t p_size, c_size, p_psize, c_psize;
    unsigned int p_time, c_time;
    unsigned int val;
 
    snd_pcm_hw_params_alloca(&p_params);
    snd_pcm_hw_params_alloca(&c_params);
    snd_pcm_hw_params_alloca(&pt_params);
    snd_pcm_hw_params_alloca(&ct_params);
    snd_pcm_sw_params_alloca(&p_swparams);
    snd_pcm_sw_params_alloca(&c_swparams);
    if ((err = setparams_stream(phandle, pt_params, "playback")) < 0) {
        printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = setparams_stream(chandle, ct_params, "capture")) < 0) {
        printf("Unable to set parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
 

    /* NRM */
    /* used buffersize passed in with -B option, otherwise set to 128 for now */
    /* Not using m,M options for 'latency'. Malloced 'buffer' using buffer_size */
    /* Originally, latency input (m,M) was cut in half and used in the call to setparams().*/
    /* Original code is very confusing with names for period and buffer. */

    if (buffer_size > 0) 
        *bufsize = buffer_size;
    else
        *bufsize = 128;

   
    if ((err = set_bufsize(phandle, pt_params, *bufsize)) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = set_bufsize(chandle, ct_params,  *bufsize)) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
 
 
/*
    if ((err = setparams_bufsize(phandle, p_params, pt_params, *bufsize, "playback")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = setparams_bufsize(chandle, c_params, ct_params, *bufsize, "capture")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
*/

    snd_pcm_hw_params_get_period_size(pt_params, &p_psize, NULL);
    snd_pcm_hw_params_get_period_size(ct_params, &c_psize, NULL);
 
    
    snd_pcm_hw_params_get_period_time(pt_params, &p_time, NULL);
    snd_pcm_hw_params_get_period_time(ct_params, &c_time, NULL);
    if (p_time != c_time){
        printf("Period times don't match for playback and capture\n");
        exit(0);
    } 

    if ((err = setparams_set(phandle, pt_params, p_swparams, "playback")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
    if ((err = setparams_set(chandle, ct_params, c_swparams, "capture")) < 0) {
        printf("Unable to set sw parameters for playback stream: %s\n", snd_strerror(err));
        exit(0);
    }
 
    if ((err = snd_pcm_prepare(phandle)) < 0) {
        printf("Prepare error: %s\n", snd_strerror(err));
        exit(0);
    }
 

    snd_pcm_dump(phandle, output);
    snd_pcm_dump(chandle, output);
    fflush(stdout);
    return 0;
}
 
void showstat(snd_pcm_t *handle, size_t frames)
{
    int err;
    snd_pcm_status_t *status;
 
    snd_pcm_status_alloca(&status);
    if ((err = snd_pcm_status(handle, status)) < 0) {
        printf("Stream status error: %s\n", snd_strerror(err));
        exit(0);
    }
    printf("*** frames = %li ***\n", (long)frames);
    snd_pcm_status_dump(status, output);
}
 
void showlatency(size_t latency)
{
    double d;
    d = (double)latency / (double)rate;
    printf("Trying latency %li frames, %.3fus, %.6fms (%.4fHz)\n", (long)latency, d * 1000000, d * 1000, (double)1 / d);
}
 
void showinmax(size_t in_max)
{
    double d;
 
    printf("Maximum read: %li frames\n", (long)in_max);
    d = (double)in_max / (double)rate;
    printf("Maximum read latency: %.3fus, %.6fms (%.4fHz)\n", d * 1000000, d * 1000, (double)1 / d);
}
 
void gettimestamp(snd_pcm_t *handle, snd_timestamp_t *timestamp)
{
    int err;
    snd_pcm_status_t *status;
 
    snd_pcm_status_alloca(&status);
    if ((err = snd_pcm_status(handle, status)) < 0) {
        printf("Stream status error: %s\n", snd_strerror(err));
        exit(0);
    }
    snd_pcm_status_get_trigger_tstamp(status, timestamp);
}
 
void setscheduler(void)
{
    struct sched_param sched_param;
 
    if (sched_getparam(0, &sched_param) < 0) {
        printf("Scheduler getparam failed...\n");
        return;
    }
   /* sched_param.sched_priority = sched_get_priority_max(SCHED_FIFO);*/
    sched_param.sched_priority = 95;
    if (!sched_setscheduler(0, SCHED_FIFO, &sched_param)) {
        printf("Scheduler set to FIFO with priority %i...\n", sched_param.sched_priority);
        fflush(stdout);
        return;
    }
    printf("!!!Scheduler set to FIFO with priority %i FAILED!!!\n", sched_param.sched_priority);
}
 
long timediff(snd_timestamp_t t1, snd_timestamp_t t2)
{
    signed long l;
 
    t1.tv_sec -= t2.tv_sec;
    l = (signed long) t1.tv_usec - (signed long) t2.tv_usec;
    if (l < 0) {
        t1.tv_sec--;
        l = 1000000 + l;
        l %= 1000000;
    }
    return (t1.tv_sec * 1000000) + l;
}
 
long readbuf(snd_pcm_t *handle, char *buf, long len, size_t *frames, size_t *max)
{
    long r;
    ssize_t a;
    //printf("--readbuf--\n"); 
    if (!block) {
        do {
           
               if (ra_flag<flag_limit){
                  a = snd_pcm_avail(handle);
                  *(r_avail+ravail_index) = a;
                  ravail_index++;
                  ravail_index=ravail_index&avail_mask;

                  temp = clock_gettime(CLOCK_REALTIME, &c_end); /* reusing c_end */
                  *(a_time+atime_index) = (long)(1000000000*(c_end.tv_sec - c_start.tv_sec) + (c_end.tv_nsec - c_start.tv_nsec)); 
                  atime_index++;
                  atime_index=atime_index&time_mask;
              /*  c_start = c_end; */
                  ra_flag++;
               }

            r = snd_pcm_readi(handle, buf, len);
            if (r_flag<flag_limit){
                *(length+len_index) = r;
                len_index++;
                len_index=len_index&mask;
                temp = clock_gettime(CLOCK_REALTIME, &c_end);
                *(r_time+rtime_index) = (long)(1000000000*(c_end.tv_sec - c_start.tv_sec) + (c_end.tv_nsec - c_start.tv_nsec)); 
                rtime_index++;
                rtime_index=rtime_index&time_mask;
              /*  c_start = c_end; */
                r_flag++;
            }
        } while (r == -EAGAIN);
        if (r > 0) {
            *frames += r;
            if ((long)*max < r)
                *max = r;
        }
      //  printf("read = %li\n", r);
    } else {
        int frame_bytes = (snd_pcm_format_width(format) / 8) * channels;
        do {
            r = snd_pcm_readi(handle, buf, len);
            if (r > 0) {
                buf += r * frame_bytes;
                len -= r;
                *frames += r;
                if ((long)*max < r)
                    *max = r;
            }
            //printf("r = %li, len = %li\n", r, len);
        } while (r >= 1 && len > 0);
    }
    //showstat(handle, 0);
    return r;
}
 
long writebuf(snd_pcm_t *handle, char *buf, long len, size_t *frames)
{
    long r;
    ssize_t a;
    snd_pcm_sframes_t d;
    int frame_bytes = (snd_pcm_format_width(format) / 8) * channels;
 
    //printf("--writebuf--\n"); 
    while (len > 0) {
       if (wa1_flag<flag_limit){
           a = snd_pcm_avail(handle);
           *(w_avail1+wavail1_index) = a;
           wavail1_index++;
           wavail1_index=wavail1_index&avail_mask;
           wa1_flag++;
        }
       if (wd_flag<flag_limit){
           a = snd_pcm_delay(handle,&d);
           *(w_delay+wdelay_index) =(int) d;
           wdelay_index++;
           wdelay_index=wdelay_index&avail_mask;
           wd_flag++;
        }

        r = snd_pcm_writei(handle, buf, len);
        if (w_flag<flag_limit){
           temp = clock_gettime(CLOCK_REALTIME, &p_end);
           *(w_time+wtime_index) = (long)(1000000000*(p_end.tv_sec - p_start.tv_sec) + (p_end.tv_nsec - p_start.tv_nsec)); 
           wtime_index++;
           wtime_index=wtime_index&time_mask;
       /*    p_start = p_end; */
           w_flag++;
        }
        if (wa2_flag<flag_limit){
           a = snd_pcm_avail(handle);
           *(w_avail2+wavail2_index) = a;
           wavail2_index++;
           wavail2_index=wavail2_index&avail_mask;
           wa2_flag++;
        }

        if (r == -EAGAIN)
            continue;
     //   printf("write = %li\n", r);
        if (r < 0)
            return r;
      //  showstat(handle, 0);
        buf += r * frame_bytes;
        len -= r;
        *frames += r;
    }
    return 0;
}
            


void help(void)
{
    int k;
    printf(
"Usage: latency [OPTION]... [FILE]...\n"
"-h,--help      help\n"
"-P,--pdevice   playback device\n"
"-C,--cdevice   capture device\n"
"-m,--min       minimum latency in frames\n"
"-M,--max       maximum latency in frames\n"
"-F,--frames    frames to transfer\n"
"-f,--format    sample format\n"
"-c,--channels  channels\n"
"-r,--rate      rate\n"
"-B,--buffer    buffer size in frames\n"
"-E,--period    period size in frames\n"
"-s,--seconds   duration of test in seconds\n"
"-b,--block     block mode\n"
"-p,--poll      use poll (wait for event - reduces CPU usage)\n"
"-e,--effect    apply an effect (bandpass filter sweep)\n"
);
        printf("Recognized sample formats are:");
        for (k = 0; k < SND_PCM_FORMAT_LAST; ++k) {
                const char *s = snd_pcm_format_name(k);
                if (s)
                        printf(" %s", s);
        }
        printf("\n\n");
        printf(
"Tip #1 (usable latency with large periods, non-blocking mode, good CPU usage,\n"
"        superb xrun prevention):\n"
"  latency -m 8192 -M 8192 -t 1 -p\n"
"Tip #2 (superb latency, non-blocking mode, but heavy CPU usage):\n"
"  latency -m 128 -M 128\n"
);
}
 
int main(int argc, char *argv[])
{
    struct option long_option[] =
    {
        {"help", 0, NULL, 'h'},
        {"pdevice", 1, NULL, 'P'},
        {"cdevice", 1, NULL, 'C'},
        {"min", 1, NULL, 'm'},
        {"max", 1, NULL, 'M'},
        {"frames", 1, NULL, 'F'},
        {"format", 1, NULL, 'f'},
        {"channels", 1, NULL, 'c'},
        {"rate", 1, NULL, 'r'},
        {"buffer", 1, NULL, 'B'},
        {"period", 1, NULL, 'E'},
        {"seconds", 1, NULL, 's'},
        {"block", 0, NULL, 'b'},
        {"poll", 0, NULL, 'p'},
        {"effect", 0, NULL, 'e'},
        {NULL, 0, NULL, 0},
    };
    snd_pcm_t *phandle, *chandle;
    char *buffer;
    int err, latency, morehelp;
    int ok;
    snd_timestamp_t p_tstamp, c_tstamp;
    ssize_t r;
    size_t frames_in, frames_out, in_max;
    int effect = 1;
    morehelp = 0;

    for(int i=0; i<512; i++){
        length[i] = 0;
    }

 /*---------------------------------------------------------------------------*/

    r_time = (int *)malloc(128*sizeof(int));
    if (r_time == NULL){
        printf("r_time malloc failed.\n");
        exit(1);
    }
    a_time = (int *)malloc(128*sizeof(int));
    if (a_time == NULL){
        printf("a_time malloc failed.\n");
        exit(1);
    }
    w_time = (int *)malloc(128*sizeof(int));
    if (w_time == NULL){
        printf("w_time malloc failed.\n");
        exit(1);
    }
    r_avail = (int *)malloc(128*sizeof(int));
    if (r_avail == NULL){
        printf("r_avail malloc failed.\n");
        exit(1);
    }
    w_avail1 = (int *)malloc(128*sizeof(int));
    if (w_avail1 == NULL){
        printf("w_avail1 malloc failed.\n");
        exit(1);
    }
    w_avail2 = (int *)malloc(128*sizeof(int));
    if (w_avail2 == NULL){
        printf("w_avail2 malloc failed.\n");
        exit(1);
    }
    w_delay = (int *)malloc(128*sizeof(int));
    if (w_delay == NULL){
        printf("w_delay malloc failed.\n");
        exit(1);
    }

    while (1) {
        int c;
        if ((c = getopt_long(argc, argv, "hP:C:m:M:F:f:c:r:B:E:s:bpen", long_option, NULL)) < 0)
            break;
        switch (c) {
        case 'h':
            morehelp++;
            break;
        case 'P':
            pdevice = strdup(optarg);
            break;
        case 'C':
            cdevice = strdup(optarg);
            break;
        case 'm':
            err = atoi(optarg) / 2;
            latency_min = err >= 4 ? err : 4;
            if (latency_max < latency_min)
                latency_max = latency_min;
            break;
        case 'M':
            err = atoi(optarg) / 2;
            latency_max = latency_min > err ? latency_min : err;
            break;
        case 'f':
            format = snd_pcm_format_value(optarg);
            if (format == SND_PCM_FORMAT_UNKNOWN) {
                printf("Unknown format, setting to default S16_LE\n");
                format = SND_PCM_FORMAT_S16_LE;
            }
            break;
        case 'c':
            err = atoi(optarg);
            channels = err >= 1 && err < 1024 ? err : 1;
            break;
        case 'r':
            err = atoi(optarg);
            rate = err >= 4000 && err < 200000 ? err : 44100;
            break;
        case 'B':
            err = atoi(optarg);
            buffer_size = err >= 4 && err < 200000 ? err : 0;
            break;
        case 'E':
            err = atoi(optarg);
            period_size = err >= 32 && err < 200000 ? err : 0;
            break;
        case 's':
            err = atoi(optarg);
            loop_sec = err >= 1 && err <= 100000 ? err : 30;
            break;
        case 'b':
            block = 1;
            break;
        case 'p':
            use_poll = 1;
            break;
        case 'e':
            effect = 1;
            break;
        case 'n':
            resample = 0;
            break;
        }
    }
 
    if (morehelp) {
        help();
        return 0;
    }
    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
        printf("Output failed: %s\n", snd_strerror(err));
        return 0;
    }
 
    loop_limit = loop_sec * rate;
    latency = latency_min - 4;
    /* NRM */
    buffer = malloc((buffer_size * snd_pcm_format_width(format) / 8) * 2);
 
    setscheduler();
 
    printf("Playback device is %s\n", pdevice);
    printf("Capture device is %s\n", cdevice);
    printf("Parameters are %iHz, %s, %i channels, %s mode\n", rate, snd_pcm_format_name(format), channels, block ? "blocking" : "non-blocking");
    printf("Poll mode: %s\n", use_poll ? "yes" : "no");
    printf("Loop limit is %lu frames, minimum latency = %i, maximum latency = %i\n", loop_limit, latency_min * 2, latency_max * 2);
 
    if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }
    if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE, block ? 0 : SND_PCM_NONBLOCK)) < 0) {
        printf("Record open error: %s\n", snd_strerror(err));
        return 0;
    }
 
             
   /* while (1) { */
        frames_in = frames_out = 0;
        if (setparams(phandle, chandle, &buffer_size) < 0)
            exit(1);
        showlatency(buffer_size);
/*   Read parameters here to see if they make sense */

        if ((err = snd_pcm_link(chandle, phandle)) < 0) {
            printf("Streams link error: %s\n", snd_strerror(err));
            exit(0);
        }
        if (snd_pcm_format_set_silence(format, buffer, (buffer_size/2)*channels) < 0) {
            fprintf(stderr, "silence error\n");
            exit(1);
        }
        clock_gettime(CLOCK_REALTIME,&p_start);
        if (writebuf(phandle, buffer, (buffer_size/2), &frames_out) < 0) {
            fprintf(stderr, "write error\n");
            exit(1);
        }
        if (writebuf(phandle, buffer, (buffer_size/2), &frames_out) < 0) {
            fprintf(stderr, "write error\n");
            exit(1);
        }
 
        if ((err = snd_pcm_start(chandle)) < 0) {
            printf("Go error: %s\n", snd_strerror(err));
            exit(0);
        }
        gettimestamp(phandle, &p_tstamp);
        gettimestamp(chandle, &c_tstamp);
        clock_gettime(CLOCK_REALTIME,&c_start);
#if 0
        printf("Playback:\n");
        showstat(phandle, frames_out);
        printf("Capture:\n");
        showstat(chandle, frames_in);
#endif
 
        ok = 1;
        in_max = 0;
       /*int i = 0;
        int mask = 0x000001FF;*/
        while (ok && frames_in < loop_limit) {
            if (use_poll) {
                /* use poll to wait for next event */
                snd_pcm_wait(chandle, 500);
            }
            if ((r = readbuf(chandle, buffer, (buffer_size/2), &frames_in, &in_max)) < 0){
                ok = 0;}
            else 
                if (writebuf(phandle, buffer, r, &frames_out) < 0)
                    ok = 0;
              
            }
       /* }*/
        if (ok)
            printf("Success\n");
        else
            printf("Failure\n");
        printf("Playback:\n");
        showstat(phandle, frames_out);
        printf("Capture:\n");
        showstat(chandle, frames_in);
        showinmax(in_max);
        if (p_tstamp.tv_sec == c_tstamp.tv_sec &&
            p_tstamp.tv_usec == c_tstamp.tv_usec)
            printf("Hardware sync\n");
        snd_pcm_drop(chandle);
        snd_pcm_nonblock(phandle, 0);
        snd_pcm_drain(phandle);
        snd_pcm_nonblock(phandle, !block ? 1 : 0);
        if (ok) {
#if 1
            printf("Playback time = %li.%i, Record time = %li.%i, diff = %li\n",
                   p_tstamp.tv_sec,
                   (int)p_tstamp.tv_usec,
                   c_tstamp.tv_sec,
                   (int)c_tstamp.tv_usec,
                   timediff(p_tstamp, c_tstamp));
#endif
        }
        snd_pcm_unlink(chandle);
        snd_pcm_hw_free(phandle);
        snd_pcm_hw_free(chandle);
/*
    fptr = fopen("r_times.txt","w");
    if(fptr == NULL){
      printf("Error opening file.\n");
    }
    for(int i=0; i< 512; i++){
        fprintf(fptr,"%fms\n",(float)r_time[i]/1000000);
    } 
    fclose(fptr);

    fptr = fopen("r_avail.txt","w");
    if(fptr == NULL){
      printf("Error opening file.\n");
    }
    for(int i=0; i< 512; i++){
        fprintf(fptr,"%d\n",r_avail[i]);
    } 
    fclose(fptr);

    fptr = fopen("w_avail1.txt","w");
    if(fptr == NULL){
      printf("Error opening file.\n");
    }
    for(int i=0; i< 512; i++){
        fprintf(fptr,"%d\n",w_avail1[i]);
    } 
    fclose(fptr);


    fptr = fopen("w_times.txt","w");
    if(fptr == NULL){
      printf("Error opening file.\n");
    }
    for(int i=0; i< 512; i++){
        fprintf(fptr,"%fms\n",(float)w_time[i]/1000000);
    } 
    fclose(fptr);
*/

    fptr = fopen("consolidate.txt","w");
    if(fptr == NULL){
      printf("Error opening file.\n");
    }
    fprintf(fptr,"   avail\t|  readi\t|  w_avail1\t| w_delay |  w_avail2\t|   writei\t\n");
    fprintf(fptr,"------------------------------------------------------------------------\n");
    fprintf(fptr,"  ------\t| ----------\t|     %d\t|    %d   |    %d\t|    %4.2fms\n",w_avail1[0],w_delay[0],w_avail2[0],(float)w_time[0]/1000000);
    fprintf(fptr,"  ------\t| ----------\t|     %d\t|    %d\t|    %d\t|    %4.2fms\n",w_avail1[1],w_delay[1],w_avail2[1],(float)w_time[1]/1000000);
    for(int i=0; i< 500; i++){
        fprintf(fptr," %d@%4.2fms\t|  %d@%4.2fms\t|     %3d\t|   %3d  |   %3d   |\t     %4.2fms\n",r_avail[i],(float)a_time[i]/1000000,length[i],(float)r_time[i]/1000000,w_avail1[i+2], w_delay[i+2],w_avail2[i+2],(float)w_time[i+2]/1000000);
    } 
    fclose(fptr);

       
    snd_pcm_close(phandle);
    snd_pcm_close(chandle);

    return 0;
}
