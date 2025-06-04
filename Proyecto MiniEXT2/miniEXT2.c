#define FUSE_USE_VERSION 26
#include <fuse.h>
#include "struct.h"

// Helpers
static struct inode *get_inode(uint32_t i) { return &inode_table[i]; }
static void         *get_block(uint32_t b) { return fs_image + b * sb->block_size; }

static void split_path(const char *path, char *dirpath, char *basename) {
    const char *s = strrchr(path, '/');
    if (!s || s == path) {
        strcpy(dirpath, "/");
        strcpy(basename, s ? s+1 : path);
    } else {
        size_t len = s - path;
        strncpy(dirpath, path, len);
        dirpath[len] = '\0';
        strcpy(basename, s+1);
    }
}

static int resolve_path(const char *path) {
    if (strcmp(path, "/") == 0) return ROOT_INODE;
    char tmp[PATH_MAX];
    strncpy(tmp, path+1, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    char *tok = strtok(tmp, "/");
    int curr = ROOT_INODE;
    while (tok) {
        struct inode *d = get_inode(curr);
        if (!(d->mode & S_IFDIR)) return -ENOENT;
        int found = 0;
        int entries_per_block = sb->block_size / sizeof(struct dir_entry);
        for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
            if (!d->direct[blk]) continue;
            struct dir_entry *e = get_block(d->direct[blk]);
            for (int i = 0; i < entries_per_block; i++) {
                if (e[i].inode && strcmp(e[i].name, tok) == 0) {
                    curr = e[i].inode;
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) return -ENOENT;
        tok = strtok(NULL, "/");
    }
    return curr;
}

static int alloc_block() {
    for (uint32_t b = sb->first_data_block; b < sb->total_blocks; b++) {
        bool usado = false;

        // Recorremos TODOS los inodos
        for (uint32_t i = 0; i < sb->total_inodes; i++) {
            struct inode *in = get_inode(i);

            // Punteros directos
            for (int j = 0; j < DIRECT_POINTERS; j++) {
                if (in->direct[j] == b) {
                    usado = true;
                    break;
                }
            }
            if (usado) break;       
        }

        if (!usado) {
            sb->free_blocks--;
            // Persiste el cambio en free_blocks
            msync(sb, sizeof(*sb), MS_SYNC);
            return b;
        }
    }
    return -ENOSPC;
}

static int alloc_inode() {
    for (uint32_t i = 1; i < sb->total_inodes; i++) {
        if (!get_inode(i)->mode) {
            sb->free_inodes--;
            msync(sb, sizeof(*sb), MS_SYNC);
            return i;
        }
    }
    return -ENOSPC;
}

static int add_entry(int pd, const char *name, int child) {
    struct inode *d = get_inode(pd);
    int entries_per_block = sb->block_size / sizeof(struct dir_entry);

    for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
        // Si el bloque directo no está asignado, asígnalo
        if (!d->direct[blk]) {
            int new_block = alloc_block();
            if (new_block < 0) return -ENOSPC;
            d->direct[blk] = new_block;
            d->blocks++;
            void *blk_ptr = get_block(new_block);
            memset(blk_ptr, 0, sb->block_size);
        }
        struct dir_entry *e = get_block(d->direct[blk]);
        for (int i = 0; i < entries_per_block; i++) {
            if (!e[i].inode) {
                e[i].inode = child;
                strncpy(e[i].name, name, MAX_NAME-1);
                e[i].name[MAX_NAME-1] = '\0';
                d->size += sizeof(*e);
                d->mtime = time(NULL);
                msync(e, sb->block_size, MS_SYNC);
                msync(d, sizeof(*d), MS_SYNC);
                return 0;
            }
        }
    }
    return -ENOSPC;
}

static int remove_entry(int pd, const char *name) {
    struct inode *d = get_inode(pd);
    int entries_per_block = sb->block_size / sizeof(struct dir_entry);

    for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
        struct dir_entry *e = get_block(d->direct[blk]);
        for (int i = 0; i < entries_per_block; i++) {
            if (e[i].inode && strcmp(e[i].name, name) == 0) {
                e[i].inode = 0;
                e[i].name[0] = '\0';
                d->size -= sizeof(*e);
                d->mtime = time(NULL);
                msync(e, sb->block_size, MS_SYNC);
                msync(d, sizeof(*d), MS_SYNC);
                return 0;
            }
        }
    }
    return -ENOENT;
}

