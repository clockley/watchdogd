#if !defined(MAIN_H)
#define MAIN_H
int SetupSignalHandlers(int isDaemon);
void Abend(void *arg);
void SignalHandler(int signum);
int CheckDeviceAndDaemonTimeout(const int *fd, const int deviceTimeout,
				const int daemonSleepTime);
static void PrintConfiguration(void *arg);
#endif
