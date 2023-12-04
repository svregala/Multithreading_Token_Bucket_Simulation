/*
 * Author:      Steve Regala (sregala@usc.edu)
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <errno.h>
#include <time.h>
#include <ctype.h>

#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>

#include "cs402.h"

#include "my402list.h"


/* ----------------------- Data Object ----------------------- */

// packet object
typedef struct {
	// all struct timeval values are relative to the start_time (beginning of emulation)
	struct timeval packetArrive;
	struct timeval enterQ1;
	struct timeval exitQ1;
	struct timeval enterQ2;
	struct timeval exitQ2;
	struct timeval beginService;
	struct timeval leaveService;

	double interArrival;
	int tokenRequire;
	double serviceTime;
	int packet_ID;

} PacketObj;

// threads
pthread_t packetThread, tokenThread, s1Thread, s2Thread, ctrlCThread;


// Global Variables
double lambda;	// arrival rate, 1/lambda is inter-arrival time
double mu;	// service rate, 1/mu is transmission/service time (number of seconds of service time)
double r;	// token rate, 1/r inter-token arrival time
int B;		// number of tokens allowed in bucket
int P;		// token requirement for a particular packet
int n;		// total number of packets to be read in
char* t;	// will hold the name of file

struct timeval start_time;
int currTokenCount;//in token thread

My402List Q1;//increment in packet thread
My402List Q2;//increment in token thread, decrement Q1 in thread

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

int runningPacketThread=0;
int runningTokenThread=0;
int runTheProgram=0;

double avgPackInterArrTime=0;
double avgPackServTime=0;

int numCompletedPack=0; // these are the packets arrived into the system but never made it even to Q1 because it needs too many tokens
int numDroppedPack=0; // these are the packets arrived into the system but never made it even to Q1 because it needs too many tokens
int numRemovedPack=0; // these are the packets that got into Q1 to begin with but never made it to the server

int numTokenDropped=0;
int totalNumToken=0;

double end=0; // end of emulation time
double avgNumPacksQ1=0;
double avgNumPacksQ2=0;
double avgNumPacksS1=0;
double avgNumPacksS2=0;

// calculate average time a packet spent in system
// both variables below are used for calculating variance and thus standard deviation
double avgTimePackSpentSystem=0;
double avgTimePackSpentSystemSQUARED=0;

// for CTRL C handling
sigset_t set;


/* ----------------------- main() ----------------------- */


/*
This is a helper function for the ProcessCommandLine.
It checks whether the file given by the user is valid and checks to see if the first line of the file is just one number.
POST: if the file is valid and if the first line is of the file is a positive integer, n will take that value
*/
void ErrorCheckFirstLine(FILE* fp, char* inputFileName) {

	if (fp==NULL) {
		int errorNum=errno;
		fprintf(stderr, "Error opening file \"%s\": %s.\n", inputFileName, strerror(errorNum));
		fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
		exit(1);
	}
	
	char buf[2000];
	int lineCount=0;
	while (lineCount==0 && fgets(buf,sizeof(buf), fp)) {
		if (sscanf(buf, "%d", &n) != 1) {
			fprintf(stderr, "ERROR: Line 1 is not just a number in the given file.\n");
			fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
			exit(1);
		}
		if (n<=0) {
			fprintf(stderr, "ERROR: Line 1 is not a positive number in the given file.\n");
			fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
			exit(1);
		}
		lineCount++;
	}
	if (lineCount==0) {
		fprintf(stderr, "ERROR: Line 1 is not just a number in the given file.\n");
		fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
		exit(1);
	}

}


