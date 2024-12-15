#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "mysh.h"

#define MAX_CMDLINE_SIZE    (128)
#define MAX_CMD_SIZE        (32)
#define MAX_ARG             (4)

typedef int  (*cmd_func_t)(int argc, char **argv);
typedef void (*usage_func_t)(void);

typedef struct cmd_t {
    char            cmd_str[MAX_CMD_SIZE];
    cmd_func_t      cmd_func;
    usage_func_t   usage_func;
    char            comment[128];
} cmd_t;

#define DECLARE_CMDFUNC(str)    int cmd_##str(int argc, char **argv); \
                                void usage_##str(void)

DECLARE_CMDFUNC(help);
DECLARE_CMDFUNC(mkdir);
DECLARE_CMDFUNC(touch);
DECLARE_CMDFUNC(rmdir);
DECLARE_CMDFUNC(cd);
DECLARE_CMDFUNC(mv);
DECLARE_CMDFUNC(ls);
DECLARE_CMDFUNC(ln);
DECLARE_CMDFUNC(rm);
DECLARE_CMDFUNC(chmod);
DECLARE_CMDFUNC(cat);
DECLARE_CMDFUNC(cp);
DECLARE_CMDFUNC(ps);
DECLARE_CMDFUNC(kill);
DECLARE_CMDFUNC(quit);
DECLARE_CMDFUNC(exec);

/* Command List */
static cmd_t cmd_list[] = {
    {"help",    cmd_help,    usage_help,  "show usage, ex) help <command>"},
    {"mkdir",   cmd_mkdir,   usage_mkdir, "create directory"},
    {"touch",   cmd_touch,   NULL,        "create file"},
    {"rmdir",   cmd_rmdir,   usage_rmdir, "remove directory"},
    {"cd",      cmd_cd,      usage_cd,    "change current directory"},
    {"mv",      cmd_mv,      usage_mv,    "rename directory & file"},
    {"ls",      cmd_ls,      NULL,        "show directory contents"},
    {"ln",      cmd_ln,      usage_ln,    "create link"},
    {"rm",      cmd_rm,      usage_rm,    "remove file"},
    {"chmod",   cmd_chmod,   usage_chmod, "change file mode"},
    {"cat",     cmd_cat,     usage_cat,   "show file contents"},
    {"cp",      cmd_cp,      usage_cp,    "copy file"},
    {"ps",      cmd_ps,      NULL,        "show process status"},
    {"kill",    cmd_kill,    usage_kill,  "terminate process"},
    {"quit",    cmd_quit,    NULL,        "terminate shell"},
    {"exec",    cmd_exec,    NULL,        ""},
};

const int command_num = sizeof(cmd_list) / sizeof(cmd_t);
char *chroot_path = "/tmp/test";
char *current_dir;
extern int client_fd;

static int search_command(char *cmd)
{
    int i;

    for (i = 0; i < command_num; i++) {
        if (strcmp(cmd, cmd_list[i].cmd_str) == 0) {
            /* found */
            return (i);
        }
    }

    /* not found */
    return (-1);
}

void get_realpath(char *usr_path, char *result)
{
    char *stack[32];
    int   index = 0;
    char  fullpath[128];
    char *tok;
    int   i;
#define PATH_TOKEN   "/"

    if (usr_path[0] == '/') {
        strncpy(fullpath, usr_path, sizeof(fullpath)-1);
    } else {
        snprintf(fullpath, sizeof(fullpath)-1, "%s/%s", current_dir + strlen(chroot_path), usr_path);
    }

    /* parsing */
    tok = strtok(fullpath, PATH_TOKEN);
    if (tok == NULL) {
        goto out;
    }

    do {
        if (strcmp(tok, ".") == 0 || strcmp(tok, "") == 0) {
            ; // skip
        } else if (strcmp(tok, "..") == 0) {
            if (index > 0) index--;
        } else {
            stack[index++] = tok;
        }
    } while ((tok = strtok(NULL, PATH_TOKEN)) && (index < 32));

out:
    strcpy(result, chroot_path);

    // TODO: boundary check
    for (i = 0; i < index; i++) {
        strcat(result, "/");
        strcat(result, stack[i]);
    }
}

void init() {
    current_dir = (char*) malloc(MAX_CMDLINE_SIZE);
    if (current_dir == NULL) {
        perror("current_dir malloc");
        exit(1);
    }

    if (chdir(chroot_path) < 0) {
        mkdir(chroot_path, 0755);
        chdir(chroot_path);
    }
}

