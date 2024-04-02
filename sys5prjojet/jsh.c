#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include "foncAnnexes.h"
#include "analys.h"

extern int dernier_appel;
sigset_t jshMask, externMask, oldmask;
struct sigaction sa ={0}, saExtern={0};


int main(){

    //mise en place du masque et des structures sigactions
    sigemptyset(&jshMask);
    sigemptyset(&externMask);
    sigaddset(&jshMask,SIGINT);
    sigaddset(&jshMask,SIGTTOU);
    sigaddset(&jshMask,SIGTERM);
    sigaddset(&jshMask,SIGTSTP);
    sigaddset(&jshMask,SIGTTIN);
    sigaddset(&jshMask,SIGQUIT);
    sigaddset(&externMask,SIGTTOU);

    if (sigprocmask(SIG_SETMASK, &jshMask, &oldmask)==-1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    
    sa.sa_handler=SIG_IGN;
    saExtern.sa_handler=SIG_DFL;

    sigactionJsh();

    char*buf;
    rl_outstream = stderr;
    while(1){
        
        char *aff=affichage();
        buf=readline(aff);
        free(aff);
        if(buf==NULL) exit(dernier_appel);
        if(strlen(buf)>0) {
            add_history(buf);
            subProcessus(buf);
        }else{
            verifSIGTERM();
        }
        free(buf);
        verifieFin(0);





    }

    if (sigprocmask(SIG_SETMASK, &oldmask, NULL)==-1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    


    exit(0);
}