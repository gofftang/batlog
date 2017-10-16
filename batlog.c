#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#define PREFIX_ROOT "/sys/class/power_supply"
#define CURRENT_NOW "current_now"
#define VOLTAGE_NOW "voltage_now"
#define CAPACITY    "capacity"

static int capture_time = 1;
static int capture_short = 0;
static char capture_file[256] = {"./batlog.txt"};
static int capture_run = 1;
static int verbose = 0;

static FILE* in_volt;
static FILE* in_cur;
static FILE* in_cap;
static FILE* out;

static char capacity[32] = {"-1"};
static char charger[32] = {"battery"};
static char battery[32] = {"battery"};

static void close_clear(FILE** pstream);
static char* fread_no_newline(char*s, int size, FILE* stream);

static void usage(const char* exec)
{
    // batd [-v] [-t argument] [-o argument] -[c argument] [-b argument] [-h]
    printf("Usage: %s [-t argument] [-o argument] -[c argument] [-b argument]\n"
           "  -v verbosely processes\n"
           "  -s short message\n"
           "  -t set the capture time (unit: sec)\n"
           "  -o set the output file\n"
           "  -c set the charger name\n"
           "  -b set the battery name\n"
           "  -h see the usage\n",
           exec);
}

int main(int argc, char *argv[]) {
    int opt;
    int got, put;
    char tim[32];
    char cap[32];
    char cur[32];
    char volt[32];
    char* pcap;
    char* pcur;
    char* pvolt;
    char buf[256];
    time_t time_now;
    struct tm* ptm;
    int title;
    char current_path[128];
    char voltage_path[128];
    char capacity_path[128];

    while ((opt = getopt(argc, argv, "vst:o:c:b:h")) != -1) {
        switch (opt) {
        case 'v':
            verbose = 1;
            break;
        case 's':
            capture_short = 1;
            break;
        case 't':
            capture_time = atoi(optarg);
            break;
        case 'o':
            strncpy(capture_file, optarg, sizeof(capture_file));
            break;
        case 'c':
            strncpy(charger, optarg, sizeof(charger));
            break;
        case 'b':
            strncpy(battery, optarg, sizeof(battery));
            break;
        case 'h':
        default:
            usage(argv[0]);
            return 0;
        }
    }

    printf("Verbose %d\n", verbose);
    printf("Time    %d sec\n", capture_time);
    printf("Output  %s\n", capture_file);
    printf("Capture start.\n");

    sprintf(current_path, "%s/%s/%s", PREFIX_ROOT, charger, CURRENT_NOW);
    sprintf(voltage_path, "%s/%s/%s", PREFIX_ROOT, charger, VOLTAGE_NOW);
    sprintf(capacity_path, "%s/%s/%s", PREFIX_ROOT, battery, CAPACITY);

    title = 1;
    while (capture_run) {

        in_cap = fopen(capacity_path, "r");
        if (!in_cap) {
            printf("open %s fail\n", capacity_path);
            goto again;
        }

        if ((pcap = fread_no_newline(cap, sizeof(cap), in_cap))) {
            if (verbose) {
                printf("capacity: %s => %s ?\n", capacity, cap);
            }

            if (strcmp(cap, capacity) != 0) {

                in_cur = fopen(current_path, "r");
                if (!in_cur) {
                    printf("open %s fail\n", current_path);
                    goto again;
                }

                in_volt = fopen(voltage_path, "r");
                if (!in_volt) {
                    printf("open %s fail\n", voltage_path);
                    goto again;
                }

                out = fopen(capture_file, "a");
                if (!out) {
                    printf("open %s fail\n", capture_file);
                    goto again;
                }

                if (title) {
                    if (capture_short) {
                        sprintf(buf, "\n"
                            "+----------+------+------+------+\n"
                            "|   Time   | C(A) | V(V) | C(%%) |\n"
                            "+----------+------+------+------+\n");
                    } else {
                        sprintf(buf, "\n"
                            "+----------+-------------+-------------+-------------+\n"
                            "|   Time   | Current(uA) | Voltage(uV) | Capacity(%%) |\n"
                            "+----------+-------------+-------------+-------------+\n");
                    }
                    got = strlen((const char*)buf);
                    put = fwrite(buf, 1, got, out);
                    title = 0;
                }

                strcpy(capacity, cap);

                time(&time_now);
                ptm = gmtime(&time_now);

                memset(tim, 0, sizeof(tim));
                sprintf(tim, "%02d:%02d:%02d",
                        ptm->tm_hour, ptm->tm_min, ptm->tm_sec);

                memset(cur, 0, sizeof(cur));
                fread_no_newline(cur, sizeof(cur), in_cur);

                memset(volt, 0, sizeof(volt));
                fread_no_newline(volt, sizeof(volt), in_volt);

                memset(buf, 0, sizeof(buf));
                if (capture_short) {
                    float fcur = atoi(cur) / 1000000.0;
                    float fvolt = atoi(volt) / 1000000.0;

                    sprintf(buf, ""
                            "| %8s | %1.2f | %1.2f | %4s |\n"
                            "+----------+------+------+------+\n",
                            tim, fcur, fvolt, cap);
                } else {
                    sprintf(buf, ""
                        "| %8s | %11s | %11s | %11s |\n"
                        "+----------+-------------+-------------+-------------+\n",
                        tim, cur, volt, cap);
                }

                got = strlen((const char*)buf);
                put = fwrite(buf, 1, got, out);

                if (verbose) {
                    printf("time_now:    %s\n", tim);
                    printf("current_now: %s\n", cur);
                    printf("voltage_now: %s\n", volt);
                    printf("capacity:    %s\n", cap);
                    // printf("got %d put %d\n", got, put);
                }
            }
        }
again:
        close_clear(&in_cap);
        close_clear(&in_cur);
        close_clear(&in_volt);
        close_clear(&out);

        sleep(capture_time);
    }

    return 0;
}

static void close_clear(FILE** pstream)
{
    if (*pstream) {
        fclose(*pstream);
        *pstream = 0;
    }
}

static char* fread_no_newline(char*buf, int size, FILE* stream)
{
    int got;

    got = fread(buf, 1, size, stream);
    if (got > 1) {
        if (buf[got - 1] == '\n')
            buf[got - 1] = '\0';
    }

    return buf;
}