/*
This function will process the command line and error checks it.
POST: if applicable, the values of the parameters will be assigned to what the user gave
*/
FILE* ProcessCommandLine(int argc, char* argv[]) {

	FILE* fp = NULL;

	if (argc == 1) {
		lambda = 1;
		mu = 0.35;
		r = 1.5;
		B = 10;
		P = 3;
		n = 20;
	}

	else if (argc > 15) {
		fprintf(stderr, "ERROR: Malformed command. Too many commandline arguments.\n");
		fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
		exit(1);
	}

	else {
		int nCount = 0;
	
		double doubVal = (double)0;
		int intVal = 0;
		for (int i=1; i < argc; i=i+2) {
			
			if (!strcmp(argv[i], "-lambda")) {

				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The lamdba value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%lf", &doubVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given lamdba value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
						
						if (doubVal > 0) {
							lambda = doubVal;
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "lambda must be a positive, real number.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The lamdba value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}

			}

			else if (!strcmp(argv[i], "-mu")) {
				
				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The mu value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%lf", &doubVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given mu value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
						
						if (doubVal > 0) {
							mu = doubVal;
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "mu must be a positive, real number.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The mu value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}

			}
			
			else if (!strcmp(argv[i], "-r")) {
				
				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The r value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%lf", &doubVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given r value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
						

						if (doubVal > 0) {
							r = doubVal;
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "r must be a positive, real number.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The r value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}

			}

			else if (!strcmp(argv[i], "-B")) {
				
				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The B value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%d", &intVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given B value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
						
						if (intVal > 0) {
							B = intVal;
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "B must be a positive integer.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The B value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}

			}

			else if (!strcmp(argv[i], "-P")) {

				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The P value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%d", &intVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given P value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
						
						if (intVal > 0) {
							P = intVal;
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "P must be a positive integer.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The P value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}

			}

			else if (!strcmp(argv[i], "-n")) {

				if (i+1 < argc) {
					if (argv[i+1][0] == '-') {
						fprintf(stderr, "ERROR: Malformed command.\n");
						fprintf(stderr, "The n value is not given.\n");
						fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
						exit(1);
					}
					else {
						if (sscanf(argv[i+1], "%d", &intVal) != 1) {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "The given n value is invalid.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}

						if (intVal > 0) {
							if (nCount==0) {
								n = intVal;
							}
						} else {
							fprintf(stderr, "ERROR: Malformed command.\n");
							fprintf(stderr, "n must be a positive integer.\n");
							fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
							exit(1);
						}
					}
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The n value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}				

			}

			else if (!strcmp(argv[i], "-t")) {
				if (i+1 < argc) {
					fp = fopen(argv[i+1], "r");
					nCount++;
					ErrorCheckFirstLine(fp, argv[i+1]);
					t = argv[i+1];
				} else {
					fprintf(stderr, "ERROR: Malformed command.\n");
					fprintf(stderr, "The t value is not given.\n");
					fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
					exit(1);
				}
			}
		
			else {
				fprintf(stderr, "ERROR: Malformed command.\n");
				fprintf(stderr, "%s is not a valid commandline option.\n", argv[i]);
				fprintf(stderr, "Proper command must be \"warmup2 [-lambda lambda] [-mu mu] [-r r] [-B B] [-P P] [-n num] [-t tsfile]\" (square brackets optional).\n");
				exit(1);
			}


		}

	}

	return fp;
}


/*
This function will calculate the time between the current time and the time in the given input.
This is a helper function used for calculating timestamps in packet object
PRE: StartTime must be a valid time, i.e. before or exactly the current time
*/
struct timeval TimeDifference(struct timeval StartTime) {
	
	struct timeval currTime;
	struct timeval returnTime;

	gettimeofday(&currTime, NULL);	
	timersub(&currTime, &StartTime, &returnTime);
	
	return returnTime;

}


/*
This is a helper function that converts the timestamp into milliseconds.
PRE: thisTime must be before or exactly the current time
*/
double ConvertTimeStampMS(struct timeval thisTime) {

	double timeDouble = (double)thisTime.tv_sec*1000+(double)thisTime.tv_usec*0.001;
	return timeDouble;

}


/*
This is a helper function that error checks a passed-in line in the given file.
*/
void ErrorCheckFileLine(const char* buffer, int lineNo) {

	// check if line is > 1024 characters
	if (strlen(buffer) > 1024) {
		fprintf(stderr, "ERROR: Too many characters on line %d.\n", lineNo);
		exit(1);	
	}

	// check for leading and trailing space/tab characters
	if (buffer[0]==' ' || buffer[0]=='\t') {
		fprintf(stderr, "ERROR: There is a tab or space character in the beginning of line %d.\n", lineNo);
		exit(1);
	}
	if (buffer[strlen(buffer)-2]==' ' || buffer[strlen(buffer)-2]=='\t') {
		fprintf(stderr, "ERROR: There is a tab or space character in the end of line %d.\n", lineNo);
		exit(1);
	}

}


/*
This a helper function that calculates the next expected arrival time of a packet.
intArrTime is the milliseconds in which we add to the current time.
*/
struct timeval nextArrivalTime(double intArrTime) {

	//convert intArrTime(ms) to struct timeval
	int sec = (int)(intArrTime/1000);
	int msec = ((int)(intArrTime))%1000;
	struct timeval timeToAdd;
	timeToAdd.tv_sec=sec;
	timeToAdd.tv_usec=msec*1000;	

	// current time
	struct timeval now;
	gettimeofday(&now, NULL);

	// our target time
	struct timeval target;
	timeradd(&now, &timeToAdd, &target);

	return target;

}


/*
This is the start procedure for the packet thread.
*/
void *startPacket(void* arg) {
	
	FILE* fp = arg;

	double amountOfSleep=0;
	double prevArrivalTime=0; // in MS
	// variables under help calculate amount of sleep
	double expectArrivalTime=0;
	double timeToSubtract=0;

	
	// ----------------------------------- FOR WHEN THERE IS NO FILE PROVIDED BY USER ----------------------------------
	if (fp == NULL) {

		// disable cancellation HERE HERE HERE
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		for (int i=0; i<n; i++) {
			
			double finalInterArr = 0;
			double finalServiceTime = 0;

			// if 1/lambda is greater than 10 seconds
			if (1/lambda > (double)(10)) {
				finalInterArr = 10000;
			} else {
				finalInterArr = round((1000)*(1/lambda));	// default in milliseconds
			}

			// if 1/mu is greater than 10 seconds
			if (1/mu > (double)(10)) {
				finalServiceTime = 10000;
			} else {
				finalServiceTime = round((1000)*(1/mu));		// default in milliseconds
			}
			
			// calculate target next arrival time
			// calculate how far away that is from current time --> sleep for that time
			// current time - expected future arrival time == time to subtract to time supposed to sleep (flip the left side because current time is either the same or later

			if (i==0) {
				amountOfSleep = finalInterArr;
				expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(finalInterArr));		
			} else {
				struct timeval tempCurrentTime;
				gettimeofday(&tempCurrentTime, NULL);
				timeToSubtract = ConvertTimeStampMS(tempCurrentTime) - expectArrivalTime;
				amountOfSleep = finalInterArr - timeToSubtract;
				expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(finalInterArr));
			}
			
			
			// enable cancellation HERE HERE HERE
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

			if ( (int)amountOfSleep > 0 ) {
				if (amountOfSleep*1000 > 1000000) {		// if amountOfSleep (in MICROsecs) > threshold
					int seconds = (int)(amountOfSleep/1000);
					int milliSec = ((int)(amountOfSleep))%1000;
					
					sleep(seconds);		// sleep for x seconds
					if (milliSec > 0) {
						usleep(milliSec*1000);	// sleep for the remaining microseconds
					}
				} 
				else {
					usleep( (int)(1000*amountOfSleep) );	// convert from ms to us
				}
			} else {
				pthread_testcancel();
			}


			// disable cancellation HERE HERE HERE
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

			pthread_mutex_lock(&mutex);

			if (runTheProgram) {

				PacketObj* ourPacket = (PacketObj*)malloc(sizeof(PacketObj));
				ourPacket->packet_ID = i+1;
				ourPacket->tokenRequire = P;
				ourPacket->interArrival = finalInterArr;
				ourPacket->serviceTime = finalServiceTime;			
				ourPacket->packetArrive = TimeDifference(start_time);

				// used to calculate average packet inter-arrival time
				avgPackInterArrTime = avgPackInterArrTime + (ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);
	
				if (ourPacket->tokenRequire <= B) {
				
					printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms\n", ConvertTimeStampMS(ourPacket->packetArrive), ourPacket->packet_ID, ourPacket->tokenRequire, ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);
				
					prevArrivalTime = ConvertTimeStampMS(ourPacket->packetArrive);
				
					My402ListAppend(&Q1, ourPacket);
					ourPacket->enterQ1 = TimeDifference(start_time);
					printf("%012.3lfms: p%d enters Q1\n", ConvertTimeStampMS(ourPacket->enterQ1), ourPacket->packet_ID);

					if (My402ListLength(&Q1) == 1) {
						My402ListElem* headElem = My402ListFirst(&Q1);
						PacketObj* tempPacket = headElem->obj;

						if (tempPacket->tokenRequire <= currTokenCount) {
							My402ListUnlink(&Q1, headElem);
							//currTokenCount -= tempPacket->tokenRequire;
							currTokenCount=0;

							tempPacket->exitQ1 = TimeDifference(start_time);
							struct timeval timeDurationQ1;
							timersub(&(tempPacket->exitQ1), &(tempPacket->enterQ1), &timeDurationQ1);
							printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d token\n", ConvertTimeStampMS(tempPacket->exitQ1), tempPacket->packet_ID, ConvertTimeStampMS(timeDurationQ1), currTokenCount);

							My402ListAppend(&Q2, tempPacket);
							tempPacket->enterQ2 = TimeDifference(start_time);
							printf("%012.3lfms: p%d enters Q2\n", ConvertTimeStampMS(tempPacket->enterQ2), tempPacket->packet_ID);
					
							pthread_cond_broadcast(&cv);
						}
			
					}


				}
			
				else {
					printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms, dropped\n", ConvertTimeStampMS(ourPacket->packetArrive), ourPacket->packet_ID, ourPacket->tokenRequire, ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);
					free(ourPacket);
					numDroppedPack++;

					prevArrivalTime = ConvertTimeStampMS(ourPacket->packetArrive);
				}

			}

			pthread_mutex_unlock(&mutex);

		}		
	}

	// --------------------------- FOR WHEN A FILE IS PROVIDED BY THE USER ----------------------------
	else {
		char buf[2000];
		int lineNum=1;


		// disable cancellation HERE HERE HERE
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		for (int i=0; i<n; i++) {
			if (fgets(buf,sizeof(buf), fp) != NULL) {

				lineNum++;
				ErrorCheckFileLine(buf, lineNum);

				int tempInterArr=0;
				int tempTokenReq=0;
				int tempServiceTime=0;
				if (sscanf(buf, "%d %d %d", &tempInterArr, &tempTokenReq, &tempServiceTime) == 3) {
					if (tempInterArr <= 0) {
						fprintf(stderr, "ERROR: First field on line %d is not a positive integer.\n", lineNum);
						exit(1);
					}
					if (tempTokenReq <= 0) {
						fprintf(stderr, "ERROR: Second field on line %d is not a positive integer.\n", lineNum);
						exit(1);
					}
					if (tempServiceTime <= 0) {
						fprintf(stderr, "ERROR: Third field on line %d is not a positive integer.\n", lineNum);
						exit(1);
					}				
				}
				else {
					fprintf(stderr, "ERROR: Line %d does not contain 3 integers.\n", lineNum);
					exit(1);
				}

				double finalInterArr = (double)(tempInterArr);
				double finalServiceTime = (double)(tempServiceTime);

				if (i==0) {
					amountOfSleep = finalInterArr;
					expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(finalInterArr));
				} else {
					struct timeval tempCurrentTime;
					gettimeofday(&tempCurrentTime, NULL);
					timeToSubtract = ConvertTimeStampMS(tempCurrentTime) - expectArrivalTime;
					amountOfSleep = finalInterArr - timeToSubtract;
					expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(finalInterArr));
				}


				// enable cancellation HERE HERE HERE
				pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

				if ( (int)amountOfSleep > 0 ) {
					if (amountOfSleep*1000 > 1000000) {		// if amountOfSleep (in MICROsecs) > threshold
						int seconds = (int)(amountOfSleep/1000);
						int milliSec = ((int)(amountOfSleep))%1000;
						
						sleep(seconds);		// sleep for x seconds
						if (milliSec > 0) {
							usleep(milliSec*1000);	// sleep for the remaining microseconds
						}
					} 
					else {
						usleep( (int)(1000*amountOfSleep) );	// convert from ms to us
					}
				} else {
					pthread_testcancel();
				}
					

				// disable cancellation HERE HERE HERE
				pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

				pthread_mutex_lock(&mutex);
		
				if (runTheProgram) {

					// create packet
					PacketObj* ourPacket = (PacketObj*)malloc(sizeof(PacketObj));
					ourPacket->packet_ID = i+1;
					ourPacket->tokenRequire = tempTokenReq;
					ourPacket->interArrival = finalInterArr;
					ourPacket->serviceTime = finalServiceTime;
					ourPacket->packetArrive = TimeDifference(start_time);

					// used to calculate average packet inter-arrival time
					avgPackInterArrTime = avgPackInterArrTime + (ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);
				
					if (ourPacket->tokenRequire <= B) {
				
						printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms\n", ConvertTimeStampMS(ourPacket->packetArrive), ourPacket->packet_ID, ourPacket->tokenRequire, ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);

						prevArrivalTime = ConvertTimeStampMS(ourPacket->packetArrive);
				
						My402ListAppend(&Q1, ourPacket);
						ourPacket->enterQ1 = TimeDifference(start_time);
						printf("%012.3lfms: p%d enters Q1\n", ConvertTimeStampMS(ourPacket->enterQ1), ourPacket->packet_ID);

						if (My402ListLength(&Q1) == 1) {
							My402ListElem* headElem = My402ListFirst(&Q1);
							PacketObj* tempPacket = headElem->obj;

							if (tempPacket->tokenRequire <= currTokenCount) {
								My402ListUnlink(&Q1, headElem);
								//currTokenCount -= tempPacket->tokenRequire;
								currTokenCount=0;

								tempPacket->exitQ1 = TimeDifference(start_time);
								struct timeval timeDurationQ1;
								timersub(&(tempPacket->exitQ1), &(tempPacket->enterQ1), &timeDurationQ1);
								printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d token\n", ConvertTimeStampMS(tempPacket->exitQ1), tempPacket->packet_ID, ConvertTimeStampMS(timeDurationQ1), currTokenCount);
		
								My402ListAppend(&Q2, tempPacket);
								tempPacket->enterQ2 = TimeDifference(start_time);
								printf("%012.3lfms: p%d enters Q2\n", ConvertTimeStampMS(tempPacket->enterQ2), tempPacket->packet_ID);
						
								pthread_cond_broadcast(&cv);
							}
			
						}


					}
			
					else {
						printf("%012.3lfms: p%d arrives, needs %d tokens, inter-arrival time = %.3lfms, dropped\n", ConvertTimeStampMS(ourPacket->packetArrive), ourPacket->packet_ID, ourPacket->tokenRequire, ConvertTimeStampMS(ourPacket->packetArrive)-prevArrivalTime);
						free(ourPacket);
						numDroppedPack++;

						prevArrivalTime = ConvertTimeStampMS(ourPacket->packetArrive);
					}

				}

				pthread_mutex_unlock(&mutex);

			}
		}
	}
	
	
	pthread_mutex_lock(&mutex);
	runningPacketThread=0;
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&mutex);

	pthread_exit(0);
}


/*
This is the start procedure the token thread.
*/
void *startToken(void* arg) {

	double amountOfSleep=0;
	double expectArrivalTime=0; // in MS
	double interTokenArr = 0;
	int token_ID=0;
	double timeToSubtract=0;
	struct timeval tokenArrive;

	// disable cancellation HERE HERE HERE
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!My402ListEmpty(&Q1) || runningPacketThread) {

		if (1/r > (double)10) {
			interTokenArr = 10000;
		} else {
			interTokenArr = round((1000)*(1/r));
		}
		
		
		if (token_ID==0) {
			amountOfSleep = interTokenArr;
			expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(interTokenArr));
		} else {
			struct timeval tempCurrentTime;
			gettimeofday(&tempCurrentTime, NULL);
			timeToSubtract = ConvertTimeStampMS(tempCurrentTime) - expectArrivalTime;
			amountOfSleep = interTokenArr - timeToSubtract;
			expectArrivalTime = ConvertTimeStampMS(nextArrivalTime(interTokenArr));
		}


		// enable cancellation HERE HERE HERE
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

		if ( (int)amountOfSleep > 0 ) {
			if (amountOfSleep*1000 > 1000000) {		// if amountOfSleep (in MICROsecs) > threshold
				int seconds = (int)(amountOfSleep/1000);
				int milliSec = ((int)(amountOfSleep))%1000;
				
				//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
				sleep(seconds);		// sleep for x seconds
				if (milliSec > 0) {
					//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
					usleep(milliSec*1000);	// sleep for the remaining microseconds
				}
			} 
			else {
				//pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
				usleep( (int)(1000*amountOfSleep) );	// convert from ms to us
			}
		} else {
			pthread_testcancel();
		}


		// disable cancellation HERE HERE HERE
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

		pthread_mutex_lock(&mutex);

		if (runTheProgram) {
		
			token_ID++;
			tokenArrive = TimeDifference(start_time);
			totalNumToken++;
		
			if (currTokenCount < B) {
				currTokenCount++;
				if (currTokenCount == 1) {
					printf("%012.3lfms: token t%d arrives, token bucket now has %d token\n", ConvertTimeStampMS(tokenArrive), token_ID, currTokenCount);
				} else {
					printf("%012.3lfms: token t%d arrives, token bucket now has %d tokens\n", ConvertTimeStampMS(tokenArrive), token_ID, currTokenCount);
				}
			}
			else {
				numTokenDropped++;
				printf("%012.3lfms: token t%d arrives, dropped\n", ConvertTimeStampMS(tokenArrive), token_ID);		
			}

			while (!My402ListEmpty(&Q1)) {
				My402ListElem* headElem = My402ListFirst(&Q1);
				PacketObj* tempPacket = headElem->obj;
				if (tempPacket->tokenRequire <= currTokenCount) {
					My402ListUnlink(&Q1, headElem);
					currTokenCount=0;

					tempPacket->exitQ1 = TimeDifference(start_time);
					struct timeval timeDurationQ1;
					timersub(&(tempPacket->exitQ1), &(tempPacket->enterQ1), &timeDurationQ1);
					printf("%012.3lfms: p%d leaves Q1, time in Q1 = %.3lfms, token bucket now has %d token\n", ConvertTimeStampMS(tempPacket->exitQ1), tempPacket->packet_ID, ConvertTimeStampMS(timeDurationQ1), currTokenCount);
		
					My402ListAppend(&Q2, tempPacket);
					tempPacket->enterQ2 = TimeDifference(start_time);
					printf("%012.3lfms: p%d enters Q2\n", ConvertTimeStampMS(tempPacket->enterQ2), tempPacket->packet_ID);

					pthread_cond_broadcast(&cv);
				} else {
					break;
				}
			}

		}
		
		pthread_mutex_unlock(&mutex);

	}
	
	pthread_mutex_lock(&mutex);
	runningTokenThread=0;
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&mutex);

	pthread_exit(0);
	
}


