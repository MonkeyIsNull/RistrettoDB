/*
** RistrettoDB Version Information
**
** This file contains version information and compilation details
** for the RistrettoDB library.
*/

#include "ristretto.h"

/*
** Return the version string
*/
const char* ristretto_version(void) {
    return RISTRETTO_VERSION;
}

/*
** Return the version number
*/
int ristretto_version_number(void) {
    return RISTRETTO_VERSION_NUMBER;
}
