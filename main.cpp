// by Alula, licensed under public domain

#include <functional>
#include <vector>
#include <array>
#include <cstdio>
#include <cmath>
#include <chrono>
#include <thread>
#include "miniaudio.h"
#include "waves.h"

namespace piyopiyo
{
    template <class T>
    constexpr static T clamp(T val, T min, T max)
    {
        return std::min(std::max(val, min), max);
    }

    class Track
    {
    public:
        char waveform[0x100];
        unsigned char envelope[0x40];
        unsigned char octave;
        unsigned int length;
        unsigned int volume;
        unsigned int activeMask = 0;
        float volLeft = 1.0f;
        float volRight = 1.0f;
        float volMix = 1.0f;
        float volMixLow = 1.0f;
        float timers[24];
        float fphases[24];
        int phases[24];
        std::vector<unsigned int> notes;
    };

    class Player
    {
    public:
        unsigned int sampleRate = 44100;
        //unsigned int sampleRate = 11025;
        unsigned int millisPerTick = 0;
        int repeatTick = 0;
        int endTick = 0;
        int records = 0;

        std::array<Track, 4> tracks;

        bool load(std::function<bool(void *data, unsigned int len)> reader);

        void render(short *samples, unsigned sampleCount);

    private:
        unsigned int currTick = 0;
        unsigned int notePtr = 0;

        bool loaded = false;
    };

    constexpr static int freqTbl[12] = {1551, 1652, 1747, 1848, 1955, 2074, 2205, 2324, 2461, 2616, 2770, 2938};
    //constexpr static int panTbl[8] = {-2560, -1600, -760, -320, 0, 320, 760, 1640};
	constexpr static int panTbl[8] = {2560, 1600, 760, 320, 0, -320, -760, -1640};

    const static std::pair<unsigned char *, unsigned int> percussion_samples[24] = {
        {&BASS1_wav[0x3a], BASS1_wav_len - 0x3a},     // BASS1
        {&BASS1_wav[0x3a], BASS1_wav_len - 0x3a},     // BASS1
        {&BASS2_wav[0x3a], BASS2_wav_len - 0x3a},     // BASS2
        {&BASS2_wav[0x3a], BASS2_wav_len - 0x3a},     // BASS2
        {&SNARE1_wav[0x3a], SNARE1_wav_len - 0x3a},   // SNARE1
        {&SNARE1_wav[0x3a], SNARE1_wav_len - 0x3a},   // SNARE1
        {&SNARE1_wav[0x3a], SNARE1_wav_len - 0x3a},   // SNARE1
        {&SNARE1_wav[0x3a], SNARE1_wav_len - 0x3a},   // SNARE1
        {&HAT1_wav[0x3a], HAT1_wav_len - 0x3a},       // HAT1
        {&HAT1_wav[0x3a], HAT1_wav_len - 0x3a},       // HAT1
        {&HAT2_wav[0x3a], HAT2_wav_len - 0x3a},       // HAT2
        {&HAT2_wav[0x3a], HAT2_wav_len - 0x3a},       // HAT2
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
        {&SYMBAL1_wav[0x3a], SYMBAL1_wav_len - 0x3a}, // SYMBAL1
    };

