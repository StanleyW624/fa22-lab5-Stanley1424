#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  int i = 0;
  int value = 0;
  while (i < len){
    value = read(fd,&buf[i],len-i);
    if (value <= 0) {
      return false;
    }
    i += value;
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  int i = 0;
  int value = 0;
  while (i < len){
    value = write(fd,&buf[i],len-i);
    if (value <= 0) {
      return false;
    }
    i += value;
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the info code (lowest bit represents the return value of the server side calling the corresponding jbod_operation function. 2nd lowest bit represent whether data block exists after HEADER_LEN.)
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/
static bool recv_packet(int sd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN];
  int offset = 0;
  if (nread(sd,HEADER_LEN,header) == false) {
    return false;
  }

  memcpy(op, header+offset, sizeof(*op));
  offset += sizeof(*op);
  *op = ntohl(*op);

  memcpy(ret, header+offset, sizeof(*ret));
  offset += sizeof(*ret);

  if (*ret & 2) {
    if (nread(sd,JBOD_BLOCK_SIZE,block) == false) {
      return false;
    } 
  }
  
  return true;
} 



/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t buffer[HEADER_LEN + JBOD_BLOCK_SIZE];
  int offset = 0;
  uint32_t newopcode;
  uint8_t infocode;

  newopcode = htonl(op);

  memcpy(buffer+offset, &newopcode, sizeof(newopcode));
  offset += sizeof(newopcode);

  if ((newopcode >> 12&0x3f) == JBOD_WRITE_BLOCK) {
    buffer[offset] = 2;
    //infocode = 2;
    //memcpy(buffer+offset, &infocode, sizeof(infocode));
    //offset += sizeof(infocode);

    memcpy(buffer+HEADER_LEN, block, JBOD_BLOCK_SIZE);
    offset += JBOD_BLOCK_SIZE;

     if (nwrite(sd,HEADER_LEN + JBOD_BLOCK_SIZE, buffer) == false) {
       return false;
     }
     
  } else {
    buffer[offset] = 0;
    //infocode = 0;
    //memcpy(buffer+offset, &infocode, sizeof(infocode));
    //offset += sizeof(infocode);

    if (nwrite(sd,HEADER_LEN, buffer) == false) {
       return false;
     }
  }
  
  return true;
}



/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in serveraddr;

  cli_sd = socket(AF_INET, SOCK_STREAM, 0);
  if (cli_sd == -1) {
    return false;
  }

  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(port);
  if(inet_aton(ip, &serveraddr.sin_addr) == 0) {
    return false;
  }

  if (connect(cli_sd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) != 0) {
    return false;
  }

  return true;
}



/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd);
  cli_sd = -1;
}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint8_t infocode;
  uint32_t holderop = op;
  
  if ( cli_sd == -1){
    return -1;
  }

  if (send_packet(cli_sd,op,block) == false) {
    return -1;
  }


  if (recv_packet(cli_sd,&op,&infocode,block) == false) {
      return -1;
  }
  
  if (infocode % 2 != 0) {
    return -1;
  }
  
  if (op != holderop) {
    return -1;
  }


  return 0;
}
