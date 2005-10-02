		.CODE

PUBLIC	_get_save_esp
_get_save_esp:
		MOV	RAX,RSP
		RET

PUBLIC	_rdtsc
_rdtsc:
		RDTSC
		MOV RCX,RDX
		SHL RCX,32
		AND RAX,0FFFFFFFFh
		OR  RAX,RCX
		RET

		END
