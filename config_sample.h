#ifndef _CONFIG_H_
#define _CONFIG_H_

/* Macro for generating ansi color codes.
 * Includes ascii values \x01 and \x02 because bash apparently doesn't bother
 * discerning the difference between printable and non-printable chars. */
#define ansi_make_color(color) "\x01" "\x1b[" color "\x02"

#define ansi_bold_cyan ansi_make_color("1;36m")
#define ansi_bold_green ansi_make_color("1;32m")
#define ansi_bold_red ansi_make_color("1;31m")
#define ansi_reset ansi_make_color("0m")

/* Format string for the prompt.
 * Special char sequences:
 *   %d - show current directory (shortened if within home dir or git repo)
 *   %g - show git status if current dir is git repo
 *   %s - print space only if next module (e.g. %d, %s) produces output
 *   %% - print % character */
#define prompt_fmt ansi_bold_cyan "%d" ansi_bold_red "%s%g\n" ansi_bold_green "> " ansi_reset

#endif /* _CONFIG_H_ */
