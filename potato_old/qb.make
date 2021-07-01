# The object files made depend on compile time configuration so we don't
# tend to share object files between programs and we don't make libraries
# either, for the same reason.
#
# Put another way we configure this code via the C preprocessor (CPP) and
# then compile the resulting C code. 

SUBDIRS := .


# Remove unused functions for gcc compiler: This still does not seem to
# make the executables as small as the case when we exclude the code to
# begin with.
CPPFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -gc-sections



httpd_SOURCES := httpd.c elog.c
httpd_CPPFLAGS := -DPO_CONFIGURE=\"httpd_conf.h\"



ifdef test
# test programs for development:

CLEAN_FILES := key.pem cert.pem
BUILT_FILES := $(CLEAN_FILES)

key.pem:
	certtool --generate-privkey --outfile key.pem 2> /dev/null

cert.pem: cert.cfg key.pem
	certtool --generate-self-signed --load-privkey key.pem\
 --template cert.cfg --outfile cert.pem 2> /dev/null


setuid_test_SOURCES := setuid_test.c elog.c
setuid_test_CPPFLAGS := -DPO_DEBUG

http_test_SOURCES :=\
 service.c http_test.c server.c tls.c elog.c threadPool.c\
 potato.c http.c
http_test_CPPFLAGS := -DPO_CONFIGURE=\"http_test_conf.h\"
http_test_LDFLAGS := -pthread
http_test_ADD_PKG := gnutls



client_SOURCES :=\
 elog.c sClient.c client.c gnutlsAssert.c murmurHash.c\
 randSequence.c threadPool.c
client_ADD_PKG := gnutls
client_CPPFLAGS := -DPO_CONFIGURE=\"client_conf.h\"
client_LDFLAGS := -pthread

elog_test_SOURCES := elog_test.c elog.c
elog_test_CPPFLAGS := -DPO_ELOG_NO_TIMESTAMP

elog_test_withTime_SOURCES := elog_test.c elog.c
elog_test_withTime_CPPFLAGS := -DPO_ELOG_TIMESTAMP -DPO_ELOG_NOTICE

assert_test_SOURCES := assert_test.c elog.c
dassert_test_SOURCES := assert_test.c elog.c
dassert_test_CPPFLAGS := -DPO_DEBUG -DPO_ELOG_TIMESTAMP

assert_test_noLog_SOURCES := assert_test.c
assert_test_noLog_CPPFLAGS := -DPO_ELOG_NONE

dassert_test_noLog_SOURCES := assert_test.c
dassert_test_noLog_CPPFLAGS := -DPO_ELOG_NONE -DPO_DEBUG

time_test_SOURCES := time_test.c
time_test_CPPFLAGS := -DPO_DEBUG -DPO_ELOG_NONE

gnutls_echo_server_SOURCES := gnutls_echo_server.c
gnutls_echo_server_ADD_PKG := gnutls

random_test_SOURCES := random_test.c murmurHash.c

threadPool_test_SOURCES := threadPool_test.c threadPool.c elog.c
threadPool_test_LDFLAGS := -pthread
threadPool_test_CPPFLAGS := -DPO_CONFIGURE=\"threadPool_test_conf.h\"

threadPool_benchmark_SOURCES :=\
 threadPool_benchmark.c threadPool.c elog.c murmurHash.c
threadPool_benchmark_LDFLAGS := -pthread
threadPool_benchmark_CPPFLAGS :=\
 -DPO_CONFIGURE=\"threadPool_benchmark_conf.h\"


server_test_SOURCES :=\
 server.c server_test.c elog.c\
 serverEchoRequest.c potato.c threadPool.c
server_test_CPPFLAGS := -DPO_CONFIGURE=\"server_test_conf.h\"
server_test_LDFLAGS := -pthread

tls_test_SOURCES :=\
 server.c tls.c tls_test.c elog.c serverEchoRequest.c\
 potato.c murmurHash.c
tls_test_CPPFLAGS := -DPO_CONFIGURE=\"tls_test_conf.h\"
tls_test_LDFLAGS := -pthread
tls_test_ADD_PKG := gnutls


service_test_SOURCES :=\
 service.c service_test.c server.c tls.c elog.c threadPool.c\
 potato.c randSequence.c murmurHash.c
service_test_CPPFLAGS := -DPO_CONFIGURE=\"service_test_conf.h\"
service_test_LDFLAGS := -pthread
service_test_ADD_PKG := gnutls

service_noThreadPool_SOURCES :=\
 service.c service_test.c server.c tls.c elog.c potato.c\
 randSequence.c murmurHash.c
service_noThreadPool_CPPFLAGS :=\
 -DPO_CONFIGURE=\"service_noThreadPool_conf.h\"
service_noThreadPool_LDFLAGS := -pthread # needed for gnutls
service_noThreadPool_ADD_PKG := gnutls


CLEAN_FILES += setuid_test.tmp

endif
