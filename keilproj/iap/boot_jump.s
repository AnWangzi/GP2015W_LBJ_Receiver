                PRESERVE8
                THUMB
                AREA    |.text|, CODE, READONLY
                EXPORT  Iap_Dispatch
                EXPORT  Iap_Jump
; Forward all app exceptions using IPSR. Only stacked caller-saved registers
; are touched and LR remains the hardware EXC_RETURN value. This does not
; depend on an optional M0+ VTOR register being implemented.
Iap_Dispatch    PROC
                LDR     R0, =0x20003FF8
                LDR     R0, [R0]
                LDR     R1, =0x31504149
                CMP     R0, R1
                BNE     boot_fault
                MRS     R0, IPSR
                LSLS    R0, R0, #2
                LDR     R1, =0x00002000
                LDR     R0, [R1, R0]
                BX      R0
boot_fault      B       boot_fault
                ENDP
Iap_Jump        PROC
                LDR     R2, =0x00002000
                LDR     R0, [R2]
                LDR     R1, [R2, #4]
                MSR     MSP, R0
                MOVS    R0, #0
                MSR     CONTROL, R0
                ISB
                CPSIE   I
                BX      R1
                ENDP
                ALIGN
                END
