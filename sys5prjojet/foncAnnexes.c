#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <linux/limits.h>
#include <signal.h>
#include "job.h"

#define CHEMIN_SIZE 500
#define ARGV_SIZE 50
extern struct job tabJobs[200];
extern int last_job;
extern char chemin[CHEMIN_SIZE];
extern char *argv[ARGV_SIZE];
extern char **red;
extern char *sauvegarde;
extern int redirInt[3];
extern char **pipeLine;
extern char ** substitutions;
extern struct sigaction sa, saExtern;
extern int nJob;

//formatage de l'affichage pour le prompt
char *affichage(){
    
    if(getcwd(chemin,CHEMIN_SIZE)==NULL) fprintf(stderr,"%s\n",strerror(errno));
    char * start="\001\033[36m[";
    char * start3="]\002\001\033[34m";
    char start2[3]; 
    snprintf(start2,sizeof(int), "%d",last_job);
    char * end="\002\001\033[00m$\002 ";
    size_t size=strlen(start)+strlen(start2)+strlen(start3)+strlen(end)+strlen(chemin)+1;
    char * s=calloc(size,sizeof(char));
    char * s30=calloc(size,sizeof(char));
    if(s==NULL) fprintf(stderr,"%s",strerror(errno));
    strcpy(s,start);
    strcat(s,start2);
    strcat(s,start3);
    strcat(s,chemin);
    strcat(s,end);
    if(strlen(s)-strlen(start)+1-strlen(start3)+1+strlen(start2)-strlen(end)+1>30){
        strcpy(s30,start);
        strcat(s30,start2);
        strcat(s30,start3);
        strcat(s30,"...");
        int p=0;
        if(last_job>=10){
            p=strlen(chemin)-21;
        }else{
            p=strlen(chemin)-22;
        }
        char *chem=chemin+p;
        //printf("%s \n", chem);
        strcat(s30,chem);
        //printf("%d \n",p);
        strcat(s30,end);
        free(s);
        return s30;
    }
    free(s30);
    return s;
}

//regarde si la chaine de caractère est vide
int estVide(char *string){
    for(int i=0; i<strlen(string); ++i){
        if(string[i]!=' ') return 0;
    }

    return 1;
}

//vérifie si la ligne de commande contient &
int contientBg(char *c){
    for(int i=0; i<strlen(c); ++i){
        if(c[i]=='&' && (i=strlen(c)-1)) return 1;
    }

    return 0;
}

//supprime le symbole & à la fin de la ligne de commande
void supprBg(char *chaine){
    int i, j = 0;
    for (i = 0; chaine[i]; i++) {
        if (chaine[i] != '&') {
            chaine[j++] = chaine[i];
        }
    }
    chaine[j] = '\0';
}

//renvoie le pid d'un job à partir de son numéro
pid_t numJobToPid(int process){
    for (int i = 0 ; i < last_job ; ++i) {
        if (process == tabJobs[i].numeroJob){
            return tabJobs[i].numeroProcessus; //renvoi le pid
        }
    }
    return -1;
}

//renvoie un job à partir de son numéro
struct job numJobToJob(int process){
    struct job newJob;
    for (int i = 0 ; i < sizeof(tabJobs) ; ++i) {
        if (process == tabJobs[i].numeroJob){
            newJob = tabJobs[i];
            return newJob;
        }
    }
    newJob.numeroJob = -1;
    snprintf(newJob.cmd, sizeof(newJob.cmd), " ");
    newJob.numeroProcessus = 0;
    newJob.statutJob = RUNNING;
    return newJob;
}

//copie une chaine de caractere sans le premier caractere
char* deleteFirstChar(char *c){ // fonction qui copie une chaine de caractere sans le premier caractere
    char *tmp = (char*)malloc(strlen(c));
    strcpy(tmp,c+1);
    return tmp;
}

//supprime les guillements d'une chaine de caractères
void deleteGuillemet(){
    int i, j, k=0;
    while(argv[k]!=NULL){
        j=0;
        for (i = 0; argv[k][i]; i++) {
            if (argv[k][i] != '\"') {
                argv[k][j++] = argv[k][i];
            }
        }
        argv[k][j] = '\0';
        //printf("%s\n",argv[k]);
        k+=1;
    }
}

//supprime les espaces d'une chaine de caractères
void deleteSpace(char *chaine) {
    int i, j = 0;
    for (i = 0; chaine[i]; i++) {
        if (chaine[i] != ' ') {
            chaine[j++] = chaine[i];
        }
    }
    chaine[j] = '\0';
}

//libère les pointeurs de redirections
void freeSpace(){
    int i=0;
    while(i<4){
        free(red[i]);
        i++;
    }
    free(red);
    free(sauvegarde);
}

//ferme les descripteurs des redirections
void closeredir(){
    for(int i=0; i<3; ++i){
        if(redirInt[i]!=-1){
            close(redirInt[i]);
        }
    }
}

//récupère l'indexe d'un job de pid pid
int getIndex(pid_t pid){
    for(int i=0; i<last_job; ++i){
        if(tabJobs[i].numeroProcessus==pid){
            return i;
        }
    }
    return -1;
}

//ferme les descripteurs en trop des pipelines
void closePfd(int pipes[][2],int compteur){
    for (int i=0; i<compteur; ++i){
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

//libère les pointeurs des pipelines
void freePipeline(int compteur){
    for(int i=0; i<compteur; ++i){
        free(pipeLine[i]);
    }
    free(pipeLine);
}

//libère les pointeurs des substitutions
void freePSub(int compteur){
    for(int i=0; i<compteur; ++i){
        free(substitutions[i]);
    }
    free(substitutions);
}

char *getEntreS(){
    char *eS=calloc(51,sizeof(char));
    size_t i=0;
    size_t j=0;
    while( (i=read(redirInt[0],eS+j,50*sizeof(char))) ){
        j=i;
    }
    //printf("%s \n",eS);
    return eS;
}

//regarde si une commande doit être lancée à l'arrière plan
int lancerBackground(){
    int i = 0;
    while (argv[i] != NULL)
    {
        if(!strcmp(argv[i], "&")) 
        {   
            argv[i] = NULL;
            return i;
        }
        i++;
    }
    return -1;
}

//envoie s'il y a une redirection d'entrée standard
int hasStdinRedir(char *c){
    int i=0;
    while(i<strlen(c)){
        if(c[i]=='<') return 1;
        i+=1;
    }
    return 0;
}

//envoie s'il y a une redirection de sortie standard
int hasStdoutRedir(char *c){
    int i=0;
    while(i<strlen(c)){
        if(c[i]=='>' && i>0 && c[i-1]!='2') return 1;
        i+=1;
    }
    return 0;

}

//met en place le handler pour jsh
void sigactionJsh(){
    sigaction(SIGINT,&sa,NULL);
    sigaction(SIGTSTP,&sa,NULL);
    sigaction(SIGTTOU,&sa,NULL);
    sigaction(SIGTTIN,&sa,NULL);
    sigaction(SIGTERM,&sa,NULL);
    sigaction(SIGQUIT,&sa,NULL);
    
}

//met en place le handler des commandes premier plan
void sigactionExtren(){
    sigaction(SIGINT,&saExtern,NULL);
    sigaction(SIGTSTP,&saExtern,NULL);
    sigaction(SIGTTIN,&saExtern,NULL);
    sigaction(SIGTERM,&saExtern,NULL);
    sigaction(SIGQUIT,&saExtern,NULL);
    
}