char* execute(char* command) {
    char *tok_str;
    char *cmd_argv[MAX_ARG];
    int  cmd_argc, i, ret;

    if (getcwd(current_dir, MAX_CMDLINE_SIZE) == NULL) {
        perror("getcwd");
        return "err";
    }

    // if (strlen(current_dir) == strlen(chroot_path)) {
    //     printf("/"); // for root path
    // }
    // printf("%s $ ", current_dir + strlen(chroot_path));

    // if (fgets(command, MAX_CMDLINE_SIZE-1, stdin) == NULL) return "err";

    /* get arguments */
    tok_str = strtok(command, " \n");
    if (tok_str == NULL) return "err";

    cmd_argv[0] = tok_str;

    for (cmd_argc = 1; cmd_argc < MAX_ARG; cmd_argc++) {
        if (tok_str = strtok(NULL, " \n")) {
            cmd_argv[cmd_argc] = tok_str;
        } else {
            break;
        }
    }

    /* search command in list and call command function */
    i = search_command(cmd_argv[0]);
    if (i < 0) {
        printf("%s: command not found\n", cmd_argv[0]);
    } else {
        if (cmd_list[i].cmd_func) {
            ret = cmd_list[i].cmd_func(cmd_argc, cmd_argv);
            if (ret == 0) {
                printf("return success\n");
            } else if (ret == -2 && cmd_list[i].usage_func) {
                printf("usage : ");
                cmd_list[i].usage_func();
            } else {
                printf("return fail(%d)\n", ret);
            }
        } else {
            printf("no command function\n");
        }
    }
}

int cmd_help(int argc, char **argv)
{
    int i;

    if (argc == 1) {
        for (i = 0; i < command_num; i++) {
            printf("%32s: %s\n", cmd_list[i].cmd_str, cmd_list[i].comment);
        }
    } else if (argv[1] != NULL) {
        i = search_command(argv[1]);
        if (i < 0) {
            printf("%s command not found\n", argv[1]);
        } else {
            if (cmd_list[i].usage_func) {
                printf("usage : ");
                cmd_list[i].usage_func();
                return (0);
            } else {
                printf("no usage\n");
                return (-2);
            }
        }
    }

    return (0);
}

int cmd_mkdir(int argc, char **argv)
{
    int  ret = 0;
    char rpath[128];

    if (argc == 2) {
        get_realpath(argv[1], rpath);
        
        if ((ret = mkdir(rpath, 0755)) < 0) {
            perror(argv[0]);
        } else {
            printf("directory created: %s\n", rpath + strlen(chroot_path));
        }
    } else {
        ret = -2; // syntax error
    }

    return (ret);
}

int cmd_touch(int argc, char **argv)
{
    int  ret = 0;
    char rpath[128];

    if (argc == 2) {
        get_realpath(argv[1], rpath);

        // O_CREAT | O_EXCL ensures the file is created only if it does not exist.
        // 0644 sets the file permissions.
        int fd = open(rpath, O_CREAT | O_EXCL | O_WRONLY, 0644);
        
        if (fd < 0) {
            perror(argv[0]);
            ret = -1; // Error creating the file
        } else {
            printf("file created: %s\n", rpath + strlen(chroot_path));
            close(fd); // Close the file descriptor
        }
    } else {
        ret = -2; // syntax error
    }

    return (ret);
}

int cmd_rmdir(int argc, char **argv)
{
    int  ret = 0;
    char rpath[128];

    if (argc == 2) {
        get_realpath(argv[1], rpath);
        
        if ((ret = rmdir(rpath)) < 0) {
            perror(argv[0]);
        } else {
            printf("directory removed: %s\n", rpath + strlen(chroot_path));
        }
    } else {
        ret = -2; // syntax error
    }

    return (ret);
}

int cmd_cd(int argc, char **argv)
{
    int  ret = 0;
    char rpath[128];
    char response[1024] = {0};

    if (argc == 2) {
        get_realpath(argv[1], rpath);

        if ((ret = chdir(rpath)) < 0) {
            perror(argv[0]);
        }
    } else {
        ret = -2;
    }

    if (getcwd(current_dir, MAX_CMDLINE_SIZE) == NULL) {
        perror("getcwd");
        return "err";
    }

    if (strlen(current_dir) == strlen(chroot_path)) {
        response[0] = '/'; // for root path
    } else {
        sprintf(response, "%s", current_dir + strlen(chroot_path));
    }
    send(client_fd, response, strlen(response), 0);

    return (ret);
}

