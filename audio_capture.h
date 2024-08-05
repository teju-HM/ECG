#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <alsa/asoundlib.h>

#define CHANNELS 1
#define SAMPLE_SIZE 16

// Initialization function
int init_pcm(snd_pcm_t **pcm_handle, const char *device);

// Parameter setting function
int set_pcm_params(snd_pcm_t *pcm_handle, unsigned int rate, snd_pcm_hw_params_t **params);

// Data reading function
int read_pcm_data(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, const char *filename, int seconds);

#endif // AUDIO_CAPTURE_H

