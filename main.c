#ifdef _WIN32

#include <direct.h>

#define popen _popen
#define pclose _pclose
#define S_ISDIR(sb) (((sb) & _S_IFDIR) == _S_IFDIR)
#define path_sep '\\'
#define home_env_var "USERPROFILE"

#else

#define _POSIX_C_SOURCE 2
#define path_sep '/'
#define home_env_var "HOME"

#endif

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
#define git_opening_delim_padded " " git_opening_delim
#define dot_path "."
#define git_dir ".git"

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

/* Helper for doing 2-pass string building.
 * General usage steps:
 * Set the str field to NULL before you begin the 2-pass loop.
 * Inside the 2-pass loop at the top, set the pos field to 0, then call
 * strb_append to append strings to the builder.
 * At the bottom of the 2-pass loop, exit the loop if the str field is non-NULL
 * (this means we're at the end of the second pass), otherwise allocate a buffer
 * pos + 1 in size and assign it to str. */
struct strb_t {
    char* str;
    size_t pos;
};

/* Append string to string builder from within a 2-pass loop.
 * In the first pass, only pos will be updated. In the second pass, the str will
 * be appended to. */
void strb_append(struct strb_t* builder, const char* source, size_t size) {
    if (builder->str) {
        memcpy(builder->str + builder->pos, source, size);
        builder->str[builder->pos + size] = '\0';
    }

    builder->pos += size;
}

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

    capacity = 0;
    total_bytes = 0;
    out_str = NULL;

    while (1) {
        size_t bytes_read;

        if (total_bytes == capacity) {
            char* new_out_str;

            capacity = capacity ? (capacity * 2) : read_size;
            new_out_str = realloc(out_str, capacity + 1);

            if (!new_out_str)
                goto cleanup_err;

            out_str = new_out_str;
        }

        bytes_read = fread(out_str + total_bytes, 1, read_size, out_stream);
        total_bytes += bytes_read;

        if (bytes_read < read_size) {
            if (ferror(out_stream))
                goto cleanup_err;

            out_str[total_bytes] = '\0';
            goto cleanup;
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
                if (!ab_ptr || ab_ptr[1] == '\0')
                    fprintf(stderr, "error: failed to parse branch ahead\n");
                else if (ab_ptr[1] != '0') {
                    git_status->ahead = git_ahead_str;
                    git_status->status_chars = 1;
                }

                ab_ptr = strchr(cmd_output, '-');
                if (!ab_ptr || ab_ptr[1] == '\0')
                    fprintf(stderr, "error: failed to parse branch behind\n");
                else if (ab_ptr[1] != '0') {
                    git_status->behind = git_behind_str;
                    git_status->status_chars = 1;
                }
            }
            else if (!strncmp(cmd_output, stash_header, stash_header_len)) {
                git_status->stashed = git_stashed_str;
                git_status->status_chars = 1;
            }
        }
        /* staged/unstaged file line */
        else if (*cmd_output == '1' || *cmd_output == '2') {
            if (cmd_output[1] == '\0' || cmd_output[2] == '\0' || cmd_output[3] == '\0')
                fprintf(stderr, "error: failed to parse staged/unstaged changes\n");
            else {
                if (cmd_output[2] != '.') {
                    git_status->staged = git_staged_str;
                    git_status->status_chars = 1;
                }
                if (cmd_output[3] != '.') {
                    git_status->unstaged = git_unstaged_str;
                    git_status->status_chars = 1;
                }
            }
        }
        /* merge conflict line */
        else if (*cmd_output == 'u') {
            git_status->unmerged = git_unmerged_str;
            git_status->status_chars = 1;
        }
        /* untracked file line */
        else if (*cmd_output == '?') {
            git_status->untracked = git_untracked_str;
            git_status->status_chars = 1;
        }

        /* advance to next line */
        cmd_output = strchr(cmd_output, '\n');
        if (!cmd_output)
            break;
        cmd_output++;
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
    struct strb_t builder;
    int cmd_status;

    builder.str = NULL;

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
        builder.pos = 0;

        /* add branch */
        strb_append(&builder, git_status.branch, git_status.branch_len);

        if (git_status.status_chars) {
            /* add space and opening delim */
            strb_append(&builder, git_opening_delim_padded, strlen(git_opening_delim_padded));

            if (git_status.ahead)
                strb_append(&builder, git_status.ahead, strlen(git_status.ahead));
            if (git_status.behind)
                strb_append(&builder, git_status.behind, strlen(git_status.behind));
            if (git_status.untracked)
                strb_append(&builder, git_status.untracked, strlen(git_status.untracked));
            if (git_status.unstaged)
                strb_append(&builder, git_status.unstaged, strlen(git_status.unstaged));
            if (git_status.staged)
                strb_append(&builder, git_status.staged, strlen(git_status.staged));
            if (git_status.unmerged)
                strb_append(&builder, git_status.unmerged, strlen(git_status.unmerged));
            if (git_status.stashed)
                strb_append(&builder, git_status.stashed, strlen(git_status.stashed));

            /* add closing delim */
            strb_append(&builder, git_closing_delim, strlen(git_closing_delim));
        }

        if (builder.str)
            break;

        builder.str = malloc(builder.pos + 1);
        if (!builder.str)
            break;
    }

