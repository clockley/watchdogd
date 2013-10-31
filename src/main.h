#if !defined(MAIN_H)
#define MAIN_H
int SetupSignalHandlers(int isDaemon);
void Abend(void *arg);
void SignalHandler(int signum);
int CheckDeviceAndDaemonTimeout(const int *fd, const int deviceTimeout,
				const int daemonSleepTime);
int SetupLogTick(void *arg);
int SetupAuxManagerThread(void *arg);
int SetupTestBinThread(void *arg);
int SetupLoadAvgThread(void *arg);
int SetupMinPagesThread(void *arg);
int SetupExeDir(void *arg);
int SetupTestFork(void *arg);
int SetupSyncThread(void *arg);
int SetupThread(void *(*startFunction) (void *), void *arg);
int StartPidFileTestThread(void *arg);
static void PrintConfiguration(void *arg);
#endif
