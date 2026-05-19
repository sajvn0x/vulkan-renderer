#include "fs.hh"

#include <fstream>

static const char* SPIRV_EXTENSION = ".spv";

Path FileSystem::ASSET_PATH = "assets";
Path FileSystem::SHADER_PATH = FileSystem::ASSET_PATH / "shaders";

Error FileSystem::set_asset_path(Path path) {
    ASSET_PATH = path;
    return OK;
}

Error FileSystem::set_shader_path(Path path) {
    SHADER_PATH = path;
    return OK;
}

SpirvFileResult FileSystem::read_spirv_file(const Path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        return {false, {}, "Failed to open file: " / path};
    }

    size_t size = file.tellg();
    if (size % 4 != 0) {
        return {false, {}, "Invalid SPIR-V file size (not aligned to 4 bytes): " / path, 0};
    }

    Vector<uint32_t> buffer(size / 4);

    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    if (!file) {
        return {false, {}, "Failed to read full SPIR-V file: " / path, 0};
    }

    return {true, std::move(buffer), "", static_cast<uint32_t>(size)};
}

const Vector<Path> FileSystem::find_shader_files_by_name(const String& name) {
    Vector<Path> shader_paths;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(SHADER_PATH)) {
        if (!entry.is_regular_file()) continue;

        Path path = entry.path();

        if (path.extension() == SPIRV_EXTENSION) {
            // name must be a exact match
            if (path.stem().stem() == name) {
                shader_paths.push_back(path);
            }
        }
    }

    return shader_paths;
}
