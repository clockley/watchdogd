#if !defined(MAIN_H)
#define MAIN_H
int SetupSignalHandlers(int isDaemon);
void Abend(struct cfgoptions *s);
void SignalHandler(int signum);
bool CheckWatchdogTimeout(watchdog_t *wdt, int timeout);
static void PrintConfiguration(struct cfgoptions *s);
#endif
