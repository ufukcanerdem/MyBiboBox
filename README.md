# MyBiboBox
A file server that enables multiple clients to connect, access  and modify the contents of  files in a specific directory.

The project includes two files called biboServer.c and biboClient.c to create a file system as server-client.

Server-Side
After the server side works, it listens to the client over FIFO. According to the connect 
requests from the client, the child processes are forked for the clients. It creates a special 
FIFO path for each client. Then it receives requests and sends responses over this FIFO
created.
When each client (childProcess) receives a command on the server, it locks the 
Semaphore before starting a task according to the command it receives and unlocks after the 
process is finished. Thus, synchronization between clients is provided.
The last important point we can mention is the use of shared memory in upload and 
download tasks. The file to be uploaded or downloaded is kept by the shared memory and 
optionally moved to the target folder.

Client-Side
As soon as the client is started, it sends a request to connect to the server via FIFO, 
according to the first command it receives. There is not much we can mention about other 
than using shared memory while uploading and downloading tasks and maybe when ctrl-c 
happens, it still manages to disconnect from the server by sending a command to the server.
In general, it receives the command from the user and transmits it to the server and prints 
the response from the server to the screen.

