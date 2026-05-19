#pragma once

#include <cstdint>
#include <filesystem>

#include "core/containers.hh"
#include "core/error/error_list.hh"

using Path = std::filesystem::path;

struct SpirvFileResult {
    bool success;
    Vector<uint32_t> content;
    String error;
    uint32_t size;
};

struct FileSystem {
    /* assets */
    static Path ASSET_PATH;
    static Path SHADER_PATH;

    static Error set_asset_path(Path path);
    static Error set_shader_path(Path path);

    static const Vector<Path> find_shader_files_by_name(const String& name);
    static SpirvFileResult read_spirv_file(const Path& path);
};
