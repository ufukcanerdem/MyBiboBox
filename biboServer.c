/*
Ufukcan Erdem
1901042686
CSE 344 SPR 23 MIDTERM
*/

#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<signal.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
#include<semaphore.h>
#include<dirent.h>

#define FIFO_NAME_REQ "/tmp/fifo_req"
#define FIFO_NAME_RES "/tmp/fifo_res"
#define FIFO_NAME_SHM_REQ "/tmp/fifo_shm_req"
#define FIFO_NAME_SHM_RES "/tmp/fifo_shm_res"
#define SHM_NAME "/client_shm"
#define SHM_NAME2 "/server_shm"
#define MAX_SIZE 256
#define MAX_CLIENTS 100

int max_clients;
char *dir_name;
int fd_req, fd_res, fd_shm_res, fd_shm_req;
int client_count = 0;
int client_pids[MAX_CLIENTS];
size_t SIZE = 10000000;

sem_t *semaphore;
sem_t *sem;

void cleanup() {
    close(fd_req);
    close(fd_res);
    close(fd_shm_res);
    close(fd_shm_req);
    unlink(FIFO_NAME_REQ);
    unlink(FIFO_NAME_RES);
    unlink(FIFO_NAME_SHM_REQ);
    unlink(FIFO_NAME_SHM_RES);
    sem_unlink("/semaphore");
    sem_close(sem);
    sem_unlink("/sem");
}

void doSigINT(int sig) {
    printf("Kill signal received... terminating...\n");
    cleanup();
    exit(0);
}

void doSigCHLD(int sig) {
    // Remove terminated child PID from the array
    for (int i = 0; i < max_clients; ++i) {
        if (client_pids[i] != 0 && waitpid(client_pids[i], NULL, WNOHANG) != 0) {
            //printf("insigchld-Client PID %d disconnected\n", client_pids[i]);
            client_pids[i] = 0;
            //--client_count;
            sem_post(semaphore);
        }
    }
}

//Manages client commands
void manageClient(int client_pid, int fd_req_child) {
    int ret = chdir(dir_name);
    char buf[MAX_SIZE];
    char command[MAX_SIZE];
    char *sem_name = "/semaphore";

    char* response = (char*)malloc(512);
    char* log_message = (char*)malloc(256);
    char* str_client_pid = (char*)malloc(16);

    sem = sem_open(sem_name, O_CREAT, 0644, 1);

    while (1) {
        memset(buf, 0, MAX_SIZE);
        read(fd_req_child, buf, MAX_SIZE);
        sscanf(buf, "%s", command);  // Extract the command from the buffer
        
        //Writes log file
        FILE *logfile = fopen("serverLog.log", "a");
        memset(log_message,0,256);
        strcat(log_message, "Client PID: ");
        sprintf(str_client_pid, "%d", client_pid);
        strcat(log_message, str_client_pid);
        strcat(log_message, " - Command: ");
        strcat(log_message, command);
        strcat(log_message, "\n");
        fprintf(logfile, "%s", log_message);
        fclose(logfile);

        //Checks commands
        int readF_result = 1;
        readF_result = strncmp(command,"readF",5);

        int writeT_result = 1;
        writeT_result = strncmp(command,"writeT",6);

        int upload_result = 1;
        upload_result = strncmp(command,"upload",6);

        int download_result = 1;
        download_result = strncmp(command,"download",8);

        //Open shared memory
        int shm_fd_download = shm_open(SHM_NAME2, O_CREAT | O_RDWR, 0666);
        ftruncate(shm_fd_download, SIZE); 
        void *ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_download, 0);
        unsigned char* binarybuffer = (unsigned char*)malloc(SIZE);

        if(readF_result == 0){

            sem_wait(sem);  // Lock the semaphore

            char filename[256];
            int which_line;
            sscanf(buf, "%s %s %d",command,filename,&which_line);

            FILE *file = fopen(filename, "r");
            if (file == NULL) {
                printf("Error opening the file.\n");
            }
            char temp_buffer_readf[256];
            int current_line = 1;

            while (fgets(temp_buffer_readf, sizeof(temp_buffer_readf), file) != NULL) {
                if (current_line == which_line) {
                    break;
                }
                current_line++;
            }

            fclose(file);

            memset(response,0,512);
            sprintf(response, "Line readed from file -> %s\n", temp_buffer_readf);
            write(fd_res, response, strlen(response));

            sem_post(sem);  // Unlock the semaphore
        }

        else if(writeT_result == 0) {
            sem_wait(sem);  // Lock the semaphore

            char filename[256];
            char strtowrite[256];
            int which_line;
            sscanf(buf, "%s %s %d %s",command,filename,&which_line,strtowrite);

            FILE* fp = fopen(filename, "a+");  // Open file in append mode, create if not exists
            if (fp == NULL) {
                printf("Error opening file.\n");
                return;
            }

            //Print to end if line is not given
            if (which_line <= 0) {
                fseek(fp, 0, SEEK_END);
            } else {
                char temp_buffer_writeT[1024];
                int currentLine = 1;
                while (fgets(temp_buffer_writeT, sizeof(temp_buffer_writeT), fp) != NULL) {
                    if (currentLine == which_line) {
                        fseek(fp, -strlen(temp_buffer_writeT), SEEK_CUR);  // Move file pointer back to the current line
                        break;
                    }
                    currentLine++;
                }
            }

            fprintf(fp, "%s\n", strtowrite);  // Write the string to the file

            fclose(fp);

            sprintf(response, "Line written to file -> %s\n", strtowrite);
            write(fd_res, response, strlen(response));
            

            sem_post(sem);  // Unlock the semaphore


        }

        else if(upload_result == 0) {
            sem_wait(sem);  // Lock the semaphore
            char temp_command[32];
            char filename[128];
            sscanf(buf, "%s %s",temp_command,filename);

            int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
            void *ptr = mmap(0, SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);

            unsigned char* temp_buffer_upload = (unsigned char*)malloc(SIZE);
            memset(temp_buffer_upload,0,SIZE);

            memcpy(temp_buffer_upload, ptr, SIZE);


            FILE* saveuploadfile = fopen(filename, "w");

            if (saveuploadfile != NULL) {
                fwrite(temp_buffer_upload, sizeof(unsigned char), strlen(temp_buffer_upload), saveuploadfile);
            }

            fclose(saveuploadfile);


            sprintf(response, "File %s uploaded.\n", filename);
            write(fd_res, response, strlen(response));

            sem_post(sem);  // Unlock the semaphore

        }

        else if( download_result == 0 ) {
            sem_wait(sem);  // Lock the semaphore
            memset(ptr,0,SIZE);
            memset(binarybuffer,0,SIZE);
            char temp_command[32];
            char filename[128];
            sscanf(buf, "%s %s",temp_command,filename);
            

            FILE *fp = fopen(filename, "rb");
            
            size_t nread = fread(binarybuffer, 1, SIZE, fp);
            
            fclose(fp);

            
            if (nread > 0) {
                memcpy(ptr, binarybuffer, nread);
            } else {
                perror("File read error");
            }
            
            sprintf(response, "File %s downloaded.\n", filename);
            write(fd_res, response, strlen(response));

            sem_post(sem);  // Unlock the semaphore
        }

        
        if (strcmp(command, "quit") == 0) {
            char *goodbyeMessage = "goodbye";
            write(fd_res, goodbyeMessage, strlen(goodbyeMessage));
            break;
        } 
        else if (strcmp(command, "list") == 0) {
            sem_wait(sem);  // Lock the semaphore
            
            DIR *directory;
            struct dirent *entry;
            char filenames[32][256];
            int file_count = 0;
            directory = opendir(".");

            while ((entry = readdir(directory)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                strncpy(filenames[file_count], entry->d_name, 256);
                file_count++;

                if (file_count > 31) {
                    break;
                }
            }

            closedir(directory);

            strcat(response, "File list\n");
            for(int i=0;i<file_count;i++) {
                strcat(filenames[i], "\n");
                strcat(response, filenames[i]);
            }
            write(fd_res, response, strlen(response));
            sem_post(sem);  // Unlock the semaphore
        }

        else if (strcmp(command, "killServer") == 0) {
            sprintf(response, "Server died.");
            write(fd_res, response, strlen(response));
            pid_t parent_pid = getppid();  // Get the parent process ID
            kill(parent_pid, SIGTERM);
            exit(0);
        }
        else{
            sem_wait(sem);  // Lock the semaphore
            strcat(response,"Undefined command!\n");
            write(fd_res, response, strlen(response));
            sem_post(sem);  // Unlock the semaphore
        }
    }

    printf("Client PID %d disconnected\n", client_pid);
    sem_close(sem);  // Close the semaphore
    kill(client_pid, SIGKILL);
    exit(0);
}




