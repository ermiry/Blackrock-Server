#ifndef _ERMIRY_ERRORS_H_
#define _ERMIRY_ERRORS_H_

// ermiry error codes
#define SERVER_ERROR                100
#define NO_ERRORS                   0
#define NOT_USER_FOUND              1
#define WRONG_PASSWORD              2
#define PROFILE_NOT_FOUND           3

typedef enum ErmiryError {

    ERR_NO_PROFILE = 1,
    ERR_INVALID_PROFILE,

} ErmiryError;

#endif