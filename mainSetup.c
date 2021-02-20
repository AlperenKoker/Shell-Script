#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_LINE 128 /* 128 chars per line, per command, should be enough. */
#define BUFFER_SIZE 1000
//define append and trunct flags for redirection operation 
//define permission mode for redirection operation
#define CREATE_APPEND_FLAGS (O_WRONLY | O_CREAT | O_APPEND)
#define CREATE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define CREATE_FLAGS (O_WRONLY | O_CREAT | O_TRUNC)
//Global variables for keep child's pid and check running any process or not on foreground
int controlForeground = 0;
int childPid;
//define struct for bookmark operation
struct Node
{
    char data[MAX_LINE];
    struct Node *next;
};
//define struct for background process information
struct Process
{
    char name[MAX_LINE];
    int pid;
    int index;
    int status;
    struct Process *next;
};
//add running background process in list
void appendRunning(struct Process **head_ref, char *processName, int processPid, int processIndex, int processStatus)
{
    // allocate node 
    struct Process *process_new_node = (struct Process *)malloc(sizeof(struct Process));

    struct Process *last = *head_ref;

    // put in the data  
    strcpy(process_new_node->name, processName);
    process_new_node->pid = processPid;
    process_new_node->index = processIndex;
    process_new_node->status = processStatus;

    // This new node is going to be the last node, so make next  of it as NULL
    process_new_node->next = NULL;

    // 4. If the Linked List is empty, then make the new node as head 
    if (*head_ref == NULL)
    {
        *head_ref = process_new_node;
        return;
    }

    //Else traverse till the last node
    while (last->next != NULL)
        last = last->next;

    //. Change the next of last node
    last->next = process_new_node;
    return;
}
//add frequent command in list
void append(struct Node **head_ref, char *new_data)
{
    // allocate node
    struct Node *new_node = (struct Node *)malloc(sizeof(struct Node));

    struct Node *last = *head_ref;

    // put in the data
    strcpy(new_node->data, new_data);

    // This new node is going to be the last node, so make next of it as NULL
    new_node->next = NULL;

    // If the Linked List is empty, then make the new node as head
    if (*head_ref == NULL)
    {
        *head_ref = new_node;
        return;
    }

    // Else traverse till the last node
    while (last->next != NULL)
        last = last->next;

    // Change the next of last node
    last->next = new_node;
    return;
}
//delete desired frequent command
void deleteNode(struct Node **head_ref, int position)
{
    // If linked list is empty
    if (*head_ref == NULL)
        return;

    // Store head node
    struct Node *temp = *head_ref;

    // If head needs to be removed
    if (position == 0)
    {
        *head_ref = temp->next; // Change head
        free(temp);             // free old head
        return;
    }

    // Find previous node of the node to be deleted
    for (int i = 0; temp != NULL && i < position - 1; i++)
        temp = temp->next;

    // If position is more than number of nodes
    if (temp == NULL || temp->next == NULL)
        return;

    // Node temp->next is the node to be deleted
    // Store pointer to the next of node to be deleted
    struct Node *next = temp->next->next;

    // Unlink the node from linked list
    free(temp->next); // Free memory

    temp->next = next; // Unlink the deleted node from list
}
//print all frequent command which in list
void printList(struct Node *node)
{
    int count = 0;
    while (node != NULL)
    {
        printf("[%d] %s\n", count, node->data);
        count++;
        node = node->next;
    }
}
//find index of node where in list
char *findSpesificIndex(struct Node *node, int index)
{
    int count = 0;
    while (node != NULL)
    {
        if (count == index)
        {
            return node->data;
        }
        count++;
        node = node->next;
    }
    return "1";
}
//print all closed background process which in list
int dShow(struct Process **rHead, struct Process *node)
{
    int c = 0;
    printf("Finished:\n");
    while (node != NULL)
    {
        if (node->status == 1)
        {
            printf("[%d] %s\n", node->index, node->name);
            c++;
            node = node->next;
        }
        else
        {
            node = node->next;
        }
    }
    return c;
}
//print all running background process which in list
void rshow(struct Process *node)
{
    printf("Running:\n");
    while (node != NULL)
    {
        if (node->status == 0)
        {
            printf("[%d] %s (Pid=%d)\n", node->index, node->name, node->pid);
            node = node->next;
        }
        else
        {
            node = node->next;
        }
    }
}
//delete closed background process where in list
void deleteNodeBackgroundProcess(struct Process **head_ref, int key)
{
    // Store head node
    struct Process *temp = *head_ref, *prev;

    // If head node itself holds the key to be deleted
    if (temp != NULL && temp->status == key)
    {
        *head_ref = temp->next; // Changed head
        free(temp);             // free old head
        return;
    }

    // Search for the key to be deleted, keep track of the
    // previous node as we need to change 'prev->next'
    while (temp != NULL && temp->status != key)
    {
        prev = temp;
        temp = temp->next;
    }

    // If key was not present in linked list
    if (temp == NULL)
        return;

    // Unlink the node from linked list
    prev->next = temp->next;

    free(temp); // Free memory
}
//control background status is closed or running
void controlStatus(struct Process *node)
{
    while (node != NULL)
    {
        if (node->pid == waitpid(node->pid, NULL, WNOHANG))
        {
            node->status = 1;
            node = node->next;
        }
        else
        {
            node = node->next;
        }
    }
}
//control any background process is running or not
int controlExit(struct Process *node)
{
    while (node != NULL)
    {
        if (node->status == 0)
        {
            return 1;
        }
        else
        {
            node = node->next;
        }
    }
    return 0;
}

