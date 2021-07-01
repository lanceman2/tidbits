/* This is a build utility that lets you make a program specific
 * configuration header include file that can override default CPP and
 * compiler parameters, like for example PO_THREADPOOL_MAX_NUM_THREADS
 * from in threadPool.c.
 *
 * so in the qb.make file:
 *
 *   threadPool_test_CPPFLAGS := -DPO_CONFIGURE=threadPool_test_conf.h
 *
 * makes all the object file linked to make threadPool_test have
 * PO_CONFIGURE defined as threadPool_test_conf.h, where-by making
 * this code include threadPool_test_conf.h in all the potato header
 * files that include this file.
 */

/* This file must be included before all other potato header files.
 */

#ifndef __PO_CONFIGURE_H__
#  define  __PO_CONFIGURE_H__
#  ifdef PO_CONFIGURE
#    include PO_CONFIGURE
#  endif
#endif
