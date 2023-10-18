; main.s
; Desenvolvido para a placa EK-TM4C1294XL
; Prof. Guilherme Peron
; Ver 1 19/03/2018
; Ver 2 26/08/2018
; Este programa deve esperar o usu�rio pressionar uma chave.
; Caso o usu�rio pressione uma chave, um LED deve piscar a cada 1 segundo.

; -------------------------------------------------------------------------------
        THUMB                        ; Instru��es do tipo Thumb-2
; -------------------------------------------------------------------------------
		
; Declara��es EQU - Defines
;<NOME>         EQU <VALOR>
; ========================
; Defini��es de Valores

NO_BTNS_PRESSED EQU 0
; -------------------------------------------------------------------------------
; �rea de Dados - Declara��es de vari�veis
		AREA  DATA, ALIGN=2
		; Se alguma vari�vel for chamada em outro arquivo
		;EXPORT  <var> [DATA,SIZE=<tam>]   ; Permite chamar a vari�vel <var> a 
		                                   ; partir de outro arquivo
;<var>	SPACE <tam>                        ; Declara uma vari�vel de nome <var>
                                           ; de <tam> bytes a partir da primeira 
                                           ; posi��o da RAM		
sysState    SPACE 0x1
masterPword SPACE 0x4
currPword   SPACE 0x4
guessPword  SPACE 0x4
lcdString   SPACE 0x20
; -------------------------------------------------------------------------------
; �rea de C�digo - Tudo abaixo da diretiva a seguir ser� armazenado na mem�ria de 
;                  c�digo
        AREA    |.text|, CODE, READONLY, ALIGN=2

		; Se alguma fun��o do arquivo for chamada em outro arquivo	
        EXPORT Start                ; Permite chamar a fun��o Start a partir de 
			                        ; outro arquivo. No caso startup.s
									
		; Se chamar alguma fun��o externa	
        ;IMPORT <func>              ; Permite chamar dentro deste arquivo uma 
									; fun��o <func>
		IMPORT  PLL_Init
		IMPORT  SysTick_Init
		IMPORT  SysTick_Wait1ms			
		IMPORT  GPIO_Init
		IMPORT LightUpLEDs	
		IMPORT GetPushBtnsState
		IMPORT DisableAllLEDs
		IMPORT printArrayInLcd
		IMPORT readKeyboard
		IMPORT blinkLEDs

; -------------------------------------------------------------------------------
; Fun��o main()
Start
MSG_OPEN	   DCB   "Cofre Aberto, digite nova senha", 0
MSG_OPENING	DCB	"Cofre Abrindo...", 0
MSG_CLOSING	DCB   "Cofre Fechando...", 0
MSG_CLOSED	DCB	"Cofre Fechado", 0
MSG_LOCKED	DCB	"Cofre Travado.", 0
	BL PLL_Init                  ;Chama a subrotina para alterar o clock do microcontrolador para 80MHz
	BL SysTick_Init              ;Chama a subrotina para inicializar o SysTick
	BL GPIO_Init                 ;Chama a subrotina que inicializa os GPIO
	BL InitilizeVars
;--------------------------------------------------------------------------------
MainLoop
	LDR R1, =sysState
	LDR R1, [R1]
	
	CMP R1, #0
	IT EQ
		BLEQ newPword
	
	CMP R1, #1
	IT EQ
		BLEQ closedSafe
	
	CMP R1, #2
	IT EQ
		BLEQ waitJ0Interrup
	
	CMP R1, #1
	IT EQ
		BLEQ waitMasterPword
	
	B MainLoop
	
;--------------------------------------------------------------------------------
; Routine for entering a new password and close the safe
newPword
	LDR R0, =MSG_OPEN
	MOV R1, #31
	PUSH{LR}
	BL printArrayInLcd
	BL readKeyboard
	POP{LR}

	CMP R0, #NO_BTNS_PRESSED
	IT EQ
		BEQ newPwordEnd

	CMP R7, #4 ; i == 4
	IT EQ
		BEQ newPwordHashtag
	B newPwordNewInput
	
newPwordHashtag
	CMP R0, #0x23 ; R0 == '#'
	IT NE
		BNE newPwordEnd

	PUSH{LR}
	MOV R0, #1000
	BL SysTick_Wait1ms

	LDR R0, =MSG_CLOSING
	MOV R1, #17
	BL printArrayInLcd

	MOV R0, #5000
	BL SysTick_Wait1ms

	MOV R7, #0 ; i = 0
	MOV R6, #0 ; errorCtr = 0
	LDR R1, =sysState
	MOV R0, #1
	STRB R0, [R1]
	POP{LR}

	B newPwordEnd

newPwordNewInput
	LDR R1, =currPword
	ADD R1, R1, R7
	STRB R0, [R1]  ; currPword[i] = R0
	ADD R7, R7, #1 ; i++

	B newPwordEnd

newPwordEnd
	BX LR

;--------------------------------------------------------------------------------
; Routine for when the safe is closed: either opens or locks permanently
closedSafe
	LDR R0, =MSG_CLOSED
	MOV R1, #13
	PUSH{LR}
	BL printArrayInLcd
	BL readKeyboard
	POP{LR}

	CMP R0, #NO_BTNS_PRESSED
	IT EQ
		BEQ closedSafeEnd

	CMP R7, #4 ; i == 4
	IT EQ
		BEQ closedSafeHashtag
	B closedSafeNewInput