/* The setup function below will not return any value, but it will just: read
in the next command line; separate it into distinct arguments (using blanks as
delimiters), and set the args array entries to point to the beginning of what
will become null-terminated, C-style strings. */

int setup(char inputBuffer[], char *args[], int *background)
{
    int length, /* # of characters in the command line */
        i,      /* loop index for accessing inputBuffer array */
        start,  /* index where beginning of next command parameter is */
        ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = read(STDIN_FILENO, inputBuffer, MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if (length == 0)
        exit(0); /* ^d was entered, end of user command stream */

    /* the signal interrupted the read system call */
    /* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if ((length < 0) && (errno != EINTR))
    {
        perror("error reading the command");
        exit(-1); /* terminate with error code of -1 */
    }

    for (i = 0; i < length; i++)
    { /* examine every character in the inputBuffer */

        switch (inputBuffer[i])
        {
        case ' ':
        case '\t': /* argument separators */
            if (start != -1)
            {
                args[ct] = &inputBuffer[start]; /* set up pointer */
                ct++;
            }
            inputBuffer[i] = '\0'; /* add a null char; make a C string */
            start = -1;
            break;

        case '\n': /* should be the final char examined */
            if (start != -1)
            {
                args[ct] = &inputBuffer[start];
                ct++;
            }
            inputBuffer[i] = '\0';
            args[ct] = NULL; /* no more arguments to this command */
            break;

        default: /* some other character */
            if (start == -1)
                start = i;
            if (inputBuffer[i] == '&')
            {
                *background = 1;
                inputBuffer[i - 1] = '\0';
            }
        }            /* end of switch */
    }                /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

    return ct;
} /* end of setup routine */

