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

  // Logic to get priority and nice value and time

  int       field_begin;
  int       stat_fd;

  char      stat_buf[2048];

  ssize_t   read_result;

  long long boot_time_since_epoch;
  long process_start_time_since_boot;

  time_t    process_start_time_since_epoch;

  struct tm local_buf;

  long jiffies_per_second = sysconf(_SC_CLK_TCK);

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  stat_fd=open("/proc/stat",O_RDONLY);

    if(stat_fd<0)
    {
      fprintf(stderr,"open() fail\n");

      exit(1);
    }

    read_result=read(stat_fd,stat_buf,sizeof(stat_buf));

    if(read_result<0)
    {
      fprintf(stderr,"read() fail\n");

      exit(1);
    }

    if(read_result>=sizeof(stat_buf))
    {
      fprintf(stderr,"stat_buf is too small\n");

      exit(1);
    }

    close(stat_fd);

    field_begin=strstr(stat_buf,"btime ")-stat_buf+6;

    sscanf(stat_buf+field_begin,"%llu",&boot_time_since_epoch);

    //printf("%lf", boot_time_since_epoch);

    //process_start_time_since_epoch
    //=
    //boot_time_since_epoch+process_start_time_since_boot/jiffies_per_second;

    //localtime_r(&process_start_time_since_epoch,&local_buf);

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
    for (int i = 0; i < 2; ++i)
      token = strtok(NULL, " ");
    process_start_time_since_boot = atol(strtok(NULL, " "));

    process_start_time_since_epoch
    =
    boot_time_since_epoch+process_start_time_since_boot/jiffies_per_second;

    localtime_r(&process_start_time_since_epoch,&local_buf);


    //strftime(data[10], sizeof(data[10]), "%H:%M.%S", &local_buf);

    int seconds = 0;

    seconds = difftime(mktime(&tm), mktime(&local_buf));

    int h = 0, m = 0, s = 0;

	h = (seconds/3600);

	m = (seconds -(3600*h))/60;

	s = (seconds -(3600*h)-(m*60));

	char time[100];

	sprintf(time, "%d:%d:%d", h, m, s);

	data[10] = time;

    /*printf("local time: %02d:%02d:%02d\n",
           local_buf.tm_hour,
           local_buf.tm_min,
           local_buf.tm_sec
          );*/

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
    printf("%5s %-10s %4s %3s  %10s %8s %8s %s  %s  %s     %s  %s", data[0], data[1], data[2], data[3], data[4], data[5],
      data[6], data[7], "none", "none", data[10], data[11]);

    //need to discuss with ahtisham
    //for (int i = 0; i < 12; ++i)
      //free(data[i]);
    //free(data);
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

//for cpu utilization
double cpu_stat(){
    char strp[100];
	char strc[100];
	const char d[2] = " ";
	char* tokenPrev;
	char* tokenCurr;
	int i = 0;

	int * dataPrev = malloc(sizeof(int) * 10); // user nice system(kernel) idle iowait irq(interrupts) softirq steal guest guestnice
	int * dataCurr = malloc(sizeof(int) * 10); // user nice system(kernel) idle iowait irq(interrupts) softirq steal guest guestnice
	int * dataAns = malloc(sizeof(int) * 10); // user nice system(kernel) idle iowait irq(interrupts) softirq steal guest guestnice

    memset(dataPrev, '\0', 10);

	for (int j = 0;j < 10; j++)
    {
        dataPrev[j] = 0;
        dataCurr[j] = 0;
        dataAns[j] = 0;
    }

    FILE* fp = fopen("/proc/stat","r");
    fgets(strp,100,fp);

    fclose(fp);
    //printf(strp);
    tokenPrev = strtok(strp,d);
    tokenPrev = atoi(strtok(NULL,d));
    while(tokenPrev!=NULL){
        dataPrev[i] = tokenPrev;

        tokenPrev = atoi(strtok(NULL,d));
        i++;
    }
    i = 0;
    sleep(1);

    fp = fopen("/proc/stat","r");
    fgets(strc,100,fp);

    fclose(fp);
    //printf(strc);
    tokenCurr = strtok(strc,d);
    tokenCurr = atoi(strtok(NULL,d));
    while(tokenCurr!=NULL){
        dataCurr[i] = tokenCurr;
        tokenCurr = atoi(strtok(NULL,d));
        i++;
    }
    int idle_prev = (dataPrev[3]) + (dataPrev[4]);
    int idle_cur = (dataCurr[3]) + (dataCurr[4]);

    int nidle_prev = (dataPrev[0]) + (dataPrev[1]) + (dataPrev[2]) + (dataPrev[5]) + (dataPrev[6]);
    int nidle_cur = (dataCurr[0]) + (dataCurr[1]) + (dataCurr[2]) + (dataCurr[5]) + (dataCurr[6]);

    int total_prev = idle_prev + nidle_prev;
    int total_cur = idle_cur + nidle_cur;

    double totald = (double) total_cur - (double) total_prev;
    double idled = (double) idle_cur - (double) idle_prev;

    double cpu_perc = (1000 * (totald - idled) / totald + 1) / 10;

    return cpu_perc;
}



int main() {

    double cpuUsage = 0.0;

    cpuUsage = cpu_stat();

    while (1) {

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
        printf("CPU: %lf%%\n", cpuUsage);

        // Fourth and fifth line of top command

        long * m_counts = get_mem_info();
        printf("MiB Mem  :%10ld total,  %10ld free,  %10ld used,  %10ld buff/cache\n", m_counts[0]/1024, m_counts[1]/1024, m_counts[2]/1024, m_counts[3]/1024);
        printf("MiB Swap :%10ld total,  %10ld free,  %10ld used,  %10ld avail Mem\n", m_counts[4]/1024, m_counts[5]/1024, m_counts[6]/1024, m_counts[7]/1024);
        free(m_counts);

        // Lower Pane

        printf("\n  PID USER         PR  NI        VIRT      RES      SHR S  %%CPU  %%MEM     TIME+  COMMAND \n");
        get_all_processes_attr();


        cpuUsage = cpu_stat();

        system("clear");
    }
    return 1;
}
