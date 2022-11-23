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

int mdadm_mount(void) {
	return -1;
}

int mdadm_unmount(void) {
	return -1;
}

int mdadm_write_permission(void){
	return -1;
}


int mdadm_revoke_write_permission(void){
	return -1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
	return 0;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
	return 0;
}
