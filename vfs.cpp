#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

struct UserInfo {
    uid_t uid;
    std::string home;
    std::string shell;
};

static std::map<std::string, UserInfo> users_cache;
static std::string vfs_base_path;

static void update_users_cache() {
    users_cache.clear();

    setpwent();
    struct passwd* pw;
    while ((pw = getpwent()) != NULL) {
        UserInfo info;
        info.uid = pw->pw_uid;
        info.home = pw->pw_dir;
        info.shell = pw->pw_shell;
        users_cache[pw->pw_name] = info;
    }
    endpwent();
}

static std::string get_vfs_path(const char* path) {
    return vfs_base_path + path;
}


static int vfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    std::string full_path(path);
    size_t slash_pos = full_path.find('/', 1);

    if (slash_pos == std::string::npos) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
    }
    else {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024; 
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
    }

    return 0;
}

static int vfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info* fi,
    enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

    filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);

    if (strcmp(path, "/") == 0) {
        update_users_cache();
        for (const auto& pair : users_cache) {
            filler(buf, pair.first.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
        }
    }
    else {
        filler(buf, "id", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "home", NULL, 0, FUSE_FILL_DIR_PLUS);
        filler(buf, "shell", NULL, 0, FUSE_FILL_DIR_PLUS);
    }

    return 0;
}

static int vfs_open(const char* path, struct fuse_file_info* fi) {
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
    }
    return 0;
}

static int vfs_read(const char* path, char* buf, size_t size, off_t offset,
    struct fuse_file_info* fi) {
    (void)fi;

    std::string full_path(path);
    size_t slash_pos = full_path.find('/', 1);

    if (slash_pos == std::string::npos) {
        return -EISDIR; 
    }

    std::string username = full_path.substr(1, slash_pos - 1);
    std::string filename = full_path.substr(slash_pos + 1);

    update_users_cache();

    if (users_cache.find(username) == users_cache.end()) {
        return -ENOENT;
    }

    const UserInfo& user = users_cache[username];
    std::string content;

    if (filename == "id") {
        content = std::to_string(user.uid) + "\n";
    }
    else if (filename == "home") {
        content = user.home + "\n";
    }
    else if (filename == "shell") {
        content = user.shell + "\n";
    }
    else {
        return -ENOENT;
    }

    if (offset < 0) return -EINVAL;
    if ((size_t)offset >= content.size()) return 0;

    size_t len = content.size() - offset;
    if (len > size) len = size;

    memcpy(buf, content.c_str() + offset, len);
    return len;
}

static int vfs_mkdir(const char* path, mode_t mode) {
    std::string username = path + 1;

    std::string cmd = "sudo adduser --disabled-password --gecos '' " + username + " 2>/dev/null";
    int result = system(cmd.c_str());

    if (result == 0) {
        update_users_cache();
        return 0;
    }
    else {
        return -EACCES;
    }
}

static int vfs_rmdir(const char* path) {
    std::string username = path + 1;

    std::string cmd = "sudo userdel -r " + username + " 2>/dev/null";
    int result = system(cmd.c_str());

    if (result == 0) {
        update_users_cache();
        return 0;
    }
    else {
        return -EACCES;
    }
}

static struct fuse_operations vfs_oper = {
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .open = vfs_open,
    .read = vfs_read,
    .mkdir = vfs_mkdir,
    .rmdir = vfs_rmdir,
};

void start_users_vfs(const std::string& mount_point) {
    vfs_base_path = mount_point;

    mkdir(mount_point.c_str(), 0755);

    const char* fuse_argv[] = {
        "users_vfs",
        "-f",           
        "-s",          
        mount_point.c_str()
    };
    int fuse_argc = sizeof(fuse_argv) / sizeof(fuse_argv[0]);

    pid_t pid = fork();
    if (pid == 0) {
        fuse_main(fuse_argc, (char**)fuse_argv, &vfs_oper, NULL);
        exit(0);
    }
    else if (pid > 0) {
        sleep(1); 
        std::cout << "[VFS] Users filesystem mounted at: " << mount_point << "\n";
    }
}

void stop_users_vfs() {
    std::string cmd = "fusermount3 -u " + vfs_base_path;
    system(cmd.c_str());
}