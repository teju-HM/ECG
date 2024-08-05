#include "audio_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BUFFER_FRAMES 32
//#define size 1024
#define BUFFER_SIZE  (32 * 1024)  // Example buffer size, adjust as needed
#define PERIOD_SIZE  (1024)       // Example period size, adjust as needed

typedef struct {
    char riff[4];
    int overall_size;
    char wave[4];
    char fmt_chunk_marker[4];
    int length_of_fmt;
    short format_type;
    short channels;
    int sample_rate;
    int byterate;
    short block_align;
    short bits_per_sample;
    char data_chunk_header[4];
    int data_size;
} wav_header_t;

static void write_wav_header(FILE *file, int sample_rate, short channels, int samples) {
    wav_header_t wav_header;

    memcpy(wav_header.riff, "RIFF", 4);
    memcpy(wav_header.wave, "WAVE", 4);
    memcpy(wav_header.fmt_chunk_marker, "fmt ", 4);
    memcpy(wav_header.data_chunk_header, "data", 4);

    wav_header.length_of_fmt = 16;
    wav_header.format_type = 1;
    wav_header.channels = channels;
    wav_header.sample_rate = sample_rate;
    wav_header.bits_per_sample = SAMPLE_SIZE;
    wav_header.byterate = sample_rate * channels * wav_header.bits_per_sample / 8;
    wav_header.block_align = channels * wav_header.bits_per_sample / 8;
    wav_header.data_size = samples * wav_header.block_align;
    wav_header.overall_size = wav_header.data_size + sizeof(wav_header) - 8;

    fwrite(&wav_header, sizeof(wav_header), 1, file);
}

int init_pcm(snd_pcm_t **pcm_handle, const char *device) {
    int rc = snd_pcm_open(pcm_handle, device, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        return rc;
    }
    return 0;
}

float calculate_rms(const char *buffer, snd_pcm_uframes_t frames, int channels) {
    float sum = 0.0;
    int16_t *samples = (int16_t *)buffer;
    int num_samples = frames * channels;

    for (int i = 0; i < num_samples; ++i) {
        sum += samples[i] * samples[i];
    }

    float mean = sum / num_samples;
    return sqrt(mean);
}

float convert_to_db(float rms) {
    if (rms <= 0) return -100.0; // To avoid log10(0) issue, return a very low value
    return 20.0 * log10(rms);
}

int set_pcm_params(snd_pcm_t *pcm_handle, unsigned int rate, snd_pcm_hw_params_t **params) {
    int rc;
    int dir;

    snd_pcm_hw_params_malloc(params);
    snd_pcm_hw_params_any(pcm_handle, *params);
    snd_pcm_hw_params_set_access(pcm_handle, *params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, *params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm_handle, *params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(pcm_handle, *params, &rate, &dir);

// Set buffer size
if (snd_pcm_hw_params_set_buffer_size(pcm_handle, *params, BUFFER_SIZE) < 0) {
    fprintf(stderr, "Error setting buffer size\n");
    return 1;
}

// Set period size
if (snd_pcm_hw_params_set_period_size(pcm_handle, *params, PERIOD_SIZE, 0) < 0) {
    fprintf(stderr, "Error setting period size\n");
    return 1;
}


    rc = snd_pcm_hw_params(pcm_handle, *params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        return rc;
    }
    return 0;
}

int read_pcm_data(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *params, const char *filename, int seconds) {
    int rc;
    snd_pcm_uframes_t frames;
    char *buffer;
    FILE *fp;
    unsigned int rate;
    int dir;

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    rate = 8000; // Default rate, can be updated based on params
    int size = frames * CHANNELS * (SAMPLE_SIZE / 8);
    buffer = (char *)malloc(size);

    fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file\n");
        return 1;
    }

    // Write the WAV header
    write_wav_header(fp, rate, CHANNELS, seconds * rate);

    for (int i = 0; i < seconds * rate / frames; ++i) {
        rc = snd_pcm_readi(pcm_handle, buffer, frames);
        if (rc == -EPIPE) {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(pcm_handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }
        fwrite(buffer, size, 1, fp);
        float rms = calculate_rms(buffer, frames, CHANNELS);
        float db = convert_to_db(rms);
    }

    fclose(fp);
    free(buffer);
    return 0;
}

