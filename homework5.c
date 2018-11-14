#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <dirent.h>

#define BACKLOG (10)


void serve_request(int);

char * request_str = "HTTP/1.0 200 OK\r\n"
                     "Content-type: text/html; charset=UTF-8\r\n\r\n";

char * request_text = "HTTP/1.0 200 OK\r\n"
                     "Content-type: text/plain; charset=UTF-8\r\n\r\n";

char * request_jpeg = "HTTP/1.0 200 OK\r\n"
                     "Context-type: image/jpeg; charset=UTF-8\r\n\r\n";
                     
char * request_jpg = "HTTP/1.0 200 OK\r\n"
                     "Context-type: image/jpg; charset=UTF-8\r\n\r\n";

char * request_gif = "HTTP/1.0 200 OK\r\n"
                     "Context-type: image/gif; charset=UTF-8\r\n\r\n";

char * request_png = "HTTP/1.0 200 OK\r\n"
                     "Context-type: image/png; charset=UTF-8\r\n\r\n";

char * request_pdf = "HTTP/1.0 200 OK\r\n"
                     "Context-type: application/pdf; charset=UTF-8\r\n\r\n";

char * request_ico = "HTTP/1.0 200 OK\r\n"
                     "Context-type: image/x-icon; charset=UTF-8\r\n\r\n";

char * request_error404 = "HTTP/1.0 404 BAD\r\n"
		     "Context-type: text/html; charset=UTF-8\r\n\r\n";

char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
                   "<title>Directory listing for %s</title>"
                   "<body>"
                   "<h2>Directory listing for %s</h2><hr><ul>";

char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";

char *error =   "<!DOCTYPE html>"
		"<html lang=\"en\">"
                "<head>"
	        "<meta charset=\"utf-8\">"
	        "<title>404 HTML Template by Colorlib</title>"
                "</head>"
                "<body>"
	        "<div id=\"notfound\">"
		"<div class=\"notfound\">"
		"<div class=\"notfound-404\">"
		"<h1>404 ERROR</h1>"
		"</div>"
		"<h2>Oops! This Page Could Not Be Found</h2>"
		"<p>Sorry but the page you are looking for does not exist, have been removed, name changed or is temporarily unavailable</p>"
		"</div>"
	        "</div>"
                "</html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X"
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request
 *
 * Does not modify the given request string.
 * The returned resource should be free'd by the caller function.
 */

// struct for multi-threading
struct thread_arg {
    int client_fd;
};

// thread function taken and edited from the thread_example.c from lab
void *thread_function(void *argument_value) {
    struct thread_arg *my_argument = (struct thread_arg *) argument_value;
    serve_request(my_argument->client_fd);
    close(my_argument->client_fd);
    return NULL;
}

int fileOrDir(const char *path, int number){
        struct stat stat_path;    

        if(number == 1){
                stat(path, &stat_path);
                return S_ISREG(stat_path.st_mode);
        }
        else if(number == 2){
                stat(path, &stat_path);
                return S_ISDIR(stat_path.st_mode);
        }
        else{
                return -1;
        }
}


// function that creates dirs bases on header, body and footer
void displayFunc(const char *filepath, int client_fd){
        char* headerIndex = (char*) malloc(sizeof(char)*4096);

        // open directory path up 
        DIR* path = opendir(filepath);

        snprintf(headerIndex, 4096, index_hdr, filepath, filepath);
	
        //send request and header
	send(client_fd, request_str, strlen(request_str), 0);
        send(client_fd, headerIndex, strlen(headerIndex), 0);
        memset(headerIndex, 0, 4096);

        // check to see if opening up directory was successful
        if(path != NULL){
                char* bodyIndex = (char*) malloc(sizeof(char)*4096); 
                struct dirent* underlying_file = NULL;

                while((underlying_file = readdir(path)) != NULL){
                        if(strcmp(underlying_file->d_name, ".") != 0 && strcmp(underlying_file->d_name, "..") != 0)
                        {
                                char *temp = (char*) malloc(sizeof(char)*4096); 
                                strncpy(temp, filepath, 4096);
                                strcat(temp, underlying_file->d_name);

                                if(fileOrDir(temp, 2)){
                                        strcat(underlying_file->d_name, "/");
                                }
                                snprintf(bodyIndex, 4096, index_body, underlying_file->d_name, underlying_file->d_name);
                                strcat(bodyIndex, "\n");
                                // send body
	                        send(client_fd, bodyIndex, strlen(bodyIndex), 0);
                                memset(bodyIndex, 0, 4096);
                        }
                }
        // send footer
	send(client_fd, index_ftr, strlen(index_ftr), 0);
        closedir(path);
        close(client_fd);
        }
}

char* parseRequest(char* request) {
        //assume file paths are no more than 256 bytes + 1 for null.
        char *buffer = malloc(sizeof(char)*257);
        memset(buffer, 0, 257);

        if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0;

        sscanf(request, "GET %s HTTP/1.", buffer);
        return buffer;
}

