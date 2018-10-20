/*-
 * Copyright (c) 2007 Joseph Kong.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 2001, Stephanie Wehner <atrak@itsx.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysent.h>
#include "sys_list_file.h"

int checkCall(char * callname, int callnum);
void usage();

int
main(int argc, char *argv[])
{


    int count = 0;
    for (int i = 0; i < 393; i++) {
        int ret = checkCall(syscall_list[i].callName, syscall_list[i].callNumber);
        if (ret == 1 || ret == 2) 
            count++;
    }
    printf("count = %d\n", count);
}


// returns: 0 if no malicious attempt has been made, and change on system otherwise
// 1 - SYSCALL missing
// -1 - callnum given has invalid address (error in syscall list)
// 2 - SYSCALL is hooked
int checkCall( char * callname, int callnum){
	char errbuf[_POSIX2_LINE_MAX];
	kvm_t *kd;
	kd = kvm_openfiles(NULL, NULL, NULL, O_RDWR, errbuf);
	if (!kd) {
		fprintf(stderr, "ERROR: %s\n", errbuf);
		exit(-1);
	}

    // nl[0] is the sys base address
    // nl[1] is the target address
	struct nlist nl[] = { { NULL }, { NULL }, { NULL }, };

	unsigned long addr;
	struct sysent call;
	nl[0].n_name = "sysent";
	nl[1].n_name = callname;

	//printf("Checking system call %d: %s\n", callnum, callname);

	/* Find the address of sysent[] and callname. */
	if (kvm_nlist(kd, nl) < 0) {
		fprintf(stderr, "ERROR: %s\n", kvm_geterr(kd));
		exit(-1);
	}

    // Address of sysent not found - system error
	if (!nl[0].n_value){
		fprintf(stderr, "ERROR: %s not found (very weird...)\n",
		    nl[0].n_name);
		exit(-1);
	}

	if (!nl[1].n_value) {
        // TODO: IF SYSCALL DOES NOT EXIST
		fprintf(stderr, "ERROR:1 %d: %s not found\n", callnum,nl[1].n_name);
        return 1;
	}

	/* Determine the address of sysent[callnum]. */
	addr = nl[0].n_value + callnum * sizeof(struct sysent);

	/* Copy sysent[callnum]. */
	if (kvm_read(kd, addr, &call, sizeof(struct sysent)) < 0) {
        //fprintf(stderr, "ERROR: %s || ", kvm_geterr(kd));
        printf("%d %s\n",callnum,callname);
        return -1;
	}

	/* Where does sysent[callnum].sy_call point to? */
	//printf("sysent[%d] is at 0x%lx and its sy_call member points to %p\n", callnum, addr, call.sy_call);

	/* Check if that's correct. */
	if ((uintptr_t)call.sy_call != nl[1].n_value) {
        //TODO: do something when syscall hooked
		printf("ALERT! %d: %s should point to 0x%lx instead of 0x%x\n",
		    callnum, callname, nl[1].n_value, (uintptr_t)call.sy_call);
        return 2;

	}

	if (kvm_close(kd) < 0) {
		fprintf(stderr, "ERROR: %s\n", kvm_geterr(kd));
		exit(-1);
	}
    return 0;

}