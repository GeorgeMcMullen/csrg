/*
 * Copyright (c) 1992 OMRON Corporation.
 * Copyright (c) 1982, 1990, 1992 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * OMRON: $Id: genassym.c,v 1.2 92/06/14 06:15:25 moti Exp $
 *
 * from: hp300/hp300/genassym.c	7.9 (Berkeley) 8/29/91
 *
 *	@(#)genassym.c	7.1 (Berkeley) 06/15/92
 */

#define KERNEL

#include "sys/param.h"
#include "sys/buf.h"
#include "sys/map.h"
#include "sys/proc.h"
#include "sys/mbuf.h"
#include "sys/msgbuf.h"
#include "../include/cpu.h"
#include "../include/trap.h"
#include "../include/psl.h"
#include "../include/reg.h"
#include "../include/stinger.h"
#include "../dev/pioreg.h"
#include "clockreg.h"
#include "sys/syscall.h"
#include "vm/vm.h"
#include "sys/user.h"
#include "pte.h"

main()
{
	register struct proc *p = (struct proc *)0;
	register struct vmmeter *vm = (struct vmmeter *)0;
	register struct user *up = (struct user *)0;
	register struct rusage *rup = (struct rusage *)0;
	struct vmspace *vms = (struct vmspace *)0;
	pmap_t pmap = (pmap_t)0;
	struct pcb *pcb = (struct pcb *)0;
	struct KernInter *KernInter = (struct KernInter *)0;
	struct pio *pio = (struct pio *)PIO0_ADDR;
	register unsigned i;

	printf("#define\tP_LINK %d\n", &p->p_link);
	printf("#define\tP_RLINK %d\n", &p->p_rlink);
	printf("#define\tP_VMSPACE %d\n", &p->p_vmspace);
	printf("#define\tVM_PMAP %d\n", &vms->vm_pmap);
	printf("#define\tPM_STCHG %d\n", &pmap->pm_stchanged);
	printf("#define\tP_ADDR %d\n", &p->p_addr);
	printf("#define\tP_PRI %d\n", &p->p_pri);
	printf("#define\tP_STAT %d\n", &p->p_stat);
	printf("#define\tP_WCHAN %d\n", &p->p_wchan);
	printf("#define\tP_FLAG %d\n", &p->p_flag);
	printf("#define\tSSLEEP %d\n", SSLEEP);
	printf("#define\tSRUN %d\n", SRUN);
	printf("#define\tV_SWTCH %d\n", &vm->v_swtch);
	printf("#define\tV_TRAP %d\n", &vm->v_trap);
	printf("#define\tV_SYSCALL %d\n", &vm->v_syscall);
	printf("#define\tV_INTR %d\n", &vm->v_intr);
	printf("#define\tV_SOFT %d\n", &vm->v_soft);
	printf("#define\tV_FAULTS %d\n", &vm->v_faults);
	printf("#define\tKI_MAXADDR %d\n", &KernInter->maxaddr);
	printf("#define\tUPAGES %d\n", UPAGES);
	printf("#define\tHIGHPAGES %d\n", HIGHPAGES);
	printf("#define\tP1PAGES %d\n", P1PAGES);
	printf("#define\tCLSIZE %d\n", CLSIZE);
	printf("#define\tNBPG %d\n", NBPG);
	printf("#define\tNPTEPG %d\n", NPTEPG);
	printf("#define\tPGSHIFT %d\n", PGSHIFT);
	printf("#define\tSYSPTSIZE %d\n", SYSPTSIZE);
	printf("#define\tUSRPTSIZE %d\n", USRPTSIZE);
	printf("#define\tUSRIOSIZE %d\n", USRIOSIZE);
#ifdef SYSVSHM
	printf("#define\tSHMMAXPGS %d\n", SHMMAXPGS);
#endif
	printf("#define\tUSRSTACK %d\n", USRSTACK);
	printf("#define\tKERNELSTACK %d\n", KERNELSTACK);
	printf("#define\tMSGBUFPTECNT %d\n", btoc(sizeof (struct msgbuf)));
	printf("#define\tNMBCLUSTERS %d\n", NMBCLUSTERS);
	printf("#define\tMCLBYTES %d\n", MCLBYTES);
	printf("#define\tNKMEMCLUSTERS %d\n", NKMEMCLUSTERS);
	printf("#define\tU_PROF %d\n", &up->u_stats.p_prof);
	printf("#define\tU_PROFSCALE %d\n", &up->u_stats.p_prof.pr_scale);
	printf("#define\tRU_MINFLT %d\n", &rup->ru_minflt);
	printf("#define\tT_BUSERR %d\n", T_BUSERR);
	printf("#define\tT_ADDRERR %d\n", T_ADDRERR);
	printf("#define\tT_ILLINST %d\n", T_ILLINST);
	printf("#define\tT_ZERODIV %d\n", T_ZERODIV);
	printf("#define\tT_CHKINST %d\n", T_CHKINST);
	printf("#define\tT_TRAPVINST %d\n", T_TRAPVINST);
	printf("#define\tT_PRIVINST %d\n", T_PRIVINST);
	printf("#define\tT_TRACE %d\n", T_TRACE);
	printf("#define\tT_MMUFLT %d\n", T_MMUFLT);
	printf("#define\tT_SSIR %d\n", T_SSIR);
	printf("#define\tT_FMTERR %d\n", T_FMTERR);
	printf("#define\tT_COPERR %d\n", T_COPERR);
	printf("#define\tT_FPERR %d\n", T_FPERR);
	printf("#define\tT_ASTFLT %d\n", T_ASTFLT);
	printf("#define\tT_TRAP15 %d\n", T_TRAP15);
	printf("#define\tT_FPEMULI %d\n", T_FPEMULI);
	printf("#define\tT_FPEMULD %d\n", T_FPEMULD);
	printf("#define\tPSL_S %d\n", PSL_S);
	printf("#define\tPSL_IPL7 %d\n", PSL_IPL7);
	printf("#define\tPSL_LOWIPL %d\n", PSL_LOWIPL);
	printf("#define\tPSL_HIGHIPL %d\n", PSL_HIGHIPL);
	printf("#define\tPSL_USER %d\n", PSL_USER);
	printf("#define\tSPL1 %d\n", PSL_S | PSL_IPL1);
	printf("#define\tSPL2 %d\n", PSL_S | PSL_IPL2);
	printf("#define\tSPL3 %d\n", PSL_S | PSL_IPL3);
	printf("#define\tSPL4 %d\n", PSL_S | PSL_IPL4);
	printf("#define\tSPL5 %d\n", PSL_S | PSL_IPL5);
	printf("#define\tSPL6 %d\n", PSL_S | PSL_IPL6);
	printf("#define\tFC_USERD %d\n", FC_USERD);
	printf("#define\tFC_PURGE %d\n", FC_PURGE);
	printf("#define\tCACHE_ON %d\n", CACHE_ON);
	printf("#define\tCACHE_OFF %d\n", CACHE_OFF);
	printf("#define\tCACHE_CLR %d\n", CACHE_CLR);
	printf("#define\tIC_CLEAR %d\n", IC_CLEAR);
	printf("#define\tDC_CLEAR %d\n", DC_CLEAR);
	printf("#define\tPG_V %d\n", PG_V);
	printf("#define\tPG_NV %d\n", PG_NV);
	printf("#define\tPG_RO %d\n", PG_RO);
	printf("#define\tPG_RW %d\n", PG_RW);
	printf("#define\tPG_CI %d\n", PG_CI);
	printf("#define\tPG_PROT %d\n", PG_PROT);
	printf("#define\tPG_FRAME %d\n", PG_FRAME);
	printf("#define\tSG_V %d\n", SG_V);
	printf("#define\tSG_NV %d\n", SG_NV);
	printf("#define\tSG_RW %d\n", SG_RW);
	printf("#define\tSG_FRAME %d\n", SG_FRAME);
	printf("#define\tSG_ISHIFT %d\n", SG_ISHIFT);
	printf("#define\tPCB_FLAGS %d\n", &pcb->pcb_flags);
	printf("#define\tPCB_PS %d\n", &pcb->pcb_ps);
	printf("#define\tPCB_USTP %d\n", &pcb->pcb_ustp);
	printf("#define\tPCB_USP %d\n", &pcb->pcb_usp);
	printf("#define\tPCB_REGS %d\n", pcb->pcb_regs);
	printf("#define\tPCB_ONFAULT %d\n", &pcb->pcb_onfault);
	printf("#define\tPCB_FPCTX %d\n", &pcb->pcb_fpregs);
	printf("#define\tSIZEOF_PCB %d\n", sizeof(struct pcb));
	printf("#define\tSP %d\n", SP);
	printf("#define\tB_READ %d\n", B_READ);
	printf("#define\tENOENT %d\n", ENOENT);
	printf("#define\tEFAULT %d\n", EFAULT);
	printf("#define\tENAMETOOLONG %d\n", ENAMETOOLONG);
	printf("#define\tPIO0_A %d\n", &pio->a_port);
	printf("#define\tPIO0_B %d\n", &pio->b_port);
	printf("#define\tPIO0_CTL %d\n", &pio->control_port);
	printf("#define\tPIO_MODED %d\n", PIO_MODED);
	printf("#define\tCLOCK_REG %d\n", CLOCK_REG);
	printf("#define\tCLK_INT %d\n", CLK_INT);
	printf("#define\tCLK_CLR %d\n", CLK_CLR);
	printf("#define\tSYS_exit %d\n", SYS_exit);
	printf("#define\tSYS_execve %d\n", SYS_execve);
	printf("#define\tSYS_sigreturn %d\n", SYS_sigreturn);
#ifdef	OLD_LUNA
	printf("define\tFLINE_VEC %d\n",44); /* F-Line excep vector offset */
	printf("define\tCOPRO_VEC %d\n",52); /* Coprocessor prot. offset */
#endif
	exit(0);
}
