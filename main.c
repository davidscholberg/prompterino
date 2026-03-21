#define _POSIX_C_SOURCE 2
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"

#define read_size 1024
#define branch_name_header "# branch.head "
#define branch_ab_header "# branch.ab "
#define stash_header "# stash"
#define branch_not_found_msg "(branch not found)"
#define opening_delim " ["
#define closing_delim "]"
#define dot_path "."
#define git_dir ".git"

#define concat_git_status_str(field) \
    if (git_status.field) { \
        if (status_str) \
            strcpy(status_str + i, git_status.field); \
        i += strlen(git_status.field); \
    }

/* Object representation of a git repo's status.
 * The branch string is the only string with an associated length field since it
 * can potentially point to the middle of the command output string.
 * The status_chars field indicates that one or more of the string fields is
 * non-null (aside from branch, which is always set to some value. */
struct git_status_t {
    const char* branch;
    const char* ahead;
    const char* behind;
    const char* untracked;
    const char* unstaged;
    const char* staged;
    const char* unmerged;
    const char* stashed;
    size_t branch_len;
    int status_chars;
};

/* Execute the given cmd in a shell.
 * The stdout is returned as a string which the caller must free.
 * The cmds's exit status is stored where exit_status points. */
char* execute_cmd(const char* cmd, int* exit_status) {
    FILE* out_stream;
    char* out_str;
    size_t capacity;
    size_t total_bytes;

    out_stream = popen(cmd, "r");
    if (!out_stream)
        return NULL;

    capacity = read_size;

    out_str = malloc(capacity + 1);
    if (!out_str)
        goto cleanup;

    total_bytes = 0;

    while (1) {
        size_t bytes_read;

        bytes_read = fread(out_str + total_bytes, 1, read_size, out_stream);
        total_bytes += bytes_read;

        if (bytes_read < read_size) {
            if (ferror(out_stream))
                goto cleanup_err;

            out_str[total_bytes] = '\0';
            goto cleanup;
        }

        if (total_bytes == capacity) {
            char* new_out_str;

            capacity = capacity * 2;
            new_out_str = realloc(out_str, capacity + 1);

            if (!new_out_str)
                goto cleanup_err;

            out_str = new_out_str;
        }
    }

cleanup_err:
    free(out_str);
    out_str = NULL;

cleanup:
    *exit_status = pclose(out_stream);

    return out_str;
}

/* Parse output of git status into object.
 * The only pointer field guaranteed to be non-null is branch.
 * Parse errors are printed to stderr. */
/* TODO: Parse errors should ideally be reflected in the exit status. */
void parse_git_status(const char* cmd_output, struct git_status_t* git_status) {
    size_t branch_name_header_len;
    size_t branch_ab_header_len;
    size_t stash_header_len;

    git_status->branch = NULL;
    git_status->ahead = NULL;
    git_status->behind = NULL;
    git_status->untracked = NULL;
    git_status->unstaged = NULL;
    git_status->staged = NULL;
    git_status->unmerged = NULL;
    git_status->stashed = NULL;
    git_status->branch_len = 0;
    git_status->status_chars = 0;

    branch_name_header_len = strlen(branch_name_header);
    branch_ab_header_len = strlen(branch_ab_header);
    stash_header_len = strlen(stash_header);

    while (*cmd_output != '\0') {
        /* header line */
        if (*cmd_output == '#') {
            if (!strncmp(cmd_output, branch_name_header, branch_name_header_len)) {
                const char* branch_name_end;

                cmd_output += branch_name_header_len;
                branch_name_end = strchr(cmd_output, '\n');
                if (branch_name_end) {
                    git_status->branch = cmd_output;
                    git_status->branch_len = branch_name_end - cmd_output;
                }
                else {
                    fprintf(stderr, "error: failed to parse branch name\n");
                }
            }
            else if (!strncmp(cmd_output, branch_ab_header, branch_ab_header_len)) {
                const char* ab_ptr;

                ab_ptr = strchr(cmd_output, '+');
                if (!ab_ptr)
                    fprintf(stderr, "error: failed to parse branch ahead\n");
                else if (ab_ptr[1] != '0') {
                    git_status->ahead = "+";
                    git_status->status_chars = 1;
                }

                ab_ptr = strchr(cmd_output, '-');
                if (!ab_ptr)
                    fprintf(stderr, "error: failed to parse branch behind\n");
                else if (ab_ptr[1] != '0') {
                    git_status->behind = "-";
                    git_status->status_chars = 1;
                }
            }
            else if (!strncmp(cmd_output, stash_header, stash_header_len)) {
                git_status->stashed = "$";
                git_status->status_chars = 1;
            }
        }
        /* staged/unstaged file line */
        else if (*cmd_output == '1' || *cmd_output == '2') {
            if (cmd_output[1] == '\0' || cmd_output[2] == '\0' || cmd_output[3] == '\0')
                fprintf(stderr, "error: failed to parse staged/unstaged changes\n");
            else {
                if (cmd_output[2] != '.') {
                    git_status->staged = "&";
                    git_status->status_chars = 1;
                }
                if (cmd_output[3] != '.') {
                    git_status->unstaged = "!";
                    git_status->status_chars = 1;
                }
            }
        }
        /* merge conflict line */
        else if (*cmd_output == 'u') {
            git_status->unmerged = "m";
            git_status->status_chars = 1;
        }
        /* untracked file line */
        else if (*cmd_output == '?') {
            git_status->untracked = "?";
            git_status->status_chars = 1;
        }

        /* advance to next line or end of string */
        while (*cmd_output != '\0') {
            cmd_output++;
            if (*cmd_output == '\n') {
                cmd_output++;
                break;
            }
        }
    }

    if (!git_status->branch) {
        git_status->branch = branch_not_found_msg;
        git_status->branch_len = strlen(branch_not_found_msg);
    }
}

