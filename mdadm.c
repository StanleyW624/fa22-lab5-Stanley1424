#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

int is_mounted = 0;                                                     //mount status 
int is_written = 0;                                                     //write premission

int diskid;
int blockid;

uint32_t newop (uint32_t block, uint32_t disk, uint32_t cmd) {

  uint32_t packvalue = 0x0, tempblock, tempdisk, tempcmd;               //setting up temp values for bytes

  tempblock = block&0xff;                                               //no bytes need to be shifted for the first temp
  tempdisk = (disk&0xff) << 8;                                          //8 position shifted for the second temp (given in instruction)
  tempcmd = (cmd&0xff) << 12;                                           //12 position shifted for the third temp (given in instruction)
  packvalue = tempblock|tempdisk|tempcmd;                               //packing value inorder, using the temp values

  return packvalue;                                                     //returning packed values
}


int mdadm_mount(void) {
  if (is_mounted == 0) {                                                //check if device is mounted
    if (jbod_client_operation(newop(0,0,JBOD_MOUNT), NULL) == JBOD_NO_ERROR){  //check if mounting will result to any error  
      is_mounted = 1;                                                   //if successfully mounted, set is_mounted = 1 
      return 1; 
    }else {
      return -1;                                                        //if any error appear, return -1
    }
  }else {
    return -1;                                                          //if already mounted, return -1
  }
}

int mdadm_unmount(void) {
   if (is_mounted == 1) {                                                  //check if device is mounted
    if (jbod_client_operation(newop(0,0,JBOD_UNMOUNT), NULL) == JBOD_NO_ERROR){ //check if unmounting will result to any error
      is_mounted = 0;                                                    //if successfully unmounted, set is_mounted = 0
      return 1;
    }else {
      return -1;                                                         //if any error appear, return -1
    }
  }else {
    return -1;                                                           //if already unmounted, return -1
  }
}

int mdadm_write_permission(void){
  if (is_mounted == 1) {                                                 //check if devices is mounted
    if (jbod_client_operation(newop(0,0,JBOD_WRITE_PERMISSION), NULL) == JBOD_NO_ERROR){ //check for any writing permission error  
      is_written = 1;                                                    //if no writing permission error, set write permission to 1
      return 0;                                                          
    }else {
      return -1;                                                         //if any error, return -1
    }
  }else {
    return -1;                                                           //if not mounted, return -1
  }
}