    [[gnu::hot]] void Player::render(short *samples, unsigned int sampleCount)
    {
        if (!loaded)
            return;

        const unsigned int samplesPerTick = sampleRate * millisPerTick / 1000;
	const float sampPhase = 22050.f / float(sampleRate);
	//const float sampPhase = 11025.f / float(sampleRate);

        for (unsigned int i = 0; i < sampleCount; i++)
        {
            if (currTick-- <= 0)
            {
                currTick = samplesPerTick;

		        for (auto &track : tracks)
		        {
			if (notePtr < track.notes.size()) {
	                track.activeMask = track.notes[notePtr];
			} else {
			track.activeMask = 0;
			}

	                for (int n = 0; n < 24; n++)
	                {
	                    if ((track.activeMask & (1 << n)) != 0)
	                    {
	                        track.timers[n] = (&track != &tracks[3]) ? track.length : percussion_samples[n].second;
	                        track.phases[n] = 0;
	                        track.fphases[n] = 0;
	                    }
	                }
	                
                    if (&track != &tracks[3]) 
                    {
                    	// melody
                    	track.volMix = pow(10.0, clamp(((int)track.volume - 300) * 8, -10000, 0) / 2000.0);
                    } 
                    else 
                    {
                    	// percussion
                        track.volMix = pow(10.0, clamp(((int)track.volume - 300) * 8, -10000, 0) / 2000.0);
                        track.volMixLow = pow(10.0, clamp((((7 * (int)track.volume) / 10) - 300) * 8, -10000, 0) / 2000.0);
                    }

		            if ((track.activeMask & 0xff000000) != 0)
		            {
		                int pan = (panTbl[clamp(track.activeMask >> 24u, 1u, 7u)]);
		                track.volLeft = pow(10.0, clamp(pan, -10000, 0) / 2000.0);
		                track.volRight = pow(10.0, clamp(-pan, -10000, 0) / 2000.0);
		            }
                }

                notePtr++;
            }

            if (notePtr >= endTick)
            {
                notePtr = repeatTick;
            }

            samples[i * 2] = 0;
            //samples[i * 2 + 1] = 0;
            samples[i * 2 + 1] &= 0xff00;

            for (auto &track : tracks)
            {

                for (int n = 0; n < 24; n++)
                {
                    if (track.timers[n] <= 0.0f)
                        continue;

                    track.timers[n] -= sampPhase;

                    if (&track != &tracks[3])
                    {
                        const int envelope = 2 * track.envelope[64 * (track.length - int(track.timers[n])) / track.length];
                        const int octShift = (1 << track.octave);

                        track.phases[n] += (octShift * ((n < 12) ? (freqTbl[n] / 16.0f) : (freqTbl[n - 12] / 8.0f))) * sampPhase;
                        const float tp = track.phases[n] / 256.0f;
                        const int tpi = int(tp);
                        const int phf = tp - tpi;

                        const float s0 = (float)track.waveform[tpi & 0xff];
                        const float s1 = (float)track.waveform[(tpi + octShift) & 0xff];
                        const int s = int(s0 + phf * (s1 - s0)) * envelope;

                        samples[i * 2] = clamp(int(samples[i * 2]) + int(s * track.volMix * track.volLeft), -32768, 32767);
                        samples[i * 2 + 1] = clamp(int(samples[i * 2 + 1]) + int(s * track.volMix * track.volRight), -32768, 32767);
                    }
                    else
                    {
                        track.fphases[n] += sampPhase;
                        const int ph = int(track.fphases[n]);
                        const int ph2 = ph + int((ph + 1) != percussion_samples->second);
                        const float phf = track.fphases[n] - ph;

                        if (ph >= percussion_samples[n].second)
                            continue;

                        const float v0 = float(int(percussion_samples[n].first[ph]) - 0x80);
                        const float v1 = float(int(percussion_samples[n].first[ph2]) - 0x80);

                        float p = (v0 + phf * (v1 - v0)) * 256 * (((n & 1) != 0) ? track.volMixLow : track.volMix);

                        samples[i * 2] = clamp(int(samples[i * 2]) + int(p * track.volLeft), -32768, 32767);
                        samples[i * 2 + 1] = clamp(int(samples[i * 2 + 1]) + int(p * track.volRight), -32768, 32767);
                    }
                }
            }
        }
    }

    bool Player::load(std::function<bool(void *data, unsigned int len)> reader)
    {
        loaded = false;

        char magic[3];
        char ignored[5];

        if (!reader(&magic, sizeof(magic)))
            return false;
        if (!reader(&ignored, sizeof(ignored)))
            return false;
        if (!reader(&millisPerTick, sizeof(int)))
            return false;
        if (!reader(&repeatTick, sizeof(int)))
            return false;
        if (!reader(&endTick, sizeof(int)))
            return false;
        if (!reader(&records, sizeof(int)))
            return false;

        for (int i = 0; i < 3; i++)
        {
            if (!reader(&tracks[i].octave, sizeof(char)))
                return false;
            if (!reader(&ignored, sizeof(char) * 3))
                return false;
            if (!reader(&tracks[i].length, sizeof(int)))
                return false;
            if (!reader(&tracks[i].volume, sizeof(int)))
                return false;
            if (!reader(&ignored, sizeof(int)))
                return false;
            if (!reader(&ignored, sizeof(int)))
                return false;
            if (!reader(&tracks[i].waveform, sizeof(Track::waveform)))
                return false;
            if (!reader(&tracks[i].envelope, sizeof(Track::envelope)))
                return false;
        }

        if (!reader(&tracks[3].volume, sizeof(int)))
            return false;

        for (auto &track : tracks)
        {
            track.notes.resize(records);
            if (!reader(track.notes.data(), sizeof(unsigned int) * track.notes.size()))
                return false;
        }

        loaded = true;

        return true;
    }
}; // namespace piyopiyo

static piyopiyo::Player piyo;

static void dataCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount)
{
    piyo.render((short *)pOutput, frameCount);
}

int main(int argc, char *argv[])
{
    ma_device device;
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = ma_format_s16;
    deviceConfig.playback.channels = 2;
    deviceConfig.sampleRate = 48000;
    //deviceConfig.sampleRate = 22050;
    deviceConfig.dataCallback = dataCallback;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s [filename.pmd]\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f)
    {
        fprintf(stderr, "failed to open .pmd file.\n");
        return 1;
    }

    if (ma_device_init(nullptr, &deviceConfig, &device) != MA_SUCCESS)
    {
        fprintf(stderr, "failed to init device.\n");
        return 1;
    }

    piyo.sampleRate = device.sampleRate / 1.25;
    //piyo.sampleRate = ;
    bool loaded = piyo.load([&](void *data, unsigned int len) -> bool
                            { return fread(data, 1, len, f) == len; });

    if (!loaded)
    {
        fprintf(stderr, "failed to load .pmd file.\n");
        return 1;
    }

    if (ma_device_start(&device) != MA_SUCCESS)
    {
        fprintf(stderr, "failed to start device.\n");
        return 1;
    }

    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