/*
This is the start procedure for both of the server threads.
*/
void *startServer(void* arg) {

	int server_ID = (int)arg;
	double amountOfSleep=0;

	while (!My402ListEmpty(&Q2) || runningTokenThread) {

		pthread_mutex_lock(&mutex);
		
		while (My402ListEmpty(&Q2) && runningTokenThread && runTheProgram) {
			pthread_cond_wait(&cv, &mutex);
		}
		

		PacketObj* tempPacket = NULL; // we need to initialize because amountOfSleep accesses this variable
		// recheck is list is not empty just in case a server already took the last one
		// both servers could return from pthread_cond_wait but only have 1 packet remaining -> this check solves segmentation fault
		if (!My402ListEmpty(&Q2)){
			My402ListElem* headElemQ2 = My402ListFirst(&Q2);
			tempPacket = headElemQ2->obj;
			My402ListUnlink(&Q2, headElemQ2);

			tempPacket->exitQ2 = TimeDifference(start_time);
			struct timeval timeDurationQ2;
			timersub(&(tempPacket->exitQ2), &(tempPacket->enterQ2), &timeDurationQ2);
			printf("%012.3lfms: p%d leaves Q2, time in Q2 = %.3lfms\n", ConvertTimeStampMS(tempPacket->exitQ2), tempPacket->packet_ID, ConvertTimeStampMS(timeDurationQ2));

			tempPacket->beginService = TimeDifference(start_time);
			printf("%012.3lfms: p%d begins service at S%d, requesting %dms of service\n", ConvertTimeStampMS(tempPacket->beginService), tempPacket->packet_ID, server_ID, (int)(tempPacket->serviceTime));
		
		}		
		
		pthread_mutex_unlock(&mutex);

		if (tempPacket!=NULL) {
			amountOfSleep = tempPacket->serviceTime;
			if ( (int)amountOfSleep > 0 ) {
				if (amountOfSleep*1000 > 1000000) {		// if amountOfSleep (in MICROsecs) > threshold
					int seconds = (int)(amountOfSleep/1000);
					int milliSec = ((int)(amountOfSleep))%1000;

					sleep(seconds);		// sleep for x seconds
					if (milliSec > 0) {
						usleep(milliSec*1000);	// sleep for the remaining microseconds
					}
				}
				else {
					usleep( (int)(1000*amountOfSleep) );	// convert from ms to us
				}
			}
		
			pthread_mutex_lock(&mutex);

			tempPacket->leaveService = TimeDifference(start_time);
			struct timeval timeOfService;	// actual time of service
			timersub(&(tempPacket->leaveService), &(tempPacket->beginService), &timeOfService);
			struct timeval timeInSystem;
			timersub(&(tempPacket->leaveService), &(tempPacket->packetArrive), &timeInSystem);	// actual time that packet spent in system

			
			printf("%012.3lfms: p%d departs from S%d, service time = %.3lfms, time in system = %.3lfms\n", ConvertTimeStampMS(tempPacket->leaveService), tempPacket->packet_ID, server_ID, ConvertTimeStampMS(timeOfService), ConvertTimeStampMS(timeInSystem));
			pthread_mutex_unlock(&mutex);		

			// used to calculate average packet service time
			avgPackServTime = avgPackServTime + ConvertTimeStampMS(timeOfService);

			// time spent in system to calculate for average
			avgTimePackSpentSystem = avgTimePackSpentSystem + ConvertTimeStampMS(timeInSystem);
			// to compute E(X^2)
			avgTimePackSpentSystemSQUARED = avgTimePackSpentSystemSQUARED + pow(ConvertTimeStampMS(timeInSystem), 2);

			numCompletedPack++;

			// add up time for completed packets that were in Q1 facility
			struct timeval timeDurationQ1;
			timersub(&(tempPacket->exitQ1), &(tempPacket->enterQ1), &timeDurationQ1);
			avgNumPacksQ1 = avgNumPacksQ1 + ConvertTimeStampMS(timeDurationQ1);

			// add up time for completed packets that were in Q2 facility
			struct timeval timeDurationQ2;
			timersub(&(tempPacket->exitQ2), &(tempPacket->enterQ2), &timeDurationQ2);
			avgNumPacksQ2 = avgNumPacksQ2 + ConvertTimeStampMS(timeDurationQ2);			 

			if (server_ID==1) {
				// add up time for completed packets that were in S1 facility
				avgNumPacksS1 = avgNumPacksS1 + ConvertTimeStampMS(timeOfService);
			}
			else if (server_ID==2) {
				// add up time for completed packets that were in S2 facility
				avgNumPacksS2 = avgNumPacksS2 + ConvertTimeStampMS(timeOfService);
			}

		}
		
	}
	
	pthread_mutex_lock(&mutex);
	runTheProgram=0;	
	pthread_cond_broadcast(&cv);
	pthread_mutex_unlock(&mutex);
		
	pthread_exit(0);

}


