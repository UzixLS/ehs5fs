#include <fuse.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "main.h"
#include "fuse.h"
#include "modem.h"


static int fs_getattr( const char *path, struct stat *st )
{
    int ret;
	printf( "fs_getattr %s\n", path );

    if( 0 == strcmp(path, "/") ) {
        st->st_mode = S_IFDIR | 0777;
        st->st_nlink = 2;
    } else {
        st->st_nlink = 1;
        ret = m_stat( &M, path, st );
        if( ret == ENOENT ) return -ret;
        if( ret < 0 ) {
            printf( "fs_getattr: tc_stat err ret=%d'\n", ret );
            return 0;
        }
    }
    return 0;
}


static int fs_readdir(
        const char *path,
        void *buf,
        fuse_fill_dir_t filler,
        off_t offset,
        struct fuse_file_info *fi )
{
	printf( "fs_readdir %s\n", path );
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    m_ls( &M, path, buf, filler );
    return 0;
}


static int fs_read(
        const char *path,
        char *buf,
        size_t size,
        off_t offset,
        struct fuse_file_info *fi )
{
    int ret = 0;
    int readchunk = min(MAX_SIZE, (int)size);
    int readed = 0;
	int left = 0;
    struct stat st;

	printf( "fs_read %s (%u+%lld)\n", path, size, offset);
    int fh = m_open( &M, path, 0 );
    if( fh < 0 ) return -1;
    if( m_seek( &M, fh, (int)offset, 0) < 0 ) { ret = -EIO; goto fs_read_exit; }
    if( m_stat( &M, path, &st ) < 0 )         { ret = -EIO; goto fs_read_exit; }
	left = min( (int)size, (int)st.st_size - (int)offset );

    while( left > 0 ) {
        int bytes = min( (int)size, left );
		if( bytes > readchunk ) bytes = readchunk;
        if( m_read( &M, fh, (unsigned)bytes, buf + readed ) < 0 ) {
            ret = -1; goto fs_read_exit;
        }
        readed += bytes;
		left -= bytes;
		printf( "fs_read readed %d bytes from %d (%d left)\n", readed, size, left );
		usleep( 350 );

    }
    ret = readed;

fs_read_exit:
    m_close( &M, fh );
    printf( "fs_read ret=%d\n", ret );
    return ret;
}


static int fs_write(
        const char *path,
        const char *buf,
        size_t size,
        off_t offset,
        struct fuse_file_info *fi )
{
    int ret = 0;
    int left = (int)size;
    int written = 0;

	printf( "fs_write %s (%u+%lld)\n", path, size, offset );
    int fh = m_open( &M, path, FLAG_APPEND );
    if( fh < 0 ) return -1;
    if( m_seek( &M, fh, (int)offset, 0) < 0 ) { ret = -EIO; goto fs_write_exit; }

    while( left > 0 ) {
        int bytes = min( MAX_SIZE, left );
        if( m_write( &M, fh, (unsigned)bytes, buf + written ) < 0 ) {
            ret = -EIO; goto fs_write_exit;
        }
        written += bytes;
        left -= bytes;
        printf( "fs_write writed %d bytes from %d (%d left)\n", written, size, left );
    }
    ret = written;

fs_write_exit:
    m_close( &M, fh );
    printf( "fs_write ret=%d\n", ret );
    return ret;
}


static int fs_mknod(
        const char *path,
        mode_t mode,
        dev_t dev )
{
    int fh = m_open( &M, path, FLAG_CREATE );
    if( fh < 0 ) return -EIO;
    m_close( &M, fh );
    return 0;
}


static int fs_mkdir( const char *path, mode_t mode )
{
    if( m_mkdir( &M, path ) < 0 )
        return -EIO;
    else
        return 0;
}

static int fs_rmdir( const char *path )
{
    if( m_rmdir( &M, path ) < 0 )
        return -EIO;
    else
        return 0;
}


static int fs_unlink( const char *path )
{
    if( m_remove( &M, path ) < 0 )
        return -EIO;
    else
        return 0;
}


static int fs_truncate( const char *path, off_t offset )
{
    if( offset != 0 ) return -EFAULT;
    int fh = m_open( &M, path, FLAG_TRUNCATE );
    if( fh < 0 ) return -EIO;
    m_close( &M, fh );
    return 0;
}


static int fs_rename( const char *oldpath, const char *newpath )
{
    int saferename = 1;
    char *ptr_oldpath, *ptr_newpath;

    ptr_oldpath = strrchr( oldpath, '/' );
    ptr_newpath = strrchr( newpath, '/' );
    if( strncmp( oldpath, newpath, (unsigned)(max( ptr_oldpath - oldpath, ptr_newpath - newpath ))
            ) != 0 )
        saferename = 1;

    if( ptr_oldpath[1] == '.' || ptr_newpath[1] == '.' )
        saferename = 1;

    if( saferename == 0 ) {
        if( m_rename( &M, oldpath+1, newpath+1 ) < 0 ) return -EIO;
    } else {
        if( m_copy( &M, oldpath, newpath ) < 0 ) return -EIO;
        if( m_remove( &M, oldpath ) < 0 ) return -EIO;
    }
    return 0;
}


static int fs_statfs( const char *path, struct statvfs *st )
{
    if( m_gstat( &M, st ) < 0 )
        return -EIO;
    else
        return 0;
}


struct fuse_operations fuse_ops = {
        .getattr = fs_getattr,
        .readdir = fs_readdir,
        .read = fs_read,
        .write = fs_write,
        .mknod = fs_mknod,
        .mkdir = fs_mkdir,
        .rmdir = fs_rmdir,
        .unlink = fs_unlink,
        .truncate = fs_truncate,
        .rename = fs_rename,
        .statfs = fs_statfs,
};
