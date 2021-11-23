#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/strerr.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <unistd.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>

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

static void build_filename(justc_tar *header) {
    size_t len = 0;
    char* s = 0;
    if(longname) {
        longname = 0;
    } else {
        stralloc_copy(&cur,&root);
        len = strlen(header->filename_prefix);
        if(len) {
            /* strip leading ./ */
            s = header->filename_prefix;
            while((*s == '.' || *s == '/') && len) {
                s++;
                len--;
            }
            stralloc_catb(&cur,s,len);
            stralloc_append(&cur,'/');
        }
        len = strlen(header->filename);
        s = header->filename;
        while((*s == '.' || *s == '/') && len) {
            s++;
            len--;
        }
        stralloc_catb(&cur,s,len);
        /* remove any final / */
        while(cur.s[cur.len-1] == '/') {
            cur.len--;
        }
        stralloc_0(&cur);
    }
}

static int extract_file(justc_tar *header, const unsigned char *s) {
    int fd;
    struct utimbuf utim;
    struct stat st;

    build_filename(header);

    buffer_puts(buffer_1,"[file] ");
    buffer_puts(buffer_1,cur.s);
    buffer_putsflush(buffer_1,"\n");

    fd = open_create(cur.s);
    if(fd == -1) {
        strerr_warn3sys("(extract_file) unable to create ",cur.s,": ");
        return 1;
    }
    ndelay_off(fd);
    if(fd_write(fd,(const char *)s,header->size) != (ssize_t)header->size) {
        strerr_warn3sys("(extract_file) unable to write ",cur.s,": ");
        return 1;
    }
    if(fd_chown(fd,header->uid,header->gid) == -1) {
        strerr_warn3sys("(extract_file) unable to chown ",cur.s,": ");
    }
    if(fd_chmod(fd,header->mode) == -1) {
        strerr_warn3sys("(extract_file) unable to chmod ",cur.s,": ");
    }
    if(fstat(fd,&st) == -1) {
        strerr_warn3sys("(extract_file) unable to fstat ",cur.s,": ");
    }
    fd_close(fd);

    utim.actime = st.st_atime;
    utim.modtime = header->mtime;
    if(utime(cur.s, &utim) < 0) {
        strerr_warn3sys("(extract_file) unable to utime ",cur.s,": ");
    }

    return 0;
}

static int extract_symlink(justc_tar *header, const unsigned char *s) {
    stralloc t = STRALLOC_ZERO;
    struct stat st;
    struct timeval lutim[2];
    (void)s;

    buffer_puts(buffer_1,"[symlink] ");
    build_filename(header);
    buffer_puts(buffer_1,cur.s);
    buffer_puts(buffer_1," -> ");
    if(longlink) {
        longlink = 0;
    } else {
        stralloc_copys(&linkname,header->linked);
        stralloc_0(&linkname);
    }
    buffer_puts(buffer_1,linkname.s);
    buffer_putsflush(buffer_1,"\n");

    if(strcmp(linkname.s,"/bin/execlineb") == 0) {
        /* check if destination /bin is a symlink, do not write out if true */
        stralloc_copy(&t,&root);
        stralloc_cats(&t,"bin");
        stralloc_0(&t);
        if(lstat(t.s,&st) == -1) {
            strerr_warn3sys("(extract_symlink) unable to lstat ",t.s,": ");
            return 1;
        }
        if(S_ISLNK(st.st_mode)) return 0;
    }

    if(lstat(cur.s,&st) != -1) {
        if(unlink(cur.s) == -1) {
            strerr_warn3sys("(extract_symlink) unable to unlink ",cur.s,": ");
            return 1;
        }
    }

    if(symlink(linkname.s,cur.s) == -1) {
        strerr_warn3sys("(extract_symlink) unable to symlink ",cur.s,": ");
        return 1;
    }

    if(lchown(cur.s,header->uid,header->gid) == -1) {
        strerr_warn3sys("(extract_symlink) unable to lchown ",cur.s,": ");
    }

    if(lstat(cur.s,&st) == -1) {
        strerr_warn3sys("(extract_symlink) unable to lstat ",cur.s,": ");
    }

    lutim[0].tv_sec = st.st_atime;
    lutim[1].tv_sec = header->mtime;

    lutim[0].tv_usec = 0;
    lutim[1].tv_usec = 0;
    if(lutimes(cur.s, lutim) < 0) {
        strerr_warn3sys("(extract_symlink) unable to lutimes ",cur.s,": ");
    }

    return 0;
}

static int extract_dir(justc_tar *header, const unsigned char *s) {
    struct stat st;
    struct utimbuf utim;
    (void)s;
    build_filename(header);

    buffer_puts(buffer_1,"[directory] ");
    buffer_puts(buffer_1,cur.s);
    buffer_putsflush(buffer_1,"\n");

    if(lstat(cur.s,&st) == -1) {
        if(mkdir(cur.s,header->mode) == -1) {
            strerr_warn3sys("(extract_dir) unable to mkdir ",cur.s,": ");
            return 1;
        }
        if(lchown(cur.s,header->uid,header->gid) == -1) {
            strerr_warn3sys("(extract_dir) unable to chown ",cur.s,": ");
        }
        if(stat(cur.s,&st) == -1) {
            strerr_warn3sys("(extract_dir) unable to stat ",cur.s,": ");
        }
        utim.actime = st.st_atime;
        utim.modtime = header->mtime;
        if(utime(cur.s, &utim) < 0) {
            strerr_warn3sys("(extract_dir) unable to utime ",cur.s,": ");
        }
    }

    return 0;
}

static int extract_gnulonglink(justc_tar *header, const unsigned char *s) {
    size_t len = header->size;
    while((*s == '.' || *s == '/') && len) {
        s++;
        len--;
    }

    stralloc_copyb(&linkname,(const char *)s,len);
    /* remove any final / */
    while(linkname.s[linkname.len-1] == '/') {
        linkname.len--;
    }
    stralloc_0(&linkname);
    longlink = 1;
    return 0;
}

static int extract_gnulongname(justc_tar *header, const unsigned char *s) {
    size_t len = header->size;
    while((*s == '.' || *s == '/') && len) {
        s++;
        len--;
    }
    stralloc_copy(&cur,&root);
    stralloc_catb(&cur,(const char *)s,len);
    /* remove any final / */
    while(cur.s[cur.len-1] == '/') {
        cur.len--;
    }
    stralloc_0(&cur);
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
