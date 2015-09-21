#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "modem.h"


static int call(
        struct modem *m,
        const char *cmd,
        unsigned cmdlen,
        char *retbuf,
        unsigned retmaxbytes,
        unsigned *retbytes )
{
    #define call_checkstr(str, retcode) \
        if( bytes >= sizeof(str)-1 && strncmp( &(retbuf[bytes-sizeof(str)+1]), str, sizeof(str)-1 ) == 0 ) { \
            ret = retcode; \
            bytes -= sizeof(str)-1; \
            break; \
        }

    unsigned bytes = 0;
    int ret = 0;
    if( m == NULL || cmd == NULL ) return -1;

    if( cmdlen == 0 ){
        printf( "call write [[[%s]]]\n", cmd );
        cmdlen = strlen( cmd );
    } else {
        printf( "call write %u raw bytes\n", cmdlen );
    }

    tcflush( m->fd, TCIOFLUSH );
    while( cmdlen-- ) {
        if( write( m->fd, cmd, 1 ) != 1 ) {
			perror( "call write error" );
			return -1;
		}
        cmd++;
    }
    fsync( m->fd );

    if( retmaxbytes < 1 || retbuf == NULL ) return 0;
    while( bytes < retmaxbytes ) {
        if( read( m->fd, &(retbuf[bytes]), 1 ) != 1 ) break;
        bytes++;
        call_checkstr( "\r\nOK\r\n", 1 );
        call_checkstr( "\r\nERROR\r\n", -2 );
        call_checkstr( "\r\nCONNECT\r\n", 1 );
    }
    retbuf[bytes] = 0;
    if( retbytes != NULL ) *retbytes = bytes;
	printf( "call read %d bytes (from %d) ret=%d [[[%s]]]\n", bytes, retmaxbytes, ret, retbuf );
    return ret;
}


int m_ls( struct modem *m, const char *path, void *fusebuf, fuse_fill_dir_t fusefiller )
{
    char buf[1024], scanbuf[128];
    char *bufptr = buf;
    if( m == NULL || path == NULL || fusebuf == NULL || fusefiller == NULL ) return -1;

    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"ls\",\"a:/%s\"\r\n", path );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;

	while( 1 ) {
        strsep( &bufptr, "\n" );
        if( bufptr == NULL ) break;
        scanbuf[0] = 0;
        if( sscanf( bufptr, "^SFSA: \"%s\"\r\n", scanbuf ) != 1) continue;
        if( strlen( scanbuf ) < 2 ) continue;
        scanbuf[strlen(scanbuf)-1] = 0;
        if( scanbuf[strlen(scanbuf)-1] == '/' )
            scanbuf[strlen(scanbuf)-1] = 0;
        if( fusebuf != NULL && fusefiller != NULL )
            fusefiller( fusebuf, scanbuf, NULL, 0 );
    }

    return 0;
}


int m_stat( struct modem *m, const char *path, struct stat *fusest )
{
    char buf[1024];
    char *bufptr = buf;
    if( m == NULL || path == NULL || fusest == NULL ) return -1;

    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"stat\",\"a:/%s\"\r\n", path );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) {
        int errcode;
        strsep( &bufptr, "\n" );
        if( bufptr == NULL ) return -1;
        if( sscanf( bufptr, "^SFSA: %d\r\n", &errcode ) != 1 ) return -1;
        printf( "m_stat errcode %d\n", errcode );
        switch( errcode ) {
        case RESULT_NOTFOUND:
            return ENOENT;
        default:
            return EIO;
        }
    }

    for( int i = 0; i < 6; i++ ) {
        int tmp;
        struct tm tp;
        strsep( &bufptr, "\n" );
        if( bufptr == NULL ) return -3;

        switch( i ) {
        case 1:
        case 2:
        case 3:
            if( sscanf( bufptr, "^SFSA: \"%d/%d/%d,%d:%d:%d\"\r\n",
                    &(tp.tm_year), &(tp.tm_mon), &(tp.tm_mday),
                    &(tp.tm_hour), &(tp.tm_min), &(tp.tm_sec) ) != 6 ) return -4-i;
            tp.tm_year += 100;
        }

        switch( i ) {
        case 0:
            if( sscanf( bufptr, "^SFSA: %d\r\n", &tmp ) != 1 ) return -4;
            fusest->st_size = tmp;
            break;
        case 1:
            fusest->st_atim.tv_sec = mktime( &tp );
            break;
        case 2:
            fusest->st_mtim.tv_sec = mktime( &tp );
            break;
        case 3:
            fusest->st_ctim.tv_sec = mktime( &tp );
            break;
        case 4:
            if( sscanf( bufptr, "^SFSA: %d\r\n", &tmp ) != 1 ) return -8;
            if( tmp == ATTR_FILE )
                fusest->st_mode = S_IFREG | 0666;
            else
                fusest->st_mode = S_IFDIR | 0777;
            break;
        }
    }

    return 0;
}


int m_read( struct modem *m, int fh, unsigned size, char *outbuf )
{
    char buf[MAX_SIZE+100];
    char *bufptr = buf;

    if ( m == NULL ) return -1;
    printf( "m_read %d (%u)\n", fh, size );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"read\",%d,%u\r\n", fh, size );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -2;

    strsep( &bufptr, "\n" );
    strsep( &bufptr, "\n" );
    if( bufptr == NULL ) return -3;

    if( outbuf != NULL )
        memcpy( outbuf, bufptr, size );

    return 0;
}


int m_seek( struct modem *m, int fh, int offset, enum seek flags )
{
    char buf[100];
    if( m == NULL ) return -1;
    printf( "m_seek %d to %d\n", fh, offset );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"seek\",%d,%d,%d\r\n", fh, offset, flags );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -2;
    return 0;
}