// FUSE handlers

static int minifs_getattr(const char *path, struct stat *st) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    memset(st, 0, sizeof(*st));
    st->st_mode  = (n->mode & S_IFMT) | (n->mode & 07777);
    st->st_nlink = n->links_count;
    st->st_size  = n->size;
    st->st_atime = n->atime;   // ← ahora atime independiente
    st->st_mtime = n->mtime;
    st->st_ctime = n->ctime;
    st->st_uid   = n->uid;
    st->st_gid   = n->gid;
    return 0;
}

static int minifs_readdir(const char *path, void *buf,
                          fuse_fill_dir_t filler,
                          off_t off, struct fuse_file_info *fi) {
    (void)off; (void)fi;
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *d = get_inode(idx);
    if (!(d->mode & S_IFDIR)) return -ENOTDIR;

    filler(buf, ".",  NULL, 0);
    filler(buf, "..", NULL, 0);

    int entries_per_block = sb->block_size / sizeof(struct dir_entry);
    for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
        if (!d->direct[blk]) continue;
        struct dir_entry *e = get_block(d->direct[blk]);
        for (int i = 0; i < entries_per_block; i++) {
            if (e[i].inode &&
                strcmp(e[i].name, ".") &&
                strcmp(e[i].name, "..")) {
                filler(buf, e[i].name, NULL, 0);
            }
        }
    }
    return 0;
}

static int minifs_open(const char *path, struct fuse_file_info *fi) {
    
    int idx = resolve_path(path);
    if (idx < 0) return -ENOENT;
    struct inode *n = get_inode(idx);
    if (n->mode & S_IFDIR) return -EISDIR;
    if (!(n->mode & S_IRUSR)) return -EACCES;
    fi->fh = idx;
    return 0;
}

static int minifs_create(const char *path, mode_t mode,
                         struct fuse_file_info *fi) {
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(path, dp, bn);
    int pd = resolve_path(dp);
    if (pd < 0) return pd;
    struct inode *d = get_inode(pd);
    if (!(d->mode & S_IFDIR)) return -ENOTDIR;
    if (!(d->mode & S_IWUSR)) return -EACCES;
    int in = alloc_inode();
    if (in < 0) return in;
    struct inode *n = get_inode(in);
    memset(n, 0, sizeof(*n));
    n->mode        = S_IFREG | (mode & 0777);
    n->uid         = getuid();
    n->gid         = getgid();
    n->size        = 0;
    n->blocks      = 0;
    n->ctime       = n->mtime = n->atime = time(NULL);
    n->links_count = 1;
    int r = add_entry(pd, bn, in);
    if (r < 0) return r;
    fi->fh = in;
    return 0;
}

static int minifs_truncate(const char *path, off_t size) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;

    struct inode *n = get_inode(idx);
    if (n->mode & S_IFDIR)    return -EISDIR;
    if (!(n->mode & S_IWUSR)) return -EACCES;
    if (size < 0)             return -EINVAL;

    off_t old_size = n->size;
    int old_blocks = (old_size + sb->block_size - 1) / sb->block_size;
    int new_blocks = (size     + sb->block_size - 1) / sb->block_size;

    // CASO 1: Reducción de tamaño
    if (size < old_size) {
        for (int i = new_blocks; i < old_blocks && i < DIRECT_POINTERS; i++) {
            if (n->direct[i]) {
                memset(get_block(n->direct[i]), 0, sb->block_size);
                sb->free_blocks++;
                n->direct[i] = 0;
                n->blocks--;
            }
        }
    }

    // CASO 2: Ampliación de tamaño
    else if (size > old_size) {
        for (int i = old_blocks; i < new_blocks && i < DIRECT_POINTERS; i++) {
            int new_block = alloc_block();
            if (new_block < 0) return -ENOSPC;
            n->direct[i] = new_block;
            memset(get_block(new_block), 0, sb->block_size);
        }
    }

    n->size   = size;
    n->blocks = new_blocks;
    n->mtime  = time(NULL);
    msync(n, sizeof(*n), MS_SYNC);
    msync(sb, sizeof(*sb), MS_SYNC);

    return 0;
}

