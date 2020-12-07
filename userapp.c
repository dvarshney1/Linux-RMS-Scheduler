#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static int num_jobs = 0;
//userapp - crashes, do not knwo why !!
void registration(int PID, int Period, int JobProcessTime){
	FILE *f = fopen("/proc/mp2/status", "w");
	fprintf(f, "R, %d, %d, %d", PID, Period, JobProcessTime);
	num_jobs++;
	fclose(f);
}

void de_registration(int PID){
	FILE *f = fopen("/proc/mp2/status", "w");
	fprintf(f, "D, %d", PID);
	fclose(f);
}

char *check_status(int PID, char *str){
	FILE *f = fopen("/proc/mp2/status", "r");
	char *ret_val =  fgets(str, 500, f);
	fclose(f);
	return ret_val;
}

void yield(int PID){
	FILE *f = fopen("/proc/mp2/status", "w");
	fprintf(f, "Y, %d", PID);
	fclose(f);
}

void calc_factorial(int n){
	int result = 1;
	while(n > 0){
		result *= n;
		n--;
	}
}

int main(){
	int PID, Period, JobProcessTime;
	char *str;
	char *ret_val;
	struct timeval t0;
	struct timeval wake_up;

	str = (char *)malloc(sizeof(char) * 500);
	PID = 1;
	Period = 10;
	JobProcessTime = 20;
	registration(PID, Period, JobProcessTime);
	ret_val = check_status(PID, str);
	if(!ret_val){return -1;}

	gettimeofday(&t0, NULL);
	yield(PID);
	while(num_jobs != 0){
		num_jobs--;
		gettimeofday(&wake_up, NULL);
		calc_factorial(100);
		yield(PID);
	}
	de_registration(PID);
	free(str);
	return 0;
}