//check path is executable or not
int checkifexecutable(const char *filename)
{
    int result;
    struct stat statinfo;

    result = stat(filename, &statinfo);
    if (result < 0)
        return 0;
    if (!S_ISREG(statinfo.st_mode))
        return 0;

    if (statinfo.st_uid == geteuid())
        return statinfo.st_mode & S_IXUSR;
    if (statinfo.st_gid == getegid())
        return statinfo.st_mode & S_IXGRP;
    return statinfo.st_mode & S_IXOTH;
}
//find path of which desired argument
int findpathof(char *pth, const char *exe)
{
    char *searchpath;
    char *beg, *end;
    int stop, found;
    int len;

    if (strchr(exe, '/') != NULL)
    {
        if (realpath(exe, pth) == NULL)
            return 0;
        return checkifexecutable(pth);
    }

    searchpath = getenv("PATH");
    if (searchpath == NULL)
        return 0;
    if (strlen(searchpath) <= 0)
        return 0;

    beg = searchpath;
    stop = 0;
    found = 0;
    do
    {
        end = strchr(beg, ':');
        if (end == NULL)
        {
            stop = 1;
            strncpy(pth, beg, PATH_MAX);
            len = strlen(pth);
        }
        else
        {
            strncpy(pth, beg, end - beg);
            pth[end - beg] = '\0';
            len = end - beg;
        }
        if (pth[len - 1] != '/')
            strcat(pth, "/");
        strncat(pth, exe, PATH_MAX - len);
        found = checkifexecutable(pth);
        if (!stop)
            beg = end + 1;
    } while (!stop && !found);

    return found;
}
//control Z signal handler control any foreground process is running or not if is have kill child with all childs
void controlZSignal()
{
    int status;
    if (controlForeground == 1)
    {
        kill(childPid, 0);
        if (errno == ESRCH)
        {
            fprintf(stderr, "process not found %d \n", childPid);
            controlForeground = 0;
        }
        else
        {
            kill(childPid, SIGKILL);
            waitpid(-childPid, &status, WNOHANG);
            controlForeground = 0;
            printf("\n");
        }
    }
    else
    {
        printf("\n");
    }
}
//find line number in file if word is in file
void indexOf(struct dirent *fptr, const char *word, int *line, char *path)
{
    char str[BUFFER_SIZE];
    char *pos;
    strcat(path, "/");
    strcat(path, fptr->d_name);
    *line = -1;
    FILE *fp;
    fp = fopen(path, "r");
    while ((fgets(str, BUFFER_SIZE, fp) != NULL))
    {
        *line += 1;

        // Find first occurrence of word in str
        pos = strstr(str, word);

        if (pos != NULL)
        {
            printf("%d: %s ->%s", *line, fptr->d_name, str);
        }
    }
    *line = -1;
}
//travell all files recursively which with sub directory
void listFilesRecursively(char *basePath, char *word)
{
    char path[1000];
    struct dirent *dp;
    DIR *dir = opendir(basePath);

    // Unable to open directory stream
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
        {
            char *ext;
            ext = strchr(dp->d_name, '.');
            if ((ext) != NULL && (!strcmp(ext + 1, "c") || !strcmp(ext + 1, "C") || !strcmp(ext + 1, "h") || !strcmp(ext + 1, "H")))
            {
                printf("%s\n", dp->d_name);
                int line;
                indexOf(dp, word, &line, basePath);
            }
            // Construct new path from our base path
            strcpy(path, basePath);
            strcat(path, "/");
            strcat(path, dp->d_name);

            listFilesRecursively(path, word);
        }
    }

    closedir(dir);
}
//travell all files which in current directory
void listFiles(char *path, char *word)
{
    struct dirent *dp;
    DIR *dir = opendir(path);

    // Unable to open directory stream
    if (!dir)
        return;

    while ((dp = readdir(dir)) != NULL)
    {
        char *ext;
        ext = strchr(dp->d_name, '.');
        if ((ext) != NULL && (!strcmp(ext + 1, "c") || !strcmp(ext + 1, "C") || !strcmp(ext + 1, "h") || !strcmp(ext + 1, "H")))
        {
            int line;
            indexOf(dp, word, &line, path);
        }
    }

    // Close directory stream
    closedir(dir);
}
//substring for throw away "
char *substr(const char *src, int m, int n)
{
    // get length of the destination string
    int len = n - m;

    // allocate (len + 1) chars for destination (+1 for extra null character)
    char *dest = (char *)malloc(sizeof(char) * (len + 1));

    for (int i = m; i < n && (*(src + i) != '\0'); i++)
    {
        *dest = *(src + i);
        dest++;
    }

    // null-terminate the destination string
    *dest = '\0';

    // return the destination string
    return dest - len;
}