int cmd_mv(int argc, char **argv)
{
    int  ret = 0;
    char rpath1[128];
    char rpath2[128];

    if (argc == 3) {

        get_realpath(argv[1], rpath1);
        get_realpath(argv[2], rpath2);

        if ((ret = rename(rpath1, rpath2)) < 0) {
            perror(argv[0]);
        } else {
            printf("file moved: %s -> %s\n", rpath1 + strlen(chroot_path), rpath2 + strlen(chroot_path));
        }
    } else {
        ret = -2;
    }

    return (ret);
}

static char *get_type_str(char type)
{
    switch (type) {
        case DT_BLK:
            return "BLK";
        case DT_CHR:
            return "CHR";
        case DT_DIR:
            return "DIR";
        case DT_FIFO:
            return "FIFO";
        case DT_LNK:
            return "LNK";
        case DT_REG:
            return "REG";
        case DT_SOCK:
            return "SOCK";
        default: // include DT_UNKNOWN
            return "UNKN";
    }
}

static char get_type_char(char type)
{
    switch (type) {
        case DT_BLK:
            return 'b';
        case DT_CHR:
            return 'c';
        case DT_DIR:
            return 'd';
        case DT_LNK:
            return 'l';
        case DT_REG:
            return '-';
        default: // include DT_UNKNOWN
            return ' ';
    }
}

static void get_perm_str(mode_t mode, char *perm_str)
{
    perm_str[0] = (mode & S_IRUSR) ? 'r' : '-';
    perm_str[1] = (mode & S_IWUSR) ? 'w' : '-';
    perm_str[2] = (mode & S_IXUSR) ? 'x' : '-';
    perm_str[3] = (mode & S_IRGRP) ? 'r' : '-';
    perm_str[4] = (mode & S_IWGRP) ? 'w' : '-';
    perm_str[5] = (mode & S_IXGRP) ? 'x' : '-';
    perm_str[6] = (mode & S_IROTH) ? 'r' : '-';
    perm_str[7] = (mode & S_IWOTH) ? 'w' : '-';
    perm_str[8] = (mode & S_IXOTH) ? 'x' : '-';
    perm_str[9] = '\0';
}

int cmd_ls(int argc, char **argv)
{
    int ret = 0;
    DIR *dp;
    struct dirent *dep;
    struct stat statbuf;

    if (argc != 1) {
        ret = -2;
        goto out;
    }

    if ((dp = opendir(".")) == NULL) {
        ret = -1;
        goto out;
    }

    while (dep = readdir(dp)) {
        lstat(dep->d_name, &statbuf);
        char perm_str[10];
        char symlink_str[1024];
        memset(symlink_str, 0, sizeof(symlink_str));
        get_perm_str(statbuf.st_mode, perm_str);
        
        if (S_ISLNK(statbuf.st_mode)) {
            ssize_t len = readlink(dep->d_name, symlink_str, sizeof(symlink_str)-1);
            if (len != -1) {
                symlink_str[len] = '\0';
            }
        }

        printf("%10ld %c%s %4s %d %d %d %d %d %d %d %s%s%s\n", 
            dep->d_ino,
            get_type_char(dep->d_type),
            perm_str,
            get_type_str(dep->d_type), 
            (int)statbuf.st_uid,
            (int)statbuf.st_gid,
            (int)statbuf.st_atim.tv_sec,
            (int)statbuf.st_mtim.tv_sec,
            (int)statbuf.st_ctim.tv_sec,
            (unsigned int)statbuf.st_nlink,
            (int)statbuf.st_size,
            dep->d_name,
            S_ISLNK(statbuf.st_mode) ? " -> " : "",
            S_ISLNK(statbuf.st_mode) ? symlink_str + strlen(chroot_path) : ""
        );
    }

    closedir(dp);
    send_info();
out:
    return (ret);
}

