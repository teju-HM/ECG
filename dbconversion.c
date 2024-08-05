#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>

#define PCM_DEVICE "hw:0,0"
#define RATE 8000
#define CHANNELS 1
#define SECONDS 10
#define SAMPLE_SIZE 16
#define BUFFER_SIZE  (32 * 1024)  // Example buffer size, adjust as needed
#define PERIOD_SIZE  (1024)       // Example period size, adjust as needed
#define BUFFER_FRAMES 32

typedef struct {
    char riff[4];        // "RIFF"
    int overall_size;    // File size - 8 bytes
    char wave[4];        // "WAVE"
    char fmt_chunk_marker[4]; // "fmt "
    int length_of_fmt;   // Length of format data
    short format_type;   // Format type (1 for PCM)
    short channels;      // Number of channels
    int sample_rate;     // Sample rate
    int byterate;        // Sample rate * Number of channels * Bytes per sample
    short block_align;   // Number of channels * Bytes per sample
    short bits_per_sample; // Bits per sample
    char data_chunk_header[4]; // "data"
    int data_size;       // Data size
} wav_header_t;

void write_wav_header(FILE *file, int sample_rate, short channels, int samples) {
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

int main() {
    int rc;
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    char *buffer;
    unsigned int rate = RATE;
    int dir;
    int size;

    // Open the PCM device in capture mode
    rc = snd_pcm_open(&pcm_handle, PCM_DEVICE, SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        return 1;
    }

    // Allocate a hardware parameters object
    snd_pcm_hw_params_malloc(&params);

    // Fill it in with default values
    snd_pcm_hw_params_any(pcm_handle, params);

    // Set the desired hardware parameters
    if ((rc = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf(stderr, "Error setting access: %s\n", snd_strerror(rc));
        return 1;
    }

    if ((rc = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE)) < 0) {
        fprintf(stderr, "Error setting format: %s\n", snd_strerror(rc));
        return 1;
    }

    if ((rc = snd_pcm_hw_params_set_channels(pcm_handle, params, CHANNELS)) < 0) {
        fprintf(stderr, "Error setting channels: %s\n", snd_strerror(rc));
        return 1;
    }

    if ((rc = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, &dir)) < 0) {
        fprintf(stderr, "Error setting rate: %s\n", snd_strerror(rc));
        return 1;
    }

    if ((rc = snd_pcm_hw_params(pcm_handle, params)) < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        return 1;
    }

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

    // Get period size
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * CHANNELS * (SAMPLE_SIZE / 8);
    buffer = (char *)malloc(size);
    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        return 1;
    }

    // Get the period time (for debugging or additional checks)
    snd_pcm_hw_params_get_period_time(params, &rate, &dir);

    FILE *fp = fopen("record8k.wav", "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file\n");
        free(buffer);
        return 1;
    }

    // Write the WAV header
    write_wav_header(fp, RATE, CHANNELS, SECONDS * RATE);

    for (int i = 0; i < SECONDS * RATE / frames; ++i) {
        rc = snd_pcm_readi(pcm_handle, buffer, frames);
        if (rc == -EPIPE) {
            // EPIPE means overrun
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(pcm_handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }
        
        // Write captured data to file
        fwrite(buffer, size, 1, fp);

        // Calculate and print dB value
        float rms = calculate_rms(buffer, frames, CHANNELS);
        float db = convert_to_db(rms);
        printf("Current dB level: %.2f dB\n", db);
    }

    fclose(fp);
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    free(buffer);
    snd_pcm_hw_params_free(params);

    return 0;
}
