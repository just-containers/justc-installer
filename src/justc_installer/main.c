#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/strerr.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <unistd.h>
#include <utime.h>

#include "payload.h"
#include "tar.h"


EXTLD(payload_tar)

static stralloc root = STRALLOC_ZERO;
static stralloc cur  = STRALLOC_ZERO;
static stralloc linkname = STRALLOC_ZERO;

static int longlink = 0;
static int longname = 0;

static inline size_t blocks(size_t len) {
    size_t blocks;
    size_t rem;
    blocks = len / 512;
    rem = len % 512;
    if(rem) blocks++;
    return blocks;
}

static void build_filename(justc_tar *header, int flush) {
    if(longname) {
        longname = 0;
    } else {
        stralloc_copy(&cur,&root);
        if(strlen(header->filename_prefix)) {
            stralloc_cats(&cur,header->filename_prefix);
            stralloc_append(&cur,'/');
        }
        stralloc_cats(&cur,header->filename);
        stralloc_0(&cur);
    }

    buffer_puts(buffer_1,&cur.s[root.len]);
    if(flush) buffer_putsflush(buffer_1,"\n");
}

static int extract_file(justc_tar *header, const unsigned char *s) {
    int fd;
    struct utimbuf utim;
    struct stat st;

    build_filename(header,1);
    fd = open_create(cur.s);
    if(fd == -1) {
        strerr_warn3sys("unable to create ",cur.s,": ");
        return 1;
    }
    ndelay_off(fd);
    if(fd_write(fd,(const char *)s,header->size) != (ssize_t)header->size) {
        strerr_warn3sys("unable to write ",cur.s,": ");
        return 1;
    }
    if(fd_chown(fd,header->uid,header->gid) == -1) {
        strerr_warn3sys("unable to chown ",cur.s,": ");
    }
    if(fd_chmod(fd,header->mode) == -1) {
        strerr_warn3sys("unable to chmod ",cur.s,": ");
    }
    if(fstat(fd,&st) == -1) {
        strerr_warn3sys("unable to fstat ",cur.s,": ");
    }
    fd_close(fd);

    utim.actime = st.st_atime;
    utim.modtime = header->mtime;
    if(utime(cur.s, &utim) < 0) {
        strerr_warn3sys("unable to utime ",cur.s,": ");
    }

    return 0;
}

static int extract_symlink(justc_tar *header, const unsigned char *s) {
    stralloc t = STRALLOC_ZERO;
    struct stat st;
    struct utimbuf utim;
    (void)s;

    build_filename(header,0);
    if(longlink) {
        longlink = 0;
    } else {
        stralloc_copys(&linkname,header->linked);
        stralloc_0(&linkname);
    }

    if(strcmp(linkname.s,"/bin/execlineb") == 0) {
        /* check if destination /bin is a symlink, do not write out if true */
        stralloc_copy(&t,&root);
        stralloc_cats(&t,"bin");
        stralloc_0(&t);
        if(lstat(t.s,&st) == -1) {
            strerr_warn3sys("unable to lstat ",t.s,": ");
            return 1;
        }
        if(S_ISLNK(st.st_mode)) return 0;
    }

    if(lstat(cur.s,&st) != -1) {
        if(unlink(cur.s) == -1) {
            strerr_warn3sys("unable to unlink ",cur.s,": ");
            return 1;
        }
    }

    if(symlink(linkname.s,cur.s) == -1) {
        strerr_warn3sys("unable to symlink ",cur.s,": ");
        return 1;
    }

    if(lchown(cur.s,header->uid,header->gid) == -1) {
        strerr_warn3sys("unable to lchown ",cur.s,": ");
    }

    if(lstat(cur.s,&st) != -1) {
        strerr_warn3sys("unable to lstat ",cur.s,": ");
    }

    utim.actime = st.st_atime;
    utim.modtime = header->mtime;
    if(utime(cur.s, &utim) < 0) {
        strerr_warn3sys("unable to utime ",cur.s,": ");
    }

    return 0;
}

static int extract_dir(justc_tar *header, const unsigned char *s) {
    struct stat st;
    struct utimbuf utim;
    (void)s;
    build_filename(header,1);
    if(lstat(cur.s,&st) == -1) {
        if(mkdir(cur.s,header->mode) == -1) {
            strerr_warn3sys("unable to mkdir ",cur.s,": ");
            return 1;
        }
        if(lchown(cur.s,header->uid,header->gid) == -1) {
            strerr_warn3sys("unable to chown ",cur.s,": ");
        }
        if(stat(cur.s,&st) != -1) {
            strerr_warn3sys("unable to stat ",cur.s,": ");
        }
        utim.actime = st.st_atime;
        utim.modtime = header->mtime;
        if(utime(cur.s, &utim) < 0) {
            strerr_warn3sys("unable to utime ",cur.s,": ");
        }
    }

    return 0;
}

static int extract_gnulonglink(justc_tar *header, const unsigned char *s) {
    stralloc_copyb(&linkname,(const char *)s,header->size);
    stralloc_0(&linkname);
    longlink = 1;
    return 0;
}

static int extract_gnulongname(justc_tar *header, const unsigned char *s) {
    stralloc_copy(&cur,&root);
    stralloc_catb(&cur,(const char *)s,header->size);
    longname = 1;
    return 0;
}

int main(int argc, const char * argv[]) {
    justc_tar tarheader;
    size_t offset = 0;

    if(argc < 2) {
        buffer_putsflush(buffer_2,"error: missing argument: destination\n");
        return 1;
    }

    if( access( argv[1], F_OK ) == -1) {
        buffer_putsflush(buffer_2,"error: destination must exist\n");
        return 1;
    }

    stralloc_copys(&root,argv[1]);
    while(root.len && root.s[root.len-1] == '/') {
        root.len--;
    }
    stralloc_append(&root,'/');

    while(justc_tar_parse(&tarheader,(const char *)&LDVAR(payload_tar)[offset],LDLEN(payload_tar) - offset) == 0) {
        offset += 512;
        switch(tarheader.type) {
            case '0': extract_file(&tarheader,&LDVAR(payload_tar)[offset]); break;
            case '2': extract_symlink(&tarheader,&LDVAR(payload_tar)[offset]); break;
            case '5': extract_dir(&tarheader,&LDVAR(payload_tar)[offset]); break;
            /* these don't actually pop up but they're easy to implement, just in case */
            case 'K': extract_gnulongname(&tarheader,&LDVAR(payload_tar)[offset]); break;
            case 'L': extract_gnulonglink(&tarheader,&LDVAR(payload_tar)[offset]); break;
            default: {
                buffer_puts(buffer_2,"error: unhandled tar file type: '");
                buffer_put(buffer_2,&tarheader.type,1);
                buffer_putsflush(buffer_2,"'\n");
                return 1;
            }
        }
        offset += (blocks(tarheader.size) * 512);
    }

    return 0;
}
