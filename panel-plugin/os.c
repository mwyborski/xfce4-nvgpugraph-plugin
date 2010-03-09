#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "os.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

#if defined (__linux__)
#define PROC_STAT "/proc/stat"
#define PROCMAXLNLEN 256 /* should make it */
#endif

#if defined (__FreeBSD__)
#include <osreldate.h>
#include <sys/types.h>
#if __FreeBSD_version < 500101
#include <sys/dkstat.h>
#else
#include <sys/resource.h>
#endif
#include <sys/sysctl.h>
#include <devstat.h>
#include <fcntl.h>
#include <nlist.h>
#endif

#if defined (__NetBSD__)
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <nlist.h>
#endif

#if defined (__OpenBSD__)
#include <sys/param.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/dkstat.h>
#include <fcntl.h>
#include <nlist.h>
#endif

CpuData *cpudata = NULL;
int nrCpus = 0;

static int detect_cpu_number();

void free_cpu_data()
{
	g_free( cpudata );
	cpudata = NULL;
	nrCpus = 0;
}

int init_cpu_data()
{
	int i, cpuNr = -1;

	/* Check if previously initalized */
	if( cpudata != NULL )
		return -2;

	cpuNr = detect_cpu_number();
	if( cpuNr < 1 )
		return -1;

	/* Alloc storage for cpu data stuff */
	cpudata = (CpuData *) g_malloc0( cpuNr * sizeof( CpuData ) );

	return nrCpus = cpuNr;
}

#if defined (__linux__)
static int detect_cpu_number()
{
	int cpuNr= -1;
	FILE *fstat = NULL;
	char cpuStr[PROCMAXLNLEN];
	/* Open proc stat file */
	if( !(fstat = fopen( PROC_STAT, "r" )) )
		return -1;

	/* Read each cpu line at time */
	do
	{
		if( !fgets( cpuStr, PROCMAXLNLEN, fstat ) )
			return cpuNr;
		cpuNr++;
	} while( strncmp( cpuStr, "cpu", 3 ) == 0 );
	fclose( fstat );
	return cpuNr;
}

CpuData *read_cpu_data()
{
	FILE *fStat = NULL;
	char cpuStr[PROCMAXLNLEN];
	unsigned long user, nice, system, idle, used, total;
	unsigned long iowait=0, irq=0, softirq=0;
	int cpuNr = 0;

	/* Check if callable */
	if( (cpudata == NULL) || (nrCpus == 0) )
		return NULL;

	/* Open proc stat file */
	if( !(fStat = fopen( PROC_STAT, "r" )) )
		return NULL;

	/* Read each cpu line at time */
	do
	{
		if( !fgets( cpuStr, PROCMAXLNLEN, fStat ) )
			return cpudata;
		if( sscanf( cpuStr, "%*s %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &system, &idle, &iowait, &irq, &softirq ) < 7 )
			iowait = irq = softirq = 0;
		used = user + nice + system + irq + softirq;
		total = used + idle + iowait;
		if( (total - cpudata[cpuNr].pTotal) != 0 )
		{
			cpudata[cpuNr].load = CPU_SCALE * (float)(used - cpudata[cpuNr].pUsed) /
			                      (float)(total - cpudata[cpuNr].pTotal);
		}
		else
		{
			cpudata[cpuNr].load = 0;
		}
		cpudata[cpuNr].pUsed = used;
		cpudata[cpuNr].pTotal = total;
		cpuNr++;
	}
	while( (cpuNr < nrCpus) && (strncmp( cpuStr, "cpu", 3 ) == 0) );

	fclose( fStat );

	return cpudata;
}

#elif defined (__FreeBSD__)
static int detect_cpu_number()
{
	return 1;
}

CpuData *read_cpu_data()
{
	unsigned long user, nice, sys, bsdidle, idle;
	unsigned long used, total;
	long cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );

	long usage;

	if( sysctlbyname( "kern.cp_time", &cp_time, &len, NULL, 0 ) < 0 )
	{
		printf( "Cannot get kern.cp_time.\n" );
		return -1;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_IDLE];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;
	if( (total - cpudata[0].pTotal) != 0 )
		cpudata[0].pTotal = (CPU_SCALE.0 * (used - cpudata[0].pTotal))/(total - cpudata[0].pTotal);
	else
		cpudata[0].pTotal = 0;

	cpudata[0].pUsed = used;
	cpudata[0].pTotal = total;

	return cpudata;
}

#elif defined (__NetBSD__)
static int detect_cpu_number()
{
	return 1;
}

CpuData *read_cpu_data()
{
	long user, nice, sys, bsdidle, idle;
	long used, total, usage;
	static int mib[] = {CTL_KERN, KERN_CP_TIME };
	u_int64_t cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );

	if( sysctl( mib, 2, &cp_time, &len, NULL, 0 ) < 0 )
	{
		printf( "Cannot get kern.cp_time\n" );
		return -1;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_IDLE];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;

	if( total - cpudata[0].pTotal != 0 )
		usage = (CPU_SCALE * (double)(used - cpudata[0].pTotal)) / (double)(total - cpudata[0].pTotal);
	else
		usage = 0;

	cpudata[0].pUsed = used;
	cpudata[0].pTotal = total;

	return cpudata;
}

#elif defined (__OpenBSD__)
static int detect_cpu_number()
{
	return 1;
}

CpuData *read_cpu_data()
{
	unsigned long user, nice, sys, bsdidle, idle;
	unsigned long used, total, usage;
	static int mib[] = {CTL_KERN, KERN_CPTIME };
	u_int64_t cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );
	if( sysctl( mib, 2, &cp_time, &len, NULL, 0) < 0 )
	{
		printf( "Cannot get kern.cp_time\n" );
		return -1;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_INTR];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;

	if( total - cpudata[0].pTotal != 0 )
		usage = (CPU_SCALE * (double)(used - cpudata[0].pTotal))/(double)(total - cpudata[0].pTotal);
	else
		usage = 0;
	cpudata[0].pUsed = used;
	cpudata[0].pTotal = total;

	return cpudata;
}
#else
#error "Your OS is not supported."
#endif
