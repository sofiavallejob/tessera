/* Checks for dx7lib.h.  usage: libtest [folder with .syx banks]  (the folder is optional) */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "dx7lib.h"

static int fails = 0;
#define CHECK(c, msg) do { if (c) printf("ok:   %s\n", msg); else { printf("FAIL: %s\n", msg); fails++; } } while (0)

static void random_voice(uint8_t *v, uint32_t *seed)
{
    int i;
    for (i = 0; i < DX7_VCED; i++) v[i] = (uint8_t)(dx7_rand(seed) & 0xFF);
    dx7_patch_sanitize(v);
    v[DX7_OP_LEVEL] = 90;       /* make sure it is not silent */
}

static int load_dir(dx7_lib *L, const char *dir, int *nfiles)
{
    DIR *d = opendir(dir);
    struct dirent *de;
    int total = 0;
    if (!d) return 0;
    while ((de = readdir(d))) {
        char p[1024];
        struct stat st;
        FILE *f;
        size_t len;
        uint8_t *buf;
        uint8_t (*vo)[DX7_VCED];
        int n, maxn;
        if (de->d_name[0] == '.') continue;
        snprintf(p, sizeof p, "%s/%s", dir, de->d_name);
        if (stat(p, &st) || !S_ISREG(st.st_mode)) continue;
        f = fopen(p, "rb");
        if (!f) continue;
        len = (size_t)st.st_size;
        buf = malloc(len);
        maxn = (int)(len / 128) + 2;
        vo = malloc((size_t)maxn * DX7_VCED);
        if (fread(buf, 1, len, f) == len) {
            n = dx7_parse_all(buf, len, vo, maxn);
            if (n > 0) { dx7_lib_add_bank(L, de->d_name, vo, n, 1); total += n; (*nfiles)++; }
        }
        fclose(f); free(buf); free(vo);
    }
    closedir(d);
    return total;
}

