#include "audio_capture.h"

int main() {
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    unsigned int sample_rate = 8000; // Set your desired sampling rate here
    const char *device = "hw:0,0";
    const char *filename = "record8k.wav";
    int seconds = 10;

    if (init_pcm(&pcm_handle, device) < 0) {
        fprintf(stderr, "Failed to initialize PCM device\n");
        return 1;
    }

    if (set_pcm_params(pcm_handle, sample_rate, &params) < 0) {
        fprintf(stderr, "Failed to set PCM parameters\n");
        snd_pcm_close(pcm_handle);
        return 1;
    }

    if (read_pcm_data(pcm_handle, params, filename, seconds) < 0) {
        fprintf(stderr, "Failed to read PCM data\n");
        snd_pcm_close(pcm_handle);
        return 1;
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    snd_pcm_hw_params_free(params);

    return 0;
}