int m_open( struct modem *m, const char *path, enum flags flags )
{
    int fh = -1;
    char buf[100];
    char *bufptr = buf;

    if( m == NULL || path == NULL ) return -1;
    printf( "m_open %s\n", path );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"open\",\"a:/%s\",%u\r\n", path, flags );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;

    strsep( &bufptr, "\n" );
    if( bufptr == NULL ) return -2;
    if( sscanf( bufptr, "^SFSA: %d,0\r\n", &fh ) != 1 ) {
        printf( "m_open sscanf failed\n" );
        return -3;
    }
    printf( "m_open ret %d\n", fh );
    return fh;
}


int m_close( struct modem *m, int fh )
{
    char buf[100];
    if( m == NULL ) return -1;
    printf( "m_close #%d\n", fh );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"close\",%d\r\n", fh );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -2;
    return 0;
}


int m_write( struct modem *m, int fh, unsigned size, const char *data )
{
    char buf[size+100];
    if( m == NULL || data == NULL ) return -1;
    printf( "m_write %u bytes to #%d\n", size, fh );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"write\",%d,%u\r\n", fh, size );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) < 0 ) return -2;
    if( call( m, data, size, buf, size, NULL ) < 0 ) return -3;
    return (int)size;
}

int m_remove( struct modem *m, const char *path )
{
    char buf[100];
    if( m == NULL || path == NULL ) return -1;
    printf( "m_remove %s\n", path );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"remove\",\"a:/%s\"\r\n", path );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;
    return 0;
}

int m_mkdir( struct modem *m, const char *path )
{
    char buf[100];
    if( m == NULL || path == NULL ) return -1;
    printf( "m_mkdir %s\n", path );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"mkdir\",\"a:/%s\"\r\n", path );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;
    return 0;
}


int m_rmdir( struct modem *m, const char *path )
{
    char buf[100];
    if( m == NULL || path == NULL ) return -1;
    printf( "m_rmdir %s\n", path );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"rmdir\",\"a:/%s\"\r\n", path );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;
    return 0;
}


int m_copy( struct modem *m, const char *srcpath, const char *dstpath )
{
    char buf[100];
    if( m == NULL || srcpath == NULL || dstpath == NULL ) return -1;
    printf( "m_copy %s to %s\n", srcpath, dstpath );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"copy\",\"a:/%s\",\"a:/%s\"\r\n", srcpath, dstpath );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;
    return 0;
}


int m_rename( struct modem *m, const char *path, const char *newname )
{
    char buf[100];
    if( m == NULL || path == NULL || newname == NULL ) return -1;
    printf( "m_rename %s to %s\n", path, newname );
    snprintf( buf, sizeof(buf)-1, "AT^SFSA=\"rename\",\"a:/%s\",\"%s\"\r\n", path, newname );
    if( call( m, buf, 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;
    return 0;
}


int m_gstat( struct modem *modem, struct statvfs *st )
{
    char buf[200];
    char *bufptr = buf;

    if( modem == NULL || st == NULL ) return -1;
    printf( "m_gstat\n" );
    if( call( modem, "AT^SFSA=\"gstat\"\r\n", 0, buf, sizeof(buf), NULL ) <= 0 ) return -1;

    for( int i = 0; i < 2; i++ ) {
        unsigned tmp;
        strsep( &bufptr, "\n" );
        if( bufptr == NULL ) return -2;
        switch( i ) {
        case 0:
            if( sscanf( bufptr, "^SFSA: %u\r\n", &tmp ) != 1) return -3;
            st->f_blocks = tmp;
            break;
        case 1:
            if( sscanf( bufptr, "^SFSA: %u\r\n", &tmp ) != 1) return -4;
            st->f_bfree = tmp;
            st->f_bavail = tmp;
            break;
        }
    }
    st->f_bsize = 1;
    st->f_namemax = 64;
    return 0;
}

int m_init( struct modem *m, const char *tty )
{
    char buf[100];
    if ( m == NULL )
        return -1;
    if ( (m->fd = open(tty, O_RDWR | O_NOCTTY | O_SYNC)) == -1 ) {
        perror( "m_init: cannot open device" );
        return -1;
    }
    tcgetattr( m->fd, &(m->termios) );
    cfmakeraw( &(m->termios) );
    m->termios.c_cc[VTIME] = 50;
	m->termios.c_cc[VMIN] = 0;
    /* set baud rates */
    cfsetispeed( &(m->termios), B115200 );
    cfsetospeed( &(m->termios), B115200 );
    /* enable the receiver and set local mode */
    m->termios.c_cflag |= (CLOCAL | CREAD);
    /* set no parity, stop bits, data bits */
    m->termios.c_cflag &= ~(unsigned)PARENB;
    m->termios.c_cflag &= ~(unsigned)CSTOPB;
    /* set character size as 8 bits */
    m->termios.c_cflag &= ~(unsigned)CSIZE;
    m->termios.c_cflag |= CS8;
    /* Raw input mode, sends the raw and unprocessed data  ( send as it is) */
    m->termios.c_lflag &= ~(unsigned)(ICANON | ECHO | ISIG);
    /* Raw output mode, sends the raw and unprocessed data  ( send as it is). */
    m->termios.c_oflag = ~(unsigned)OPOST;
    tcflush( m->fd, TCOFLUSH );
    tcsetattr( m->fd, TCSANOW, &(m->termios) );
    if( call( m, "ATE0\r\n", 0, buf, sizeof(buf), NULL ) <= 0 )
        return -1;
    return 0;
}


int m_deinit( struct modem *m )
{
    close( m->fd );
    return 0;
}
