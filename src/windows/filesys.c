#include "main.h"

#include "../main.h"

static FILE* get_file(wchar_t path[UTOX_FILE_NAME_LENGTH], UTOX_FILE_OPTS opts) {
    // assert(UTOX_FILE_NAME_LENGTH <= (32,767 wide characters) );
    DWORD rw  = 0;
    char mode[4] = {0};
    DWORD create = OPEN_ALWAYS;

    if (opts & UTOX_FILE_OPTS_READ) {
        rw |= GENERIC_READ;
        mode[0] = 'r';
    }

    if (opts & UTOX_FILE_OPTS_APPEND) {
        rw |= GENERIC_WRITE;
        mode[0] = 'a';
    } else if (opts & UTOX_FILE_OPTS_WRITE) {
        rw |= GENERIC_WRITE;
        mode[0] = 'w';
        create = CREATE_ALWAYS;
    }

    mode[1] = 'b';
    if ((opts & (UTOX_FILE_OPTS_WRITE | UTOX_FILE_OPTS_APPEND)) && (opts & UTOX_FILE_OPTS_READ)) {
        mode[2] = '+';
    }

    HANDLE WINAPI winFile = CreateFileW(path, rw, FILE_SHARE_READ, NULL,
                                        create, FILE_ATTRIBUTE_NORMAL, NULL);

    return _fdopen(_open_osfhandle((intptr_t)winFile, 0), mode);
}

static bool make_dir(wchar_t path[UTOX_FILE_NAME_LENGTH]) {
    return SHCreateDirectoryExW(NULL, path, NULL); // Fall back to the default permissions on Windows
}

FILE *native_get_file(char *name, size_t *size, UTOX_FILE_OPTS opts) {
    char path[UTOX_FILE_NAME_LENGTH] = { 0 };

    if (settings.portable_mode) {
        strcpy((char *)path, portable_mode_save_path);
    } else {
        if (FAILED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path))) {
            if (FAILED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path))) {
                strcpy(path, portable_mode_save_path);
            }
        }
    }

    if (opts > UTOX_FILE_OPTS_DELETE) {
        debug_error("NATIVE:\tDon't call native_get_file with UTOX_FILE_OPTS_DELETE in combination with other options.\n");
        return NULL;
    }

    if (strlen(path) + strlen("\\Tox\\") + strlen(name) >= UTOX_FILE_NAME_LENGTH) {
        debug_error("NATIVE:\tLoad directory name too long\n");
        return NULL;
    }

    if (opts & UTOX_FILE_OPTS_WRITE || opts & UTOX_FILE_OPTS_MKDIR) {
        wchar_t make_path[UTOX_FILE_NAME_LENGTH] = { 0 }; // I still don't trust windows
        MultiByteToWideChar(CP_UTF8, 0, path, strlen(path), make_path, UTOX_FILE_NAME_LENGTH);
        make_dir(make_path);
    }

    snprintf(path + strlen(path), UTOX_FILE_NAME_LENGTH - strlen(path), "\\Tox\\%s", name);

    wchar_t wide[UTOX_FILE_NAME_LENGTH] = { 0 };
    MultiByteToWideChar(CP_UTF8, 0, path, strlen(path), wide, UTOX_FILE_NAME_LENGTH);

    if (opts == UTOX_FILE_OPTS_DELETE) {
        if (!DeleteFile(path)) {
            debug_error("NATIVE:\tCould not delete file: %s - Error: %d\n", path, GetLastError());
        }
        return NULL;
    }

    FILE *fp = get_file(wide, opts);

    if (fp == NULL) {
        debug_error("Windows:\tCould not open %s\n", path);
        return NULL;
    }

    if (size != NULL && opts & UTOX_FILE_OPTS_READ) {
        fseek(fp, 0, SEEK_END);
        *size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }

    return fp;
}