/* Return string representation of git status in current repo.
 * This function will print any errors to stderr.
 * The caller is responsible for freeing the returned string. */
char* get_git_status(void) {
    struct git_status_t git_status;
    const char* cmd_output;
    char* status_str;
    int cmd_status;

    status_str = NULL;

    cmd_output = execute_cmd("git status --porcelain=v2 --branch --show-stash 2>&1", &cmd_status);
    if (!cmd_output) {
        perror("failed to execute git command");
        return NULL;
    }
    if (cmd_status) {
        fprintf(stderr, "error: git status failed:\n%s", cmd_output);
        goto cleanup;
    }

    parse_git_status(cmd_output, &git_status);

    /* This loop runs twice, once to get the size of the string (which is then
     * allocated on the heap) and once to write the string. */
    while (1) {
        size_t i;

        i = 0;

        /* add branch */
        if (status_str) {
            memcpy(status_str + i, git_status.branch, git_status.branch_len);
            status_str[i + git_status.branch_len] = '\0';
        }
        i += git_status.branch_len;

        if (git_status.status_chars) {
            /* add space and opening delim */
            if (status_str)
                strcpy(status_str + i, opening_delim);
            i += strlen(opening_delim);

            concat_git_status_str(ahead);
            concat_git_status_str(behind);
            concat_git_status_str(untracked);
            concat_git_status_str(unstaged);
            concat_git_status_str(staged);
            concat_git_status_str(unmerged);
            concat_git_status_str(stashed);

            /* add closing delim */
            if (status_str)
                strcpy(status_str + i, closing_delim);
            i += strlen(closing_delim);
        }

        if (status_str)
            break;

        status_str = malloc(i + 1);
        if (!status_str)
            break;
    }

cleanup:
    free((void*)cmd_output);

    return status_str;
}

/* Check if current dir is a git repo.
 * On error, this function will print to stderr and return 0. */
int check_for_git_repo(void) {
    struct stat stat_buf;

    if (stat(git_dir, &stat_buf) == -1) {
        if (errno != ENOENT) {
            perror("couldn't stat git dir");
            return 0;
        }
    }

    return S_ISDIR(stat_buf.st_mode);
}

/* Get current working dir (shortened in some cases).
 * On error this function will print to stderr and return NULL.
 * Caller is responsible for freeing returned string. */
