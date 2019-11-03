#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <dirent.h>
#include <netdb.h>
#include <math.h>


int main(int argc, char const *argv[])
{
    char *dir = "provadir";

    DIR *currDir;
    currDir = opendir(dir);

    struct dirent *currItem;

    while((currItem = readdir(currDir)) != NULL){
        printf("%s/%s\n", dir, currItem->d_name);
    }

    return 0;
}
