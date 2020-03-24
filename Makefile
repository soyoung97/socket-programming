all :
	gcc server.c -o server
	gcc client.c -o client

server: 
	rm server
	gcc server.c -o server

client:
	rm client
	gcc client.c -o client

clean:
	rm -f server client