void serve_request(int client_fd){
        int read_fd;
        int bytes_read;
        int file_offset = 0;
        char client_buf[4096];
        char send_buf[4096];
        char filename[4096];
        char concatedFilename[4096]; //variable that holds the new filename
        char * requested_file;
        memset(client_buf,0,4096);
        memset(filename,0,4096);

        while(1) {

                file_offset += recv(client_fd,&client_buf[file_offset],4096,0);
                if(strstr(client_buf,"\r\n\r\n"))
                        break;
        }
        requested_file = parseRequest(client_buf);
        filename[0] = '.';
        strncpy(&filename[1],requested_file, 4095);

        //if filename is dir
        if(fileOrDir(filename, 2)){
                strcpy(concatedFilename, filename);
                strcat(concatedFilename, "/index.html");

                if(fileOrDir(concatedFilename, 1)){
                        read_fd = open(concatedFilename,0,0);

                        while(1){
                        bytes_read = read(read_fd,send_buf,4096);
                                if(bytes_read == 0)
                                        break;
                        send(client_fd,request_str,strlen(request_str),0);
                        send(client_fd,send_buf,bytes_read,0);
                        }
                }
                else{
                        displayFunc(filename,client_fd);
                }
        close(client_fd);
        }
        //check if filename is file
        else if(fileOrDir(filename, 1)){
        
                char * file;
                if(strstr(filename, ".html") != NULL)
                        file = request_str;
                else if(strstr(filename, ".txt") != NULL)
                        file = request_text;
                else if(strstr(filename, ".jpg") != NULL)
                        file = request_jpg;
                else if(strstr(filename, ".jpeg") != NULL)
                        file = request_jpeg;
                else if(strstr(filename, ".gif") != NULL)
                        file = request_gif;
                else if(strstr(filename, ".png") != NULL)
                        file = request_png;
                else if(strstr(filename, ".pdf") != NULL)
                        file = request_pdf;
                else if(strstr(filename, ".ico") != NULL)
                        file = request_ico;
                else    
                        file =  NULL;
                
                send(client_fd,file,strlen(file),0);
                read_fd = open(filename,0,0);
                while(1) {
                        bytes_read = read(read_fd,send_buf,4096);
                        if(bytes_read == 0)
                                break;
                        send(client_fd,send_buf,bytes_read,0);
                }        
        close(read_fd);
        close(client_fd);
        }else{
                send(client_fd,request_error404,strlen(request_error404),0);
		send(client_fd,error,strlen(error),0);
		close(client_fd);  
        }
}

/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */

int main(int argc, char** argv) {
        /* For checking return values. */
        int retval;

        /* Read the port number from the first command line argument. */
        int port = atoi(argv[1]);
        chdir(argv[2]);
        
         /* Create a socket to which clients will connect. */
        int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
        if(server_sock < 0) {
                perror("Creating socket failed");
                exit(1);
        }

        /* A server socket is bound to a port, which it will listen on for incoming
         * connections.  By default, when a bound socket is closed, the OS waits a
         * couple of minutes before allowing the port to be re-used.  This is
         * inconvenient when you're developing an application, since it means that
         * you have to wait a minute or two after you run to try things again, so
         * we can disable the wait time by setting a socket option called
         * SO_REUSEADDR, which tells the OS that we want to be able to immediately
         * re-bind to that same port. See the socket(7) man page ("man 7 socket")
         * and setsockopt(2) pages for more details about socket options. */
        int reuse_true = 1;
        retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                            sizeof(reuse_true));
        if (retval < 0) {
                perror("Setting socket option failed");
                exit(1);
        }

        /* Create an address structure.  This is very similar to what we saw on the
         * client side, only this time, we're not telling the OS where to connect,
         * we're telling it to bind to a particular address and port to receive
         * incoming connections.  Like the client side, we must use htons() to put
         * the port number in network byte order.  When specifying the IP address,
         * we use a special constant, INADDR_ANY, which tells the OS to bind to all
         * of the system's addresses.  If your machine has multiple network
         * interfaces, and you only wanted to accept connections from one of them,
         * you could supply the address of the interface you wanted to use here. */


        struct sockaddr_in6 addr; // internet socket address data structure
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(port); // byte order is significant
        addr.sin6_addr = in6addr_any; // listen to all interfaces


        /* As its name implies, this system call asks the OS to bind the socket to
         * address and port specified above. */
        retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
        if(retval < 0) {
                perror("Error binding to port");
                exit(1);
        }

        /* Now that we've bound to an address and port, we tell the OS that we're
         * ready to start listening for client connections.  This effectively
         * activates the server socket.  BACKLOG (#defined above) tells the OS how
         * much space to reserve for incoming connections that have not yet been
         * accepted. */
        retval = listen(server_sock, BACKLOG);
        if(retval < 0) {
                perror("Error listening for connections");
                exit(1);
        }

        while(1) {
                /* Declare a socket for the client connection. */
                int sock;
                char buffer[256];

                /* Another address structure.  This time, the system will automatically
                 * fill it in, when we accept a connection, to tell us where the
                 * connection came from. */
                struct sockaddr_in remote_addr;
                unsigned int socklen = sizeof(remote_addr);

                /* Accept the first waiting connection from the server socket and
                 * populate the address information.  The result (sock) is a socket
                 * descriptor for the conversation with the newly connected client.  If
                 * there are no pending connections in the back log, this function will
                 * block indefinitely while waiting for a client connection to be made.
                 * */
                sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
                if(sock < 0) {
                        perror("Error accepting connection");
                        exit(1);
                }
                pthread_t threads;
                struct thread_arg *arguments = malloc(sizeof(struct thread_arg));
                arguments->client_fd = sock;
                int retval = pthread_create(&threads, NULL, &thread_function, (void *)arguments);
                if (retval) {
                        printf("pthread_create() failed\n");
                        exit(1);
                }
        }
        close(server_sock);
}
