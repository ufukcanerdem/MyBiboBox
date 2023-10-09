/*
Ufukcan Erdem
1901042686
CSE 344 SPR 23 MIDTERM
*/

#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<signal.h>

#define FIFO_NAME_REQ "/tmp/fifo_req"
#define FIFO_NAME_RES "/tmp/fifo_res"
#define FIFO_NAME_SHM_REQ "/tmp/fifo_shm_req"
#define FIFO_NAME_SHM_RES "/tmp/fifo_shm_res"
#define SHM_NAME "/client_shm"
#define SHM_NAME2 "/server_shm"
#define MAX_SIZE 256

int fd_req, fd_res, fd_shm_req, fd_shm_res, fd_req_child;
size_t SIZE = 10000000;

//Close fifos before exit
void cleanup() {
    close(fd_req);
    close(fd_res);
    close(fd_shm_req);
    close(fd_shm_res);
    close(fd_req_child);
}

void doSigINT(int signum) {
    printf("Ctrl+C command received. Program terminated.\n");
    write(fd_req_child, "quit", 4);
    char temp_goodbye[32];
    read(fd_res, temp_goodbye, 32);
    printf("Server response -> %s\n",temp_goodbye);
    exit(signum);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Incorrect usage of terminal arguments! Exiting program!");
        exit(-1);
    }

    signal(SIGINT, doSigINT);

    char *connection_type = argv[1];
    int server_pid = atoi(argv[2]);
    char buf1[MAX_SIZE];
    char buf2[MAX_SIZE];

    fd_req = open(FIFO_NAME_REQ, O_RDWR);
    fd_res = open(FIFO_NAME_RES, O_RDWR);
    fd_shm_req = open(FIFO_NAME_SHM_REQ, O_RDWR);
    fd_shm_res = open(FIFO_NAME_SHM_RES, O_RDWR);

    sprintf(buf1, "%s%d", connection_type[0] == 'C' ? "c" : "t", getpid());
    write(fd_req, buf1, strlen(buf1) + 1);


    //Read and handle server response
    memset(buf1, 0, MAX_SIZE);
    read(fd_res, buf1, MAX_SIZE);

    //Open shared memory
    fd_req_child = open(buf1, O_RDWR);
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, SIZE); 
    void *ptr = mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    unsigned char* binarybuffer = (unsigned char*)malloc(SIZE);
    
    while(1) {
        char *req_temp = "N";
        memset(buf2, 0, MAX_SIZE);
        printf("Enter command: ");
        fgets(buf2, MAX_SIZE, stdin);
        buf2[strlen(buf2) - 1] = '\0';  // Remove newline at the end

        int isHelp = 1;
        isHelp = strncmp(buf2,"help",4);

        if ( isHelp == 0 ) {
            if(strlen(buf2) < 5) {
                printf("Available comments are :\nhelp, list, readF, writeT, upload, download, quit, killServer\n");
            }
            else{
                char temp_help[16];
                char temp_which[128];
                sscanf(buf2, "%s %s",temp_help,temp_which);
                printf("helpwhich->%s\n",temp_which);

                if(strcmp(temp_which, "readF") == 0) { printf("readF <file> <line #>\ndisplay the #th line of the <file>, returns with an\nerror if <file> does not exists\n");}
                else if(strcmp(temp_which, "writeT") == 0) {printf("writeT <file> <line #> <string>\nrequest to write the content of “string” to the #th line the <file>, if the line # is not given writes to the end of file.\nIf the file does not exists in Servers directory creates and edits the file at the same time\n");}
                else if(strcmp(temp_which, "upload") == 0){printf("upload <file>\nuploads the file from the current working directory of client to the Servers directory\n");}
                else if(strcmp(temp_which, "download") == 0){printf("download <file>\nrequest to receive <file> from Servers directory to client side\n");}
                else if(strcmp(temp_which, "quit") == 0){printf("Send write request to Server side log file and quits\n");}
                else if(strcmp(temp_which, "killServer") == 0){printf("Sends a kill request to the Server\n");}
                else if(strcmp(temp_which, "list") == 0){printf("Prints the files in server directory.\n");}
                else{printf("Unrecognized command for help!\n");}
            }
            continue;
        }

        write(fd_req_child, buf2, strlen(buf2) + 1);

        int isUpload = 1;
        isUpload = strncmp(buf2,"upload",6);

        int isDownload = 1;
        isDownload = strncmp(buf2,"download",8);

        if(isUpload == 0) {
            memset(ptr,0,SIZE);
            memset(binarybuffer,0,SIZE);
            char temp_command[32];
            char filename[128];
            sscanf(buf2, "%s %s",temp_command,filename);
            
            FILE *fp = fopen(filename, "rb");
            
            size_t nread = fread(binarybuffer, 1, SIZE, fp);
            
            fclose(fp);

            
            if (nread > 0) {
                memcpy(ptr, binarybuffer, nread);
            } else {
                perror("File read error");
            }
        }

        if(isDownload == 0) {
            char temp_command[32];
            char filename[128];
            sscanf(buf2, "%s %s",temp_command,filename);

            int shm_fd_download = shm_open(SHM_NAME2, O_RDONLY, 0666);
            void *ptr = mmap(0, SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);

            unsigned char* temp_buffer_download = (unsigned char*)malloc(SIZE);
            memset(temp_buffer_download,0,SIZE);

            memcpy(temp_buffer_download, ptr, SIZE);

            FILE* savedownloadfile = fopen(filename, "w");

            if (savedownloadfile != NULL) {
                fwrite(temp_buffer_download, sizeof(unsigned char), strlen(temp_buffer_download), savedownloadfile);
            }

            fclose(savedownloadfile);

        }
        memset(buf1,0,MAX_SIZE);
        read(fd_res, buf1, MAX_SIZE);
        printf("Server response -> %s\n",buf1);

        if (strcmp(buf2, "quit") == 0) {
            break;
        }
        if (strcmp(buf1, "Server died.") == 0) {
            printf("Server is dead. Client is terminating too!\n");
            break;
        }
    }
    
    munmap(ptr, SIZE);
    close(shm_fd);
    shm_unlink(SHM_NAME);

    cleanup();

    return 0;
}