int cmd_ln(int argc, char **argv)
{
    int ret = 0;
    char real_src[128], real_dst[128];

    if (argc < 3 || argc > 4) {
        ret = -2;
        goto out;
    }

    int arg_idx = 1;
    int sflag = 0;

    if (strcmp(argv[1], "-s") == 0) {
        if (argc != 4) {
            ret = -2; 
            goto out;
        }
        sflag = 1;
        arg_idx++;
    }

    get_realpath(argv[arg_idx], real_src);
    get_realpath(argv[arg_idx+1], real_dst);

    if (sflag) {
        if (symlink(real_src, real_dst) == 0) {
            printf("symbolic link created: %s\n", real_dst + strlen(chroot_path));
        } else {
            perror("symlink");
            ret = -1;
        }
    } else {
        if (link(real_src, real_dst) == 0) {
            printf("hard link created: %s\n", real_dst + strlen(chroot_path));
        } else {
            perror("link");
            ret = -1;
        }
    }

out:
    return ret;
}

int cmd_rm(int argc, char **argv)
{
    int ret = 0;
    char rpath[128];

    if (argc == 2) {
        get_realpath(argv[1], rpath);
        
        if ((ret = unlink(rpath)) < 0) {
            perror(argv[0]);
        } else {
            printf("file removed: %s\n", rpath + strlen(chroot_path));
        }
    } else {
        ret = -2; // syntax error
    }
    return ret;
}

int cmd_chmod(int argc, char **argv)
{
    int ret = 0;
    char rpath[128];
    mode_t new_mode;
    struct stat statbuf;

    if (argc != 3) {
        ret = -2;
        goto out;
    }

    get_realpath(argv[2], rpath);

    // 파일 정보 읽기
    if (stat(rpath, &statbuf) == -1) {
        perror(argv[0]);
        ret = -1;
        goto out;
    }

    // 8진수 형식의 권한 변경 처리
    if (argv[1][0] >= '0' && argv[1][0] <= '7') {
        new_mode = strtol(argv[1], NULL, 8);
        if (chmod(rpath, new_mode) == -1) {
            perror(argv[0]);
            ret = -1;
            goto out;
        }
        printf("mode changed: %s\n", argv[1]);
        goto out;
    }

    // 문자열 형식의 권한 변경 처리
    new_mode = statbuf.st_mode;
    char *perm = argv[1];
    char *c;

    for (c = perm; *c != '\0'; c++) {
        if (perm[0] == 'u') {
            if (perm[1] == '+') {
                if (*c == 'r') new_mode |= S_IRUSR;
                else if (*c == 'w') new_mode |= S_IWUSR;
                else if (*c == 'x') new_mode |= S_IXUSR;
            } else if (perm[1] == '-') {
                if (*c == 'r') new_mode &= ~(S_IRUSR);
                else if (*c == 'w') new_mode &= ~(S_IWUSR);
                else if (*c == 'x') new_mode &= ~(S_IXUSR);
            }
        } else if (perm[0] == 'g') {
            if (perm[1] == '+') {
                if (*c == 'r') new_mode |= S_IRGRP;
                else if (*c == 'w') new_mode |= S_IWGRP;
                else if (*c == 'x') new_mode |= S_IXGRP;
            } else if (perm[1] == '-') {
                if (*c == 'r') new_mode &= ~(S_IRGRP);
                else if (*c == 'w') new_mode &= ~(S_IWGRP);
                else if (*c == 'x') new_mode &= ~(S_IXGRP);
            }
        } else if (perm[0] == 'o') {
            if (perm[1] == '+') {
                if (*c == 'r') new_mode |= S_IROTH;
                else if (*c == 'w') new_mode |= S_IWOTH;
                else if (*c == 'x') new_mode |= S_IXOTH;
            } else if (perm[1] == '-') {
                if (*c == 'r') new_mode &= ~(S_IROTH);
                else if (*c == 'w') new_mode &= ~(S_IWOTH);
                else if (*c == 'x') new_mode &= ~(S_IXOTH);
            }
        } else if (perm[0] == '+') {
            if (*c == 'r') {
                new_mode |= S_IRUSR;
                new_mode |= S_IRGRP;
                new_mode |= S_IROTH;
            } else if (*c == 'w') {
                new_mode |= S_IWUSR;
                new_mode |= S_IWGRP;
                new_mode |= S_IWOTH;
            } else if (*c == 'x') {
                new_mode |= S_IXUSR;
                new_mode |= S_IXGRP;
                new_mode |= S_IXOTH;
            }
        } else if (perm[0] == '-') {
            if (*c == 'r') {
                new_mode &= ~(S_IRUSR);
                new_mode &= ~(S_IRGRP);
                new_mode &= ~(S_IROTH);
            } else if (*c == 'w') {
                new_mode &= ~(S_IWUSR);
                new_mode &= ~(S_IWGRP);
                new_mode &= ~(S_IWOTH);
            } else if (*c == 'x') {
                new_mode &= ~(S_IXUSR);
                new_mode &= ~(S_IXGRP);
                new_mode &= ~(S_IXOTH);
            }
        }
    }

    if (chmod(rpath, new_mode) == -1) {
        perror(argv[0]);
        ret = -1;
        goto out;
    }
    printf("mode changed: %s\n", argv[1] + strlen(chroot_path));

out:
    return ret;
}

