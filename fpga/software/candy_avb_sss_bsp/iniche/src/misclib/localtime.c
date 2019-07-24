
#include "ipport.h"

#include <time.h>

#ifdef INCLUDE_LOCAL_TIME

#define BASE_YEAR	1970
#define START_WDAY	4	/* constant. don't change */
#define SECS_PER_MIN	60
#define SECS_PER_HOUR	(SECS_PER_MIN*60)
#define SECS_PER_DAY	(SECS_PER_HOUR*24)

static unsigned int month_days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static unsigned long tzoffset;		/* timezone offset in seconds */

static void secs_to_tm (unsigned long t, struct tm *tm)
{
	unsigned long days, rem;
	unsigned int y, mon;
	days = t/SECS_PER_DAY;
	rem = t - (days*SECS_PER_DAY);
	tm->tm_hour = rem / SECS_PER_HOUR;
	rem -= tm->tm_hour*SECS_PER_HOUR;
	tm->tm_min = rem / SECS_PER_MIN;
	rem -= tm->tm_min*SECS_PER_MIN;
	tm->tm_sec = rem;
	tm->tm_wday = (days + START_WDAY) % 7;
	y = BASE_YEAR;
	while (days >= 365) {
		if ((y % 4) == 0) {
			if (days == 365)
				break;
			/* leap year */
			days--;
		}
		days -= 365;
		y++;
	}
	tm->tm_year = y - 1900;
	tm->tm_yday = days;

	/* Set correct # of days for feburary */
	month_days[1] = ((y % 4) == 0) ? 29 : 28;

	for (mon = 0; mon < 12; mon++) {
		if (days < month_days[mon]) {
			tm->tm_mon = mon;
			break;
		}
		days -= month_days[mon];
	}
	if (mon == 12)  {
		/* PANIC */
		printf ("secs_to_tm:remaining days=%d\n", days);
	}

	tm->tm_mday = days + 1;
	tm->tm_isdst = 0;
	/* tm->tm_gmtoff = 0; */
}

void settimezone (int hours, int mins)
{
	tzoffset = hours*SECS_PER_HOUR + mins*SECS_PER_MIN;
}

struct tm *
localtime ( const time_t *timep)
{
	static struct tm tm;
	unsigned long t;

	t = *timep + tzoffset;
	secs_to_tm (t, &tm);
	return &tm;
}

struct tm *
gmtime (const time_t *timep)
{
	static struct tm tm;
	unsigned long t;

	t = *timep;
	secs_to_tm (t, &tm);
	return &tm;
}

time_t 
time (time_t *tp)
{
	long int t;

	t = (long int) (cticks / TPS);
	if (tp)
		*tp = t;
	return t;
}
#endif /* INCLUDE_LOCAL_TIME */

time_t convLocalTimeToString(char *timeBuffer)
{
   time_t dateTime;
   struct tm *tmDateTime;

   time(&dateTime);
   tmDateTime = localtime((const time_t *)&dateTime);
   sprintf(timeBuffer, "%02d:%02d:%02d", tmDateTime->tm_hour, 
	  tmDateTime->tm_min, tmDateTime->tm_sec); 

   return(dateTime);
}

