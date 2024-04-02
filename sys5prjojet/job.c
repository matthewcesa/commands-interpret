#include "job.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

int last_job = 0;
int lastNum=1;
struct job tabJobs[200];
extern int redirInt[3];
int nJob=0;

struct job createJob(__pid_t numProc, char *comd, enum etat statJob, int cmdEx, __pid_t nG, int isG, int finG){
    struct job newJob;
    newJob.numeroProcessus = numProc;
    newJob.numeroJob=last_job+1;
    newJob.statutJob = statJob;
    newJob.numGrp=nG;
    strcpy(newJob.cmd, comd);
    if(cmdEx){
        newJob.cmd[strlen(newJob.cmd)-2]='\0';
    }
    tabJobs[last_job] = newJob;
    /*if(!isG || finG){
        lastNum+=1;
    }
    if(!isG || finG) nJob+=1;*/
    last_job+=1;
    return newJob;
}

int afficherJob(struct job j,int job, int redir){
    char *statut = NULL;
    switch(j.statutJob){
        case STOPPED : statut = "Stopped";
                    break;
        case DONE : statut = "Done";
                    break;
        case DETACHED : statut = "Detached";
                    break;
        case KILLED : statut = "Killed";
                    break;
        default : statut = "Running";
                    break;
    }
    if(job){
        if(redir!=-1){
            char *c1="[";
            char *c11="]";
            char *c2=calloc(1,sizeof(int));
            snprintf(c2,sizeof(int),"%d",j.numeroJob);
            char *c3=calloc(1,sizeof(int));
            snprintf(c3,sizeof(int),"%d",j.numeroProcessus);
            char *c4=calloc(strlen(statut)+1,sizeof(char));
            strncpy(c4,statut,strlen(statut));
            char *c5=calloc(strlen(j.cmd)+1,sizeof(char));
            strncpy(c5,j.cmd,strlen(j.cmd));
            char *c=calloc(strlen(c1)+strlen(c11)+strlen(c2)+strlen(c3)+strlen(c4)+strlen(c5)+5,sizeof(char));
            strcat(c,c1);
            strcat(c,c2);
            strcat(c,c11);
            strcat(c," ");
            strcat(c,c3);
            strcat(c," ");
            strcat(c,c4);
            strcat(c," ");
            strcat(c,c5);
            strcat(c,"\n");
            write(redir,c,strlen(c));
            free(c2);
            free(c3);
            free(c4);
            free(c5);
            free(c);
        }else{
            printf("[%d] %d %s %s\n",j.numeroJob,j.numeroProcessus,statut,j.cmd);
        }
    }else{
        fprintf(stderr,"[%d] %d %s %s\n",j.numeroJob,j.numeroProcessus,statut,j.cmd);
    }
    return 0;
}

int afficherAllJobs(int job, int redir){
    
    for (int i =0 ; i < last_job ; ++i) {
        if (tabJobs[i].numeroJob != -1 && tabJobs[i].numeroProcessus != 0){
            afficherJob(tabJobs[i], job,redir);
        }
    }
    return 0;
}

