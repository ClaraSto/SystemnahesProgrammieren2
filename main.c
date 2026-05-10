#include <errno.h>
#include <stdio.h>
#include "stdlib.h"
#include <fcntl.h>
#include <unistd.h>


float smooth[3][3] = {{1.0/9.0, 1.0/9.0, 1.0/9.0},
                    {1.0/9.0, 1.0/9.0, 1.0/9.0}, 
                    {1.0/9.0, 1.0/9.0, 1.0/9.0}};


int main()
{
int* filter_option;
char* dataname_option;

printf("Enter a filename: \n");

scanf("%s", dataname_option);

printf("Choose a filter: 1: smooth 2: sharp 3: edge 4: emboss  \n");
scanf("%d", filter_option);

int fd = open(dataname_option , O_RDONLY);

if (fd == -1) {
printf("Error: Could not open file, code %n", errno);
exit(EXIT_FAILURE);
}

int count = 14;

char *buf = (char *) malloc((count + 1) * sizeof(char));
int ret = read(fd, buf, count);
buf[ret] = '\0';
printf("%s", buf);


return 0;
}