	.file	"muldiv"
gcc2_compiled.:
___gnu_compiled_c:
.text
	.align 2,0x90
.globl _muldiv
_muldiv:
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	mull 12(%ebp)
	divl 16(%ebp)
	leave
	ret