int main(int argc, char **argv)
{
    uint32_t seed = 12345;
    uint8_t v[40][DX7_VCED], back[40][DX7_VCED], msg[DX7_BANK_SYSEX_LEN], one[DX7_VOICE_SYSEX_LEN];
    int i, n;
    dx7_lib L;

    /* --- pack / sysex round trip on synthetic voices --- */
    for (i = 0; i < 40; i++) random_voice(v[i], &seed);
    dx7_make_bank_sysex(v, 32, msg);
    CHECK(msg[0] == 0xF0 && msg[DX7_BANK_SYSEX_LEN - 1] == 0xF7, "bank message framing");
    { int sum = 0; for (i = 0; i < 4096; i++) sum += msg[6 + i]; CHECK(((sum + msg[4102]) & 0x7F) == 0, "bank checksum"); }
    n = dx7_parse_all(msg, sizeof msg, back, 40);
    CHECK(n == 32, "bank parses back to 32 voices");
    for (i = 0; i < 32; i++) if (memcmp(v[i], back[i], DX7_VCED)) break;
    CHECK(i == 32, "32 voices survive pack/unpack unchanged");
    dx7_make_voice_sysex(v[3], one);
    n = dx7_parse_all(one, sizeof one, back, 4);
    CHECK(n == 1 && !memcmp(v[3], back[0], DX7_VCED), "single voice message round trip");
    { uint8_t two[2 * DX7_BANK_SYSEX_LEN]; memcpy(two, msg, sizeof msg); memcpy(two + sizeof msg, msg, sizeof msg);
      n = dx7_parse_all(two, sizeof two, back, 40); CHECK(n == 40, "two banks in one file are both read (capped at maxn)"); }

    /* --- library on synthetic data --- */
    dx7_lib_init(&L);
    memcpy(back, v, 32 * DX7_VCED);
    memcpy(back[5], back[2], DX7_VCED);                      /* duplicate */
    memset(back[7], 0, DX7_VCED);                            /* silent */
    dx7_lib_add_bank(&L, "BANKA", back, 32, 1);
    CHECK(L.n == 30 && L.skipped_dup == 1 && L.skipped_blank == 1, "duplicates and silent voices are skipped");
    dx7_lib_add_bank(&L, "BANKB", v, 32, 1);
    CHECK(L.n == 30 + 2, "second bank only adds the 2 voices not already seen (the others are duplicates)");
    dx7_lib_rebuild(&L, -1);
    CHECK(L.nview == L.n && L.pos == 0, "view holds everything at first");
    {
        int c0 = dx7_lib_current(&L), c1 = dx7_lib_step(&L, 1), c2 = dx7_lib_step(&L, -1);
        CHECK(c1 == c0 + 1 && c2 == c0, "next / prev");
        dx7_lib_step(&L, -1);
        CHECK(L.pos == L.nview - 1, "prev wraps to the end");
    }
    L.e[10].fav = 1; L.e[11].fav = 1;
    L.f_favs = 1; dx7_lib_rebuild(&L, -1);
    CHECK(L.nview == 2, "favourites filter");
    L.f_favs = 0; L.f_bank = 1; dx7_lib_rebuild(&L, -1);
    CHECK(L.nview == 2, "bank filter");
    L.f_bank = -1;
    memcpy(L.e[4].v + DX7_P_NAME, "Warm Pad  ", 10);
    L.f_text[0] = 0; strcpy(L.f_text, "warm"); dx7_lib_rebuild(&L, -1);
    CHECK(L.nview == 1 && dx7_lib_current(&L) == 4, "search is case-insensitive");
    strcpy(L.f_text, "banka"); dx7_lib_rebuild(&L, -1);
    CHECK(L.nview == 30, "search also matches the bank name");
    L.f_text[0] = 0;
    L.f_algo = L.e[0].v[DX7_P_ALGO] + 1; dx7_lib_rebuild(&L, -1);
    CHECK(L.nview >= 1 && L.e[L.view[0]].v[DX7_P_ALGO] + 1 == L.f_algo, "algorithm filter");
    L.f_algo = 0; dx7_lib_rebuild(&L, -1);
    {
        int b = dx7_lib_bank_step(&L, 1);
        CHECK(b == 1, "bank step goes to the next bank");
        L.f_bank = b; dx7_lib_rebuild(&L, -1);
        CHECK(dx7_lib_bank_step(&L, 1) == 0, "bank step wraps");
    }
    L.f_bank = -1; dx7_lib_rebuild(&L, -1);
    {
        uint8_t u[DX7_VCED];
        int idx;
        memcpy(u, v[1], DX7_VCED); u[DX7_OP_LEVEL] = 77;
        idx = dx7_lib_store(&L, u);
        CHECK(idx == L.n - 1 && L.e[idx].fav && !strcmp(L.bank[L.e[idx].bank], "USER"), "store goes to a USER bank as favourite");
        CHECK(dx7_lib_count_favs(&L) == 3, "favourites counted");
        CHECK(dx7_lib_find_hash(&L, dx7_voice_hash(u)) == idx, "voice found again by hash");
    }

    /* --- morph and mutate --- */
    {
        uint8_t a[DX7_VCED], b[DX7_VCED], m[DX7_VCED];
        random_voice(a, &seed); random_voice(b, &seed);
        dx7_morph(a, b, 0.f, m); CHECK(!memcmp(a, m, DX7_VCED), "morph t=0 gives A");
        dx7_morph(a, b, 1.f, m); CHECK(!memcmp(b, m, DX7_VCED), "morph t=1 gives B");
        dx7_morph(a, b, 0.5f, m);
        CHECK(m[DX7_OP_LEVEL] == (uint8_t)floorf(0.5f * a[DX7_OP_LEVEL] + 0.5f * b[DX7_OP_LEVEL] + 0.5f), "morph blends levels");
        for (i = 0; i < 200; i++) {
            memcpy(m, a, DX7_VCED);
            dx7_mutate(m, 1.f, &seed);
            if (m[DX7_P_ALGO] != a[DX7_P_ALGO]) break;
            { int o; for (o = 0; o < 6; o++) if (m[o * 21 + DX7_OP_COARSE] != a[o * 21 + DX7_OP_COARSE]) i = 1000; }
            if (i >= 1000) break;
        }
        CHECK(i == 200, "mutate keeps algorithm and frequency ratios");
        { uint8_t c[DX7_VCED]; int d = 0; memcpy(c, a, DX7_VCED); dx7_mutate(c, 0.5f, &seed); d = memcmp(c, a, DX7_VCED) != 0;
          CHECK(d, "mutate changes the voice"); }
    }
    /* mutated and morphed voices must render without NaN */
    {
        uint8_t a[DX7_VCED], b[DX7_VCED], m[DX7_VCED];
        float buf[2048];
        int bad = 0, k;
        dx7_synth *s = dx7_new(8, 44100.0);
        for (k = 0; k < 100; k++) {
            random_voice(a, &seed); random_voice(b, &seed);
            dx7_morph(a, b, (k % 11) / 10.f, m);
            dx7_mutate(m, 1.f, &seed);
            dx7_set_patch(s, m);
            dx7_note_on(s, 60, 100);
            dx7_process(s, buf, 2048);
            for (i = 0; i < 2048; i++) if (!(buf[i] == buf[i]) || fabsf(buf[i]) > 8.f) bad++;
            dx7_all_off(s);
        }
        dx7_free(s);
        CHECK(bad == 0, "100 morphed + mutated random voices render finite and bounded");
    }
    dx7_lib_free(&L);

    /* --- real data, if a folder was given --- */
    if (argc > 1) {
        int files = 0, total;
        dx7_lib R;
        dx7_lib_init(&R);
        total = load_dir(&R, argv[1], &files);
        printf("data: %d files, %d voices read, %d kept (%d duplicates, %d blank), %d banks\n",
               files, total, R.n, R.skipped_dup, R.skipped_blank, R.nbanks);
        CHECK(R.n > 0, "real data loads");
        {   /* pack(unpack(x)) must reproduce the voice for every real voice */
            int bad = 0;
            for (i = 0; i < R.n; i++) {
                uint8_t p[128], u[DX7_VCED];
                dx7_pack(R.e[i].v, p);
                dx7_unpack(p, u);
                if (memcmp(u, R.e[i].v, DX7_VCED)) bad++;
            }
            CHECK(bad == 0, "every real voice survives pack/unpack");
        }
        {   /* render a spread of real voices */
            dx7_synth *s = dx7_new(8, 44100.0);
            float buf[4096];
            int bad = 0, k;
            for (k = 0; k < R.n; k += 7) {
                dx7_set_patch(s, R.e[k].v);
                dx7_note_on(s, 48 + (k % 36), 100);
                dx7_process(s, buf, 4096);
                for (i = 0; i < 4096; i++) if (!(buf[i] == buf[i]) || fabsf(buf[i]) > 8.f) bad++;
                dx7_all_off(s);
            }
            dx7_free(s);
            CHECK(bad == 0, "a spread of real voices renders finite and bounded");
        }
        dx7_lib_free(&R);
    }
    printf(fails ? "\n%d FAILED\n" : "\nALL CHECKS PASSED\n", fails);
    return fails != 0;
}
