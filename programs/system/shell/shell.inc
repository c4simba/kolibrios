

; SHELL <-> console-application protocol (client side, fasm example library).
;
; Communication goes through a shared memory ring buffer.  Layout of the
; segment (must match program_console.h / shell_api.h):
;
;   +0   write_ptr   next byte the client will write   (client owns)
;   +4   read_ptr    next byte the shell will read      (shell owns)
;   +8   resp_ready  shell sets 1 when a reply is ready
;   +12  resp_len    length of the reply payload
;   +16  resp[]      reply payload (shell -> client), SC_RESP_MAX bytes
;   ...  ring[]       command stream, starts at SC_DATA_OFF
;
; The command stream is a sequence of variable length frames:
;
;   [cmd:1][len_lo:1][len_hi:1][payload:len]
;
; Output frames (puts/putc/cls) are only queued and block just while the ring
; is full.  Commands that need an answer (gets/pid/ping/exit) clear resp_ready,
; queue the request and wait for resp_ready.

SC_OK	= 0
SC_EXIT = 1
SC_PUTC = 2
SC_PUTS = 3
SC_GETC = 4
SC_GETS = 5
SC_CLS	= 6
SC_PID = 7
SC_PING = 8

SHM_WRITE =  0x01
SHM_OPEN_ALWAYS = 0x04

; ring buffer layout
SC_SHM_SIZE   = 1024*16
SC_RESP_MAX   = 1024
SC_OFF_WRITE  = 0
SC_OFF_READ   = 4
SC_OFF_RESP   = 8
SC_OFF_RLEN   = 12
SC_OFF_RDATA  = 16
SC_DATA_OFF   = SC_OFF_RDATA + SC_RESP_MAX	; 1040
SC_RING_SIZE  = SC_SHM_SIZE - SC_DATA_OFF	; 15344
SC_FRAME_HDR  = 3

;============================

align 4
sc_name    rb 64
sc_pid	   dd 0
sc_buffer  dd 0
sc_process dd 0

;============================

if used _sc_pid2name
align 4
_sc_pid2name:

 push esp
 push ebx

 xor ecx, ecx
 mov eax, [sc_pid]
 mov ebx, 10
@@:
 xor edx, edx
 div ebx
 push edx
 inc ecx
 test eax, eax
 jnz @b

mov edi, sc_name

@@:
 pop eax
 add al, '0'
 stosb
 loop @b


 mov al, '-'
 stosb
 mov al, 'S'
 stosb
 mov al, 'H'
 stosb
 mov al, 'E'
 stosb
 mov al, 'L'
 stosb
 mov al, 'L'
 stosb
 mov al,  0
 stosb


 pop ebx
 pop esp

 ret
end if

;============================
; internal: queue one frame into the ring buffer.
;   in: al = command, esi = payload pointer, ecx = length
;   blocks only while the ring is full; clobbers eax, ecx, edx
;============================

if used _sc_puts | used _sc_putc | used _sc_cls | used _sc_gets | used _sc_pid | used _sc_ping | used _sc_exit
align 4
_sc_send:

 push ebx
 push esi
 push edi
 push ebp

 movzx ebp, al		; ebp = command byte
 mov   edi, ecx		; edi = length

.wait:			; wait until the ring can hold length+3 bytes
 mov   ebx, [sc_buffer]
 mov   eax, [ebx + SC_OFF_WRITE]
 sub   eax, [ebx + SC_OFF_READ]		; used = write_ptr - read_ptr
 jns   @f
 add   eax, SC_RING_SIZE
@@:
 mov   edx, SC_RING_SIZE - 1
 sub   edx, eax				; edx = free bytes
 mov   eax, edi
 add   eax, SC_FRAME_HDR		; eax = length + 3
 cmp   edx, eax
 jae   .room
 mov   eax, 5				; sleep 1/100 s, then retry
 mov   ebx, 1
 int   0x40
 jmp   .wait

.room:
 mov   ebx, [sc_buffer]
 mov   edx, [ebx + SC_OFF_WRITE]	; edx = write_ptr
 add   ebx, SC_DATA_OFF			; ebx = ring base

 mov   eax, ebp				; command byte
 mov   [ebx + edx], al
 inc   edx
 cmp   edx, SC_RING_SIZE
 jb    @f
 xor   edx, edx
@@:
 mov   eax, edi				; length low byte
 mov   [ebx + edx], al
 inc   edx
 cmp   edx, SC_RING_SIZE
 jb    @f
 xor   edx, edx
@@:
 mov   eax, edi				; length high byte
 shr   eax, 8
 mov   [ebx + edx], al
 inc   edx
 cmp   edx, SC_RING_SIZE
 jb    @f
 xor   edx, edx
@@:
 mov   ecx, edi				; payload
 test  ecx, ecx
 jz    .done
.copy:
 mov   al, [esi]
 mov   [ebx + edx], al
 inc   esi
 inc   edx
 cmp   edx, SC_RING_SIZE
 jb    @f
 xor   edx, edx
@@:
 dec   ecx
 jnz   .copy

.done:
 mov   ebx, [sc_buffer]
 mov   [ebx + SC_OFF_WRITE], edx	; publish the whole frame at once

 pop   ebp
 pop   edi
 pop   esi
 pop   ebx
 ret
