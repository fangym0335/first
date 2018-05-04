#include "mycomm.h"
#include "dotime.h"
#include "memctl.h"

main()
{
	char datebuff[DATE_LEN+1];
	char timebuff[TIME_LEN+1];
	char datetimebuff[DATETIME_LEN+1];
	char mbegin[MICROSIZE];
	char mend[MICROSIZE];
	char buf[100];
	char day[DATE_LEN+1];

	char first[DATE_LEN+1];
	char last[DATE_LEN+1];

	ulong timediff;
	struct timeval *tm;

	InitMemCtl();
	DMsg( "Now Record Time" );
	BeginTime();

	Trace(GetSysDateTime( datetimebuff ));
	DMsg( "datetime = [%s], len = [%d], size = [%d]", datetimebuff, strlen(datetimebuff), sizeof(datetimebuff) );

	Trace(GetSysDateAndTime( datebuff, timebuff ));
	DMsg( "date = [%s], len = [%d], size = [%d]", datebuff, strlen(datebuff), sizeof(datebuff) );
	DMsg( "time = [%s], len = [%d], size = [%d]", timebuff, strlen(timebuff), sizeof(timebuff) );

	Trace(GetMicroTime(mbegin));
	DMsg( "time = [%s], len = [%d], size = [%d]", mbegin, strlen(mbegin), sizeof(mbegin) );
	tm = (struct timeval*)mbegin;
	DMsg( "sec = [%d], usec = [%ld]", tm->tv_sec, tm->tv_usec );

	Trace(GetMicroTime(mend));
	DMsg( "time = [%s], len = [%d], size = [%d]", mend, strlen(mend), sizeof(mend) );
	tm = (struct timeval*)mend;
	DMsg( "sec = [%d], usec = [%ld]", tm->tv_sec, tm->tv_usec );

	Trace(timediff = GetDiffTime( mbegin, mend ));
	DMsg( "timediff = [%ld]", timediff );

	DMsg( "Now End Time" );
	EndTime(				);

	ShowDiffTime(			    );
	ShowPassTime(      );

	DMsg( "Diff Time = [%ld]", DiffTime());

	DMsg( "Pass Time = [%ld]", PassTime(    ) );

	Trace(GetFormatTime( buf, sizeof(buf), "%Y%m%d%H%M%S" ));
	DMsg( "buf = [%s]", buf );

	Trace(GetFormatTime( buf, 15, "%S%M%H%d%m%Y" ));
	DMsg( "buf = [%s]", buf );

	Trace(strcpy( day, "20041231" ));

	Trace(GetNextDay( datebuff ));
	DMsg( "NextDay = [%s]", datebuff );

	Trace(GetDay( datebuff, day, 1 ));
	DMsg( "datebuf = [%s]", datebuff );

	Trace(GetDay( datebuff, day, -1 ));
	DMsg( "datebuf = [%s]", datebuff );

	Trace(GetFirstAndLastOfNowMonth( first, last ));
	DMsg( "first = [%s], last = [%s]", first, last );

	Trace(GetFirstAndLastOfMonth( first, last, day ));
	DMsg( "first = [%s], last = [%s]", first, last );

	return 0;
}

