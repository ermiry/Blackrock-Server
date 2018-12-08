#ifndef MY_UTILS_H
#define MY_UTILS_H

#define ARRAY_NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

extern int randomInt (int min, int max);

/*** STRINGS ***/

extern char *createString (const char *stringWithFormat, ...);
extern char **splitString (char *str, const char delim);

#endif