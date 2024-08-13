#include <stdint.h>
#include <stdio.h>

int moobin_check(FILE *f, int offset, const uint8_t str[], int len)
{
    fseek(f, offset, SEEK_SET);
    if (feof(f) || ferror(f)) {
        printf("feof/ferror\n");
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        uint8_t c = getc(f) & 0xff;
        if (feof(f) || ferror(f)) {
            printf("feof/ferror\n");
            return -1;
        }
        if (c != str[i]) {
            return -2;
        }
    }
    return 0;
}

int moobin_check_nop(FILE *f, int offset, int len)
{
    fseek(f, offset, SEEK_SET);
    if (feof(f) || ferror(f)) {
        printf("feof/ferror\n");
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        uint8_t c = getc(f) & 0xff;
        if (feof(f) || ferror(f)) {
            printf("feof/ferror\n");
            return -1;
        }
        if (c != 0x90) {
            return -2;
        }
    }
    return 0;
}

int moobin_set_nop(FILE *f, int offset, int len)
{
    fseek(f, offset, SEEK_SET);
    if (feof(f) || ferror(f)) {
        printf("feof/ferror\n");
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        putc(0x90, f);
        if (feof(f) || ferror(f)) {
            printf("feof/ferror\n");
            return -1;
        }
    }
    return 0;
}

int moobin_replace(FILE *f, int offset, const uint8_t str[], int len)
{
    fseek(f, offset, SEEK_SET);
    if (feof(f) || ferror(f)) {
        printf("feof/ferror\n");
        return -1;
    }
    for (int i = 0; i < len; ++i) {
        putc(str[i], f);
        if (feof(f) || ferror(f)) {
            printf("feof/ferror\n");
            return -1;
        }
    }
    return 0;
}

typedef struct {
    const uint8_t *match;
    const uint8_t *replace;
    int offset;
    int len;
} moobin_chunk_t;

int moobin_check_chunk(FILE *f, const moobin_chunk_t *chunk)
{
    return moobin_check(f, chunk->offset, chunk->match, chunk->len);
}

int moobin_check_chunk_patched(FILE *f, const moobin_chunk_t *chunk)
{
    if (chunk->replace) {
        return moobin_check(f, chunk->offset, chunk->replace, chunk->len);
    } else {
        return moobin_check_nop(f, chunk->offset, chunk->len);
    }
}

void moobin_apply_chunk(FILE *f, const moobin_chunk_t *chunk)
{
    if (chunk->replace) {
        moobin_replace(f, chunk->offset, chunk->replace, chunk->len);
    } else {
        moobin_set_nop(f, chunk->offset, chunk->len);
    }
}

int moobin_apply_chunk_list(FILE *f, const moobin_chunk_t chunk_list[])
{
    for (int ci = 0; chunk_list[ci].match != NULL; ++ci) {
        const moobin_chunk_t *ch = &chunk_list[ci];
        if (moobin_check_chunk(f, ch)) {
            if (moobin_check_chunk_patched(f, ch)) {
                printf("Wrong file\n");
                return -1;
            } else {
                printf("Warning: Chunk %d has already been applied\n", ci);
            }
        }
    }
    for (int ci = 0; chunk_list[ci].match != NULL; ++ci) {
        moobin_apply_chunk(f, &chunk_list[ci]);
    }
    return 0;
}

int ban_races(FILE *f, uint8_t racei[4], int n)
{
    const uint8_t match0[] = {
        0xEB, 0x7D, 0xB8, 0x0A, 0x00, 0x50, 0x9A, 0xD7, 0x00
    };
    uint8_t replace0[] = {
        0xEB, 0x7D, 0xB8, 0x0A, 0x00, 0x50, 0x9A, 0xD7, 0x00
    };
    int len0 = 9;
    int off0 = 0x31265;
    const uint8_t match1[] = {
        0x07, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03, 0x00,
        0x09, 0x00, 0x04, 0x00, 0x06, 0x00, 0x02, 0x00, 0x08, 0x00
    };
    uint8_t replace1[] = {
        0x07, 0x00, 0x01, 0x00, 0x05, 0x00, 0x00, 0x00, 0x03, 0x00,
        0x09, 0x00, 0x04, 0x00, 0x06, 0x00, 0x02, 0x00, 0x08, 0x00
    };
    int len1 = 20;
    int off1 = 0x23572;

    for (int i = 0; i < n; ++i) {
        int replace_i = 9 - i;
        if (replace1[replace_i << 1] != racei[i]) {
            for (int j = 0; j < replace_i; ++j) {
                if (replace1[j << 1] == racei[i]) {
                    replace1[j << 1] = replace1[replace_i << 1];
                    replace1[replace_i << 1] = racei[i];
                    --replace0[3];
                    break;
                }
            }
        } else {
            --replace0[3];
        }
    }

    const moobin_chunk_t chunk_list[] = {
        {match0, replace0, off0, len0},
        {match1, replace1, off1, len1},
        {NULL, NULL, 0, 0},
    };
    return moobin_apply_chunk_list(f, chunk_list);
}

int main(int argc, char **argv)
{
    int error = 0;
    const char *err_text[] = {
        "",
        "Syntax error",
        "Race ID error",
        "Race ID repetition error",
        "Ban number error",
    };
    uint8_t race_i[4];
    uint8_t num_bans;
    FILE *f = fopen("ORION.EXE", "r+b");
    if (!f) {
        printf("ORION.EXE not found\n");
        return -1;
    }

    printf ("Select the number of bans (1-4)\n");
    if (scanf("%c%*1[\n]", &num_bans) != 1) {
        error = 1;
    };
    num_bans -= '0';
    if (num_bans == 0 || num_bans > 4) {
        error = 4;
    }
    if (!error) {
        for (int i = 0; i < num_bans; ++i) {
            printf ("Select a race to ban (0-9)\n"
                    "HUMAN = 0, MRRSHAN = 1\n"
                    "SILICOID = 2, SAKKRA = 3\n"
                    "PSILON = 4, ALKARI = 5\n"
                    "KLACKON = 6, BULRATHI = 7\n"
                    "MEKLAR = 8, DARLOK = 9\n");
            if (scanf("%c%*1[\n]", &race_i[i]) != 1) {
                error = 1;
                break;
            };
            race_i[i] -= '0';
            if (race_i[i] > 9) {
                error = 2;
                break;
            }
            for (int j = 0; j < i; ++j) {
                if (race_i[i] == race_i[j]) {
                    error = 3;
                    break;
                }
            }
        }
    }
    if (error || ban_races(f, race_i, num_bans)) {
        printf("Fail\n");
        if (error) {
            printf("%s\n", err_text[error]);
        }
        getc(stdin);
    }
    if (f) {
        fclose(f);
    }
    return 0;
}
