#include<stdio.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h> 
#include<unistd.h>
#include<stdlib.h>

void caesar_transform(short op, char shift, char *data, size_t data_length){
    //shift is always valid.(0~25)
    int i;
    char letter;
    if (op) {
        shift = (26 - shift) % 26;
    }
    for (i=0; i < data_length; i++){
        letter = tolower(data[i]);
    if ((97 <= letter) && (letter <= 122)){ //convert only for alphabet letters
        if ((letter + shift) > 122){
        data[i] = (letter + shift) % 123 + 97;
        }
        else {
        data[i] = letter + shift;
        }
    }
    }
}

struct packet
{
    unsigned char op; //1byte
    unsigned char shift; //1byte
    unsigned short checksum; //2byte
    unsigned int length; //4byte
    char data[0];
};

int validate_request(unsigned char op, unsigned char shift, unsigned int length){
    //op validity check
    if (!((op == 0) || (op == 1))){
        return 0;
    }
    //transform to valid shift (should be 0~25)
    if (shift > 25){
        shift = shift % 26;
    }
    // reject if length > 10MB
    if (length > 10000000) {
        return 0;
    }
    return 1;
}
unsigned short make_checksum( unsigned short *temp, size_t cnt){
    unsigned short checksum;
    size_t idx;
    idx = 0;
    unsigned int template;
    template = 0;
    for(idx = 0; idx < (cnt / 2 + (cnt % 2)); idx ++){
        template += temp[idx];
        template = ((template >> 16) + template) & 0xFFFF;
    }
    checksum = (unsigned short)template;
    return checksum;
}

int main(int argc , char *argv[])
{
    int port;
    unsigned int length;
    unsigned char shift;
    unsigned char op;
    char data[0];
    unsigned short checksum, server_checksum;
    int socket_desc , client_sock , c , read_size;
    struct sockaddr_in server , client;
    struct packet header;
    char *data_message;
    fd_set readfds, allfds;
    int maxfd = 0, fd_num = 0, selecti = 0;

    //get port
    if (!((argv[1][0] == '-') && (argv[1][1] == 'p'))){
        exit(-1);
    }
    else{
        port = atoi(argv[2]);
    }

    pid_t pid;
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        exit(-1);
    }
    //("Socket created");

    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( port );

    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        //perror("bind failed. Error");
        return 1;
    }
    //puts("bind done");

    //Listen
    listen(socket_desc , 3);

    FD_ZERO(&readfds);
    FD_SET(socket_desc, &readfds);
    maxfd = socket_desc;

    //Accept and incoming connection
    c = sizeof(struct sockaddr_in);

    for(;;)
    {
        allfds = readfds;
        fd_num = select(maxfd + 1, &allfds, (fd_set *)0, (fd_set *)0, NULL);
        if (FD_ISSET(socket_desc, &allfds))
        {
            //accept connection from an incoming client
            client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
            if (client_sock < 0)
            {
                //perror("accept failed");
                return 1;
            }
            //puts("Connection accepted");
            FD_SET(client_sock, &readfds);
            if (client_sock > maxfd)
                maxfd = client_sock;
            continue;
        }
        for (selecti = 0; selecti <= maxfd; selecti++)
        {
            if (FD_ISSET(selecti, &allfds))
            {
                //Receive a message from client
                if( (read_size = read(selecti , &header , sizeof(struct packet))) == sizeof(struct packet))
                {
                    length = htobe32(header.length);
                    op = header.op;
                    checksum = ntohs(header.checksum);
                    shift = header.shift;
                }
                if(read_size == 0)
                {
                    //puts("Client disconnected");
                    fflush(stdout);
                    close(selecti);
                    FD_CLR(selecti, &readfds);
                    continue;
                }
                else if((read_size == -1) || (0 == validate_request(op, shift, length)))
                {
                    close(selecti);
                    FD_CLR(selecti, &readfds);
                    continue;
                }
                data_message = malloc(length - sizeof(struct packet) + 1);
                memset(data_message, 0, length - sizeof(struct packet));
                size_t inside_buffer;
                inside_buffer = 0;
                while (inside_buffer != (length - sizeof(struct packet))){
                    read_size = read(selecti, &data_message[inside_buffer], length - sizeof(struct packet) - inside_buffer);
                    inside_buffer += read_size;
                }
                read_size = length - sizeof(struct packet);
                data_message[read_size] = '\0';
                //Verify if length protocol is valid.
                if (!(length - 8 == read_size)){
                    exit(-1);
                }
                //Verify if checksum is correct.
                unsigned int helper_checksum;
                helper_checksum = 0;

                helper_checksum = make_checksum((unsigned short *)&header, sizeof(struct packet));
                helper_checksum += make_checksum((unsigned short *)data_message, read_size);
                server_checksum = 0xFFFF & ((helper_checksum >> 16) + helper_checksum);
                if ((unsigned short)(~server_checksum) == 0){
                    //puts("checksum calculation was correct!");
                }
                else{
                    free(data_message);
                    return 0;
                }
                caesar_transform(op, shift, data_message, read_size);
                //write the server's calculated checksum to the header.
                helper_checksum = 0;
                header.checksum = 0;
                helper_checksum = make_checksum((unsigned short *)&header, sizeof(struct packet));
                helper_checksum += make_checksum((unsigned short *)data_message, read_size);
                checksum = 0;
                checksum = 0xFFFF & ((helper_checksum >> 16) + helper_checksum);
                header.checksum = ~checksum;

                //Send the message back to client
                write(selecti , &header, (sizeof(struct packet)));
                write(selecti , data_message , length - sizeof(struct packet));
                memset( &header, 0, sizeof(header));
                free(data_message);
                if (--fd_num < 0){
                    break;
                }
            }
        }
    }
}