closedSafeHashtag
	CMP R0, #0x23 ; R0 == '#'
	IT NE
		BNE closedSafeEnd

	LDR R0, =currPword
	LDR R1, =guessPword
	MOV R2, #4
	PUSH{LR}
	BL arraysCmp
	POP{LR}

	CMP R0, #1
	ITTE NE
		MOVNE R7, #0 ; i = 0
		ADDNE R6, R6, #1 ; errorCtr++
		BEQ closedSafeOpenSafe

	CMP R6, #3 ; errorCtr == 3
	IT EQ
		BEQ closedSafeLockSafe

	B closedSafeEnd

closedSafeOpenSafe
	PUSH{LR}
	LDR R0, =MSG_OPENING
	MOV R1, #16
	BL printArrayInLcd

	MOV R0, #5000
	BL SysTick_Wait1ms

	MOV R7, #0 ; i = 0
	MOV R6, #0 ; errorCtr = 0
	LDR R1, =sysState
	MOV R0, #0
	STRB R0, [R1]
	POP{LR}

closedSafeLockSafe
	PUSH{LR}
	LDR R0, =MSG_LOCKED
	MOV R1, #14
	BL printArrayInLcd

	MOV R0, #5000
	BL SysTick_Wait1ms

	MOV R7, #0 ; i = 0
	MOV R6, #0 ; errorCtr = 0
	MOV R3, #0
	LDR R1, =guessPword
	STR R3, [R1]
	LDR R1, =sysState
	MOV R0, #2
	STRB R0, [R1]
	POP{LR}

	B closedSafeEnd

closedSafeNewInput
	LDR R1, =guessPword
	ADD R1, R1, R7
	STRB R0, [R1]  ; guessPword[i] = R0
	ADD R7, R7, #1 ; i++

	B closedSafeEnd

closedSafeEnd
	BX LR

;--------------------------------------------------------------------------------
; Routine that waits for a interruption and disables all other funcions
waitJ0Interrup
	LDR R0, =MSG_LOCKED
	MOV R1, #14
	PUSH{LR}
	BL printArrayInLcd
	BL blinkLEDs
	POP{LR}

	BX LR

;--------------------------------------------------------------------------------
; Routine to check if the master password was correctly written
waitMasterPword
	LDR R0, =MSG_LOCKED
	MOV R1, #14
	PUSH{LR}
	BL printArrayInLcd
	BL readKeyboard
	POP{LR}

	CMP R0, #NO_BTNS_PRESSED
	IT EQ
		BEQ waitMasterPwordEnd

	CMP R7, #4 ; i == 4
	IT EQ
		BEQ waitMasterPwordHashtag
	B waitMasterPwordNewInput

waitMasterPwordHashtag
	CMP R0, #0x23 ; R0 == '#'
	IT NE
		BNE waitMasterPwordEnd

	LDR R0, =masterPword
	LDR R1, =guessPword
	MOV R2, #4
	PUSH{LR}
	BL arraysCmp
	POP{LR}

	CMP R0, #1
	ITE NE
		MOVNE R7, #0 ; i = 0
		BEQ waitMasterPwordOpenSafe
	
	B waitMasterPwordEnd

waitMasterPwordOpenSafe
	MOV R7, #0 ; i = 0
	LDR R1, =sysState
	MOV R0, #0
	STRB R0, [R1]

	B waitMasterPwordEnd

waitMasterPwordNewInput
	LDR R1, =guessPword
	ADD R1, R1, R7
	STRB R0, [R1]  ; guessPword[i] = R0
	ADD R7, R7, #1 ; i++

	B waitMasterPwordEnd

waitMasterPwordEnd
	BX LR

;--------------------------------------------------------------------------------
; Initializes variables before the main loop
InitilizeVars
	LDR R1, =masterPword
	MOV R2, #0x0304
	MOVT R2, #0x0102
	STR R2, [R1]
	
	MOV R7, #0 ; Iterator for passwords

	BX LR

;--------------------------------------------------------------------------------
; Verifies if two arrays are equal
; Input: R0 = array1 starting address
;        R1 = array2 starting address
;		   R2 = array size
; Output: R0 = 1 if the arrays are equal, 0 if not
arraysCmp
	CMP R2, #0
	ITT EQ
		MOVEQ R0, #1
		BEQ arraysCmpEnd
	
	LDRB R3, [R0], #1
	LDRB R4, [R1], #1
	CMP R3, R4
	ITT NE
		MOVNE R0, #0
		BNE arraysCmpEnd

	SUBS R2, R2, #1
	B arraysCmp

arraysCmpEnd
	BX LR

;--------------------------------------------------------------------------------
; Verifies if the interruption should change state
checkJ0Interrup
	LDR R0, =sysState
	LDRB R1, [R0]
	CMP R1, #2
	ITT EQ
		MOVEQ R1, #3
		STRBEQ R1, [R0]
	
	BX LR

; -------------------------------------------------------------------------------------------------------------------------
; Fim do Arquivo
; -------------------------------------------------------------------------------------------------------------------------	
	ALIGN                        ;Garante que o fim da se��o est� alinhada 
	END                          ;Fim do arquivo
