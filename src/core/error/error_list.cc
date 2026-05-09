#include "error_list.hh"

const char *error_names[] = {
    "OK",  // OK

    // generic
    "Failed",             // FAILED
    "Unavailable",        // ERR_UNAVAILABLE
    "Unconfigured",       // ERR_UNCONFIGURED
    "Unauthorized",       // ERR_UNAUTHORIZED
    "Invalid parameter",  // ERR_INVALID_PARAMETER
    "Invalid data",       // ERR_INVALID_DATA
    "Already exists",     // ERR_ALREADY_EXISTS
    "Does not exist",     // ERR_DOES_NOT_EXIST
    "Timeout",            // ERR_TIMEOUT
    "Busy",               // ERR_BUSY
    "Interrupted",        // ERR_INTERRUPTED
    "Cannot create",      // ERR_CANT_CREATE

    // memory
    "Out of memory",         // ERR_OUT_OF_MEMORY
    "Out of video memory",   // ERR_OUT_OF_VIDEO_MEMORY
    "Memory leak detected",  // ERR_MEMORY_LEAK
    "Buffer overflow",       // ERR_BUFFER_OVERFLOW

    // file system
    "File not found",             // ERR_FILE_NOT_FOUND
    "File: Bad drive",            // ERR_FILE_BAD_DRIVE
    "File: Bad path",             // ERR_FILE_BAD_PATH
    "File: Permission denied",    // ERR_FILE_NO_PERMISSION
    "File already in use",        // ERR_FILE_ALREADY_IN_USE
    "Cannot open file",           // ERR_FILE_CANT_OPEN
    "Cannot create file",         // ERR_FILE_CANT_CREATE
    "Cannot write file",          // ERR_FILE_CANT_WRITE
    "Cannot read file",           // ERR_FILE_CANT_READ
    "Unrecognized file format",   // ERR_FILE_UNRECOGNIZED
    "Corrupted file",             // ERR_FILE_CORRUPT
    "File missing dependencies",  // ERR_FILE_MISSING_DEPENDENCIES
    "End of file",                // ERR_FILE_EOF

    // resource system
    "Resource not found",         // ERR_RESOURCE_NOT_FOUND
    "Corrupted resource",         // ERR_RESOURCE_CORRUPT
    "Resource version mismatch",  // ERR_RESOURCE_VERSION_MISMATCH

    // link failed
    "Link failed",  // ERR_LINK_FAILED
};

static_assert(sizeof(error_names) / sizeof(error_names[0]) == ERR_MAX,
              "Error string table size mismatch");
