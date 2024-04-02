#ifndef FONCANNEXES_H
#define FONCANNEXES_H   

char *affichage();
int estVide(char *string);
int contientBg(char *c);
void supprBg(char *chaine);
pid_t numJobToPid(int process);
struct job numJobToJob(int process);
char* deleteFirstChar(char *c);
void deleteGuillemet();
void deleteSpace(char *chaine);
void freeSpace();
void closeredir();
int getIndex(pid_t pid);
void closePfd(int pipes[][2],int compteur);
void freePipeline(int compteur);
void freePSub(int compteur);
char *getEntreS();
int lancerBackground();
int hasStdinRedir(char *c);
int hasStdoutRedir(char *c);
void sigactionJsh();
void sigactionExtren();

#endif