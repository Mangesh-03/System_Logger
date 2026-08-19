/*

    // argc = 1
    // argv[0]
    ./myexe

    // argc = 2
    // argv[0] argv[1]
    ./myexe /home/Demo

    // argc = 3
    // argv[0] argv[1] argv[2]
    ./myexe /home/Demo 5

    BUILD:
        gcc -Wall -Wextra -pthread marvellous_logger.c -o myexe
    RUN:
        ./myexe [disk_path] [interval_seconds]

*/

//////////////////////////////////////////////////////////////////
//
// Header file inclusion
//
//////////////////////////////////////////////////////////////////
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/statvfs.h>

//////////////////////////////////////////////////////////////////
//
// Global variable creation
//
//////////////////////////////////////////////////////////////////

static volatile sig_atomic_t stop_flag = 0;

// ctrl+c handler
static void sigint_handler(int sig)
{
    (void)sig;
    write(STDOUT_FILENO, "\nMarvellous System Logger is terminating...\n", 44);

    // Tell the threads to stop the execution
    stop_flag = 1;
}


// structure which holds all system information
typedef struct
{
    double cpu;
    // CPU usage percentage
    double mem;
    // RAM usage percentage
    double disk; // Harddisk usage percentage
} Snapshot;

// Global object which holds information
static Snapshot snap;

// Mutex lock for critical section
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

// Default path for disk
static char *disk_path = "/";

// Sleep timer for log
static int interval_sec = 2;

//////////////////////////////////////////////////////////////////
//
// Helper functions definitions
//
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
// Timestamp helper
//////////////////////////////////////////////////////////////////

static void timestamp(char *out, size_t sz)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(out, sz, "%Y-%m-%d %H:%M:%S", &t);
}

//////////////////////////////////////////////////////////////////
// CPU helpers (/proc/stat)
// CPU% = (delta_total - delta_idle) / delta_total * 100
//////////////////////////////////////////////////////////////////

static int read_cpu(unsigned long long *total, unsigned long long *idle_all)
{
    FILE *fp = fopen("/proc/stat", "r");

    if (!fp) 
        return -1;

    char line[512];

    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    unsigned long long user = 0, nice = 0, sys = 0, 
    idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    
    int n = sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                    &user, &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);


    if (n < 4) return -1;

    *idle_all = idle + iowait;

    *total = user + nice + sys + idle + iowait + irq + softirq + steal;

    return 0;
}

//////////////////////////////////////////////////////////////////
// Function to collect CPU information
//////////////////////////////////////////////////////////////////

static double cpu_percent(void)
{
    unsigned long long t1 = 0, i1 = 0, t2 = 0, i2 = 0;
    
    if (read_cpu(&t1, &i1) != 0) return 0.0;
    
    // NOTE: We measure CPU delta over 1 second (teaching-friendly)
    for (int i = 0; i < 1 && !stop_flag; i++) sleep(1);
    
    if (stop_flag) return 0.0;
    
    if (read_cpu(&t2, &i2) != 0) return 0.0;

    unsigned long long dt = t2 - t1;
    unsigned long long di = i2 - i1;

    if (dt == 0) return 0.0;

    return ((double)(dt - di) / (double)dt) * 100.0;

}

//////////////////////////////////////////////////////////////////
// Function to collect memory information (/proc/meminfo)
// Mem% = (MemTotal - MemAvailable) / MemTotal * 100
//////////////////////////////////////////////////////////////////
static double mem_percent(void)
{
    FILE *fp = fopen("/proc/meminfo", "r");

    if (!fp) return 0.0;
    
    unsigned long long total = 0, avail = 0;
    char key[64], unit[32];
    unsigned long long val = 0;
    
    while (fscanf(fp, "%63s %llu %31s", key, &val, unit) == 3)
    {
        if (strcmp(key, "MemTotal:") == 0) total = val;
        else if (strcmp(key, "MemAvailable:") == 0) avail = val;
        
        if (total && avail) break;
    }

    fclose(fp);
    
    if (total == 0) return 0.0;
    
    return ((double)(total - avail) / (double)total) * 100.0;
}

//////////////////////////////////////////////////////////////////
// Function to collect disk information (statvfs)
// Disk% = (Total - Free) / Total * 100
//////////////////////////////////////////////////////////////////
static double disk_percent(char *path)
{
    struct statvfs v;

    if (statvfs(path, &v) != 0) return 0.0;

    unsigned long long total = (unsigned long long)v.f_blocks * (unsigned long long)v.f_frsize;
    unsigned long long freeb = (unsigned long long)v.f_bavail * (unsigned long long)v.f_frsize;
    
    if (total == 0) return 0.0;
    
    return ((double)(total - freeb) / (double)total) * 100.0;
}