static int minifs_write(const char *path, const char *buf,
                        size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void)path;

    struct inode *n = get_inode(fi->fh);
    if (!(n->mode & S_IFREG))   return -EISDIR;
    if (!(n->mode & S_IWUSR))   return -EACCES;
    size_t written = 0;
    while (written < size) {
        int blk_idx = (offset + written) / sb->block_size;
        int blk_off = (offset + written) % sb->block_size;
        size_t to_write = sb->block_size - blk_off;
        if (to_write > size - written) to_write = size - written;

        if (blk_idx < DIRECT_POINTERS) {
            if (!n->direct[blk_idx]) {
                int b = alloc_block();
                if (b < 0) return written ? (int)written : b;
                n->direct[blk_idx] = b;
                n->blocks++;
            }
            memcpy(get_block(n->direct[blk_idx]) + blk_off, buf + written, to_write);
        }
        written += to_write;
    }
    n->size = offset + written > n->size ? offset + written : n->size;
    n->mtime = time(NULL);
    return written;

}

static int minifs_read(const char *path, char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
    (void)path;
    
    struct inode *n = get_inode(fi->fh);
    if (!(n->mode & S_IFREG))   return -EISDIR;
    if (!(n->mode & S_IRUSR))   return -EACCES;

    size_t read = 0;
    while (read < size && offset + read < n->size) {
        int blk_idx = (offset + read) / sb->block_size;
        int blk_off = (offset + read) % sb->block_size;
        size_t to_read = sb->block_size - blk_off;
        if (to_read > size - read) to_read = size - read;
        if (to_read > n->size - (offset + read)) to_read = n->size - (offset + read);

        if (blk_idx < DIRECT_POINTERS && n->direct[blk_idx]) {
            memcpy(buf + read, get_block(n->direct[blk_idx]) + blk_off, to_read);
        }
        read += to_read;
    }
    n->atime = time(NULL);
    return read;
}

static int minifs_unlink(const char *path) {
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(path, dp, bn);
    int pd = resolve_path(dp);
    if (pd < 0) return pd;
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    if (n->mode & S_IFDIR) return -EISDIR;
    // Manejo de hard links
    if (n->links_count > 1) {
        n->links_count--;
        return remove_entry(pd, bn);
    }
    // Último enlace: liberar
    if (n->blocks) {
        memset(get_block(n->direct[0]), 0, sb->block_size);
        sb->free_blocks++;
        n->blocks = 0; // Liberamos el bloque
        n->direct[0] = 0; // Liberamos el puntero directo
    }
    memset(n, 0, sizeof(*n));
    sb->free_inodes++;
    return remove_entry(pd, bn);
}

