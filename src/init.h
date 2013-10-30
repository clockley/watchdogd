#if !defined(INIT_H)
#define INIT_H
int MakeLogDir(void *arg);
const char
*LibconfigWraperConfigSettingSourceFile(const config_setting_t *setting);
int SetSchedulerPolicy(int priority);
int CheckPriority(int priority);
int InitializePosixMemlock(void);
int Usage(void);
int PrintVersionString(void);
int LoadConfigurationFile(void *arg);
int ParseCommandLine(int *argc, char **argv, void *arg);
#endif
