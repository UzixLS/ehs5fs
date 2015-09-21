#pragma once
#include <fuse.h>
#include <termios.h>

#define CALL_TIMEOUT 2000
#define MAX_OPENS 24
#define MAX_SIZE 1500


enum flags {
    FLAG_APPEND = 4,
    FLAG_CREATE = 8,
    FLAG_TRUNCATE = 16,
};

enum seek {
    SEEK_FROMBEGIN = 0,
    SEEK_FROMCURRENT = 1,
    SEEK_FROMEND = 2,
};

enum attrs {
    ATTR_FILE = 0,
    ATTR_VOL = 8,
    ATTR_DIR = 16
};

enum results {
    RESULT_SUCCESS = 0,
    RESULT_NOTFOUND = 2,

};

struct modem {
    int fd;
    struct termios termios;
};


int m_close(  struct modem *m, int fh );
int m_copy(   struct modem *m, const char *srcpath, const char *dstpath );
int m_crc(    struct modem *m, const char *path );
int m_gstat(  struct modem *m, struct statvfs *st );
int m_ls(     struct modem *m, const char *path, void *fusebuf, fuse_fill_dir_t fusefiller );
int m_mkdir(  struct modem *m, const char *path );
int m_open(   struct modem *m, const char *path, enum flags flags );
int m_read(   struct modem *m, int fh, unsigned size, char *outbuf );
int m_remove( struct modem *m, const char *path );
int m_rename( struct modem *m, const char *path, const char *newname );
int m_rmdir(  struct modem *m, const char *path );
int m_seek(   struct modem *m, int fh, int offset, enum seek flags );
int m_stat(   struct modem *m, const char *path, struct stat *fusest );
int m_write(  struct modem *m, int fh, unsigned size, const char *data );

int m_init(   struct modem *m, const char *tty );
int m_deinit( struct modem *m );