/*
This is the start procedure forthe CTRL+C thread.
*/
void *catchSignal(void* arg) {

	int sig;
	while (1) {
		sigwait(&set, &sig);
		pthread_mutex_lock(&mutex);

		struct timeval caughtSigInt;
		caughtSigInt = TimeDifference(start_time);
		
		printf("%012.3lfms: SIGINT caught, no new packets or tokens will be allowed\n", ConvertTimeStampMS(caughtSigInt));
		runningPacketThread=0;
		runningTokenThread=0;
		runTheProgram=0;

		pthread_cancel(packetThread);
		pthread_cancel(tokenThread);
		pthread_cond_broadcast(&cv);


		// get rid of stuff in Q1 and Q2
		while (!My402ListEmpty(&Q1)) {
			My402ListElem* headElem = My402ListFirst(&Q1);
			PacketObj* tempPacket = headElem->obj;
			My402ListUnlink(&Q1, headElem);

			struct timeval remFromQ1;
			remFromQ1 = TimeDifference(start_time);
			printf("%012.3lfms: p%d removed from Q1\n", ConvertTimeStampMS(remFromQ1), tempPacket->packet_ID);

			numRemovedPack++;
		}

		while (!My402ListEmpty(&Q2)) {
			My402ListElem* headElem = My402ListFirst(&Q2);
			PacketObj* tempPacket = headElem->obj;
			My402ListUnlink(&Q2, headElem);

			struct timeval remFromQ2;
			remFromQ2 = TimeDifference(start_time);
			printf("%012.3lfms: p%d removed from Q2\n", ConvertTimeStampMS(remFromQ2), tempPacket->packet_ID);

			numRemovedPack++;
		}

		pthread_mutex_unlock(&mutex);
	}
	
	pthread_exit(0);

}