int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Incorrect usage of terminal arguments! Exiting program!");
        exit(-1);
    }

    dir_name = argv[1];
    max_clients = atoi(argv[2]);

    sem = sem_open("/sem", O_CREAT, 0644, 1);  // Initialize semaphore

    signal(SIGINT, doSigINT);
    signal(SIGCHLD, doSigCHLD);

    DIR* dir = opendir(dir_name);
    if (dir) {
        closedir(dir);
    } else {
        mkdir(dir_name, 0755);
    }

    FILE *logfile;
    char filename[] = "serverLog.log";
    logfile = fopen(filename, "w");
    if (logfile == NULL) {
        printf("Error opening the file.\n");
        return -1;
    }
    fclose(logfile);

    mkfifo(FIFO_NAME_REQ, 0644);
    mkfifo(FIFO_NAME_RES, 0644);

    
    fd_req = open(FIFO_NAME_REQ, O_RDWR);
    fd_res = open(FIFO_NAME_RES, O_RDWR);
    fd_shm_res = open(FIFO_NAME_SHM_RES, O_RDWR);
    fd_shm_req = open(FIFO_NAME_SHM_REQ, O_RDWR);

    printf("Server Started PID %d…\n", getpid());
    printf("waiting for clients...\n");

    semaphore = sem_open("/semaphore", O_CREAT, 0644, max_clients);
    if (semaphore == SEM_FAILED) {
        perror("Could not create semaphore");
        return -1;
    }

    char buf[MAX_SIZE];

    while (1) {
        read(fd_req, buf, MAX_SIZE);
        buf[0] = 'c';
        char command = buf[0];
        int client_pid = atoi(buf + 2); 
        int client_type = atoi(buf + 1);

        switch (command) {
            case 'c': { // Connect
                if (client_count < max_clients) {
                    int child_pid = fork();
                    
                    if (child_pid == 0) {
                        char fifo_name[256];
                        sprintf(fifo_name, "/tmp/fifo_req_%d", client_pid);
                        
                        //Create the new FIFO for child process
                        mkfifo(fifo_name, 0644);
                        write(fd_res, fifo_name, strlen(fifo_name) + 1);
                        int fd_req_child = open(fifo_name, O_RDWR);
                        manageClient(client_pid, fd_req_child);
                    } else {
                        client_pids[client_count++] = child_pid;
                        printf("Client count->%d\n",client_count);
                        printf("Client PID %d connected as client%02d\n", client_pid, client_count);
                    }
                } else {
                    printf("Connection request PID %d… Queue FULL\n", client_pid);
                }
                break;
            }
        }
    }
    cleanup();
    return 0;
}

