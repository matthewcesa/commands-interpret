#include "job.h"
#include "foncAnnexes.h"
#include "analys.h"
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


//variable globale de chemin, servant à afficher correctement le prompt
#define CHEMIN_SIZE 500
#define ARGV_SIZE 50
#define RED_SIZE 10
#define BUFFER_SIZE sizeof(int)

extern struct job tabJobs[200];
extern int last_job;
extern sigset_t jshMask, externMask;
char chemin[CHEMIN_SIZE];
char *argv[ARGV_SIZE];
char **red;
char **pipeLine;
char ** substitutions;
char *redUtil[RED_SIZE];
int redirInt[3];
char *entree;
int dernier_appel = 0;
char dernierCD[500];
char *sauvegarde;
int sigTerm=0;
int term_job=-1;
int isGroup=-1;
int sigRecu=15;
int hasPipe=0;
int redProc=0;
int isSub=0;
int pipeSub[2];
int comSub=0;
char *sauvBuf;
int bgSub=0;




//mettre ici les fonctions cd, pwd, ? et exit
int pwd(){
    char chemin[10024];

    if(getcwd(chemin, sizeof(chemin)) == NULL){
        if(redirInt[2]!=-1){
            write(redirInt[2],strerror(errno),strlen(strerror(errno)));
        }else{
            fprintf(stderr,"%s",strerror(errno));
        }
        return 1;
    }
    ssize_t ret_w =0;
    if(redirInt[1]!=-1){
        ret_w=write(redirInt[1], chemin, strlen(chemin));
        write(redirInt[1], "\n", 1);
    }else{
        ret_w=write(STDOUT_FILENO, chemin, strlen(chemin));
        write(STDOUT_FILENO, "\n", 1);
    }
    if(ret_w != strlen(chemin)){
        if(redirInt[2]!=-1){
            write(redirInt[2],strerror(errno),strlen(strerror(errno)));
        }else{
            fprintf(stderr,"%s",strerror(errno));
        }
        return 1;
    }
    return 0;
}

int cd(char *chemin) {
    if (strcmp(chemin, "vide") == 0)
    {
        chdir(getenv("HOME"));
        return 0;
    }
    if(strcmp(chemin,"-") == 0) {
       if(chdir(dernierCD)==0)
        return 0;
    }else{
        if (chdir(chemin) == 0) {
            return 0;
        }   
    }
    if(redirInt[2]!=-1){
        write(redirInt[2],strerror(errno),strlen(strerror(errno)));
    }else{
        fprintf(stderr,"%s\n",strerror(errno));
    }
    return 1;
}

// verifie l'existence d'autres jobs, si oui affiche une advertissement et retourne 1,
// sinon exit avec le val donnée
int exit_call(int val){
    int j=activeJobs();
    if (j > 0)
    {
        if(redirInt[2]!=-1){
            char *c="exit\n certains processus n'ont pas fini\n";
            write(redirInt[2],c,strlen(c));
        }else{
            fprintf(stderr, "exit\n certains processus n'ont pas fini\n");
        }
        return 1;
    } else
    {
        exit(val);
    }
}


//modifie les variables d'envoi d'un signal pour un groupe
int kill_group(int signal, pid_t index){
    redirInterne();
    for (int i = 0 ; i < last_job ; ++i){
        if(tabJobs[i].numeroJob == index){
                term_job=i;
                sigTerm=1;
                isGroup=1;
                sigRecu=signal;
            return 0;
        }
    }
    if(redirInt[2]!=-1){
        char *c="Groupe inexistant! \n";
        write(redirInt[2],c,strlen(c));
    }else{
        fprintf(stderr,"Groupe inexistant! \n");
    }
    return 1;
}

//modifie les variables d'envoi d'un signal pour un processus
int kill_processus(int signal, pid_t pid){
    redirInterne();
    //on regarde si le job existe 
    for (int i = 0 ; i < last_job ; ++i){
        if (pid == tabJobs[i].numeroProcessus){
                term_job = i;
                sigTerm = 1;
                isGroup=-1;
                sigRecu=signal;    
            return 0;
        }
    }
    if(redirInt[2]!=-1){
        char *c="Processus inexistant! \n";
        write(redirInt[2],c,strlen(c));
    }else{
        fprintf(stderr,"Processus inexistant! \n");
    }
    return 1;
} 




