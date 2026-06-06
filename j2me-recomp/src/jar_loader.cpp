#include "jar_loader.h"
#include "class_parser.h"
#include <zlib.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>

// ── Minimal ZIP reader (JAR = ZIP) ────────────────────────────────────────────
// We walk the Central Directory and decompress each .class entry with zlib.

#pragma pack(push,1)
struct ZipLocalHeader {
    uint32_t sig;         // 0x04034b50
    uint16_t ver_needed;
    uint16_t flags;
    uint16_t compression; // 0=store, 8=deflate
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t name_len;
    uint16_t extra_len;
};
struct ZipCDEntry {
    uint32_t sig;         // 0x02014b50
    uint16_t ver_made;
    uint16_t ver_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time;
    uint16_t mod_date;
    uint32_t crc32;
    uint32_t comp_size;
    uint32_t uncomp_size;
    uint16_t name_len;
    uint16_t extra_len;
    uint16_t comment_len;
    uint16_t disk_start;
    uint16_t int_attr;
    uint32_t ext_attr;
    uint32_t local_offset;
};
struct ZipEOCD {
    uint32_t sig;         // 0x06054b50
    uint16_t disk_num;
    uint16_t cd_disk;
    uint16_t cd_count_disk;
    uint16_t cd_count;
    uint32_t cd_size;
    uint32_t cd_offset;
    uint16_t comment_len;
};
#pragma pack(pop)

static bool inflate_entry(const uint8_t* comp, uint32_t comp_size,
                           uint8_t* out, uint32_t uncomp_size) {
    z_stream zs{};
    zs.next_in   = const_cast<uint8_t*>(comp);
    zs.avail_in  = comp_size;
    zs.next_out  = out;
    zs.avail_out = uncomp_size;
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) return false;
    int r = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    return r == Z_STREAM_END;
}

bool JarLoader::load(const std::string& jar_path) {
    std::ifstream f(jar_path, std::ios::binary | std::ios::ate);
    if (!f) { std::cerr << "[JarLoader] Cannot open: " << jar_path << "\n"; return false; }

    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);

    // Find EOCD
    size_t eocd_off = std::string::npos;
    for (size_t i = sz >= 22 ? sz - 22 : 0; i < sz; ++i) {
        if (buf[i]==0x50 && buf[i+1]==0x4b && buf[i+2]==0x05 && buf[i+3]==0x06) {
            eocd_off = i; break;
        }
    }
    if (eocd_off == std::string::npos) {
        std::cerr << "[JarLoader] EOCD not found\n"; return false;
    }

    auto* eocd = reinterpret_cast<ZipEOCD*>(&buf[eocd_off]);
    size_t cd_off = eocd->cd_offset;

    for (uint16_t i = 0; i < eocd->cd_count; ++i) {
        auto* cd = reinterpret_cast<ZipCDEntry*>(&buf[cd_off]);
        if (cd->sig != 0x02014b50) break;

        std::string name(reinterpret_cast<char*>(&buf[cd_off + sizeof(ZipCDEntry)]),
                         cd->name_len);

        if (name.size() > 6 && name.substr(name.size()-6) == ".class") {
            // Read local header to find data offset
            size_t lh_off = cd->local_offset;
            auto* lh = reinterpret_cast<ZipLocalHeader*>(&buf[lh_off]);
            size_t data_off = lh_off + sizeof(ZipLocalHeader)
                            + lh->name_len + lh->extra_len;

            std::vector<uint8_t> class_data(cd->uncomp_size);
            if (cd->compression == 0) {
                std::memcpy(class_data.data(), &buf[data_off], cd->uncomp_size);
            } else if (cd->compression == 8) {
                if (!inflate_entry(&buf[data_off], cd->comp_size,
                                   class_data.data(), cd->uncomp_size)) {
                    std::cerr << "[JarLoader] inflate failed: " << name << "\n";
                    goto next_entry;
                }
            }

            {
                ClassParser parser;
                ClassFile cls = parser.parse(class_data);
                if (!cls.name.empty())
                    classes_.push_back(std::move(cls));
            }
        }
        next_entry:
        cd_off += sizeof(ZipCDEntry) + cd->name_len + cd->extra_len + cd->comment_len;
    }

    std::cout << "[JarLoader] Loaded " << classes_.size() << " class(es)\n";
    return true;
}