end if

;============================
; internal: queue a request frame and wait for the reply slot.
;   in: al = command, esi = payload pointer, ecx = length
;============================

if used _sc_gets | used _sc_pid | used _sc_exit
align 4
_sc_request:

 push  ebx
 mov   ebx, [sc_buffer]
 mov   dword [ebx + SC_OFF_RESP], 0	; resp_ready = 0
 call  _sc_send				; al / esi / ecx still valid

 mov   ebx, [sc_buffer]
.wait:
 cmp   dword [ebx + SC_OFF_RESP], 0
 jne   @f
 push  ebx
 mov   eax, 5
 mov   ebx, 2
 int   0x40
 pop   ebx
 jmp   .wait
@@:
 pop   ebx
 ret
end if

;============================

if used _sc_init
align 4
; void __stdcall sc_init();
_sc_init:

 push ebx

 mov eax, 68				; init heap
 mov ebx, 11
 int 0x40

 mov eax, 68				; allocate a scratch buffer
 mov ebx, 12
 mov ecx, 1024
 int 0x40

 mov [sc_process], eax

 mov eax, 9				; read our own process info
 mov ebx, [sc_process]
 mov ecx, -1
 int 0x40

 mov dword eax, [ebx+30]		; our PID
 mov [sc_pid], eax

 mov eax, 68				; free the scratch buffer
 mov ebx, 13
 mov ecx, [sc_process]
 int 0x40

 call _sc_pid2name

 mov eax, 68				; create the shared buffer
 mov ebx, 22
 mov dword ecx, sc_name
 mov edx, SC_SHM_SIZE
 mov esi, SHM_OPEN_ALWAYS or SHM_WRITE
 int 0x40

 mov [sc_buffer], eax

 mov dword [eax + SC_OFF_WRITE], 0	; empty ring, no pending reply
 mov dword [eax + SC_OFF_READ], 0
 mov dword [eax + SC_OFF_RESP], 0
 mov dword [eax + SC_OFF_RLEN], 0

 pop ebx

 ret
end if

;============================

if used _sc_puts
align 4
; void __stdcall sc_puts(char *str);
_sc_puts:

 mov   esi, [esp+4]			; str
 xor   ecx, ecx
@@:					; ecx = strlen(str)
 cmp   byte [esi+ecx], 0
 je    @f
 inc   ecx
 jmp   @b
@@:
 inc   ecx				; include the terminating 0
 mov   al, SC_PUTS
 call  _sc_send
 ret 4
end if

;============================

if used _sc_putc
align 4
; void __stdcall sc_putc(char c);
_sc_putc:

 lea   esi, [esp+4]			; &c
 mov   ecx, 1
 mov   al, SC_PUTC
 call  _sc_send
 ret 4
end if

;============================

if used _sc_cls
align 4
; void __stdcall sc_cls();
_sc_cls:

 xor   ecx, ecx
 mov   al, SC_CLS
 call  _sc_send
 ret
end if

;============================

if used _sc_exit
align 4
; void __stdcall sc_exit();
_sc_exit:

 xor   ecx, ecx
 mov   al, SC_EXIT
 call  _sc_request			; wait until the shell drained & acked

 mov eax, 68				; close the shared buffer
 mov ebx, 23
 mov dword ecx, sc_name
 int 0x40

 ret
end if

;============================

if used _sc_gets
align 4
; void __stdcall sc_gets(char *str);
_sc_gets:

 push  edi
 mov   edi, [esp+8]			; str

 push  256				; requested max length (payload)
 mov   esi, esp
 mov   ecx, 4
 mov   al, SC_GETS
 call  _sc_request
 add   esp, 4

 mov   esi, [sc_buffer]			; copy reply string to str
 add   esi, SC_OFF_RDATA
@@:
 mov   al, [esi]
 mov   [edi], al
 inc   esi
 inc   edi
 test  al, al
 jnz   @b

 pop   edi
 ret 4
end if

;============================

if used _sc_pid
align 4
; int __stdcall sc_pid(void);
_sc_pid:

 xor   ecx, ecx
 mov   al, SC_PID
 call  _sc_request

 mov   eax, [sc_buffer]
 mov   eax, [eax + SC_OFF_RDATA]	; 4-byte PID from the reply slot
 ret
end if

;============================

if used _sc_ping
align 4
; int __stdcall sc_ping(void);  returns 0 if a shell replied, -1 otherwise
_sc_ping:

 push  ebx
 mov   ebx, [sc_buffer]
 mov   dword [ebx + SC_OFF_RESP], 0
 xor   ecx, ecx
 mov   al, SC_PING
 call  _sc_send

 mov   ecx, 20				; bounded wait
.wait:
 mov   ebx, [sc_buffer]
 cmp   dword [ebx + SC_OFF_RESP], 0
 jne   .ok
 push  ecx
 mov   eax, 5
 mov   ebx, 1
 int   0x40
 pop   ecx
 dec   ecx
 jnz   .wait

 pop   ebx				; timeout
 mov   eax, -1
 ret
.ok:
 pop   ebx
 xor   eax, eax
 ret
end if