int main(void)
{
    char inputBuffer[MAX_LINE];      /*buffer to hold command entered */
    int background;                  /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1];    /*command line arguments */
    struct sigaction act;            //create signal
    act.sa_handler = controlZSignal; //implement signal handler
    act.sa_flags = SA_RESTART;       // implement signal flag
    struct Node *head = NULL;        //create linked list bookmark header
    struct Process *rHead = NULL;    //create linked list running background process header
    int index = 1;                   //background process index
    //control signal set and action is valid
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGTSTP, &act, NULL) == -1))
    {
        fprintf(stderr, "Failed to set SIGTSTP handler\n");
        return 1;
    }

    while (1)
    {
        background = 0;
        printf("myshell: ");
        fflush(stdout);
        int argCount = setup(inputBuffer, args, &background);
        controlStatus(rHead);//control background process status running or finish
        //control coming argument what will do
        if (args[0] == NULL)
        {
            continue;
        }
        else if (strcmp(args[0], "ps_all") == 0)
        {
            rshow(rHead);//print running process
            int len = dShow(&rHead, rHead);//print closed process
            for (int i = 0; i < len; i++)
            {
                deleteNodeBackgroundProcess(&rHead, 1);//delete closed process in list
            }

            continue;
        }
        else if (strcmp(args[0], "search") == 0 && args[1] != NULL)
        {
            char directory[100];
            strcpy(directory, getcwd(directory, sizeof(directory)));//copy starting path 
            //check is recursively or not
            if (strcmp(args[1], "-r") == 0)
            {
                if (args[2][0] != 34 || args[2][strlen(args[2]) - 1] != 34)//check argument is valid or not
                {
                    printf("Invalid argument: %s\n", args[2]);
                    continue;
                }
                //if it is valid travel recusively all files 
                char *com = substr(args[2], 1, (int)strlen(args[2]) - 1);
                listFilesRecursively(directory, com);
            }
            else
            {
                if (args[1][0] != 34 || args[1][strlen(args[1]) - 1] != 34)//check argument is valid or not
                {
                    printf("Invalid argument: %s\n", args[1]);
                    continue;
                }
                //if it is valid travel all files which in current directory
                char *com = substr(args[1], 1, (int)strlen(args[1]) - 1);
                listFiles(directory, com);
            }
            continue;
        }
        else if (strcmp(args[0], "bookmark") == 0 && args[1] != NULL)
        {
            if (strcmp(args[1], "-l") == 0)//list all frequent command 
            {
                printList(head);
            }
            else if (strcmp(args[1], "-d") == 0)//delete frequent command in spesific index
            {
                deleteNode(&head, atoi(args[2]));
            }
            else if (strcmp(args[1], "-i") == 0)//execute frequent command in spesific index
            {
                char command[MAX_LINE];
                strcpy(command, findSpesificIndex(head, atoi(args[2])));
                char *com = substr(command, 1, (int)strlen(command) - 1);

                system(com);
            }
            else
            {
                //add frequent command into list
                char frequentCmd[MAX_LINE];
                strcpy(frequentCmd, args[1]);
                for (int i = 2; i < argCount; i++)
                {
                    strcat(frequentCmd, " ");
                    strcat(frequentCmd, args[i]);
                }
                if (frequentCmd[0] != 34 || frequentCmd[strlen(frequentCmd) - 1] != 34)
                {
                    printf("Invalid argument: %s\n", frequentCmd);
                    continue;
                }
                append(&head, frequentCmd);
            }
            continue;
        }
        else if (strcmp(args[0], "exit") == 0)
        {
            //control any backgorund process is running if ıt is write message, if it is not close our terminal
            int control = controlExit(rHead);
            if (control == 1)
            {
                printf("Background process still running and do not terminate shell process unless the user terminates all background process\n");
            }
            else
            {
                exit(1);
            }
            continue;
        }

        else
        {
            if (argCount > 1)
            {
                //check redirection args
                if (strcmp(args[argCount - 2], ">") == 0 || strcmp(args[argCount - 2], ">>") == 0 || strcmp(args[argCount - 2], "<") == 0 || strcmp(args[argCount - 2], "2>") == 0)
                {
                    //read file after write another file
                    if ((args[argCount - 4] != NULL && strcmp(args[argCount - 4], "<") == 0))
                    {
                        //create new child process
                        int pid = fork();
                        if (pid == 0)
                        {
                            int in;
                            int out;
                            //open input and output file with 
                            in = open(args[argCount - 3], O_RDWR, CREATE_MODE);
                            out = open(args[argCount - 1], CREATE_FLAGS, CREATE_MODE);
                            //check have any error if have terminal send error message
                            if (in == -1)
                            {
                                fprintf(stderr, "Failed to open my.file");
                                return 1;
                            }
                            if (dup2(in, STDIN_FILENO) == -1)
                            {
                                fprintf(stderr, "Failed to redirect standard output");
                                return 1;
                            }
                            if (close(in) == -1)
                            {
                                fprintf(stderr, "Failed to close the file");
                                return 1;
                            }

                            if (out == -1)
                            {
                                fprintf(stderr, "Failed to open my.file");
                                return 1;
                            }
                            if (dup2(out, STDOUT_FILENO) == -1)
                            {
                                fprintf(stderr, "Failed to redirect standard output");
                                return 1;
                            }
                            if (close(out) == -1)
                            {
                                fprintf(stderr, "Failed to close the file");
                                return 1;
                            }
                            //execute read args and write outpur file
                            char path[PATH_MAX + 1];
                            strcpy(path, "/bin/");
                            strcat(path, args[0]);
                            args[argCount - 4] = NULL;
                            execv(path, args);
                        }
                        else
                        {
                            //wait child 
                            wait(&pid);
                        }
                    }
                    else if (strcmp(args[argCount - 2], ">") == 0)
                    {
                        //create new child process
                        int pid = fork();
                        if (pid == 0)
                        {
                            int fd;
                            //open output file with
                            fd = open(args[argCount - 1], CREATE_FLAGS, CREATE_MODE);
                            if (fd == -1)
                            {
                                fprintf(stderr, "Failed to open my.file");
                                return 1;
                            }
                            if (dup2(fd, STDOUT_FILENO) == -1)
                            {
                                fprintf(stderr, "Failed to redirect standard output");
                                return 1;
                            }
                            if (close(fd) == -1)
                            {
                                fprintf(stderr, "Failed to close the file");
                                return 1;
                            }
                            char path[PATH_MAX + 1];
                            char *exe;
                            exe = args[0];
                            if (!findpathof(path, exe))
                            {
                                fprintf(stderr, "No executable \"%s\" found\n", exe);
                                return 1;
                            }
                            //execute arg and trunct output file
                            args[argCount - 1] = NULL;
                            args[argCount - 2] = NULL;
                            execv(path, args);
                        }
                        else
                        {
                            //wait child 
                            wait(&pid);
                        }
                    }
                    else if (strcmp(args[argCount - 2], ">>") == 0)
                    {
                        //create new child process
                        int pid = fork();
                        if (pid == 0)
                        {
                            int fd;
                            //open output file with
                            fd = open(args[argCount - 1], CREATE_APPEND_FLAGS, CREATE_MODE);
                            if (fd == -1)
                            {
                                fprintf(stderr, "Failed to open my.file");
                                return 1;
                            }
                            if (dup2(fd, STDOUT_FILENO) == -1)
                            {
                                fprintf(stderr, "Failed to redirect standard output");
                                return 1;
                            }
                            if (close(fd) == -1)
                            {
                                fprintf(stderr, "Failed to close the file");
                                return 1;
                            }
                            //execute args and append into output file
                            char path[PATH_MAX + 1];
                            char *exe;
                            exe = args[0];
                            if (!findpathof(path, exe))
                            {
                                fprintf(stderr, "No executable \"%s\" found\n", exe);
                                return 1;
                            }
                            args[argCount - 1] = NULL;
                            args[argCount - 2] = NULL;
                            execv(path, args);
                        }
                        else
                        {
                            //wait child 
                            wait(&pid);
                        }
                    }
                    else if (strcmp(args[argCount - 2], "<") == 0)
                    {
                        //create new child process
                        int pid = fork();
                        if (pid == 0)
                        {
                            int fd;
                            //open input file with
                            if ((fd = open(args[argCount - 1], O_RDONLY)) < 0)
                            {
                                fprintf(stderr, "Couldn't open input file\n");
                                exit(-1);
                            }
                            //read input file and execute in terminal
                            dup2(fd, STDIN_FILENO);
                        }
                        else
                        {
                            //wait child 
                            wait(&pid);
                        }
                    }
                    else if (strcmp(args[argCount - 2], "2>") == 0)
                    {
                        //create new child process
                        int pid = fork();
                        if (pid == 0)
                        {
                            int fd_err;
                            //open output file with
                            if ((fd_err = open(args[argCount - 1], O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)
                            {
                                fprintf(stderr, "Couldn't open output file\n");
                                return 1;
                            }
                            //if args is valid execute but if it is not valid write standart error into output file
                            dup2(fd_err, STDERR_FILENO);
                            char path[PATH_MAX + 1];
                            strcpy(path, "/bin/");
                            strcat(path, args[0]);
                            args[argCount - 2] = NULL;
                            execv(path, args);
                            close(fd_err);
                        }
                        else
                        {
                            //wait child 
                            wait(&pid);
                        }
                    }
                    continue;
                }
            }
        }
        //fork child process
        childPid = fork();
        if (background == 1)
        {
            args[argCount - 1] = NULL;
        }
        //if child can't create terminal send error message
        if (childPid < 0)
        {
            fprintf(stderr, "failed to fork");
        }
        if (childPid == 0)//child process
        {
            //find path of will executed process
            char path[PATH_MAX + 1];
            char *exe;
            exe = args[0];
            if (!findpathof(path, exe))
            {
                fprintf(stderr, "No executable \"%s\" found\n", exe);
                exit(-1);
            }
            //execute process with args
            execv(path, args);
            fprintf(stderr, "Child failed upon running execv function %d", childPid);
        }
        else
        {
            //control is background process or not if it is foreground parent wait child, if it is not execute child without block parent
            if (background == 0)
            {
                controlForeground = 1;
                wait(&childPid);
                controlForeground = 0;
            }
            else
            {
                char path[PATH_MAX + 1];
                char *exe;
                exe = args[0];
                if (!findpathof(path, exe))
                {
                    continue;
                }
                // control background process if no have in list index equal again one 
                int control = controlExit(rHead);
                if (control == 1)
                {
                    index++;
                }
                else
                {
                    index = 1;
                }
                //add background process in to list
                appendRunning(&rHead, args[0], childPid, index, 0);
            }
            background = 0;
        }
    }
    return 0;
}