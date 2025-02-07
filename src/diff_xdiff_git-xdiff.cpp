extern "C" {
# include <regex.h>

# define xdl_regex_t regex_t
# define xdl_regmatch_t regmatch_t

int xdl_regexec_buf(
	const xdl_regex_t *preg, const char *buf, size_t size,
	size_t nmatch, xdl_regmatch_t pmatch[], int eflags)
{
    pmatch[0].rm_so = 0;
    pmatch[0].rm_eo = (regoff_t)size;

    return regexec(preg, buf, nmatch, pmatch, eflags | REG_STARTEND);
}

} // extern "C"