int mdadm_revoke_write_permission(void){
  if(is_mounted == 1) {                                                 //check if devices is mounted
    if (jbod_client_operation(newop(0,0,JBOD_REVOKE_WRITE_PERMISSION), NULL) == JBOD_NO_ERROR){ //check for any revoke writing permission error
      is_written = 0;                                                   //if no error occur, set write permission to 0
      return 0;
    }else {
      return -1;                                                        //if any error return -1
    }
  }else {
    return -1;                                                          //if not mounted, return -1
  }
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int length = addr + len;                                              //total amount of address needs to read
  int boundary = JBOD_NUM_DISKS * JBOD_DISK_SIZE;                       //set boundary size base on amount of disks
  int current_addr = addr;                                              //set current address to keep track of address location
  int read_bytes;                                                       //set amount of bytes read
  int offset;                                                           //set any unread bytes at the beginning of the current block, this will be constant
  uint8_t *tempbuf = malloc(256);                                       //set temp buffer to keep track of buffer we need, this will be constant
  jbod_error_t err = JBOD_NO_ERROR;                                     //check if any error comes up

  int blockvalue = 0;                                                   //keep track of how many block passed
  int original_off = addr % 256;                                        //set any unread bytes at the beginning of the first block, this will NOT be constant
  int blank = 0;                                                        //amount of unread bytes at the end of the block
  
  if (len == 0 && buf == NULL) {                                        //check for condition: length is 0 while buffer is empty
    return 0;
  }

  if (length > boundary) {                                              //check for out of bound
    return -1;
  }

  if(is_mounted != 1) {                                                 //check if device is mounted
    return -1;
  }
  
  if (len > 2048) {                                                     //check if length is more than 2048
    return -1;
  } else if (len > 0 && buf == NULL) {                                  //check for condition: length is not 0 while buffer is not empty
    return -1;
  }

  diskid = addr/JBOD_DISK_SIZE;                                         //locate the disk
  blockid = (addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;                     //locate the block
  
  for(int i = 0 ; i < len ; i+=read_bytes){                             //loopthrough the current disk and block for the given length
    
    offset = current_addr % 256;                                        //find offset of the current block

    err = jbod_client_operation(newop(0,diskid, JBOD_SEEK_TO_DISK), NULL);     //seek to current disk  
    assert (err == JBOD_NO_ERROR);                                      //check for any error

    err = jbod_client_operation(newop(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);   //seek to current block   
    assert (err == JBOD_NO_ERROR);                                      //check for any error

    if (cache_lookup(diskid,blockid,tempbuf) == -1) {                   //check if item exists in cache
      err = jbod_client_operation(newop(0,0,JBOD_READ_BLOCK), tempbuf);        //if not read current block  
      assert (err == JBOD_NO_ERROR);                                    //check for any error
      cache_insert(diskid,blockid,tempbuf);                             //insert into cache if does not exist
    }                                                                   //if cache already exists, cache_lookup copies required item is into tempbuf, skipping JBOD
    
    if(offset != 0){                                                    //check if there is any extra bytes at the beginnig of the block
      blockvalue++;                                                     //read through one block
      if (((blockvalue*256) - offset) > len) {                          //test to see if read_len is within one block
        blank = ((blockvalue*256) - original_off) - len;                //find extra value at the end of read_len
	memcpy(buf+i, tempbuf+offset, JBOD_BLOCK_SIZE-offset-blank);    //copy memory from tempbuf to read_buf at size of blocksize-offset-blank
	read_bytes = 256-offset-blank;                                  //keep track of bytes read
      }else {                                                           //else when read_len is not within the firt block
	memcpy(buf+i, tempbuf+offset, JBOD_BLOCK_SIZE-offset);          //copy memory from tempbuf to readbuf at size of blocksize-offset
	read_bytes = 256-offset;                                        //keep track of bytes read
      }
    }else {
      blockvalue++;                                                     //read through one block
      if((blockvalue*256)-original_off <= len) {                        //check if reading full block
	memcpy(buf + i, tempbuf, JBOD_BLOCK_SIZE);                      //copy memory from tempbuf to readbuf at the size of blocksize
	read_bytes = JBOD_BLOCK_SIZE;                                   //keep track of bytes read
      } else if((blockvalue*256)-original_off > len){                   //check if reading partial block
	blank = ((blockvalue*256)-original_off)-len;                    //find the value of bytes does not need to read at the end of the block
	memcpy(buf + i, tempbuf, JBOD_BLOCK_SIZE-blank);                //copy memory from tempbuf to readbuf at size of blocksize-blank
	read_bytes = 256-blank;                                         //keep track of bytes read
      }	  
    }
    
    current_addr += read_bytes;                                         //update current address location
    diskid = current_addr/JBOD_DISK_SIZE;                               //update disk location
    blockid = (current_addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;           //update block location
  }
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int final_bound = addr + len;                                         //find the final address writing to
  int write_bound = JBOD_NUM_DISKS * JBOD_DISK_SIZE;                    //check boundary of how much can be written
  int current_addr = addr;                                              //keep track of address location
  int read_bytes;                                                       //amount of bytes already read/write
  int offset;                                                           //offset of the block
  uint8_t *tempbuf = malloc(256);                                       //temp buffer to store bytes upto 256
  jbod_error_t err = JBOD_NO_ERROR;                                     //checking for error

  int blockvalue = 0;                                                   //keeping track of amount of blocks read through
  int original_off = addr % 256;                                        //keeping value of the offset in the first block
  int blank = 0;                                                        //value of extra memory we do not need

  if (len == 0 && buf == NULL) {                                        //check for condition: write_len = 0, write_buff == NULL
    return 0;
  }
  
  if (final_bound > write_bound) {                                      //check if value to be written is out of bound
    return -1;
  }
  
  if (is_mounted != 1) {                                                //check if device is mounted
    return -1;
  }
  
  if(len > 2048) {                                                      //check if length we need to write is over 2048
    return -1;
  } else if (len > 0 && buf == NULL) {                                  //check for condition: write_len > 0, write_buff == NULL
    return -1;
  }

  diskid = addr/JBOD_DISK_SIZE;                                         //find the start address disk location
  blockid = (addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;                     //find the start address block location
  
  for(int i = 0; i < len; i += read_bytes) {                            //loop through the given disk and block with given length
    
    offset = current_addr % 256;                                        //find offset of the current block

    err = jbod_client_operation(newop(0,diskid, JBOD_SEEK_TO_DISK), NULL);     //seek to the current disk 
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(newop(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);   //seek to the current block  
    assert (err == JBOD_NO_ERROR);

    if (cache_lookup(diskid,blockid,tempbuf) == -1) {                   //determine if cache exists
      cache_insert(diskid,blockid,buf);                                 //if cache does not exist, insert cache
      err = jbod_client_operation(newop(0,0,JBOD_READ_BLOCK), tempbuf);        //read the current block into the tempbuf   
      assert (err == JBOD_NO_ERROR);                                    //check for any error
    } else {
      cache_update(diskid,blockid,buf);                                 //if cache exist, update cache
      err = jbod_client_operation(newop(0,0,JBOD_READ_BLOCK), tempbuf);        //read the current block into the tempbuf  
      assert (err == JBOD_NO_ERROR);                                    //check for any error
    }
    
    if(offset != 0){                                                    //check of offset
      blockvalue++;                                                     //read through one block
      if (((blockvalue*256) - offset) > len) {                          //check for if write_len is within one block
        blank = ((blockvalue*256) - original_off) - len;                //find extra value at the end of write_len
	memcpy(tempbuf+offset, buf, JBOD_BLOCK_SIZE-offset-blank);      //copy memory from write_buff into tempbuf with the size of blocksize-offset-blank
	read_bytes = 256-offset-blank;                                  //keep track of bytes read
      }else {                                                           //else when read_len is not within the first block
	memcpy(tempbuf+offset, buf, JBOD_BLOCK_SIZE-offset);            //copy memory from write_buff into tempbuf with the size of blocksize-offset
	read_bytes = 256-offset;                                        //keep track of bytes read
      }
    } else {
      blockvalue++;                                                     //read through one block
      if((blockvalue*256)-original_off <= len) {                        //check if reading full block
	memcpy(tempbuf, buf+i, JBOD_BLOCK_SIZE);                        //copy memory from write_buff into tempbuf with the size of blocksize
	read_bytes = JBOD_BLOCK_SIZE;                                   //keep track of bytes read
      }else if ((blockvalue*256) -original_off > len) {                 //check if reading partial block
	blank = ((blockvalue*256) - original_off) - len;                //find extra value at the end of write_len
	memcpy(tempbuf, buf+i, JBOD_BLOCK_SIZE-blank);                  //copy memory from write_buff into tempbuf with the size of blocksize-blank
	read_bytes = 256-blank;                                         //keep track of bytes read
      }
    }

    err = jbod_client_operation(newop(0,diskid, JBOD_SEEK_TO_DISK), NULL);     //seek to current disk   
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(newop(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);   //seek to current block 
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(newop(0,0,JBOD_WRITE_BLOCK), tempbuf);         //overwrite value in the tempbuf  
    assert (err == JBOD_NO_ERROR);

    current_addr += read_bytes;                                         //update current address locaation
    diskid = current_addr/JBOD_DISK_SIZE;                               //update disk location
    blockid = (current_addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;           //update block location

  }
  return len;
}
