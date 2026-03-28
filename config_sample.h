#ifndef _CONFIG_H_
#define _CONFIG_H_

#define ascii_start_of_heading "\x01"
#define ascii_start_of_text "\x02"

/* Macro for generating ansi color codes.
 * Includes ascii values \x01 and \x02 because bash apparently doesn't bother
 * discerning the difference between printable and non-printable chars. */
#define ansi_make_color(color) ascii_start_of_heading "\x1b[" color ascii_start_of_text

#define ansi_bold_cyan ansi_make_color("1;36m")
#define ansi_bold_green ansi_make_color("1;32m")
#define ansi_bold_red ansi_make_color("1;31m")
#define ansi_reset ansi_make_color("0m")

/* Format string for the prompt.
 * Special char sequences:
 *   %d - show current directory (shortened if within home dir or git repo)
 *   %g - show git status summary if current dir is git repo
 *   %s - print space only if there's previous prompt text and the next module
 *          (e.g. %d, %g) produces output.
 *   %% - print % character */
#define prompt_fmt ansi_bold_cyan "%d" ansi_bold_red "%s%g\n" ansi_bold_green "> " ansi_reset

/* Indicator strings for the git status output. */
#define git_opening_delim "["
#define git_closing_delim "]"
#define git_ahead_str "+"
#define git_behind_str "-"
#define git_untracked_str "?"
#define git_unstaged_str "!"
#define git_staged_str "&"
#define git_unmerged_str "m"
#define git_stashed_str "$"

#endif /* _CONFIG_H_ */