static int minifs_mkdir(const char *path, mode_t mode) {
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(path, dp, bn);
    int pd = resolve_path(dp);
    if (pd < 0) return pd;
    struct inode *parent = get_inode(pd);
    if (!(parent->mode & S_IFDIR)) return -ENOTDIR;
    if (!(parent->mode & S_IWUSR)) return -EACCES;

    int in = alloc_inode();
    if (in < 0) return in;
    int b = alloc_block();
    if (b < 0) return b;

    struct inode *n = get_inode(in);
    printf("Creating directory %s with inode %d and block %d\n", path, in, b);
    memset(n, 0, sizeof(*n));
    n->mode        = S_IFDIR | (mode & 0777);
    n->uid         = getuid();
    n->gid         = getgid();
    n->size        = 2 * sizeof(struct dir_entry);
    n->blocks      = 1;
    n->direct[0]   = b;
    n->atime       = n->ctime = n->mtime = time(NULL);
    n->links_count = 2;

    void *blk = get_block(b);
    memset(blk, 0, sb->block_size);

    struct dir_entry *e = blk;
    e[0].inode = in; strcpy(e[0].name, ".");
    e[1].inode = pd; strcpy(e[1].name, "..");

    parent->links_count++;
    return add_entry(pd, bn, in);
}


static int minifs_rmdir(const char *path) {
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(path, dp, bn);
    int pd  = resolve_path(dp);
    if (pd < 0) return pd;
    int idx = resolve_path(path);
    if (idx < 0) return idx;

    struct inode *n = get_inode(idx);
    if (!(n->mode & S_IFDIR)) return -ENOTDIR;

    int entries_per_block = sb->block_size / sizeof(struct dir_entry);
    // Verifica que todos los bloques directos estén vacíos (excepto "." y "..")
    for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
        if (!n->direct[blk]) continue;
        struct dir_entry *e = get_block(n->direct[blk]);
        for (int i = 0; i < entries_per_block; i++) {
            if (!strcmp(e[i].name, ".") || !strcmp(e[i].name, ".."))
                continue;
            if (e[i].inode != 0)
                return -ENOTEMPTY;
        }
    }

    int rem = remove_entry(pd, bn);
    if (rem < 0) return rem;

    // Limpia todos los bloques directos asignados
    for (int blk = 0; blk < DIRECT_POINTERS; blk++) {
        if (n->direct[blk]) {
            memset(get_block(n->direct[blk]), 0, sb->block_size);
            sb->free_blocks++;
            n->direct[blk] = 0;
            n->blocks--;
        }
    }
    memset(n, 0, sizeof(*n));
    sb->free_inodes++;

    get_inode(pd)->links_count--;
    return 0;
}

static int minifs_rename(const char *from, const char *to) {
    char fp[PATH_MAX], fn[MAX_NAME];
    split_path(from, fp, fn);
    int fpd = resolve_path(fp); if (fpd < 0) return fpd;
    int fi  = resolve_path(from); if (fi  < 0) return fi;
    char tp[PATH_MAX], tn[MAX_NAME];
    split_path(to, tp, tn);
    int tpd = resolve_path(tp); if (tpd < 0) return tpd;
    int ti  = resolve_path(to);
    if (ti >= 0) {
        struct inode *nd = get_inode(ti);
        if ((nd->mode & S_IFDIR) && nd->size > 2*sizeof(struct dir_entry))
            return -ENOTEMPTY;
        remove_entry(tpd, tn);
        memset(nd, 0, sizeof(*nd));
        sb->free_inodes++;
    }
    int r = add_entry(tpd, tn, fi); if (r < 0) return r;
    return remove_entry(fpd, fn);
}

static int minifs_statfs(const char *path, struct statvfs *stbuf) {
    (void)path;
    memset(stbuf, 0, sizeof(*stbuf));
    stbuf->f_bsize   = sb->block_size;
    stbuf->f_frsize  = sb->block_size;
    stbuf->f_blocks  = sb->total_blocks;
    stbuf->f_bfree   = sb->free_blocks;
    stbuf->f_bavail  = sb->free_blocks;
    stbuf->f_files   = sb->total_inodes;
    stbuf->f_ffree   = sb->free_inodes;
    stbuf->f_namemax = MAX_NAME;
    return 0;
}

static int minifs_chmod(const char *path, mode_t mode) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    n->mode  = (n->mode & S_IFMT) | (mode & 07777);
    n->ctime = time(NULL);
    return 0;
}