/*
This function prints the emulation parameters in the beginning of the simulation.
*/
void PrintEmulationParameters(FILE* thisFile) {
	
	printf("Emulation Parameters:\n");
	printf("\tnumber to arrive = %d\n", n);

	if (thisFile==NULL) {
		printf("\tlambda = %.6g\n", lambda);
		printf("\tmu = %.6g\n", mu);
	}

	printf("\tr = %.6g\n", r);
	printf("\tB = %d\n", B);
	
	if (thisFile==NULL) {
		printf("\tP = %d\n\n", P);
	}

	if (thisFile != NULL) {
		printf("\ttsfile = %s\n\n", t);
	}

}


/*
This function prints the statistics at the end of the simulation.
*/
void PrintStatistics() {

	printf("\nStatistics:\n\n");

	// average packet inter-arrival time
	if (n==0) {
		printf("\taverage packet inter-arrival time = N/A because no packet was served\n");
	} else {
		double finalAvgIntArrTime = avgPackInterArrTime/n;
		printf("\taverage packet inter-arrival time = %.6g\n", finalAvgIntArrTime/1000);
	}

	// average packet service time
	if (numCompletedPack==0) {
		printf("\taverage packet service time = N/A because no packet was completed\n\n");
	} else {
		double finalAvgServTime = avgPackServTime/numCompletedPack;
		printf("\taverage packet service time = %.6g\n\n", finalAvgServTime/1000);
	}

	// average number of packets at Q1
	if (end==0) {
		printf("\taverage number of packets in Q1 = N/A because emulation never started\n");
	} else {
		double finalAvgQ1Pack = avgNumPacksQ1/end;
		printf("\taverage number of packets in Q1 = %.6g\n", finalAvgQ1Pack);
	}

	// average number of packets at Q2
	if (end==0) {
		printf("\taverage number of packets in Q2 = N/A because emulation never started\n");
	} else {
		double finalAvgQ2Pack = avgNumPacksQ2/end;
		printf("\taverage number of packets in Q2 = %.6g\n", finalAvgQ2Pack);
	}

	// average number of packets at S1
	if (end==0) {
		printf("\taverage number of packets in S1 = N/A because emulation never started\n");
	} else {
		double finalAvgS1Pack = avgNumPacksS1/end;
		printf("\taverage number of packets in S1 = %.6g\n", finalAvgS1Pack);
	}

	// average number of packets at S2
	if (end==0) {
		printf("\taverage number of packets in S2 = N/A because emulation never started\n\n");
	} else {
		double finalAvgS2Pack = avgNumPacksS2/end;
		printf("\taverage number of packets in S2 = %.6g\n\n", finalAvgS2Pack);
	}

	// average time a packet spent in system
	if (numCompletedPack==0) {
		printf("\taverage time a packet spent in system = N/A because no packet was completed\n");
	} else {
		double finalAvgTimePackSystem = avgTimePackSpentSystem/numCompletedPack;
		printf("\taverage time a packet spent in system = %.6g\n", finalAvgTimePackSystem/1000);
	}

	// standard deviation for time spent in system
	if (numCompletedPack==0) {
		printf("\tstandard deviation for time spent in system = N/A because no packet was completed\n\n");
	} else {
		double finalAvgTime = avgTimePackSpentSystem/numCompletedPack;
		double finalAvgTimeSQUARED = avgTimePackSpentSystemSQUARED/numCompletedPack;

		double variance = finalAvgTimeSQUARED - pow(finalAvgTime, 2);
		double finalStandDev = sqrt(variance);
		printf("\tstandard deviation for time spent in system = %.6g\n\n", finalStandDev/1000);
	}
	
	// token drop probability
	if (totalNumToken==0) {
		printf("\ttoken drop probability = N/A because no token was generated\n");
	} else {
		double finalTokenProb = ((double)numTokenDropped)/totalNumToken;
		printf("\ttoken drop probability = %.6g\n", finalTokenProb);
	}

	// packet drop probability
	if (n==0) {
		printf("\tpacket drop probability = N/A because no packet was served\n");
	} else {
		double finalPackDropProb = ((double)numDroppedPack)/n;
		printf("\tpacket drop probability = %.6g\n", finalPackDropProb);
	}

}