//////////////////////////////////////////////////////////////////
// Thread proc for thread which collects system information
//////////////////////////////////////////////////////////////////
static void *collector_thread(void *arg)
{
    (void)arg;

    double c = 0.0, m = 0.0, d = 0.0;
    
    printf("Inside collector thread\n");
    
    // Enter if ctrl+c is not arrived
    while (!stop_flag)
    {
        // Calculate the current resource usage
        c = cpu_percent();
        // includes 1 second measurement sleep
   
        if (stop_flag) break;
   
        m = mem_percent();
        d = disk_percent(disk_path);
    
        // Start the critical section
        pthread_mutex_lock(&mtx);
    
        snap.cpu = c;
        snap.mem = m;
        snap.disk = d;
    
        // End of critical section
        pthread_mutex_unlock(&mtx);

    // No extra sleep here. Logger thread controls logging frequency.

    }
    return NULL;
}

//////////////////////////////////////////////////////////////////
// Thread proc for thread which writes log
//////////////////////////////////////////////////////////////////
static void *logger_thread(void *arg)
{
    (void)arg;
    printf("Inside logger thread\n");
    
    int fd = open("Marvellous_log.txt", O_CREAT | O_WRONLY | O_APPEND, 0666);
    
    if (fd < 0)
    {
        perror("open(Marvellous_log.txt)");
        return NULL;
    }

    // Log header with timestamp
    char ts[64];
    timestamp(ts, sizeof(ts));
    char header[512];

    int hn = snprintf(header, sizeof(header),
                    "\n================ Marvellous System Logger ================\n"
                                    "Log created at: %s\n"
                    "Disk path: %s | Interval: %d sec\n"
                    "==========================================================\n",ts, disk_path, interval_sec);

    write(fd, header, (size_t)hn);

    // Enter if ctrl+c is not arrived

    while (!stop_flag)

    {

        double c = 0.0, m = 0.0, d = 0.0;


        // Read shared snapshot safely

        pthread_mutex_lock(&mtx);
        d = snap.disk;
        c = snap.cpu;
        m = snap.mem;
        pthread_mutex_unlock(&mtx);

        // Prepare line with timestamp
        timestamp(ts, sizeof(ts));
        char line[256];

        int n = snprintf(line, sizeof(line),
            "[%s] CPU: %6.2f%% | MEM: %6.2f%% | DISK(%s): %6.2f%%\n",ts, c, m, disk_path, d);

        // Terminal + file
        printf("%s", line);
        write(fd, line, (size_t)n);
    }

    // Sleep for interval (but allow clean stop quickly)
    for (int i = 0; i < interval_sec && !stop_flag; i++)
    {
        sleep(1);
    }


    // Footer with timestamp

    timestamp(ts, sizeof(ts));
    
    char footer[256];
    
    int fn = snprintf(footer, sizeof(footer),
                "================= Logger Stopped  ==================================\n"
                                    "Stopped at: %s\n"
                "=============================================\n ",ts);
    write(fd, footer, (size_t)fn);

    close(fd);
    return NULL;
}

//////////////////////////////////////////////////////////////////
//
// Entry point function of project
//
//////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    // Argument parsing as covered in class
    // ./myexe /home/Demo
    if (argc == 2)
    {
        disk_path = argv[1];
    }
    // ./myexe /home/Demo 5
    else if (argc == 3)
    {
        disk_path = argv[1];
        interval_sec = atoi(argv[2]);
        
        // Validation
        if (interval_sec <= 0) interval_sec = 2;
    }
    // else argc == 1 -> defaults
    printf("Marvellous System Logger\n");
    printf("Path is : %s\n", disk_path);
    printf("Interval is : %d\n", interval_sec);
    printf("Press Ctrl+C to stop...\n");

    // Structure for handling ctrl+c
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = sigint_handler;
    
    sigaction(SIGINT, &sa, NULL);
    
    // Thread to collect the information
    pthread_t t_collect;
    
    // Thread to write the data into log
    pthread_t t_log;

    // Create thread to collect information
    if (pthread_create(&t_collect, NULL, collector_thread, NULL) != 0)
    {
        perror("pthread_create(t_collect)");
        return 1;
    }

    // Create thread to write data into log
    if (pthread_create(&t_log, NULL, logger_thread, NULL) != 0)
    {
        perror("pthread_create(t_log)");
        stop_flag = 1;

        pthread_join(t_collect, NULL);
        return 1;
    }
    
    // Main thread waiting for child threads to terminate
    pthread_join(t_collect, NULL);
    pthread_join(t_log, NULL);
    printf("Terminating the Marvellous System Logger\n");
    printf("Log saved in Marvellous_log.txt\n");

    return 0;
}