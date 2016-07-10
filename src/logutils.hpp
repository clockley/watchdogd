#ifndef LOGUTILS_H
#define LOGUTILS_H
#ifndef LOG_PRIMASK
#define	LOG_PRIMASK 0x07
#endif

#ifndef LOG_PRI
#define LOG_PRI(p) ((p) & LOG_PRIMASK)
#endif 

#ifndef LOG_MASK
#define	LOG_MASK(pri)	(1 << (pri))
#endif

#ifndef LOG_UPTO
#define	LOG_UPTO(pri)	((1 << ((pri)+1)) - 1)
#endif

void Logmsg(int priority, const char *const fmt, ...);
void SetLogTarget(sig_atomic_t target, ...);
void SetAutoUpperCase(bool);
void SetAutoPeriod(bool);
void HashTagPriority(bool);
bool LogUpTo(const char *const, bool);
bool LogUpToInt(long pri, bool);
bool MyStrerrorInit(void);
char * MyStrerror(int);
pid_t StartLogger(void);
#endif
