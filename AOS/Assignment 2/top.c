#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <utmp.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdbool.h>

#define NAME_WIDTH 8

int get_no_of_users() {
  FILE * ufp;
  int numberOfUsers = 0;
  struct utmp usr;
  ufp = fopen("/var/run/utmp", "r");
  while (fread((char * ) & usr, sizeof(usr), 1, ufp) == 1) {
    if ( * usr.ut_name && * usr.ut_line && * usr.ut_line != '~') {
      numberOfUsers++;
    }
  }
  fclose(ufp);
  return numberOfUsers;
}

int * get_processes_count() {

  //Following array will be used to store number of processes in respective order
  //{total, running, sleeping, stopped, zombie}

  int * counts = malloc(sizeof(int) * 5);
  for (int i = 0; i < 5; ++i) {
    counts[i] = 0;
  }

  struct dirent * entry;
  DIR * dp = opendir("/proc");
  if (dp == NULL) {
    fprintf(stderr, "Cannot open directory: /proc\n");
    exit(1);
  }
  errno = 0;
  while ((entry = readdir(dp)) != NULL) {
    if (entry == NULL && errno != 0) {
      perror("readdir failed");
      exit(1);
    } else {
      if (entry -> d_name[0] == '.' || entry -> d_name[0] < '1' || entry -> d_name[0] > '9')
        continue;
    }

    //Entry is valid so increment total count
    counts[0] = counts[0] + 1;

    //Logic to increment the count based on status of process
    char * path = malloc(sizeof(char) * 100);
    char str[50];

    // Making path for file
    strcpy(path, "/proc/");
    strcat(path, entry -> d_name);
    strcat(path, "/stat");

    int fd = open(path, O_RDONLY);
    read(fd, str, 50);

    // String tokenization to find status of process		
    char * token = strtok(str, " ");
    while (token != NULL) {
      if (strcmp(token, "R") == 0) {
        counts[1] = counts[1] + 1;
      } else if ((strcmp(token, "S") == 0) || (strcmp(token, "D") == 0)) {
        counts[2] = counts[2] + 1;
      } else if (strcmp(token, "T") == 0) {
        counts[3] = counts[3] + 1;
      } else if (strcmp(token, "Z") == 0) {
        counts[4] = counts[4] + 1;
      }
      token = strtok(NULL, " ");
    }

    close(fd);
    free(path);

  }
  closedir(dp);
  return counts;
}

long * get_mem_info() {

  // Following array will be used to store memory information in following order
  // {memTotal, memFree, memUsed, buffCache, swapTotal, swapFree, swapUsed, availMem}

  long * counts = malloc(sizeof(long) * 8);

  FILE * fp = fopen("/proc/meminfo", "r");
  char buff[200];
  while (fgets(buff, 200, fp) != NULL) {
    char * token = strtok(buff, " ");
    while (token != NULL) {
      if (strcmp(token, "MemTotal:") == 0) {
        counts[0] = atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "MemFree:") == 0) {
        counts[1] = atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "Buffers:") == 0) {
        counts[3] = atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "Cached:") == 0) {
        counts[3] = counts[3] + atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "SwapTotal:") == 0) {
        counts[4] = atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "SwapFree:") == 0) {
        counts[5] = atol(strtok(NULL, " "));
        break;
      } else if (strcmp(token, "MemAvailable:") == 0) {
        counts[7] = atol(strtok(NULL, " "));
        break;
      }
      token = strtok(NULL, " ");
    }
  }
  counts[2] = counts[0] - counts[1] - counts[3];
  counts[6] = counts[4] - counts[5];
  fclose(fp);
  return counts;
}

