#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// Audio — wraps SDL2_mixer for MIDP 2.0 Manager/Player semantics.
// Supports tone sequences and WAV/MP3 clip playback.
// ─────────────────────────────────────────────────────────────────────────────

struct _Mix_Music;
struct Mix_Chunk;

enum class PlayerState { UNREALIZED, REALIZED, PREFETCHED, STARTED, CLOSED };
enum class ContentType  { TONE_SEQUENCE, WAV, MP3, UNKNOWN };

struct J2MEPlayer {
    int          id       = -1;
    PlayerState  state    = PlayerState::UNREALIZED;
    ContentType  type     = ContentType::UNKNOWN;
    int          loop_count = 1;   // -1 = infinite
    bool         muted    = false;
    float        volume   = 1.0f;

    _Mix_Music*  music    = nullptr;  // for MP3/OGG
    Mix_Chunk*   chunk    = nullptr;  // for WAV/SFX
    int          channel  = -1;

    bool realize(const uint8_t* data, size_t size, ContentType ct);
    bool prefetch();
    bool start();
    void stop();
    void close();
    void set_loop(int count);
    void set_volume(float v);  // 0.0 – 1.0
};

struct J2MEAudio {
    bool init();
    void destroy();

    // Manager.createPlayer()
    J2MEPlayer* create_player(const uint8_t* data, size_t size,
                               const char* content_type);
    void release_player(J2MEPlayer* p);

    // Tone synthesis (Manager.playTone)
    void play_tone(int note, int duration_ms, int volume);

private:
    std::unordered_map<int, J2MEPlayer*> players_;
    int next_id_ = 0;
};
