# ARCHTECTURE

- Ce document sert à décrire la strategie adoptée dans la creation du projet de Système d'explotation de 3ème année de Licence en Informatique à l'Université Paris Cité.
- L'objective de ce projet c'était programmer un shell de commandes jsh. Pour ça on a decidé de diviser le projet sur 4 fichier principaux:
    * jsh - Contient la boucle principale du programme;
    * analys - Verifie et exécute les commandes données;
    * job -Job controle du programme
    * foncAnnexes - Contient toutes les fonctions auxiliaires 


## STRUCTURE

Dans cette section on va approfondir logique et les structures utilisées dans chaque fichier:

### jsh
**Fonctionnalité:** jsh contient une grand boucle while que prendre et sauvegarde les commandes passé par l'utilisateur.
La récuperation des commandes est donnée par la fonction readline de la bibliothèque readline

### analys

**Fonctionnalité:** Analys est responsable pour interpreter et executer la commande passé par l'utilisateur. Analys est composé de fonctions principales `analys`, qui prendre la ligne passé par readline et l'analise. Elle contient aussi toutes founctions d'appel sysèteme, c'est-à-dire tous les commandes qu'on puisse executer sur le terminal, les `commandes externes` sont executés dans la fonction analys.
Les fonctions `getRed` `pipeline` `subProcessus` gèrent le parsing de la ligne de commande et l'exécution dans le cas des pipelines et des substitutions.
Une ligne de commande passe d'abord par subProcessus pour gérer les éventuelles substitutions. Puis elle passe dans pipeline pour cette fois ci y gérer les potentielles pipelines. Puis dans get red pour définir les substitutions s'il y en a. 
Sont ensuites appelées getArg pour définir la commande et ses arguments puis analys pour l'exécuter.
Analys est aussi responsable de gérer les groupes de processus lors d'execution d'une commande.

### job
**Fonctionnalité:** Job à comme rôle de gérer le `job control`, pour ça on a decidé de créer un tableaux de struct que garde chaque groupe de processus actif (jobs). Tous les fouctions dans job serts que à gerer et garantir un bon fonctionnement de plusieurs jobs.
La structure dénommée job a 5 atributs:
 * sa position dans `jobs control` (numeroJob);
 * son numéro de processus (numeroProcessus);
 * sa commande (cmd);
 * le numero de son groupe processus (numGrp);
 * son etat (statutJob);

### foncAnnexes
**Fonctionalité:** foncAnnexes contient toutes les fonctions auxiliaires utilisées partout dans le programme, plus precisement les fonctions ne contenant pas d'appels système majeurs et n'ayant pas d'impact direct sur le programme (sauf pour les handler). FoncAnnexes est aussi responsable de moduler les sa_handler de jsh. 