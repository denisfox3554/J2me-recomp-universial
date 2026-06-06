#include "j2me_internal.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

// ── File format: simple binary ────────────────────────────────────────────────
// [uint32 magic=0x524D5321]
// [uint32 num_records]
// for each record:
//   [int32  id]
//   [uint32 size]
//   [uint8  data * size]

static constexpr uint32_t RMS_MAGIC = 0x524D5321u; // 'RMS!'

bool RecordStore::load() {
    if (!fs::exists(file_path)) return true;  // empty store is OK
    std::ifstream f(file_path, std::ios::binary);
    if (!f) return false;

    uint32_t magic = 0, count = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != RMS_MAGIC) {
        std::cerr << "[rms] Bad magic in: " << file_path << "\n";
        return false;
    }
    f.read(reinterpret_cast<char*>(&count), 4);

    for (uint32_t i = 0; i < count; ++i) {
        int32_t  id   = 0;
        uint32_t size = 0;
        f.read(reinterpret_cast<char*>(&id),   4);
        f.read(reinterpret_cast<char*>(&size),  4);
        RMSRecord rec;
        rec.id   = id;
        rec.data.resize(size);
        f.read(reinterpret_cast<char*>(rec.data.data()), size);
        records[id] = std::move(rec);
        if (id >= next_id) next_id = id + 1;
    }
    return true;
}

bool RecordStore::save() {
    fs::create_directories(fs::path(file_path).parent_path());
    std::ofstream f(file_path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    uint32_t magic = RMS_MAGIC;
    uint32_t count = static_cast<uint32_t>(records.size());
    f.write(reinterpret_cast<char*>(&magic), 4);
    f.write(reinterpret_cast<char*>(&count), 4);

    for (auto& [id, rec] : records) {
        int32_t  rid  = rec.id;
        uint32_t size = static_cast<uint32_t>(rec.data.size());
        f.write(reinterpret_cast<char*>(&rid),  4);
        f.write(reinterpret_cast<char*>(&size), 4);
        f.write(reinterpret_cast<char*>(rec.data.data()), size);
    }
    return true;
}

int RecordStore::add_record(const uint8_t* data, int offset, int num_bytes) {
    RMSRecord rec;
    rec.id = next_id++;
    rec.data.assign(data + offset, data + offset + num_bytes);
    records[rec.id] = std::move(rec);
    dirty = true;
    save();
    return next_id - 1;
}

void RecordStore::set_record(int record_id, const uint8_t* data,
                              int offset, int num_bytes) {
    if (records.find(record_id) == records.end())
        throw std::runtime_error("InvalidRecordIDException");
    records[record_id].data.assign(data + offset, data + offset + num_bytes);
    dirty = true;
    save();
}

void RecordStore::delete_record(int record_id) {
    records.erase(record_id);
    dirty = true;
    save();
}

std::vector<uint8_t> RecordStore::get_record(int record_id) const {
    auto it = records.find(record_id);
    if (it == records.end())
        throw std::runtime_error("InvalidRecordIDException");
    return it->second.data;
}

int RecordStore::get_record_size(int record_id) const {
    auto it = records.find(record_id);
    if (it == records.end()) return -1;
    return static_cast<int>(it->second.data.size());
}

int RecordStore::get_num_records() const {
    return static_cast<int>(records.size());
}

// ── J2MORMS ───────────────────────────────────────────────────────────────────
bool J2MORMS::init(const std::string& save_directory) {
    save_dir = save_directory;
    fs::create_directories(save_dir);
    return true;
}

RecordStore* J2MORMS::open_record_store(const std::string& name,
                                         bool create_if_necessary) {
    auto it = open_stores_.find(name);
    if (it != open_stores_.end()) return it->second;

    auto* rs = new RecordStore();
    rs->name      = name;
    rs->file_path = save_dir + "/" + name + ".rms";

    if (!rs->load()) {
        delete rs;
        return nullptr;
    }
    if (!create_if_necessary && rs->records.empty() &&
        !fs::exists(rs->file_path)) {
        delete rs;
        return nullptr;
    }
    open_stores_[name] = rs;
    return rs;
}

void J2MORMS::close_record_store(RecordStore* rs) {
    if (!rs) return;
    if (rs->dirty) rs->save();
    open_stores_.erase(rs->name);
    delete rs;
}

void J2MORMS::delete_record_store(const std::string& name) {
    auto it = open_stores_.find(name);
    if (it != open_stores_.end()) {
        delete it->second;
        open_stores_.erase(it);
    }
    fs::remove(save_dir + "/" + name + ".rms");
}

// ── Public API wrappers (declared in j2me_runtime.h) ─────────────────────────
// g_runtime->rms is the global J2MORMS instance
extern struct J2MERuntime* g_runtime;

RecordStore* j2me_open_record_store(const char* name, bool create) {
    if (!g_runtime) return nullptr;
    return g_runtime->rms.open_record_store(name, create);
}
void j2me_close_record_store(RecordStore* rs) {
    if (!g_runtime) return;
    g_runtime->rms.close_record_store(rs);
}
