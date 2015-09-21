# ehs5fs
FUSE filesystem for EHS5/BGS5 modems

usage: ehs5fs [fuse options] device mountpoint
   --version          show software version inforamtion
   --readonly         disable any writting operations
   --debug            start in debug mode

example: ./ehs5fs /dev/ttyUSB0 ~/tmp1/
