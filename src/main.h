#if !defined(MAIN_H)
#define MAIN_H
int SetupSignalHandlers(int isDaemon);
void FatalError(struct cfgoptions *s);
void SignalHandler(int signum);
static void PrintConfiguration(struct cfgoptions *s);
static int InstallSignalAction(struct sigaction *act, ...);
#endif