static int minifs_chown(const char *path, uid_t uid, gid_t gid) {
    
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    if (uid != (uid_t)-1) n->uid = uid;
    if (gid != (gid_t)-1) n->gid = gid;
    n->mtime = time(NULL);
    return 0;
}

static int minifs_link(const char *from, const char *to) {
    int fi = resolve_path(from);
    if (fi < 0) return fi;
    struct inode *src = get_inode(fi);
    if (src->mode & S_IFDIR) return -EPERM;
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(to, dp, bn);
    int pd = resolve_path(dp);
    if (pd < 0) return pd;
    int r = add_entry(pd, bn, fi);
    if (r < 0) return r;
    src->links_count++;
    src->mtime = time(NULL);
    return 0;
}

static int minifs_symlink(const char *target, const char *linkpath) {
    char dp[PATH_MAX], bn[MAX_NAME];
    split_path(linkpath, dp, bn);
    int pd = resolve_path(dp);
    if (pd < 0) return pd;
    int in = alloc_inode();
    if (in < 0) return in;
    int b  = alloc_block();
    if (b < 0) return b;
    struct inode *n = get_inode(in);
    memset(n, 0, sizeof(*n));
    n->mode        = S_IFLNK | 0777;
    n->uid         = getuid();
    n->gid         = getgid();
    n->size        = strlen(target);
    n->blocks      = 1;
    n->direct[0]   = b;
    n->ctime       = n->mtime = n->atime = time(NULL);
    n->links_count = 1;
    memcpy(get_block(b), target, n->size);
    return add_entry(pd, bn, in);
}

static int minifs_readlink(const char *path, char *buf, size_t size) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    if ((n->mode & S_IFMT) != S_IFLNK) return -EINVAL;
    size_t len = n->size;
    if (len >= size) len = size-1;
    memcpy(buf, get_block(n->direct[0]), len);
    buf[len] = '\0';
    return 0;
}

static int minifs_utimens(const char *path, const struct timespec ts[2]) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    n->atime = ts[0].tv_sec;    // tiempo de acceso
    n->mtime = ts[1].tv_sec;    // tiempo de modificación
    return 0;
}

static int minifs_access(const char *path, int mask) {
    int idx = resolve_path(path);
    if (idx < 0) return idx;
    struct inode *n = get_inode(idx);
    mode_t m = n->mode & 0777;
    if ((mask & R_OK) && !(m & S_IRUSR)) return -EACCES;
    if ((mask & W_OK) && !(m & S_IWUSR)) return -EACCES;
    if ((mask & X_OK) && !(m & S_IXUSR)) return -EACCES;
    return 0;
}

static struct fuse_operations minifs_oper = {
    .getattr   = minifs_getattr,
    .readdir   = minifs_readdir,
    .open      = minifs_open,
    .create    = minifs_create,
    .truncate  = minifs_truncate,
    .read      = minifs_read,
    .write     = minifs_write,
    .unlink    = minifs_unlink,
    .mkdir     = minifs_mkdir,
    .rmdir     = minifs_rmdir,
    .rename    = minifs_rename,
    .statfs    = minifs_statfs,
    .chmod     = minifs_chmod,
    .chown     = minifs_chown,
    .link      = minifs_link,
    .symlink   = minifs_symlink,
    .readlink  = minifs_readlink,
    .utimens   = minifs_utimens,
    .access    = minifs_access,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <imagen> <punto_montaje> [opts]\n", argv[0]);
        return 1;
    }
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) { perror("open imagen"); return 1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); return 1; }
    fs_image    = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    sb          = (struct superblock *)fs_image;
    inode_table = (struct inode *)(fs_image + INODE_TABLE_BLK * BLOCK_SIZE);

    int fac = 1;
    char **fav = malloc(sizeof(char*)*(argc+1));
    fav[0] = argv[0];
    for (int i = 3; i < argc; i++) fav[fac++] = argv[i];
    fav[fac++] = argv[2];
    fav[fac]   = NULL;

    return fuse_main(fac, fav, &minifs_oper, NULL);
}

