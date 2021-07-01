/* We pulled this out of elog.h because we would otherwise have the
 * following circular dependency:
 *
 *   aSsert.h -> elog.h -> tIme.h -> aSert.h
 *
 */

#ifdef  PO_ELOG_ERROR
#ifndef PO_ELOG_FILE
// default elog file
#  define PO_ELOG_FILE  stderr
#endif
#endif

extern
void _poELog(
        const char *pre, FILE *fstream,
        const char *sourceFile, int sourceLine,
        const char *func, const char *format, ...) 
        __attribute__((format(printf,6,7)));
extern // used by ASSERT() and family
void _poELogV(
        const char *pre, FILE *fstream,
        const char *sourceFile, int sourceLine,
        const char *func, const char *format, va_list ap);