char* get_working_dir(int is_git_dir) {
    const char* pwd;
    const char* home;
    char* result_path;
    unsigned long home_len;

    pwd = getenv("PWD");
    if (!pwd) {
        /* couldn't find pwd, return '.' */
        result_path = malloc(strlen(dot_path) + 1);
        if (!result_path) {
            perror("malloc failed");
            return NULL;
        }
        return strcpy(result_path, dot_path);
    }

    if (is_git_dir) {
        /* we're in a git repo, return repo name */
        const char* basename;

        basename = strrchr(pwd, '/');
        if (!basename) {
            fprintf(stderr, "couldn't determine basename of current dir\n");
            return NULL;
        }
        basename++;

        result_path = malloc(strlen(basename) + 1);
        if (!result_path) {
            perror("malloc failed");
            return NULL;
        }
        return strcpy(result_path, basename);
    }

    home = getenv("HOME");
    if (!home || strstr(pwd, home) != pwd) {
        /* return pwd as is */
        result_path = malloc(strlen(pwd) + 1);
        if (!result_path) {
            perror("malloc failed");
            return NULL;
        }
        return strcpy(result_path, pwd);
    }

    /* abbreviate home component of pwd */
    home_len = strlen(home);
    result_path = malloc(strlen(pwd) - home_len + 2);
    if (!result_path) {
        perror("malloc failed");
        return NULL;
    }
    result_path[0] = '~';
    strcpy(result_path + 1, pwd + home_len);
    return result_path;
}

/* Generate the prompt string based on the value of prompt_fmt.
 * Prints to stderr and returns NULL on error.
 * The caller is responsible for freeing the returned string. */
char* get_prompt_str(void) {
    const char* working_dir;
    const char* git_status_str;
    char* prompt_str;
    int is_git_dir;

    working_dir = NULL;
    git_status_str = NULL;
    prompt_str = NULL;
    is_git_dir = -1;

    /* This loop runs twice, once to get the size of the prompt string (which is
     * then allocated on the heap) and once to write the string. */
    while (1) {
        const char* prompt_fmt_ptr;
        size_t i;
        int space_pad;

        prompt_fmt_ptr = prompt_fmt;
        i = 0;
        space_pad = 0;

        while (*prompt_fmt_ptr != '\0') {
            if (*prompt_fmt_ptr == '%') {
                char next_char;

                next_char = *(prompt_fmt_ptr + 1);

                if (next_char == 'd') {
                    if (!prompt_str) {
                        if (is_git_dir == -1)
                            is_git_dir = check_for_git_repo();
                        working_dir = get_working_dir(is_git_dir);
                    }
                    else if (working_dir) {
                        if (space_pad)
                            prompt_str[i] = ' ';
                        strcpy(prompt_str + i + space_pad, working_dir);
                    }
                    if (working_dir)
                        i += space_pad + strlen(working_dir);
                }
                else if (next_char == 'g') {
                    if (is_git_dir == -1)
                        is_git_dir = check_for_git_repo();
                    if (is_git_dir) {
                        if (!prompt_str) {
                            git_status_str = get_git_status();
                        }
                        else if (git_status_str) {
                            if (space_pad)
                                prompt_str[i] = ' ';
                            strcpy(prompt_str + i + space_pad, git_status_str);
                        }
                        if (git_status_str)
                            i += space_pad + strlen(git_status_str);
                    }
                }
                else if (next_char == 's') {
                    space_pad = 2;
                }
                else if (next_char == '%') {
                    if (prompt_str) {
                        prompt_str[i] = '%';
                        prompt_str[i + 1] = '\0';
                    }
                    i++;
                }
                else if (next_char == '\0') {
                    fprintf(stderr, "error: unexpected eof after '%%' in prompt_fmt\n");
                    goto cleanup_err;
                }
                else {
                    fprintf(stderr, "error: unexpected char '%c' after '%%' in prompt_fmt\n", next_char);
                    goto cleanup_err;
                }

                prompt_fmt_ptr += 2;
            }
            else {
                if (prompt_str) {
                    prompt_str[i] = *prompt_fmt_ptr;
                    prompt_str[i + 1] = '\0';
                }
                i++;
                prompt_fmt_ptr++;
            }

            if (space_pad)
                space_pad--;
        }

        if (prompt_str)
            goto cleanup;

        prompt_str = malloc(i + 1);
        if (!prompt_str) {
            perror("malloc failed");
            goto cleanup_err;
        }
    }

cleanup_err:
    free(prompt_str);
    prompt_str = NULL;

cleanup:
    free((void*)git_status_str);
    free((void*)working_dir);

    return prompt_str;
}

int main(void) {
    const char* prompt_str;

    prompt_str = get_prompt_str();
    if (prompt_str)
        printf("%s", prompt_str);
    else
        printf("%s", "$ ");

    free((void*)prompt_str);

    return prompt_str ? EXIT_SUCCESS : EXIT_FAILURE;
}
