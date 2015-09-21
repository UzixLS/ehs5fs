#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fuse.h"
#include "modem.h"

struct modem M;


struct {
    char    *point;
    char    *dev;
    char    *mountpoint;
    int     readonly;
    int     debug;
} options = {
    .point    = NULL,
    .dev      = NULL,
    .readonly = 0,
    .debug    = 0,
};
enum {
    KEY_VERSION,
    KEY_HELP,
    KEY_PORT,
    KEY_READONLY,
    KEY_DEBUG,
};
static struct fuse_opt opts[] = {
    FUSE_OPT_KEY("-V",          KEY_VERSION),
    FUSE_OPT_KEY("--version",   KEY_VERSION),
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_KEY("--readonly",  KEY_READONLY),
    FUSE_OPT_KEY("--debug",     KEY_DEBUG),
    FUSE_OPT_END
};

const char *usage = "\
usage: tc65fs [fuse options] device mountpoint\n\
   -V --version       show software version inforamtion\n\
   --port=/dev/ttyS0  connect to specified port\n\
   --readonly         disable any writting operations\n\
   --debug            start in debug mode\n\
";


static int opt_proc(
        void *data,
        const char *arg,
        int key,
        struct fuse_args *outargs )
{
    switch (key) {
    case KEY_VERSION:
        printf("ehs5fs build at %s %s, (c)2015 Eugene Lozovoy <lozovoy.ep@gmail.com>\n", __DATE__, __TIME__);
        exit( 1 );
    case KEY_HELP:
        printf( usage );
        exit( 0 );
    case FUSE_OPT_KEY_NONOPT:
        if (options.dev == NULL) {
            options.dev = strdup(arg);
            return 0;
        }
        if (options.point == NULL) {
            options.point = strdup(arg);
            return 0;
        }
        return 1;
    case KEY_PORT:
        if (options.dev == NULL) {
            options.dev = strdup(arg+11);
            return 0;
        }
        return 1;
    case KEY_READONLY:
        options.readonly = 1;
        return 0;
    case KEY_DEBUG:
        options.debug = 1;
        return 0;
    }
    return 0;
}


int main( int argc, char *argv[] )
{
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    if( fuse_opt_parse( &args, NULL, opts, opt_proc ) ) return 1;
    fuse_opt_add_arg( &args, "-s" );
    if( options.debug ) fuse_opt_add_arg( &args, "-d" );
    if( options.readonly ) fuse_opt_add_arg( &args, "-oro" );
    if( options.point ) fuse_opt_add_arg( &args, options.point );
    if( !options.point || !options.dev  ) {
        printf( usage );
        return -2;
    }

    if( m_init( &M, options.dev ) < 0 ) {
        printf( "m_init failed!\n" );
        return -1;
	}

    return fuse_main( args.argc, args.argv, &fuse_ops, NULL );
}
