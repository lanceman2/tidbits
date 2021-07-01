#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>

#include "aSsert.h"

int main(void)
{
    struct passwd *p;

    ASSERT(p = getpwnam("lanceman"));

    printf("pw_name=\"%s\"\n"
            "pw_paswd=\"%s\"\n"
            "pw_uid=%u\n",
            p->pw_name,
            p->pw_passwd,
            p->pw_uid);
    
    char buf[1024];

    FILE *file;
    ASSERT(file = fopen("setuid_test.tmp", "w"));

    ASSERT(setuid(p->pw_uid) == 0);

    ASSERT(getcwd(buf, sizeof(buf)));

    printf("pwd=\"%s\"\n", buf);

    fprintf(file, "Writing to file as user with id=%d\n", getuid());

    return 0;
}
