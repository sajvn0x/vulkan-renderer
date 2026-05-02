#include "error_list.hh"

const char *error_names[] = {
    "OK",                             // OK
    "Failed",                         // FAILED
    "Unavailable",                    // ERR_UNAVAILABLE
    "Out of memory",                  // ERR_OUT_OF_MEMORY
    "File not found",                 // ERR_FILE_NOT_FOUND
    "File: Bad drive",                // ERR_FILE_BAD_DRIVE
    "File: Bad path",                 // ERR_FILE_BAD_PATH
    "File: Permission denied",        // ERR_FILE_NO_PERMISSION
    "File already in use",            // ERR_FILE_ALREADY_IN_USE
    "Can't open file",                // ERR_FILE_CANT_OPEN
    "Can't write file",               // ERR_FILE_CANT_WRITE
    "Can't read file",                // ERR_FILE_CANT_READ
    "File unrecognized",              // ERR_FILE_UNRECOGNIZED
    "File corrupt",                   // ERR_FILE_CORRUPT
    "Missing dependencies for file",  // ERR_FILE_MISSING_DEPENDENCIES
    "End of file",                    // ERR_FILE_EOF
    "Link failed",                    // ERR_LINK_FAILED
};
