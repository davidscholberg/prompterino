#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define dot_path "."

char* get_working_dir(void) {
    const char* pwd;
    const char* home;
    char* abbrev_path;
    unsigned long home_len;

    pwd = getenv("PWD");
    if (!pwd) {
        abbrev_path = malloc(strlen(dot_path) + 1);
        if (!abbrev_path)
            return 0;
        return strcpy(abbrev_path, dot_path);
    }

    home = getenv("HOME");
    if (!home || strstr(pwd, home) != pwd) {
        abbrev_path = malloc(strlen(pwd) + 1);
        if (!abbrev_path)
            return 0;
        return strcpy(abbrev_path, pwd);
    }

    home_len = strlen(home);
    abbrev_path = malloc(strlen(pwd) - home_len + 2);
    if (!abbrev_path)
        return 0;
    abbrev_path[0] = '~';
    strcpy(abbrev_path + 1, pwd + home_len);
    return abbrev_path;
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
