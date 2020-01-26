// To list hardware input devices:
// arecord --list-devices
// usually you get: hw:0 for the microphone, and plughw:0 for the line input.




#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <aubio.h>
#include <time.h>
#include <math.h>
#include <assert.h>


#ifdef HAVE_C99_VARARGS_MACROS
#define PRINT_ERR(...)   fprintf(stderr, "AUBIO-TESTS ERROR: " __VA_ARGS__)
#define PRINT_MSG(...)   fprintf(stdout, __VA_ARGS__)
#define PRINT_DBG(...)   fprintf(stderr, __VA_ARGS__)
#define PRINT_WRN(...)   fprintf(stderr, "AUBIO-TESTS WARNING: " __VA_ARGS__)
#else
#define PRINT_ERR(format, args...)   fprintf(stderr, "AUBIO-TESTS ERROR: " format , ##args)
#define PRINT_MSG(format, args...)   fprintf(stdout, format , ##args)
#define PRINT_DBG(format, args...)   fprintf(stderr, format , ##args)
#define PRINT_WRN(format, args...)   fprintf(stderr, "AUBIO-TESTS WARNING: " format, ##args)
#endif




#define PCM_DEVICE "hw:0"

// arecord -D hw:0 -f cd -V=mono > testfile

#include <sys/asoundlib.h>
const char *snd_strerror( int errnum );
const char * errorchar;

int extract_tempo ()
{

// PREPARE AUDIO ACQUISITION ////////////////////////////////////////////////////////////////////////////////////
    int i;
    int err;
    char *buffer;
    int buffer_size;
    int buffer_frames = 512;
    unsigned int rate = 10000;
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;

    if ((err = snd_pcm_open (&capture_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0)) < 0) ;//exit (1);
    errorchar = snd_strerror(err);
    printf(snd_strerror(err));
    if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) exit (2);
    if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) exit (3);
    if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) exit (4);
    if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, format)) < 0) exit (5);
    if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, &rate, 0)) < 0) exit (6);
    if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) exit (7);
    if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) exit (8);
    snd_pcm_hw_params_free (hw_params);
    if ((err = snd_pcm_prepare (capture_handle)) < 0) exit (9);
    buffer_size = buffer_frames * snd_pcm_format_width(format) / 8 * 2; // 2 because 2 channels (left/right)
    buffer = (char *) malloc(buffer_size);

    // PREPARE AUBIO TEMPO TRACKING ///////////////////////////////////////////////////////////////

    uint_t          samplerate   = 0;
    samplerate                   = rate;
    uint_t          win_size     = 1024; // window size
    uint_t          hop_size     = win_size / 2;

    uint_t          n_frames     = 0;
    uint_t          read         = 0;
    fvec_t *        in           = new_fvec (hop_size); // input audio buffer
    fvec_t *        out          = new_fvec (1); // output position
    aubio_tempo_t * o            = new_aubio_tempo("default", win_size, hop_size, samplerate);
    // MAIN LOOP ///////////////////////////////////////////////////////
    while(1)
    {
        // FROM ALSA:
        if ((err = snd_pcm_readi (capture_handle, buffer, buffer_frames)) != buffer_frames) exit (100);

        if(hop_size != buffer_size/4)
        {
            printf("Error buffer size\n");
            return 0;
        }

        int k = 0;
        float tmpfloat = 0;
        for (unsigned int j = 0; j< buffer_size-4; j+=4)
        {
            in->data[k] = buffer[j] + buffer[j+1] * 256;
            tmpfloat = tmpfloat + fabs(in->data[k]) / 1000.0;
            k++;
            if (k > in->length -1) exit(111);
        }

        aubio_tempo_do(o,in,out);
        // do something with the beats
        if (out->data[0] != 0)
        {
            //PRINT_MSG("beat at %.3fms, %.3fs, frame %d, %.2fbpm with confidence %.2f\nLengh:%i energy:%f",
            //          aubio_tempo_get_last_ms(o), aubio_tempo_get_last_s(o),
            //          aubio_tempo_get_last(o), aubio_tempo_get_bpm(o), aubio_tempo_get_confidence(o), out->length, tmpfloat);
            // Update some global variable about its findings on tempo tracking:
            PCMInputTempoValue = aubio_tempo_get_bpm(o);
            PCMInputTempoConfidence = aubio_tempo_get_confidence(o);
        }
        n_frames += read;
    }

    free(buffer);

    fprintf(stdout, "buffer freed\n");

    snd_pcm_close (capture_handle);
    fprintf(stdout, "audio interface closed\n");

    exit (0);
}


int PlayWav(char * FileName)
{
    int loops, channels, pcm, buff_size;
    int seconds;
    unsigned int tmp, rate;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    char * buff;



    rate 	 = 44100;
    channels = 2;
    seconds  = 10;

    /* Open the PCM device in playback mode */
    if (pcm = snd_pcm_open(&pcm_handle, PCM_DEVICE,
                           SND_PCM_STREAM_PLAYBACK, 0) < 0)
        printf("ERROR: Can't open \"%s\" PCM device. %s\n",
               PCM_DEVICE, snd_strerror(pcm));

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm_handle, params);


    /* Set parameters */
    if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
        printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
                                           SND_PCM_FORMAT_S16_LE) < 0)
        printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
        printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

    if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0)
        printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

    /* Write parameters */
    if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

    /* Resume information */
    printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

    printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

    snd_pcm_hw_params_get_channels(params, &tmp);
    printf("channels: %i ", tmp);

    if (tmp == 1)
        printf("(mono)\n");
    else if (tmp == 2)
        printf("(stereo)\n");

    snd_pcm_hw_params_get_rate(params, &tmp, 0);
    printf("rate: %d bps\n", tmp);

    printf("seconds: %d\n", seconds);

    /* Allocate buffer to hold single period */
    snd_pcm_hw_params_get_period_size(params, &frames, 0);

    buff_size = frames * channels * 2 /* 2 -> sample size */;
    buff = (char *) malloc(buff_size);

    snd_pcm_hw_params_get_period_time(params, &tmp, NULL);


    snd_pcm_hw_params_get_channels(params, &tmp);

    for (loops = (seconds * 1000000) / tmp; loops > 0; loops--)
    {

        if (pcm = read(0, buff, buff_size) == 0)
        {
            printf("Early end of file.\n");
            return 0;
        }

        if (pcm = snd_pcm_writei(pcm_handle, buff, frames) == -EPIPE)
        {
            printf("XRUN.\n");
            snd_pcm_prepare(pcm_handle);
        }
        else if (pcm < 0)
        {
            printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
        }

    }

}