int afficheArbo(){
    struct dirent *entry;
    DIR *dir = opendir("/proc");
    struct stat st;
    if (dir == NULL)
    {
        perror("failed to open /proc: ");
        return 1;
    }
    

    while ((entry = readdir(dir))) // lise chaque file de proc
    {
        char ref[200];
        snprintf(ref,strlen(ref)+1, "/proc/%s", entry->d_name);
        int pid = atoi(entry->d_name);
        if(stat(ref, &st)==0){
            if (S_ISDIR(st.st_mode) && pid != 0)
            {
                for(int i=0; i<last_job; i++){
                    snprintf(ref, strlen(ref)+1, "/proc/%s/status", entry->d_name);
                    FILE *status_file = fopen(ref, "r");
                    if (status_file == NULL) {
                        perror("open /proc failed: ");
                        closedir(dir);
                        return 1;
                    }
                    char line[200];
                    int processID = -1;
                    int procesPID = -1;
                    char state[3];
                    char name[256];
                    int found = 0;
                    while(fgets(line, sizeof(line), status_file) != NULL){ // parcous sur le fichier status
                        int process_group;
                        if (strncmp(line, "Pid:", 4) == 0) { //enregistre le pid
                            sscanf(line + 5, "%d", &processID);
                        } else if (strncmp(line, "State:", 6) == 0) { //enregistre le etat
                            sscanf(line + 7, "%s", state);
                        } else if (strncmp(line, "Name:", 5) == 0) { // enregistre le nom cmd
                            sscanf(line + 6, "%s", name);
                        }else if (strncmp(line, "PPid:", 5) == 0){
                            sscanf(line + 6, "%d", &procesPID);
                        }else if (strncmp(line, "Tgid:", 5) == 0) { // verifie que le numero de groupe est le meme que du jobs
                            sscanf(line + 6, "%d", &process_group);
                            if (process_group == tabJobs[i].numGrp) {
                                found = 1;
                            }
                        }
                        
                    }
                    if (found)
                    {
                        if (!strcmp(state, "R"))
                        {
                            strncpy(state, "Running", sizeof(state) - 1);
                        }else if (!strcmp(state, "S"))
                        {
                            strncpy(state, "Stopped", sizeof(state) - 1);
                        }

                        if(pid == tabJobs[i].numeroProcessus){
                            printf("[%d] %d %s %s\n",tabJobs[i].numeroJob,procesPID,state,name);
                        }else if (procesPID == tabJobs[i].numeroProcessus)
                        {
                            printf("\t| %d %s %s\n",procesPID,state,name);
                        }else
                        {
                            printf("\t %d %s %s\n",procesPID,state,name);
                        }
                    }
                    fclose(status_file);
                }
            }
            
        }
        
    }
    closedir(dir);
    return 0;
}


// retourne 1 au cas d'un
int activeJobs(){
    int jobs_en_cours = 0;
    for (int i = 0; i < last_job; ++i){
        if(tabJobs[i].statutJob == RUNNING || tabJobs[i].statutJob == STOPPED)
            jobs_en_cours+=1;
    }
    return jobs_en_cours;
}

int isGrp(struct job j){
    for(int i=0; i< last_job; i++){
        if(tabJobs[i].numGrp==j.numGrp && tabJobs[i].numeroProcessus!=j.numeroProcessus){
            return 1;
        }
    }
    return 0;
}

int minGNum(){
    int k=0;
    for(int i=0; i<last_job; i++){
        if(tabJobs[i].numeroJob>=k){
            k=tabJobs[i].numeroJob;
        }
    }
    return k;
}

int freeJob(struct job j){ //remet la valeur de la case du tableau a vide
    int k=0;
    for( int i = 0 ; i < last_job ; ++i){
        if (tabJobs[i].numeroJob == j.numeroJob && tabJobs[i].numeroProcessus == j.numeroProcessus ){
           k=i;
           break;
        }
    }
    for(int i=k; i<last_job-1; ++i){
        tabJobs[i]=tabJobs[i+1];
    }
    last_job-=1;
    return 0;
}


void changeStatus(int newStat, int pos){
    if (newStat == 1 || newStat == 2 || newStat == 9
        || (newStat >= 13 && newStat <= 15))
    {
        tabJobs[pos].statutJob = KILLED;
    }else if (newStat >= 17 && newStat <= 26)
    {
        if(tabJobs[pos].statutJob == STOPPED){
            if(newStat == 18 || newStat == 19 || newStat == 25)
                tabJobs[pos].statutJob = RUNNING;

        }else
        {
            tabJobs[pos].statutJob=STOPPED;
        }
    }
}




// a utiliser dans une boucle infini
void majStatutJobs(int job){ 
    //elle met sans arrêt à jour tous le tableau de jobs si le statut des jobs sont killed, done.
    for (int i = 0 ; i < last_job ; ++i) {
        if(tabJobs[i].statutJob == KILLED || tabJobs[i].statutJob == DONE){
            afficherJob(tabJobs[i],job,-1);
            freeJob(tabJobs[i]);
        }
    }
}