char ** get_process_attr(char * pid) {

  // Following array will be used for storing process' attributes in following order
  // {pid, user, pr, ni, virt, res, shr, s, cpu, mem, time, command}
  char ** data = malloc(sizeof(char * ) * 12);
  for (int i = 0; i < 12; ++i) {
    data[i] = malloc(sizeof(char) * 100);
    memset(data[i], '\0', 100);
  }

  // Make path for files
  char stat[20];
  strcpy(stat, "/proc/");
  strcat(stat, pid);
  strcat(stat, "/stat");

  char status[20];
  strcpy(status, "/proc/");
  strcat(status, pid);
  strcat(status, "/status");

  // Copy pid to array
  strcpy(data[0], pid);

  // Logic to get uid, virt, res, shr

  FILE * fp = fopen(status, "r");
  char buff[500];

  while (fgets(buff, 500, fp) != NULL) {
    char * token = strtok(buff, "\t");
    while (token != NULL) {
      if (strcmp(token, "Name:") == 0) {
        strcpy(data[11], strtok(NULL, "\t"));
      } else if (strcmp(token, "Uid:") == 0) {
        // This line will copy user id
        strcpy(data[1], strtok(NULL, "\t"));
      } else if (strcmp(token, "VmSize:") == 0) {
        strcpy(data[4], strtok(NULL, " "));
      } else if (strcmp(token, "VmRSS:") == 0) {
        strcpy(data[5], strtok(NULL, " "));
      } else if (strcmp(token, "RssFile:") == 0) {
        strcpy(data[6], strtok(NULL, " "));
      } else if (strcmp(token, "State:") == 0) {
        strcpy(data[7], strtok(NULL, " "));
      }
      token = strtok(NULL, "\t");
    }
  }

  // Change user id to name
  struct passwd * user_struct;
  user_struct = getpwuid(atoi(data[1]));
  strcpy(data[1], user_struct -> pw_name);
  fclose(fp);

  // Logic to get priority and nice value

  fp = fopen(stat, "r");
  char buff1[500];
  if (fgets(buff1, 300, fp) != NULL) {
    char * temp = strchr(buff1, (int)
      ')');
    char * token = strtok(temp, " ");
    for (int i = 0; i < 15; ++i)
      token = strtok(NULL, " ");
    strcpy(data[2], strtok(NULL, " "));
    strcpy(data[3], strtok(NULL, " "));
  }
  fclose(fp);

  // Some processes have no virt, res and ssr value so set them to zero
  if (data[4][0] == '\0') {
    strcpy(data[4], "0");
    strcpy(data[5], "0");
    strcpy(data[6], "0");
  }

  return data;
}

void get_all_processes_attr() {

  struct dirent * entry;
  DIR * dp = opendir("/proc");
  if (dp == NULL) {
    fprintf(stderr, "Cannot open directory: /proc\n");
    exit(1);
  }
  errno = 0;
  while ((entry = readdir(dp)) != NULL) {
    if (entry == NULL && errno != 0) {
      perror("readdir failed");
      exit(1);
    } else {
      if (entry -> d_name[0] == '.' || entry -> d_name[0] < '1' || entry -> d_name[0] > '9')
        continue;
    }
    char ** data = get_process_attr(entry -> d_name);
    printf("%5s %-10s %4s %3s  %10s %8s %8s %s               %s", data[0], data[1], data[2], data[3], data[4], data[5],
      data[6], data[7], data[11]);
    for (int i = 0; i < 12; ++i)
      free(data[i]);
    free(data);
  }

}

char * get_time_only() {
  time_t curr_time;
  time( & curr_time);
  char * d_string = ctime( & curr_time);
  d_string = strtok(d_string, " ");
  for (int i = 0; i < 3; ++i) {
    d_string = strtok(NULL, " ");
  }
  return d_string;
}

void print_uptime() {
  FILE * fp = fopen("/proc/uptime", "r");
  int boot_time;
  fscanf(fp, "%d", & boot_time);
  if ((boot_time / 60) < 60)
    printf("up %d min,  ", boot_time / 60);
  else
    printf("up %d:%d,  ", boot_time / 3600, (boot_time - (3600 * (boot_time / 3600))) / 60);
  fclose(fp);
}

int main() {

  // Upper Pane

  // First line of top command

  printf("top - %s ", get_time_only());
  print_uptime();
  printf("%d user,  ", get_no_of_users());

  FILE * fp = fopen("/proc/loadavg", "r");
  float time1, time2, time3;
  fscanf(fp, "%f %f %f", & time1, & time2, & time3);
  printf("load average: %.2f, %.2f, %.2f\n", time1, time2, time3);
  fclose(fp);

  // Second line of top command

  int * p_counts = get_processes_count();
  printf("Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie\n", p_counts[0], p_counts[1], p_counts[2], p_counts[3], p_counts[4]);
  free(p_counts);

  // Third line of top command

  // Needs implementation

  // Fourth and fifth line of top command

  long * m_counts = get_mem_info();
  printf("KiB Mem  :%10ld total,  %10ld free,  %10ld used,  %10ld buff/cache\n", m_counts[0], m_counts[1], m_counts[2], m_counts[3]);
  printf("KiB Swap :%10ld total,  %10ld free,  %10ld used,  %10ld avail Mem\n", m_counts[4], m_counts[5], m_counts[6], m_counts[7]);
  free(m_counts);

  // Lower Pane

  printf("\n  PID USER         PR  NI        VIRT      RES      SHR S  %%CPU  %%MEM     TIME+ COMMAND \n");
  get_all_processes_attr();

  while (1) {
    int a = 1;
  }
  return 1;
}
