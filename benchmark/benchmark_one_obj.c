#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <time.h>
#include <npheap.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    int i=0,number_of_processes = 1, number_of_objects=1024, max_size_of_objects = 8192 ,j; 
    int a;
    int pid;
    int size;
    char data[8192];
    char filename[256];
    char *mapped_data;
    int devfd;
    unsigned long long msec_time;
    FILE *fp;
    struct timeval current_time;
    number_of_objects = 10;
    max_size_of_objects = 65536;//2147483647;
    number_of_processes = 4;
    devfd = open("/dev/npheap",O_RDWR);
    if(devfd < 0)
    {
        fprintf(stderr, "Device open failed");
        exit(1);
    }
    // Writing to objects
    for(i=0;i<(number_of_processes-1) && pid != 0;i++)
    {
        pid=fork();
        srand((int)time(NULL)+(int)getpid());
    }
    sprintf(filename,"npheap.%d.log",(int)getpid());
    fp = fopen(filename,"w");
    for(i = 0; i < number_of_objects; i++)
    {
        npheap_lock(devfd,i);
        size = npheap_getsize(devfd,i);
        while(size ==0 || size <= 10)
        {
            size = rand() % max_size_of_objects;
        }
        mapped_data = (char *)npheap_alloc(devfd,i,size);
		printf("obj %d size = %d\n", i, size);
        if(!mapped_data)
        {
            fprintf(stderr,"Failed in npheap_alloc()\n");
            exit(1);
        }
        memset(mapped_data, 0, size);
        a = rand()+1;
        gettimeofday(&current_time, NULL);
        for(j = 0; j < size-10; j=strlen(mapped_data))
        {
            sprintf(mapped_data,"%s%d",mapped_data,a);
        }
        fprintf(fp,"S\t%d\t%ld\t%d\t%lu\t%s\n",pid,current_time.tv_sec * 1000000 + current_time.tv_usec,i,strlen(mapped_data),mapped_data);
        npheap_unlock(devfd,i);
    }
    
    // try delete something
    int begin = rand()%number_of_objects;
	for (int i=begin; i < number_of_objects; i+=2){
		fprintf(stderr, "try delete something i = %d\n", i);
		npheap_lock(devfd,i);
		npheap_delete(devfd,i);
		fprintf(fp,"D\t%d\t%ld\t%d\t%lu\t%s\n",pid,current_time.tv_sec * 1000000 + current_time.tv_usec,i,strlen(mapped_data),mapped_data);

		size = npheap_getsize(devfd,i);
		fprintf(stderr, "obj %d size before npheap_alloc=%d\n", i, size);
		while(size ==0 || size <= 10)
		{
			size = rand() % max_size_of_objects;
		}
		mapped_data = (char *)npheap_alloc(devfd,i,size);
		printf("obj %d size = %d\n", i, size);
		if(!mapped_data)
		{
			fprintf(stderr,"Failed in npheap_alloc()\n");
			exit(1);
		}
		memset(mapped_data, 0, size);
		a = rand()+1;
		gettimeofday(&current_time, NULL);
		for(j = 0; j < size-10; j=strlen(mapped_data))
		{
			sprintf(mapped_data,"%s%d",mapped_data,a);
		}
		fprintf(fp,"S\t%d\t%ld\t%d\t%lu\t%s\n",pid,current_time.tv_sec * 1000000 + current_time.tv_usec,i,strlen(mapped_data),mapped_data);
		npheap_unlock(devfd,i);
	}
    close(devfd);
    if(pid != 0)
        wait(NULL);
    return 0;
}

