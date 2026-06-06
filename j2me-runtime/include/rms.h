#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// RMS — Record Management System (javax.microedition.rms)
// Backed by a flat binary file on the host filesystem.
// ─────────────────────────────────────────────────────────────────────────────

struct RMSRecord {
    int                  id   = 0;
    std::vector<uint8_t> data;
};

struct RecordStore {
    std::string name;
    std::string file_path;
    std::unordered_map<int, RMSRecord> records;
    int next_id = 1;
    bool dirty  = false;

    // MIDP API surface
    int  add_record(const uint8_t* data, int offset, int num_bytes);
    void set_record(int record_id, const uint8_t* data, int offset, int num_bytes);
    void delete_record(int record_id);
    std::vector<uint8_t> get_record(int record_id) const;
    int  get_record_size(int record_id) const;
    int  get_num_records() const;
    int  get_next_record_id() const { return next_id; }

    bool load();
    bool save();
};

// Global RMS manager
struct J2MORMS {
    std::string save_dir;  // set by j2me_runtime_init

    bool init(const std::string& save_directory);

    RecordStore* open_record_store(const std::string& name,
                                   bool create_if_necessary);
    void close_record_store(RecordStore* rs);
    void delete_record_store(const std::string& name);

private:
    std::unordered_map<std::string, RecordStore*> open_stores_;
};
