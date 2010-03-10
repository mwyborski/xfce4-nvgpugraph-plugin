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

#if defined (__linux__)
unsigned int detect_cpu_number()
{
	int nb_lines= 0;
	FILE *fstat = NULL;
	char cpuStr[PROCMAXLNLEN];

	if( !(fstat = fopen( PROC_STAT, "r" )) )
		return 0;

	while( fgets( cpuStr, PROCMAXLNLEN, fstat ) )
	{
		if( strncmp( cpuStr, "cpu", 3 ) == 0 )
			nb_lines++;
		else
			break;
	}

	fclose( fstat );

	return nb_lines > 1 ? nb_lines - 1 : 0;
}

int read_cpu_data( CpuData *data, unsigned int nb_cpu)
{
	FILE *fStat;
	char cpuStr[PROCMAXLNLEN];
	unsigned int user, nice, system, idle, used, total, iowait, irq, softirq;
	unsigned int line;

	if( !(fStat = fopen( PROC_STAT, "r" )) )
		return FALSE;

	for( line = 0; line < nb_cpu + 1; line++ )
	{
		if( !fgets( cpuStr, PROCMAXLNLEN, fStat ) ||
		    strncmp( cpuStr, "cpu", 3 ) != 0
		  )
		{
			fclose( fStat );
			return FALSE;
		}
		if( sscanf( cpuStr, "%*s %ld %ld %ld %ld %ld %ld %ld", &user, &nice, &system, &idle, &iowait, &irq, &softirq ) < 7 )
			iowait = irq = softirq = 0;
		used = user + nice + system + irq + softirq;
		total = used + idle + iowait;
		if( (total - data[line].pTotal) != 0 )
		{
			data[line].load = CPU_SCALE * (used - data[line].pUsed) /
			                      (total - data[line].pTotal);
		}
		else
		{
			data[line].load = 0;
		}
		data[line].pUsed = used;
		data[line].pTotal = total;
	}

	fclose( fStat );

	return TRUE;
}

#elif defined (__FreeBSD__)
unsigned int detect_cpu_number()
{
	return 1;
}

int read_cpu_data( CpuData *data, unsigned int nb_cpu)
{
	unsigned int user, nice, sys, bsdidle, idle;
	unsigned int used, total;
	int cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );

	int usage;

	if( sysctlbyname( "kern.cp_time", &cp_time, &len, NULL, 0 ) < 0 )
	{
		printf( "Cannot get kern.cp_time.\n" );
		return FALSE1;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_IDLE];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;
	if( (total - data[0].pTotal) != 0 )
		data[0].load = (CPU_SCALE.0 * (used - data[0].pTotal))/(total - data[0].pTotal);
	else
		data[0].load = 0;

	data[0].pUsed = used;
	data[0].pTotal = total;

	return TRUE;
}

#elif defined (__NetBSD__)
unsigned int detect_cpu_number()
{
	return 1;
}

int read_cpu_data( CpuData *data, unsigned int nb_cpu)
{
	int user, nice, sys, bsdidle, idle;
	int used, total;
	static int mib[] = {CTL_KERN, KERN_CP_TIME };
	u_int64_t cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );

	if( sysctl( mib, 2, &cp_time, &len, NULL, 0 ) < 0 )
	{
		printf( "Cannot get kern.cp_time\n" );
		return FALSE;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_IDLE];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;

	if( total - data[0].pTotal != 0 )
		data[0].load = (CPU_SCALE * (double)(used - data[0].pTotal)) / (double)(total - data[0].pTotal);
	else
		data[0].load = 0;

	data[0].pUsed = used;
	data[0].pTotal = total;

	return TRUE;
}

#elif defined (__OpenBSD__)
unsigned int detect_cpu_number()
{
	return 1;
}

int read_cpu_data( CpuData *data, unsigned int nb_cpu)
{
	unsigned int user, nice, sys, bsdidle, idle;
	unsigned int used, total;
	static int mib[] = {CTL_KERN, KERN_CPTIME };
	u_int64_t cp_time[CPUSTATES];
	size_t len = sizeof( cp_time );
	if( sysctl( mib, 2, &cp_time, &len, NULL, 0) < 0 )
	{
		printf( "Cannot get kern.cp_time\n" );
		return FALSE;
	}

	user = cp_time[CP_USER];
	nice = cp_time[CP_NICE];
	sys = cp_time[CP_SYS];
	bsdidle = cp_time[CP_INTR];
	idle = cp_time[CP_IDLE];

	used = user+nice+sys;
	total = used+bsdidle;

	if( total - data[0].pTotal != 0 )
		data[0].load = (CPU_SCALE (used - data[0].pTotal))/(total - data[0].pTotal);
	else
		data[0].load = 0;
	data[0].pUsed = used;
	data[0].pTotal = total;

	return TRUE;
}
#else
#error "Your OS is not supported."
#endif