cleanup:
    free((void*)cmd_output);

    return builder.str;
}

/* Check if current dir is a git repo.
 * On error, this function will print to stderr and return 0. */
int check_for_git_repo(void) {
    struct stat stat_buf;

    if (stat(git_dir, &stat_buf) == -1) {
        if (errno != ENOENT)
            perror("couldn't stat git dir");
        return 0;
    }

    return S_ISDIR(stat_buf.st_mode);
}

/* Get heap allocated string containing current dir path.
 * The caller must free this string. */
char* get_current_dir(void) {
#ifdef _WIN32
    return _getcwd(NULL, 0);
#else
    char* current_dir;
    char* pwd;

    current_dir = NULL;

    pwd = getenv("PWD");
    if (pwd) {
        current_dir = malloc(strlen(pwd) + 1);
        if (current_dir)
            strcpy(current_dir, pwd);
    }

    return current_dir;
#endif
}

/* Get current working dir (shortened in some cases).
 * On error this function will print to stderr and return NULL.
 * Caller is responsible for freeing returned string. */
char* get_directory(int is_git_dir) {
    const char* home;
    char* result_path;
    unsigned long home_len;

    result_path = get_current_dir();
    if (!result_path) {
        /* couldn't find cwd, return '.' */
        result_path = malloc(strlen(dot_path) + 1);
        if (!result_path) {
            perror("malloc failed");
            goto cleanup_err;
        }
        return strcpy(result_path, dot_path);
    }

    if (is_git_dir) {
        /* we're in a git repo, return repo name */
        const char* basename;

        basename = strrchr(result_path, path_sep);
        if (!basename) {
            fprintf(stderr, "couldn't determine basename of current dir\n");
            goto cleanup_err;
        }
        basename++;

        memmove(result_path, basename, strlen(basename) + 1);
        return result_path;
    }

    home = getenv(home_env_var);
    if (!home || strstr(result_path, home) != result_path) {
        /* return cwd as is */
        return result_path;
    }

    /* abbreviate home component of cwd */
    home_len = strlen(home);
    result_path[0] = '~';
    memmove(result_path + 1, result_path + home_len, strlen(result_path) - home_len + 1);
    return result_path;

cleanup_err:
    free((void*)result_path);
    return NULL;
}

/* Generate the prompt string based on the value of prompt_fmt.
 * Prints to stderr and returns NULL on error.
 * The caller is responsible for freeing the returned string. */
char* get_prompt_str(void) {
    const char* working_dir;
    const char* git_status_str;
    struct strb_t builder;
    int first_pass;
    int is_git_dir;

    working_dir = NULL;
    git_status_str = NULL;
    builder.str = NULL;
    first_pass = 1;
    is_git_dir = -1;

    /* This loop runs twice, once to get the size of the prompt string (which is
     * then allocated on the heap) and once to write the string. */
    while (1) {
        const char* prompt_fmt_ptr;
        int space_pad;

        prompt_fmt_ptr = prompt_fmt;
        builder.pos = 0;
        space_pad = 0;

        while (*prompt_fmt_ptr != '\0') {
            if (*prompt_fmt_ptr == '%') {
                char next_char;

                next_char = *(prompt_fmt_ptr + 1);

                if (next_char == 'd') {
                    if (first_pass) {
                        if (is_git_dir == -1)
                            is_git_dir = check_for_git_repo();
                        working_dir = get_directory(is_git_dir);
                    }
                    if (working_dir) {
                        if (space_pad)
                            strb_append(&builder, " ", 1);
                        strb_append(&builder, working_dir, strlen(working_dir));
                    }
                }
                else if (next_char == 'g') {
                    if (is_git_dir == -1)
                        is_git_dir = check_for_git_repo();
                    if (is_git_dir) {
                        if (first_pass)
                            git_status_str = get_git_status();
                        if (git_status_str) {
                            if (space_pad)
                                strb_append(&builder, " ", 1);
                            strb_append(&builder, git_status_str, strlen(git_status_str));
                        }
                    }
                }
                else if (next_char == 's') {
                    space_pad = 2;
                }
                else if (next_char == '%') {
                    strb_append(&builder, "%", 1);
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
                strb_append(&builder, prompt_fmt_ptr, 1);
                prompt_fmt_ptr++;
            }

            if (space_pad)
                space_pad--;
        }

        if (!first_pass)
            goto cleanup;

        builder.str = malloc(builder.pos + 1);
        if (!builder.str) {
            perror("malloc failed");
            goto cleanup_err;
        }

        if (first_pass)
            first_pass = 0;
    }

cleanup_err:
    free(builder.str);
    builder.str = NULL;

cleanup:
    free((void*)git_status_str);
    free((void*)working_dir);

    return builder.str;
}

int main(void) {
    const char* prompt_str;
    int success;

    prompt_str = get_prompt_str();
    if (prompt_str) {
        printf("%s", prompt_str);
        success = EXIT_SUCCESS;
    }
    else {
        printf("%s", "$ ");
        success = EXIT_FAILURE;
    }

    free((void*)prompt_str);

    return success;
}
