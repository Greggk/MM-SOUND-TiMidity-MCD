	.file	"muldivor"
gcc2_compiled.:
___gnu_compiled_c:
.text
	.align 2,0x90
.globl _muldiv
_muldiv:
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	imull 12(%ebp),%eax
	xorl %edx,%edx
	divl 16(%ebp)
	leave
	ret