int cmd_cat(int argc, char **argv)
{
    int ret = 0;
    char rpath[128];
    FILE *fp;
    char *fileContent = NULL;
    size_t fileSize = 0;

    if (argc != 2) {
        ret = -2; // syntax error
        goto out;
    }

    get_realpath(argv[1], rpath);

    // 파일 열기
    fp = fopen(rpath, "r");
    if (fp == NULL) {
        perror(argv[0]);
        ret = -1; // 파일 열기 실패
        send(client_fd, "error", 5, 0);
        goto out;
    }

    // 파일 크기 확인
    fseek(fp, 0, SEEK_END);
    fileSize = ftell(fp);
    rewind(fp);

    // 파일 내용을 메모리에 읽기
    fileContent = (char *)malloc(fileSize + 1); // 파일 크기 + NULL
    if (fileContent == NULL) {
        perror("Memory allocation failed");
        ret = -1;
        fclose(fp);
        goto out;
    }
    fread(fileContent, 1, fileSize, fp);
    fileContent[fileSize] = '\0'; // NULL-terminate
    fclose(fp);

    // FILE_CONTENT_START 헤더 생성
    char header[256];
    snprintf(header, sizeof(header), "FILE_CONTENT_START:%s\n", rpath + strlen(chroot_path));

    // 헤더와 파일 내용 결합
    size_t totalSize = strlen(header) + fileSize;
    char *response = (char *)malloc(totalSize + 1); // 총 크기 + NULL
    if (response == NULL) {
        perror("Memory allocation failed");
        free(fileContent);
        ret = -1;
        goto out;
    }
    snprintf(response, totalSize + 1, "%s%s", header, fileContent);

    // 클라이언트로 전송
    if (send(client_fd, response, totalSize, 0) == -1) {
        perror("Error sending file content");
        ret = -1;
    }

    // 메모리 해제
    free(fileContent);
    free(response);

out:
    return ret;
}

