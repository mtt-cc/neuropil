#include <criterion/redirect.h>

int cr_file_match_str(FILE* f, const char *str) {
    size_t len = strlen(str);

    char buf[512];
    size_t read;
    int matches = 0;
    while ((read = fread(buf, 1, sizeof (buf), f)) > 0) {
        matches = !strncmp(buf, str, read);
        if (!matches || read > len) {
            matches = 0;
            break;
        }

        len -= read;
        str += read;
    }

    // consume the rest of what's available
    while (fread(buf, 1, sizeof (buf), f) > 0);

    return matches;
}

int cr_file_match_file(FILE* f, FILE* ref) {
    if (f == ref)
        return true;

    char buf1[512];
    char buf2[512];

    fpos_t orig_pos;
    fgetpos(ref, &orig_pos);
    rewind(ref);

    size_t read1 = 1, read2 = 1;
    int matches = 0;
    while ((read1 = fread(buf1, 1, sizeof (buf1), f)) > 0
        && (read2 = fread(buf2, 1, sizeof (buf2), ref)) > 0) {

        if (read1 != read2) {
            matches = 0;
            break;
        }

        matches = !memcmp(buf1, buf2, read1);
    }

    // consume the rest of what's available
    while (fread(buf1, 1, sizeof (buf1), f) > 0);

    fsetpos(ref, &orig_pos);

    return matches;
}
