#include<stdio.h> 
#include<stdlib.h>
#include <unistd.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>


// Calculate checksum
unsigned short make_checksum(unsigned short *temp, size_t cnt){
    unsigned short checksum;
	size_t idx;
    idx = 0;
    unsigned int template = 0;
    for(idx = 0; idx < (cnt / 2 + (cnt % 2)); idx ++){
        template += temp[idx];
        template = ((template >> 16) + template) & 0xFFFF;
    }
    checksum = template;
    return checksum;
}

unsigned char convert_valid_shift(int shift){
	shift = shift % 26;
	if (shift < 0){
		shift += 26;
	}
	return (unsigned char)shift;
}


struct packet
{
	unsigned char op;
    unsigned char shift;
	unsigned short checksum;
    unsigned int length;
	char data[0];
};

//need to rotate for client....
int main(int argc , char *argv[])
{
    int saved_key;
    // parse the command-line parameters.
    short op;
    int i = 0, opt;
    int shift,j,n,port;
    char letter;
    char *ipaddr;
    int sock;
    struct sockaddr_in server;
    struct packet server_reply;
    char buffer[1000000];
    int read_byte;
    unsigned int len;
    char *temp;
    unsigned int helper_checksum;
    short client_checksum;
	char ipaddr_provided, port_provided, shift_provided, op_provided;
	ipaddr_provided = 0;
	port_provided = 0;
	shift_provided = 0;
	op_provided = 0;
	while ((opt = getopt(argc, argv, "h:p:s:o:")) != -1)
        switch (opt) {
            case 'h' :
				ipaddr_provided = 1;
                ipaddr = optarg;
                break;
            case 'p' :
				port_provided = 1;
                port = atoi(optarg);
                break;
            case 's':
				shift_provided = 1;
                shift = atoi(optarg);
                break;
            case 'o':
				op_provided = 1;
                op = (char)atoi(optarg);
				break;
			default:
				exit(-1);
        }
	if (!(ipaddr_provided && port_provided && shift_provided && op_provided)) {
		exit(-1);
	}
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        return 0;
        //printf("Could not create socket");
    }
    //puts("Socket created");

    server.sin_addr.s_addr = inet_addr(ipaddr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //perror("connect failed. Error");
        return 1;
    }

    //puts("Connected\n");
    memset(buffer, 0, 1000000);
    while ((read_byte = read(0, buffer, 1000000)) > 0){
        //buffer : input data.
       // Wrap all data nicely!!!
        struct packet* a = malloc(sizeof(struct packet) + read_byte);
        memset(a, 0 , sizeof(struct packet) + read_byte);
		memcpy(a->data, buffer, read_byte);
        if (read_byte < 1000000)
            a->data[read_byte] = 0;
		a-> op = op;
        a-> shift = convert_valid_shift(shift);
		a-> length = be32toh(read_byte + sizeof(struct packet));
        a-> checksum = 0;
        a-> checksum = ~(make_checksum((short*)a, sizeof(struct packet) + read_byte));

        //keep communicating with server
        memset( &server_reply, 0, sizeof(struct packet) );
        //Send some data
        if( send(sock , a , sizeof(struct packet) + read_byte , 0) < 0)
        {
           // puts("Send failed");
            free(a);
            return 1;
        }
        //Receive a reply from the server
        if( recv(sock , &server_reply , sizeof(struct packet) , 0) < 0)
        {
            //puts("recv failed");
            break;
        }
        if (op != server_reply.op){
            //puts("different op.");
            exit(-1);
        }
        char *server_reply_data;
        server_reply_data = malloc(read_byte + 1);
        size_t inside_buffer = 0;
        size_t byte_read;
        while( inside_buffer != read_byte){
            byte_read = recv(sock, &server_reply_data[inside_buffer], read_byte - inside_buffer, 0);
            inside_buffer += byte_read;
        }
        server_reply_data[read_byte] = 0;
        //calculate checksum.
        helper_checksum = make_checksum((short *)&server_reply, sizeof(struct packet));
        helper_checksum += make_checksum((short *)server_reply_data, read_byte );
        client_checksum = 0xFFFF & ((helper_checksum >> 16) + helper_checksum);
        if ((~client_checksum) == 0){
            //puts("checksum was correct");
        }
        else{
            //puts("checksum was WRONGGGGG");
            exit(-1);
        }

        write(1, server_reply_data, read_byte);
        free(server_reply_data);
    }
    close(sock);
    return 0;
}

