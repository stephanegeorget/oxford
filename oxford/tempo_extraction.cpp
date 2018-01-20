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






// arecord -D hw:0 -f cd -V=mono > testfile

// These two global variables are shared with oxford_main.cpp
// to communicate information about tempo.
float PCMInputTempoValue;
float PCMInputTempoConfidence;

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

    if ((err = snd_pcm_open (&capture_handle, "hw:0", SND_PCM_STREAM_CAPTURE, 0)) < 0) exit (1);
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
