#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#define dot_path "."
#define git_dir ".git"

int check_if_git_repo(const char* const cur_dir) {
    struct stat stat_buf;
    char* git_path;
    unsigned long cur_dir_len;
    int result;

    result = -1;

    cur_dir_len = strlen(cur_dir);
    git_path = malloc(cur_dir_len + strlen(git_dir) + 2);
    if (!git_path)
        goto cleanup;
    strcpy(git_path, cur_dir);
    git_path[cur_dir_len] = '/';
    strcpy(git_path + cur_dir_len + 1, git_dir);

    if (stat(git_path, &stat_buf) == -1) {
        if (errno == ENOENT)
            result = 0;
        goto cleanup;
    }

    result = S_ISDIR(stat_buf.st_mode);

cleanup:
    if (git_path)
        free(git_path);

    return result;
}

char* get_working_dir(void) {
    const char* pwd;
    const char* home;
    char* result_path;
    unsigned long home_len;
    int is_git_dir;

    pwd = getenv("PWD");
    if (!pwd) {
        /* couldn't find pwd, return '.' */
        result_path = malloc(strlen(dot_path) + 1);
        if (!result_path)
            return 0;
        return strcpy(result_path, dot_path);
    }

    if ((is_git_dir = check_if_git_repo(pwd)) == -1) {
        return 0;
    }
    else if (is_git_dir) {
        /* we're in a git repo, return repo name */
        const char* basename;

        basename = strrchr(pwd, '/');
        if (!basename) {
            errno = EDOM;
            return 0;
        }
        basename++;

        result_path = malloc(strlen(basename) + 1);
        if (!result_path)
            return 0;
        return strcpy(result_path, basename);
    }

    home = getenv("HOME");
    if (!home || strstr(pwd, home) != pwd) {
        /* return pwd as is */
        result_path = malloc(strlen(pwd) + 1);
        if (!result_path)
            return 0;
        return strcpy(result_path, pwd);
    }

    /* abbreviate home component of pwd */
    home_len = strlen(home);
    result_path = malloc(strlen(pwd) - home_len + 2);
    if (!result_path)
        return 0;
    result_path[0] = '~';
    strcpy(result_path + 1, pwd + home_len);
    return result_path;
}

int main(void) {
    char* working_dir;

    working_dir = get_working_dir();
    if (!working_dir) {
        perror("failed to get working dir");
        return 1;
    }

    printf("%s\n> ", working_dir);
    free(working_dir);

    return 0;
}
