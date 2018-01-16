
/**
 * @file  version.h
 * @brief Version information.
 */

/*
 *  Prevent multiple inclusions of the same header file
 */
#ifndef VERSION_H
#define VERSION_H

/*
 *  Modify when incompatible changes are made to published interfaces.
 */
#define VERSION_MAJOR	2	/**< Major Version Number */
 
/* 
 *  Modify when new functionality is added or new interfaces are
 *  defined, but all changes are backward compatible.
 */
#define VERSION_MINOR	0	/**< Minor Version Number */
 
/*
 *  Modify for every released fix.
 */
#define VERSION_FIX	0	/**< Fix Version Number */

/*
 *  Produce a standard version string.
 */
#define VERSION_STRING(PN) ( printf("%s, version %d.%d.%d\n",PN,VERSION_MAJOR,VERSION_MINOR,VERSION_FIX)) /**< version MAJOR.MINOR.FIX */

#endif  /*  VERSION_H  */

