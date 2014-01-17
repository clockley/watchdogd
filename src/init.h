#if !defined(INIT_H)
#define INIT_H
int MakeLogDir(struct cfgoptions *s);
const char
*LibconfigWraperConfigSettingSourceFile(const config_setting_t * setting);
int SetSchedulerPolicy(int priority);
int CheckPriority(int priority);
int InitializePosixMemlock(void);
int Usage(void);
int PrintVersionString(void);
int LoadConfigurationFile(struct cfgoptions *options);
int ParseCommandLine(int *argc, char **argv, struct cfgoptions *s);
bool SetDefaultConfig(struct cfgoptions *options);
#endif