void last_call(){
   char appel_buffer[BUFFER_SIZE];
   int tmp = snprintf(appel_buffer, sizeof(appel_buffer), "%d", dernier_appel);
   if (tmp < 0 || tmp >= sizeof(appel_buffer)) {
        if(redirInt[2]!=-1){
            write(redirInt[2],strerror(errno),strlen(strerror(errno)));
        }else{
            fprintf(stderr,"%s",strerror(errno));
        }
        return;
    }
    int nwrite=0;
    if(redirInt[1]!=-1){
        nwrite=write(redirInt[1], appel_buffer, tmp);
    }else{
        nwrite=write(STDOUT_FILENO, appel_buffer, tmp);
    }
    if(nwrite != tmp){
        if(redirInt[2]!=-1){
            write(redirInt[2],strerror(errno),strlen(strerror(errno)));
        }else{
            fprintf(stderr,"%s",strerror(errno));
        }
        return;
    }
    write(STDOUT_FILENO, "\n", 1);
}



//récupère la commande et ses arguments
void getArg(){
    char *tmp;
    int i=0;
    sauvegarde=calloc(strlen(red[0])+1,sizeof(char));
    strncpy(sauvegarde,red[0],strlen(red[0]));
    tmp=strtok(red[0]," ");
    while(tmp!=NULL){
        argv[i]=tmp;
        i+=1;
        tmp=strtok(NULL," ");
    }
    argv[i]=NULL;
    deleteGuillemet();

}