/*
Main function
*/
int main(int argc, char *argv[])
{
	
	// initialize global variable start_time
	struct timeval tempStartTime;
	gettimeofday(&start_time, NULL);
	timersub(&start_time, &start_time, &tempStartTime);

	// default values
	lambda = 1;
	mu = 0.35;
	r = 1.5;
	B = 10;
	P = 3;
	n = 20;

	FILE* file_pointer;
	file_pointer = ProcessCommandLine(argc, argv);	
	// ^ could be NULL or could be something --> check if not null in packet thread function to either read in or use default values

	My402ListInit(&Q1);
	My402ListInit(&Q2);
	runTheProgram=1;

	// for CTRL C handling
	sigemptyset(&set);
	sigaddset(&set, SIGINT); // signal #2
	sigprocmask(SIG_BLOCK, &set, 0);

	PrintEmulationParameters(file_pointer);

	double start = ConvertTimeStampMS(tempStartTime);
	printf("%012.3lfms: emulation begins\n", start);

	pthread_create(&packetThread, 0, startPacket, (void*)file_pointer);
	runningPacketThread=1;
	pthread_create(&tokenThread, 0, startToken, NULL);
	runningTokenThread=1;
	pthread_create(&s1Thread, 0, startServer, (void*)1);
	pthread_create(&s2Thread, 0, startServer, (void*)2);

	pthread_create(&ctrlCThread, 0 , catchSignal, NULL);


	pthread_join(packetThread, 0);
	pthread_join(tokenThread, 0);
	pthread_join(s1Thread, 0);
	pthread_join(s2Thread, 0);
	
	//pthread_join(ctrlCThread, 0);
	pthread_cancel(ctrlCThread);


	if (file_pointer != NULL) {
		fclose(file_pointer);
	}


	struct timeval end_time;
	struct timeval tempEndTime;
	gettimeofday(&end_time, NULL);
	timersub(&end_time, &start_time, &tempEndTime);
	end = ConvertTimeStampMS(tempEndTime);
	printf("%012.3lfms: emulation ends\n", end);

	PrintStatistics();
	

	return(0);
}
