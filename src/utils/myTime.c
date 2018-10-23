#include "utils/myTime.h"

TimeSpec getTimeSpec (void) {

    TimeSpec timespc;
    clock_gettime (4, &timespc);
    return timespc;

}

double timeElapsed (TimeSpec *start, TimeSpec *end) {

    return (double) end->tv_sec + (double) end->tv_nsec / 1000000000
		- (double) start->tv_sec - (double) start->tv_nsec / 1000000000;

}

void sleepFor (double seconds) {

    TimeSpec timespc;
	timespc.tv_sec = (time_t) seconds;
	timespc.tv_nsec = (long) ((seconds - timespc.tv_sec) * 1e+9);

	int result;
	do {
		result = nanosleep (&timespc, &timespc);
	} while (result == -1 && errno == EINTR);
    // If interrupted by a signal, go back to sleep for the remaining time.

}