int cmd_cp(int argc, char **argv)
{
    int ret = 0;
    char rpath1[128];
    char rpath2[128];
    FILE *src, *dst;
    char buf[256];
    size_t nread;

    int recursive = 0;

    // Check for -r option
    if (argc >= 2 && strcmp(argv[1], "-r") == 0) {
        recursive = 1;
        argv++; // Shift arguments
        argc--;
    }

    if (argc != 3) {
        ret = -2; // syntax error
        goto out;
    }

    get_realpath(argv[1], rpath1);
    get_realpath(argv[2], rpath2);

    struct stat statbuf;
    if (stat(rpath1, &statbuf) < 0) {
        perror(argv[0]);
        ret = -1;
        goto out;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        if (!recursive) {
            fprintf(stderr, "%s: %s is a directory (use -r to copy recursively)\n", argv[0], argv[1]);
            ret = -1;
            goto out;
        }

        // Create target directory
        mkdir(rpath2, statbuf.st_mode);

        DIR *dir = opendir(rpath1);
        if (dir == NULL) {
            perror(argv[0]);
            ret = -1;
            goto out;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char src_path[256];
            char dst_path[256];
            snprintf(src_path, sizeof(src_path), "%s/%s", rpath1, entry->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", rpath2, entry->d_name);

            char *new_argv[] = {argv[0], "-r", src_path, dst_path};
            if (cmd_cp(4, new_argv) != 0) {
                ret = -1;
                break;
            }
        }

        closedir(dir);
    } else {
        // Copy single file
        src = fopen(rpath1, "rb");
        if (src == NULL) {
            perror(argv[0]);
            ret = -1;
            goto out;
        }

        dst = fopen(rpath2, "wb");
        if (dst == NULL) {
            perror(argv[0]);
            fclose(src);
            ret = -1;
            goto out;
        }

        while ((nread = fread(buf, 1, sizeof(buf), src)) > 0) {
            fwrite(buf, 1, nread, dst);
        }

        fclose(src);
        fclose(dst);
        printf("file copied: %s -> %s\n", argv[1], argv[2]);
    }

out:
    return ret;
}


int cmd_ps(int argc, char **argv)
{
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char cmdline[256];
    char buffer[4096];     // 임시 데이터 저장 버퍼
    char sendBuffer[65536]; // 클라이언트로 보낼 전체 데이터 저장 버퍼
    size_t sendBufferLen = 0;
    FILE *fp;

    // 시작 문자열 추가
    const char *header = "PROCESS_LIST_START\n";
    size_t headerLen = strlen(header);
    memcpy(sendBuffer, header, headerLen);
    sendBufferLen += headerLen;

    dir = opendir("/proc");
    if (!dir) {
        perror("opendir");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // PID 디렉토리만 처리 (숫자로 된 이름)
        if (entry->d_type == DT_DIR && entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
            int pid = atoi(entry->d_name);
            int ppid = 0;

            // /proc/[pid]/stat 읽기
            snprintf(path, sizeof(path), "/proc/%d/stat", pid);
            fp = fopen(path, "r");
            if (fp) {
                fscanf(fp, "%*d %s %*c %d", cmdline, &ppid);
                fclose(fp);

                // 대괄호 제거
                cmdline[strlen(cmdline) - 1] = '\0';

                // 프로세스 정보를 임시 버퍼에 저장
                int len = snprintf(buffer, sizeof(buffer), "%5d %5d %s\n", pid, ppid, cmdline + 1);

                // 임시 버퍼 내용을 클라이언트로 보낼 버퍼에 추가
                if (sendBufferLen + len < sizeof(sendBuffer)) {
                    memcpy(sendBuffer + sendBufferLen, buffer, len);
                    sendBufferLen += len;
                } else {
                    fprintf(stderr, "Buffer overflow: too much process data\n");
                    closedir(dir);
                    return -1;
                }
            }
        }
    }
    closedir(dir);

    // 클라이언트로 전송
    if (send(client_fd, sendBuffer, sendBufferLen, 0) < 0) {
        perror("send");
        return -1;
    }

    return 0;
}


// int cmd_ps(int argc, char **argv)
// {
//     DIR *dir;
//     struct dirent *entry;
//     char path[256];
//     char cmdline[256];
//     FILE *fp;
    
//     printf("%5s %5s %s\n", "PID", "PPID", "CMD");
    
//     dir = opendir("/proc");
//     if (!dir) {
//         perror("opendir");
//         return -1;
//     }
    
//     while ((entry = readdir(dir)) != NULL) {
//         // PID 디렉토리만 처리 (숫자로 된 이름)
//         if (entry->d_type == DT_DIR && entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
//             int pid = atoi(entry->d_name);
//             int ppid = 0;
            
//             // /proc/[pid]/stat 읽기
//             snprintf(path, sizeof(path), "/proc/%d/stat", pid);
//             fp = fopen(path, "r");
//             if (fp) {
//                 fscanf(fp, "%*d %s %*c %d", cmdline, &ppid);
//                 fclose(fp);
                
//                 // 대괄호 제거
//                 cmdline[strlen(cmdline)-1] = '\0';
//                 printf("%5d %5d %s\n", pid, ppid, cmdline+1);
//             }
//         }
//     }
//     closedir(dir);
//     return 0;
// }

int cmd_kill(int argc, char **argv)
{
    if (argc != 2) {
        return -2;  // 문법 오류
    }
    
    pid_t pid = atoi(argv[1]);
    if (kill(pid, SIGTERM) < 0) {
        perror(argv[0]);
        return -1;
    }
    
    printf("Signal sent to process %d\n", pid);
    return 0;
}

int cmd_quit(int argc, char **argv)
{
    exit(1);
    return 0;
}

void usage_help(void)
{
    printf("help <command>\n");
}

void usage_mkdir(void)
{
    printf("mkdir <directory>\n");
}

void usage_rmdir(void)
{
    printf("rmdir <directory>\n");
}

void usage_cd(void)
{
    printf("cd <directory>\n");
}

void usage_mv(void)
{
    printf("mv <old_name> <new_name>\n");
}

void usage_ln(void)
{
    printf("ln [-s] <link_name> <target_name>\n");
}

void usage_rm(void)
{
    printf("rm <file>\n");
}

void usage_chmod(void)
{
    printf("chmod <mode> <file>\n");
}

void usage_cat(void)
{
    printf("cat <file>\n");
}

void usage_cp(void)
{
    printf("cp <source_file> <destination_file>\n");
}

void usage_kill(void)
{
    printf("kill <pid>\n");
}

void send_info()
{
    DIR *dp;
    struct dirent *dep;
    struct stat statbuf;
    char buffer[4096]; // 개별 데이터를 저장할 임시 버퍼
    char sendBuffer[65536]; // 누적 데이터를 저장할 버퍼 (큰 크기 설정 필요)
    size_t sendBufferLen = 0; // 누적 데이터 길이

    if ((dp = opendir(".")) == NULL) {
        return;
    }

    // 초기화
    memset(sendBuffer, 0, sizeof(sendBuffer));

    while ((dep = readdir(dp))) {
        lstat(dep->d_name, &statbuf);
        char perm_str[10];
        char symlink_str[1024];
        memset(symlink_str, 0, sizeof(symlink_str));
        get_perm_str(statbuf.st_mode, perm_str);
        
        if (S_ISLNK(statbuf.st_mode)) {
            ssize_t len = readlink(dep->d_name, symlink_str, sizeof(symlink_str) - 1);
            if (len != -1) {
                symlink_str[len] = '\0';
            }
        }

        // 데이터를 포맷팅하여 임시 버퍼에 저장
        int len = snprintf(buffer, sizeof(buffer), 
            "%d %c%s %4s %d %d %d %d %d %d %d %s%s%s\n",
            dep->d_ino,
            get_type_char(dep->d_type),
            perm_str,
            get_type_str(dep->d_type), 
            (int)statbuf.st_uid,
            (int)statbuf.st_gid,
            (int)statbuf.st_atim.tv_sec,
            (int)statbuf.st_mtim.tv_sec,
            (int)statbuf.st_ctim.tv_sec,
            (unsigned int)statbuf.st_nlink,
            (int)statbuf.st_size,
            dep->d_name,
            S_ISLNK(statbuf.st_mode) ? " -> " : "",
            S_ISLNK(statbuf.st_mode) ? symlink_str + strlen(chroot_path) : ""
        );

        // 임시 버퍼 내용을 누적 버퍼로 복사
        if (sendBufferLen + len < sizeof(sendBuffer)) {
            memcpy(sendBuffer + sendBufferLen, buffer, len);
            sendBufferLen += len;
        } else {
            // 누적 버퍼가 가득 찬 경우
            fprintf(stderr, "Buffer overflow: too much data to send\n");
            closedir(dp);
            return;
        }
    }

    closedir(dp);

    // 클라이언트 소켓으로 한 번에 전송
    if (send(client_fd, sendBuffer, sendBufferLen, 0) < 0) {
        perror("send");
    }
}

int cmd_exec(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: exec <command> [args...]\n");
        return -2; // Syntax error
    }

    pid_t pid;
    int status;
    char rpath[256]; // 명령어의 절대 경로 저장

    // get_realpath로 명령어 경로 확인
    get_realpath(argv[1], rpath);
    if (access(rpath, X_OK) != 0) {
        fprintf(stderr, "Error: %s is not executable or does not exist.\n", rpath);
        return -1;
    }

    // 자식 프로세스 생성
    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1; // Fork 실패
    }

    if (pid == 0) {
        // 자식 프로세스: 명령 실행
        char *exec_args[argc];
        exec_args[0] = rpath; // 절대 경로로 설정
        for (int i = 2; i < argc; i++) {
            exec_args[i - 1] = argv[i]; // 나머지 인자 복사
        }
        exec_args[argc - 1] = NULL; // execvp에 전달할 NULL 종료

        // 명령 실행
        if (execvp(exec_args[0], exec_args) < 0) {
            perror("execvp"); // 실행 실패 시 에러 출력
            exit(EXIT_FAILURE);
        }
    } else {
        // 부모 프로세스: 자식 프로세스 종료 대기
        printf("Executing process: %s\n", rpath);
        waitpid(pid, &status, 0);

        // 실행 결과 확인
        if (WIFEXITED(status)) {
            printf("Process exited with status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Process terminated by signal: %d\n", WTERMSIG(status));
        }
    }
    return 0;
}