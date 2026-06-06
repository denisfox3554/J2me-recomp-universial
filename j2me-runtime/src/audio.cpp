#include "j2me_internal.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <SDL2/SDL.h>

#ifdef HAVE_SDL2_MIXER
#include <SDL2/SDL_mixer.h>
#endif

bool J2MEAudio::init() {
#ifdef HAVE_SDL2_MIXER
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        std::cerr << "[audio] Mix_OpenAudio: " << Mix_GetError() << "\n";
        return false;
    }
    Mix_AllocateChannels(16);
    return true;
#else
    std::cout << "[audio] SDL2_mixer not available — audio disabled\n";
    return false;
#endif
}

void J2MEAudio::destroy() {
#ifdef HAVE_SDL2_MIXER
    for (auto& [id, p] : players_) { p->close(); delete p; }
    players_.clear();
    Mix_CloseAudio();
#endif
}

J2MEPlayer* J2MEAudio::create_player(const uint8_t* data, size_t size,
                                      const char* content_type)
{
    ContentType ct = ContentType::UNKNOWN;
    if (content_type) {
        if (strstr(content_type, "wav"))  ct = ContentType::WAV;
        else if (strstr(content_type, "mp3") || strstr(content_type, "mpeg")) ct = ContentType::MP3;
        else if (strstr(content_type, "tone")) ct = ContentType::TONE_SEQUENCE;
    }

    auto* p = new J2MEPlayer();
    p->id = next_id_++;
    if (data && size > 0)
        p->realize(data, size, ct);
    players_[p->id] = p;
    return p;
}

void J2MEAudio::release_player(J2MEPlayer* p) {
    if (!p) return;
    p->close();
    players_.erase(p->id);
    delete p;
}

void J2MEAudio::play_tone(int note, int duration_ms, int volume) {
#ifdef HAVE_SDL2_MIXER
    // Simple tone via SDL2: generate a sine PCM chunk on the fly
    int freq = static_cast<int>(440.0 * std::pow(2.0, (note - 69) / 12.0));
    int samples = 44100 * duration_ms / 1000;
    std::vector<int16_t> buf(samples);
    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / 44100.0;
        buf[i] = static_cast<int16_t>(
            32767.0 * (volume / 100.0) * std::sin(2.0 * M_PI * freq * t));
    }
    Mix_Chunk* chunk = Mix_QuickLoad_RAW(
        reinterpret_cast<uint8_t*>(buf.data()),
        static_cast<uint32_t>(buf.size() * sizeof(int16_t)));
    if (chunk) {
        Mix_PlayChannel(-1, chunk, 0);
        // chunk will leak here; for a real impl, track and free after playback
    }
#endif
}

// ── J2MEPlayer ────────────────────────────────────────────────────────────────
bool J2MEPlayer::realize(const uint8_t* data, size_t size, ContentType ct) {
    type  = ct;
    state = PlayerState::REALIZED;
#ifdef HAVE_SDL2_MIXER
    SDL_RWops* rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw) return false;
    if (ct == ContentType::WAV) {
        chunk = Mix_LoadWAV_RW(rw, 1);
        return chunk != nullptr;
    } else if (ct == ContentType::MP3) {
        music = Mix_LoadMUS_RW(rw, 1);
        return music != nullptr;
    }
#endif
    return true;
}

bool J2MEPlayer::prefetch() {
    if (state == PlayerState::REALIZED) state = PlayerState::PREFETCHED;
    return true;
}

bool J2MEPlayer::start() {
    if (state == PlayerState::UNREALIZED) return false;
    state = PlayerState::STARTED;
#ifdef HAVE_SDL2_MIXER
    int loops = (loop_count == -1) ? -1 : loop_count - 1;
    if (chunk) {
        channel = Mix_PlayChannel(-1, chunk, loops);
        if (volume != 1.0f)
            Mix_Volume(channel, static_cast<int>(MIX_MAX_VOLUME * volume));
    } else if (music) {
        Mix_PlayMusic(music, loops);
        Mix_VolumeMusic(static_cast<int>(MIX_MAX_VOLUME * volume));
    }
#endif
    return true;
}

void J2MEPlayer::stop() {
    state = PlayerState::PREFETCHED;
#ifdef HAVE_SDL2_MIXER
    if (chunk && channel >= 0) Mix_HaltChannel(channel);
    if (music) Mix_HaltMusic();
#endif
}

void J2MEPlayer::close() {
    stop();
    state = PlayerState::CLOSED;
#ifdef HAVE_SDL2_MIXER
    if (chunk) { Mix_FreeChunk(chunk); chunk = nullptr; }
    if (music) { Mix_FreeMusic(music); music = nullptr; }
#endif
}

void J2MEPlayer::set_loop(int count) { loop_count = count; }
void J2MEPlayer::set_volume(float v) {
    volume = v;
#ifdef HAVE_SDL2_MIXER
    if (chunk && channel >= 0) Mix_Volume(channel, static_cast<int>(MIX_MAX_VOLUME * v));
    if (music) Mix_VolumeMusic(static_cast<int>(MIX_MAX_VOLUME * v));
#endif
}
