#ifndef JOB_H
#define JOB_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <bits/types.h>

enum etat{
    RUNNING, STOPPED, DONE,DETACHED,KILLED
};
typedef enum etat etat;


struct job{

    int numeroJob;
    char cmd[500];
    __pid_t numeroProcessus;
    __pid_t numGrp;
    etat statutJob;

};
typedef struct job job;


//void initialiseTabJob(struct job *tabJobs);
struct job createJob(__pid_t numProc, char *comd, enum etat statJob, int cmdEx, __pid_t nG, int isG, int finG);
int afficherJob(struct job j,int job, int redir);
int afficherAllJobs(int job, int redir);
int activeJobs();
int afficheArbo();
int freeJob(struct job j);
void majStatutJobs(int job);
void changeStatus(int newStat, int pos);

#endif