//fonction qui s'occupe de rediriger les descripteurs pour les commandes externes
int redirections(char *c, char *fic){
    if(!strcmp(c,"<")){
        int fd=open(fic,O_RDONLY);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDIN_FILENO)!=STDIN_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }else if(!strcmp(c,">")){
        int fd=open(fic,O_WRONLY|O_EXCL|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }else if(!strcmp(c,">>")){
        int fd=open(fic,O_WRONLY|O_APPEND|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    } else if(!strcmp(c,">|")){
        int fd=open(fic,O_WRONLY|O_TRUNC|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDOUT_FILENO)!=STDOUT_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }else if(!strcmp(c,"2>")){
        int fd=open(fic,O_WRONLY|O_EXCL|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDERR_FILENO)!=STDERR_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }else if(!strcmp(c,"2>>")){
        int fd=open(fic,O_WRONLY|O_APPEND|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDERR_FILENO)!=STDERR_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }else if(!strcmp(c,"2>|")){
        int fd=open(fic,O_WRONLY|O_TRUNC|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return -1;}
        if(dup2(fd,STDERR_FILENO)!=STDERR_FILENO) fprintf(stderr,"%s\n",strerror(errno));
        close(fd);
    }
    return 0;
}

//fonction qui s'occupe d'ouvrir des fichiers pour les redirections des commandes internes 
void redirectionsInt(char *c, char *fic){
    if(!strcmp(c,"<")){
        int fd=open(fic,O_RDONLY);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[0]=fd;
    }else if(!strcmp(c,">")){
        int fd=open(fic,O_WRONLY|O_EXCL|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[1]=fd;
    }else if(!strcmp(c,">>")){
        int fd=open(fic,O_WRONLY|O_APPEND|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[1]=fd;
    } else if(!strcmp(c,">|")){
        int fd=open(fic,O_WRONLY|O_TRUNC|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[1]=fd;
    }else if(!strcmp(c,"2>")){
        int fd=open(fic,O_WRONLY|O_EXCL|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[2]=fd;
    }else if(!strcmp(c,"2>>")){
        int fd=open(fic,O_WRONLY|O_APPEND|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[1]=fd;
    }else if(!strcmp(c,"2>|")){
        int fd=open(fic,O_WRONLY|O_TRUNC|O_CREAT,0666);
        if(fd<0){ fprintf(stderr,"%s\n",strerror(errno));
            return ;}
        redirInt[1]=fd;
    }

}

//fonction qui appelle le gestionnaire des redirections internes
void redirInterne(){
    int j=0;
    redirInt[0]=-1;
    redirInt[1]=-1;
    redirInt[2]=-1;
    while(j<=RED_SIZE && redUtil[j]!=NULL){
        if(red[j+1]==NULL && redUtil[j]!=NULL){
            fprintf(stderr,"redirection incomplete\n");
            dernier_appel=1;
            return;
        }else{
            deleteSpace(red[j+1]);
            redirectionsInt(redUtil[j],red[j+1]);
        }
        j+=1;
    }

}

int foreground_cmd(){
    char *tmp = deleteFirstChar(argv[1]);
    int numJob = atoi(tmp);
    
    for (int i = 0; i < last_job; i++)
    {
        if(numJob == tabJobs[i].numeroJob){
            if(sigprocmask(SIG_SETMASK,&externMask,&jshMask)==-1){
                perror("sigprocmask");
                return 1;
            }
            int retour = 0;
            sigactionExtren();

            if(kill(-(tabJobs[i].numGrp), SIGCONT) < 0) {
                perror("foregorund error type 2: ");
                free(tmp);
                return 1;
            }
            
            if(tcsetpgrp(STDIN_FILENO, tabJobs[i].numGrp) < 0){
                perror("foreground error type 1: ");
                free(tmp);
                return 1;
            }
            waitpid(tabJobs[i].numGrp, &retour,WUNTRACED);
            // operations du reprendre le shell
            if(sigprocmask(SIG_SETMASK,&jshMask,&externMask)==-1){
                perror("sigprocmask");
                return 1;
            }
            sigactionJsh();
            tcsetpgrp(STDIN_FILENO, getpid());
            if(WIFSTOPPED(retour)) {
                tabJobs[i].statutJob=STOPPED;
                afficherJob(tabJobs[i],0,-1);
            }
            else if(WIFEXITED(retour)){
                tabJobs[i].statutJob=DONE;
                freeJob(tabJobs[i]);
                free(tmp);
            }
            else if(WIFSIGNALED(retour)){
                tabJobs[i].statutJob=KILLED;
                afficherJob(tabJobs[i],0,-1);
                freeJob(tabJobs[i]);
                free(tmp);
            }
            
            
            return 0;
        }
    }
    fprintf(stderr, "job non existant\n");
    free(tmp);
    return 1;
}

// recupere le numero de job et le relance s'il existe envoie une message d'erreur sinon
int bg_cmd(){
    char *tmp = deleteFirstChar(argv[1]);
    int numJob = atoi(tmp);
    for (int i = 0; i < last_job; i++)
    {
        if (numJob == tabJobs[i].numeroJob)
        {
            tabJobs->statutJob = RUNNING;
            if(kill(-(tabJobs[i].numGrp), SIGCONT) == -1){
                perror("background Error: ");
                free(tmp);
                return 1;
            }
            free(tmp);
            return 0;
        }
    }
    fprintf(stderr, "job non existant\n");
    free(tmp);
    return 1;
}

//fonction qui lance les commandes en arrière plan
void background_cmd(){
    if(sigprocmask(SIG_SETMASK,&externMask,&jshMask)==-1){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    int retour;
    enum etat start = RUNNING;
    if (pid == 0) {
        sigactionExtren();
        int j=0;
        //appelle la fonction qui redirige les descripteurs pour chaque redirection trouvée
        while(j<=RED_SIZE && redUtil[j]!=NULL){
            if(red[j+1]==NULL && redUtil[j]!=NULL){
                fprintf(stderr,"redirection incomplete\n");
                dernier_appel=1;
                exit(1);
            }else{
                deleteSpace(red[j+1]);
                //printf("ICI %s \n",red[j+1]);
                if(redirections(redUtil[j],red[j+1])==-1){
                    dernier_appel=1;
                    exit(1);
                    }
            }
                j+=1;
        }
        if(execvp(argv[0],argv)== -1) fprintf(stderr,"%s\n",strerror(errno));
        exit(1);
    }else{
        setpgid(pid,pid);
        int i=getIndex(pid);
         if(bgSub){
            afficherJob(createJob(pid,sauvBuf,start,0,pid,0,0),0,-1);
            bgSub=0;
        }else{
            afficherJob(createJob(pid,sauvegarde,start,1,pid,0,0),0,-1);
        }
        //int i=getIndex(pid);
        if(waitpid(pid,&retour,WNOHANG|WUNTRACED|WCONTINUED)!=0){
                if(WIFSTOPPED(retour)) {
                    tabJobs[i].statutJob=STOPPED;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(WIFCONTINUED(retour)){
                    tabJobs[i].statutJob=RUNNING;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(WIFSIGNALED(retour)){
                    tabJobs[i].statutJob=KILLED;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(tabJobs[i].statutJob!=KILLED && tabJobs[i].statutJob!=STOPPED){
                    tabJobs[i].statutJob=DONE;
                }

        }
        if(sigprocmask(SIG_SETMASK,&jshMask,&externMask)==-1){
            perror("sigprocmask");
            exit(EXIT_FAILURE);
        }
        sigactionJsh();
        dernier_appel=WEXITSTATUS(retour);
    }
}

//fonction qui vérifie si un job à l'arrière plan est fini/tué/stoppé
void verifieFin(int job){
    int retour=0;
    for(int i=0; i<last_job;++i){
        if(waitpid(tabJobs[i].numeroProcessus,&retour,WNOHANG|WUNTRACED|WCONTINUED)>0){
                if(WIFSTOPPED(retour)) {
                    tabJobs[i].statutJob=STOPPED;
                    afficherJob(tabJobs[i],job,-1);
                }else if(WIFCONTINUED(retour)){
                    tabJobs[i].statutJob=RUNNING;
                    afficherJob(tabJobs[i],job,-1);
                }else if(WIFSIGNALED(retour)){
                    tabJobs[i].statutJob=KILLED;
                }else if(tabJobs[i].statutJob!=KILLED && tabJobs[i].statutJob!=STOPPED){
                    tabJobs[i].statutJob=DONE;
                }
        }else{
            //printf("failed\n");
        }
    }

    majStatutJobs(job);
}

//envoie le signal désigné via kill
void verifSIGTERM(){
    if(sigTerm==1){
        sigTerm=0;
        if(isGroup==1){
            changeStatus(sigRecu, term_job);
            if(kill((-(tabJobs[term_job].numGrp)), sigRecu)==-1){
                    perror("Erreur");
                    exit(1);
            }
        }else{
            if(tabJobs[term_job].numeroProcessus==tabJobs[term_job].numGrp){
                changeStatus(sigRecu, term_job);
                    if(kill(tabJobs[term_job].numeroProcessus, sigRecu)== -1){
                        perror("Erreur");
                        exit(1);
                    }
            }
        }
        sigRecu=15;
        if(tabJobs[term_job].statutJob==STOPPED) afficherJob(tabJobs[term_job],0,-1);
    }
}


//fonction qui analyse la commande envoyée et appele les fonctions des commandes internes ou créé un fils pour les commandes externes
void analys (){
    if(argv[0]==NULL) argv[0]="";
        if(!strcmp("exit",argv[0])){
            redirInterne();
            if(argv[1]==NULL){
                dernier_appel=exit_call(dernier_appel);
            }else{
                dernier_appel=exit_call(atoi(argv[1]));
            }
            closeredir();
        }else if(!strcmp("cd",argv[0])){
            //appel à cd
            redirInterne();
            char *tmp = malloc(PATH_MAX);
            getcwd(tmp,PATH_MAX);
            if(argv[1] == NULL) {
                dernier_appel = cd("vide");
            }else{
                dernier_appel = cd(argv[1]);
            }
            strcpy(dernierCD, tmp);
            free(tmp);
            closeredir();
        }else if (!strcmp("kill",argv[0])){
            //appel à kill
            if(argv[2] == NULL){
                char* num = argv[1];
                if(num[0] == '%'){
                    num = deleteFirstChar(num);
                    pid_t num2=atoi(num);
                    dernier_appel = kill_group(SIGTERM, num2);   
                    free(num);
                }else{
                    pid_t num2=atoi(num);
                    dernier_appel = kill_processus(SIGTERM, num2);
                }
            }else
            {
                char* si = argv[1];
                char* num = argv[2];
                if(si[0] == '-')
                    si = deleteFirstChar(si);
                if(num[0] == '%'){
                    num = deleteFirstChar(num);
                    kill_group(atoi(si), atoi(num));
                    free(num);
                }else{
                    dernier_appel = kill_processus(atoi(si), atoi(num));
                }
                if(strlen(si) < strlen(argv[1])) free(si);
            }
        }else if(!strcmp("pwd",argv[0])){
            //appel à pwd
            redirInterne();
            dernier_appel = pwd();
        }else if(!strcmp("?",argv[0])){
            //appel à ?
            redirInterne();
            last_call();
            dernier_appel=0;
            closeredir();
        }else if(!strcmp("jobs",argv[0])){
            verifieFin(1);
            redirInterne();
            if (argv[1] == NULL){
                dernier_appel = afficherAllJobs(1,redirInt[1]); // jobs
                
            }else{
                char *tmp = argv[1];
                if(tmp[0] == '%'){ //jobs %..
                    char *tmp2 = deleteFirstChar(tmp);
                    dernier_appel = afficherJob(numJobToJob(atoi(tmp2)),1,redirInt[1]);
                    
                }else if(!strcmp(tmp,"-t")){ //jobs -l
                    //afficher l'arborescence
                    dernier_appel = afficheArbo(); 
                }
            }
        }else if(!strcmp("bg", argv[0])){
            redirInterne();
            dernier_appel = bg_cmd();
            closeredir();
        }else if(!strcmp("fg", argv[0])){
            redirInterne();
            dernier_appel = foreground_cmd();
            closeredir();
        }else{
             //appel aux fonctions externes
            if(hasPipe){
                int j=0;
                //méthode qui gère la mise en place des redirections
                while(j<=RED_SIZE && redUtil[j]!=NULL){
                            if(red[j+1]==NULL && redUtil[j]!=NULL){
                                fprintf(stderr,"redirection incomplete\n");
                                dernier_appel=1;
                                exit(1);
                            }else{
                                deleteSpace(red[j+1]);
                                if(redirections(redUtil[j],red[j+1])==-1){
                                    dernier_appel=1;
                                    exit(1);
                                }
                            }
                            j+=1;
                }
                if(execvp(argv[0],argv) == -1) fprintf(stderr,"%s\n",strerror(errno));
                exit(1);
            }else{
                //on vérifie si la commande est au premier ou à l'arrière plan
                if(lancerBackground() != -1 || bgSub){
                    background_cmd();
                    return;
                }
                //appel aux fonctions externes si premier plan
                if(sigprocmask(SIG_SETMASK,&externMask,&jshMask)==-1){
                    perror("sigprocmask");
                    exit(EXIT_FAILURE);
                }
                pid_t pid=fork();
                int retour=0;
                if(pid == 0){
                    sigactionExtren();
                        int j=0;
                        //appelle la fonction qui redirige les descripteurs pour chaque redirection trouvée
                        while(j<=RED_SIZE && redUtil[j]!=NULL){
                            if(red[j+1]==NULL && redUtil[j]!=NULL){
                                fprintf(stderr,"redirection incomplete\n");
                                dernier_appel=1;
                                exit(1);
                            }else{
                                deleteSpace(red[j+1]);
                                //printf("ICI %s \n",red[j+1]);
                                if(redirections(redUtil[j],red[j+1])==-1){
                                    dernier_appel=1;
                                    exit(1);
                                }
                            }
                            j+=1;
                        }

                        if(isSub){
                            isSub=0;
                            if(dup2(pipeSub[1],1)<0) {
                                fprintf(stderr,"Erreur dup2 %d\n",errno);
                                close(pipeSub[1]);
                                return;
                            }
                            close(pipeSub[1]);
                        }
                        
                        if(execvp(argv[0],argv) == -1) fprintf(stderr,"%s\n",strerror(errno));
                        exit(1);
                }else{
                    setpgid(pid,pid);
                    tcsetpgrp(STDERR_FILENO,pid);
                    tcsetpgrp(STDIN_FILENO,pid);
                        if(waitpid(pid, &retour,WUNTRACED|WCONTINUED)>0){
                            if(WIFEXITED(retour)){
                                dernier_appel=WEXITSTATUS(retour);
                            }else if(WIFSTOPPED(retour)) {
                                    createJob(pid,sauvegarde,STOPPED,0,pid,0,0);
                                    int i=getIndex(pid);
                                    afficherJob(tabJobs[i],0,-1);
                            }else if(WIFCONTINUED(retour)){
                                    createJob(pid,sauvegarde,RUNNING,0,pid,0,0);
                                    int i=getIndex(pid);
                                    afficherJob(tabJobs[i],0,-1);
                            }else if(WIFSIGNALED(retour)){
                                    //createJob(pid,sauvegarde,KILLED,0,pid);
                                    fprintf(stderr,"Terminated\n");
                                }
                            
                        }

                        if(sigprocmask(SIG_SETMASK,&jshMask,&externMask)==-1){
                            perror("sigprocmask");
                             exit(EXIT_FAILURE);
                        }
                        sigactionJsh();
                        tcsetpgrp(STDERR_FILENO,getpid());
                        tcsetpgrp(STDIN_FILENO,getpid());
                        verifieFin(0);
                }
            }
        }
        
       
}                         


//fonction qui analyse les redirections de la ligne de commande
void getRed(char * buf){
    int i=0;
    int sauvegarde=0;
    int compteur=0;
    red=calloc(4,sizeof(char *));
    for(int k=0; k<3;k++){
        red[k]=NULL;
    }   
    //remplit le tableau avec la commande et les fichiers de redirection, ansi que le tableau des redirections utilisées
    while(i<=strlen(buf)-1){
        if(buf[i]=='<'){
                red[compteur]=calloc((i+2-sauvegarde),sizeof(char));
                strncpy(red[compteur],buf+sauvegarde,i-sauvegarde);
                redUtil[compteur]="<";
                i+=1;
                sauvegarde=i;
                compteur+=1;
        }else if(buf[i]=='>'){
            if(buf[i+1]!='\0'){
                red[compteur]=calloc((i+2-sauvegarde),sizeof(char));
                strncpy(red[compteur],buf+sauvegarde,i-sauvegarde);
                if(buf[i+1]=='>'){
                    redUtil[compteur]=">>";
                    i+=2;
                    sauvegarde=i;
                    compteur+=1;
                }else if(buf[i+1]=='|'){
                    redUtil[compteur]=">|";
                    i+=2;
                    sauvegarde=i;
                    compteur+=1;
                }else if(buf[i+1]==' ' && buf[i+2]!='\0' && buf[i+2]=='|'){
                    fprintf(stderr,"Erreur de syntaxe\n");
                    goto error;
                }else{
                    redUtil[compteur]=">";
                    i+=1;
                    sauvegarde=i;
                    compteur+=1;
                }
            }else{
                fprintf(stderr,"Chaine incomplete\n");
                goto error;
            }
        }else if(buf[i]=='2'){
            if(buf[i+1]!='\0'){
                if(buf[i+1]=='>'){
                    if(buf[i+2]!='\0'){ 
                        red[compteur]=calloc((i+2-sauvegarde),sizeof(char));
                        strncpy(red[compteur],buf+sauvegarde,i-sauvegarde);
                        if(buf[i+2]=='>'){
                            redUtil[compteur]="2>>";
                            i+=3;
                            sauvegarde=i;
                            compteur+=1;
                        }else if(buf[i+2]=='|'){
                            redUtil[compteur]="2>|";
                            i+=3;
                            sauvegarde=i;
                            compteur+=1;
                        }else{
                            redUtil[compteur]="2>";
                            i+=2;
                            sauvegarde=i;
                            compteur+=1;
                        }
                    }else{
                        fprintf(stderr,"Redirection invalide\n");
                        goto error;
                    }
                }else{
                    i+=1;
                }            
            }else{
                red[compteur]=calloc((i+2-sauvegarde),sizeof(char));
                strncpy(red[compteur],buf+sauvegarde,i+1-sauvegarde);
                i+=1;
            }
        }else if(i>=strlen(buf)-1){
            red[compteur]=calloc((i+2-sauvegarde),sizeof(char));
            strncpy(red[compteur],buf+sauvegarde,i+1-sauvegarde);
            i+=1;
        }else{
            i+=1;
        }
    }
    
    redUtil[compteur]=NULL;

    //appel au lancement des commandes
    getArg();
    analys();
    freeSpace();
    return;

    error:
    freeSpace();
}



//fonction qui analyse les pipelines de la ligne de commande
void pipeline(char * buf){
    verifSIGTERM();
    int i=0;
    int compteur=0;
    if(isSub) close(pipeSub[0]);
    //compte le nombre de pipe
    while(i<strlen(buf)){
        if(buf[i]=='|' && i==0){
            fprintf(stderr,"Erreur de syntaxe\n");
            dernier_appel=1;
            return;
        }else if(buf[i]=='|' && i>0 && buf[i-1]!='>') compteur+=1;
        i+=1;
    }

    if(compteur==0){
        hasPipe=0;
        getRed(buf);
        
        return;
    }

    hasPipe=1;

    pipeLine=calloc(compteur+1,sizeof(char *));
    for(int k=0; k<compteur+1; ++k){
        pipeLine[k]=NULL;
    }

    i=0;
    int sauvegarde=0;
    int c=0;
    int isBg=0;
    //parsing
    while(i<strlen(buf)){
        if(buf[i]=='|' && i==0){
            fprintf(stderr,"Erreur de syntaxe\n");
            freePipeline(compteur+1);
            dernier_appel=1;
            return;
        }else if(buf[i]=='|' && i>0 && buf[i-1]!='>'){
            pipeLine[c]=calloc((i+2-sauvegarde),sizeof(char));
            strncpy(pipeLine[c],buf+sauvegarde,i-sauvegarde);
            if(estVide(pipeLine[c])){
                fprintf(stderr,"Erreur de syntaxe\n");
                freePipeline(compteur+1);
                dernier_appel=1;
                return;
            }else if((c!=0 && hasStdinRedir(pipeLine[c])) || (c!=compteur && hasStdoutRedir(pipeLine[c])) ){
                fprintf(stderr,"Redirection incorrecte! \n");
                freePipeline(compteur+1);
                dernier_appel=1;
                return;
            }
            sauvegarde=i+1;
            c+=1;
            i+=1;
        }else if(i>=strlen(buf)-1){
            pipeLine[c]=calloc((i+2-sauvegarde),sizeof(char));
            strncpy(pipeLine[c],buf+sauvegarde,i+1-sauvegarde);
            if(estVide(pipeLine[c]) || strlen(pipeLine[c])==0){
                fprintf(stderr,"Erreur de syntaxe\n");
                freePipeline(compteur+1);
                dernier_appel=1;
                return;
            }else if ((c!=0 && hasStdinRedir(pipeLine[c]))){
                fprintf(stderr,"Redirection incorrecte! \n");
                freePipeline(compteur+1);
                dernier_appel=1;
                return;
            }else{
                if(contientBg(pipeLine[c])) {
                    isBg=1;
                    supprBg(pipeLine[c]);
                }
            }
            i+=1;
        }else{
            i+=1;
        }
    }

    
    int cmd=0;
    int tab_pipe[compteur][2];
    

    for(int h=0; h<compteur; ++h){
        if(pipe(tab_pipe[h])!=0) goto error;
    }

    pid_t childS=-1;
    pid_t childF=-1; 
    //création des enfants, liaisions avec les pipes, exécution de leurs commandes et changement de masque
    if(sigprocmask(SIG_SETMASK,&externMask,&jshMask)==-1){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    for(int j=0; j<=compteur; ++j){
            pid_t child=-1;
            if((child=fork())==-1){
                goto error;
            }else if(child==0){
                sigactionExtren();
                if(cmd==0){
                    if(dup2(tab_pipe[cmd][1],1)<0) goto error;
                    closePfd(tab_pipe,compteur);
                    getRed(pipeLine[cmd]);
                    exit(dernier_appel);
                }else if(j==compteur){
                    if(dup2(tab_pipe[cmd-1][0],0)<0) goto error;
                    if(isSub){
                        isSub=0;
                        if(dup2(pipeSub[1],1)<0) goto error;
                        close(pipeSub[1]);
                    }
                    closePfd(tab_pipe,compteur);
                    getRed(pipeLine[cmd]);
                    exit(dernier_appel);
                }else{
                    if(dup2(tab_pipe[cmd-1][0],0)<0) goto error;
                    if(dup2(tab_pipe[cmd][1],1)<0) goto error;
                    closePfd(tab_pipe,compteur);
                    getRed(pipeLine[cmd]);
                    exit(dernier_appel);
                }
            }else{
                if(j==0){
                    childS=child;
                }if(j==compteur){
                    childF=child;
                }
                setpgid(child,childS);
                cmd+=1;
            }
    }
    if(!isBg && !bgSub){
        tcsetpgrp(STDERR_FILENO,childS);
        tcsetpgrp(STDIN_FILENO,childS);
    }
    closePfd(tab_pipe,compteur);
    if(isBg){
        //création d'un job si la pipeline est en arrière plan et wait non bloquant
        afficherJob(createJob(childF,buf,RUNNING,1,childS,1,1),0,-1);
        int i=getIndex(childF);
        int retour=0;
        if(waitpid(childS,&retour,WNOHANG|WUNTRACED|WCONTINUED)!=0){
                if(WIFSTOPPED(retour)) {
                    tabJobs[i].statutJob=STOPPED;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(WIFCONTINUED(retour)){
                    tabJobs[i].statutJob=RUNNING;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(WIFSIGNALED(retour)){
                    tabJobs[i].statutJob=KILLED;
                    afficherJob(tabJobs[i],0,-1);
                }
                if(tabJobs[i].statutJob!=KILLED && tabJobs[i].statutJob!=STOPPED){
                    tabJobs[i].statutJob=DONE;
                }

        }
        dernier_appel=WEXITSTATUS(retour);
    }else{
        //wait bloquant si la pipeline est au premier plan
        while(1){
            if(waitpid(-1,NULL,0)<0){
                break;
            }
        }
    }

    //protection de jsh
    if(sigprocmask(SIG_SETMASK,&jshMask,&externMask)==-1){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    sigactionJsh();
    tcsetpgrp(STDERR_FILENO,getpid());
    tcsetpgrp(STDIN_FILENO,getpid());

    freePipeline(compteur+1);
    return;



    error:
    dernier_appel=1;
    fprintf(stderr,"Erreur pipeline\n");
    freePipeline(compteur+1);

}




//fonction qui analyse les substitutions de la ligne de commande
void subProcessus(char  * buf){
    int compteur =0;

    int i=0;

    int securite=0;
    int debut=0;
    //compte le nombre de redirections
    while(i<strlen(buf)){
        if(buf[i]=='<' && buf[i+1]!='\0' && buf[i+1]=='('){
            debut=1;
            i+=1;
        }else if(i>0 && buf[i]=='(' && buf[i-1]!='<'){
            securite=1;
            i+=1;
        }else if(buf[i]==')'){
            if(debut){
                compteur+=1;
                debut=0;
                i+=1;
            }else if(securite){
                securite=0;
                i+=1;
            }else{
                fprintf(stderr,"Substitution mal formée!\n");
                dernier_appel =1;
                return;
            }
        }else{
            i+=1;
        }
    }
    
    if(debut){
        fprintf(stderr,"Substitution mal formée!\n");
        dernier_appel=1;
        return;
    }

    if(compteur ==0){
        isSub=0;
        pipeline(buf);
        return;
    }

    isSub=1;
    int isBg=0;
    substitutions=calloc(compteur+1,sizeof(char *));
    for(int k=0; k<compteur+1; ++k){
        substitutions[k]=NULL;
    }
    i=0;
    int sauvegarde=0;
    int c=0;
    int deb=0;
    char *sauv=NULL;
    //parsing
    while(i<strlen(buf)){
        if(buf[i]=='<' && buf[i+1]!='\0' && buf[i+1]=='('){
            if(c==0){
                substitutions[c]=calloc((i+2-sauvegarde),sizeof(char));
                strncpy(substitutions[c],buf+sauvegarde,i-sauvegarde);
                sauvegarde=i+2;
                c+=1;
                i+=2;
                deb=1;
            }else if(buf[i]=='<' && buf[i+1]!='\0' && buf[i+1]==' ' && buf[i+2]!='\0' && buf[i+1]=='('){
                goto error;
            }else{
                char *ca=calloc(i-sauvegarde,sizeof(char));
                strncpy(ca,buf+sauvegarde,i-1-sauvegarde);
                if(!estVide(ca)){
                    free(ca);
                    goto error;
                }
                free(ca);
                sauvegarde=i+2;
                i+=2;
                deb=1;
            }
        }else if(buf[i]==')' && deb){
            deb=0;
            substitutions[c]=calloc((i+2-sauvegarde),sizeof(char));
            strncpy(substitutions[c],buf+sauvegarde,i-sauvegarde);
            if(estVide(substitutions[c])){
                goto error;
            }
            
            sauvegarde=i+1;
            c+=1;
            i+=1;
        }else if(i>=strlen(buf)-1){
            char *cc=calloc(i-sauvegarde+1,sizeof(char));
            strncpy(cc,buf+sauvegarde,i-sauvegarde+1);
            if(!estVide(cc)){
                if(contientBg(cc)){
                    isBg=1;
                    free(cc);
                }else if(hasStdoutRedir(cc)){
                    sauv=cc;
                }else{
                    free(cc);
                    goto error;
                }
            }
            //free(cc);
            i+=1;
        }else{
            i+=1;
        }
    }

    
    if(pipe(pipeSub)!=0) goto error;
    pid_t childS=-1;
    //changement du masque et lancement des processus fils
    if(sigprocmask(SIG_SETMASK,&externMask,&jshMask)==-1){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    for(int j=1; j<=compteur; ++j){
        pid_t child=-1;
        if((child=fork())==-1){
            goto error;
        }else if(child==0){
            sigactionExtren();
            close(pipeSub[0]);
            pipeline(substitutions[j]);
            exit(dernier_appel);
        }else{
            if(j==0) childS=child;
            setpgid(child,childS);
        }
    }
    if(!isBg){
        tcsetpgrp(STDERR_FILENO,childS);
        tcsetpgrp(STDIN_FILENO,childS);
    }
    if(sigprocmask(SIG_SETMASK,&jshMask,&externMask)==-1){
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    sigactionJsh();

    //appel de la commande principale
    isSub=0;
    close(pipeSub[1]);
    char *cc="/dev/fd/";
    char *p=calloc(1,sizeof(int));
    snprintf(p,sizeof(int), "%d",pipeSub[0]);
    char *x;
    if(sauv!=NULL){
        x=calloc(strlen(substitutions[0])+strlen(cc)+strlen(p)+4+strlen(sauv),sizeof(char));
    }else{
        x=calloc(strlen(substitutions[0])+strlen(cc)+strlen(p)+3,sizeof(char));
    }
    strcat(x,substitutions[0]);
    strcat(x," ");
    strcat(x,cc);
    strcat(x,p);
    if(sauv!=NULL){
        strcat(x," ");
        strcat(x,sauv);
    }
    if(isBg){
        sauvBuf=buf;
        bgSub=1;
    }
    getRed(x);
    free(sauv);
    free(p);
    free(x);
    close(pipeSub[0]);
    close(pipeSub[1]);
    bgSub=0;
    
    
    
    freePSub(compteur+1);

    return;

    error:
    fprintf(stderr,"Erreur substitution\n");
    dernier_appel=1;
    freePSub(compteur+1);




}


