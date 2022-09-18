.586P

; ----- Структура для описания дескрипторов сегментов ----- ;
descriptor_segment struc
    lim         dw  0       ; Граница - Limit (0...15)
    base_l      dw  0       ; База, биты 0...15
    base_m      db  0       ; База, биты 16...23
    attr_1      db  0       ; Байт атрибутов 1
    attr_2      db  0       ; Граница (биты 16...19) и атрибуты 2
    base_h      db  0       ; База, биты 24...31
descriptor_segment ends
; 5 и 6 биты dpl = 00
; limit base_h base_m attr_1 attr_2 base_l
; attr_1:
; 92h = 10010010b
; 7-й - бит присутствия
; 5-6 - DPL.
; 4-й - идентефикатор сег
; 3 - предназначения 0 - данных или стека, 1 - кода
; 2 - для кода - подчиненный (0) или обычный (1), 0 - данных, 1 - стека
; 1 - для кода - 1 - чтение разрешено, для данных - модификация данных - 1 - разрешена
; 0 - attach

; attr_2:
; 8Fh = 10001111
; 7-й - бит гранулярности
; 6-й - бит разрядности операндов
; 5-й - не исп.
; 4-й - AVL (исп. прикл. прогр.)
; 0-3 - limit.

; ----- Структура для описания шлюзов ловушек ----- ;
descriptor_trap struc
    offset_1    dw  0       ; Смещение (0...15)
    sel         dw  16      ; Селектор (0...15)
    cntr        db  0       ; Не используется
    dtype       db  8Fh     ; Тип шлюза - ловушка 80386 и выше
    offs_h      dw  0       ; Смещение (16...31)
descriptor_trap ends

; ----- Сегмент данных -----
data segment use16
    ; --- Таблица глобальных дескрипторов - GDT ---
        gdt_null descriptor_segment <0, 0, 0, 0, 0, 0>		                    ; Селектор 0, нулевой селектор
        gdt_data descriptor_segment <data_size - 1, 0, 0, 92h, 0, 0>            ; Селектор 8, сегмент данных
        gdt_code32 descriptor_segment <text32_size - 1, 0, 0, 98h, 0, 0>            ; Селектор 16, сегмент кода
        gdt_stack descriptor_segment <255, 0, 0, 92h, 0, 0>                     ; Селектор 24, сегмент стека
        gdt_screen32 descriptor_segment <3999, 8000h, 0Bh, 92h, 0, 0>             ; Селектор 32, видеопамять

        gdt_data_4gb descriptor_segment <0FFFFh, 0, 10h, 92h, 8Fh, 0>           ; Селектор 40, большой сегмент данных на 4ГБ,
            ;граница - 0FFFFFh + 1 => 2^20 страниц по 4КБ 
        gdt_size = $ - gdt_null     ; Размер GDT

    ; --- Таблица дескрипторов прерываний - IDT ---
        idt label word
            ; 5 и 6 биты rpl
            ; 8Eh = 10001110b
            descriptor_trap 13 dup (<dummy>) ; Дескрипторы исключений 0...12
            descriptor_trap <exc13>
            descriptor_trap 18 dup (<dummy>) ; Дескрипторы исключений 14...31
            descriptor_trap <new_08, , 8Eh>  ; Дескриптор прерывания от таймера
            descriptor_trap <new_09, , 8EH>  ; Дескриптор прерывания от клавиатуры
        idt_size = $ - idt

    ; --- Данные программы --- 
        ; Для изменения режимов работы процессора
            pdescr      df  0       ; Псевдодескриптор для команды lgdt - информация о GDT // df - 6 байт
            mask_master db  0       ; Маска прерываний ведущего контроллера
            mask_slave  db  0       ; Маска прерываний ведомого контроллера
            exit_flag   db  0       ; Когда осуществить выход из программы

        ; Для отладки
            msg_1   db  'In real mode$'
            msg_2   db  10, 13, 10, 13, 'welcome back to real mode$'
            msg_to_protected db 'Welcome to protected mode'
            msg_to_pr_len = $ - msg_to_protected
            dbg_str db '****-**** ****-****'
            dbg_str_len = $ - dbg_str

        ; Для вывода важной информации
            timer_output    dw  158     ; Позиция для вывода информации от таймера
            timer_symbol    db  2Fh     ; Символ таймера
            timer_count     db  0       ; Счетчик таймера

            keyboard_color  db  20h     ; Цвет вводимого символа
            keyboard_shift  db  0       ; Флаг нажатия шифта
            keyboard_output dw  480    ; Позиция для вывода символов

            memory_sign         db  45h ; Сигнатура для проверки записи
            memory_count        dd  0   ; Счетчик для подсчета памяти
            memory_str_output   dw  3840
            memory_str          db  'Memory amount - **** ****h'
            memory_str_len  = $ - memory_str
        
        ; ascii-map
        ; Symbols:
        ; 31h...39h -> 1..9 + 30h -> 0, 2dh -> '-', 2bh -> '+'
        ; 41h...5Ah -> 'A'-'Z'
        ; Functional letters:
        ; backspace - 0E, tab - 0F, ctrl - 1D, enter - 1C, shift - 2A
            ascii_map db 0, 0, 31h, 32h, 33h, 34h, 35h, 36h, 37h, 38h, 39h, 30h, 2dh, 2bh, 0    ; zeros - none, backspace, 
            db 0, 51h, 57h, 45h, 52h, 54h, 59h, 55h, 49h, 4Fh, 50h, 5Bh, 5Dh, 0, 0              ; zeros - tab, enter, ctrl 
            db 41h, 53h, 44h, 46h, 47h, 48h, 4Ah, 4Bh, 4Ch, 3bh, 27h, 0                         ; zeros - 29 (Ё)
            db 0, 0, 5Ah, 58h, 43h, 56h, 42h, 4Eh, 4Dh, 2Ch, 2Eh, 2Fh, 0                        ; zeros - lshift, 2b, rshift 
            db 0, 0, 20h                                                                        ; zeros: alt

        data_size = $ - gdt_null    ; Размер сегмента данных, вычисляемый на этапе трансляции

data ends

;  ----- Сегмент кода -----
text segment use16      ; 16-разрядный сегмент
    assume CS:text, DS:data
textseg label word

main proc
    ; Очистить экран
    mov AX, 3
    int 10h

    xor EAX, EAX        ; EAX = 0
    mov AX, data
    mov DS, AX          ; DS = адрес сегмента данных
    
    mov AH, 09h
    mov DX, offset msg_1
    int 21h

    mov AX, data

    ; Вычисление 32-битного линейного адреса сегмента ДАННЫХ в EAX
    ; Чтобы загрузить его в дескриптор сегмента данных в GDT
        shl EAX, 4
        mov EBP, EAX                ; Сохранение для будущего
        mov BX, offset gdt_data     ; BX - смещение сегмента данных
        mov [BX].base_l, AX         ; Загрузить в дескриптор сегмента данных младшую часть базы (смещение же 0)
        shr EAX, 16                 ; Сдвинуть, чтобы записать старшую часть
        mov [BX].base_m, AL         ; Загрузить в дескриптор сегмента данных старшую часть базы - 8 битов (0000хххх)

    ; Вычисление 32-битного линейного адреса сегмента КОДА в EAX
    ; Загрузка в GDT
        xor EAX, EAX                ;
        mov AX, text32              ;
        shl EAX, 4                  ;
        mov BX, offset gdt_code32   ;
        mov [BX].base_l, AX         ;
        shr EAX, 24                 ;
        mov [BX].base_m, AL         ;

    ; Вычисление 32-битного линейного адреса сегмента СТЕКА в EAX
    ; Загрузка в GDT
        xor EAX, EAX                ;
        mov AX, SS                  ;
        shl EAX, 4                  ;
        mov BX, offset gdt_stack    ;
        mov [BX].base_l, AX         ;
        shr EAX, 32                 ;
        mov [BX].base_m, AL         ;


    ; Загрузка GDTR
        mov dword ptr pdescr + 2, EBP       ; База GDT
        mov word ptr pdescr, gdt_size - 1  ; Граница GDT
        lgdt pdescr ; Загрузим регистр GDTR

    cli                         ; Запретить прерывания

    ; Сохранить маски прерываний контроллеров
        in AL, 21h
        mov mask_master, AL  ; Ведущий
        in AL, 0A1h
        mov mask_slave, AL   ; Ведомый

    ; Инициализируем ведущий контроллер, базовый вектор - 32
        mov AL, 11h     ; СКИ1:
        out 20h, AL
        mov AL, 32      ; СКИ2: базовый вектор
        out 21h, AL
        mov AL, 4       ; СКИ3: ведомый подключен к уровню 2
        out 21h, AL
        mov AL, 1       ; СКИ4: 80х86, требуетяс EOI
        out 21h, AL
        mov AL, 0FCh    ; Маска прерываний
        out 21h, AL

    ; Запретим все прерывания в ведомом контроллере
        mov AL, 0FFh    ; Маска прерываний в порт
        out 0A1h, AL

    ; Загрузка IDTR
        mov word ptr pdescr, idt_size - 1
        xor EAX, EAX
        mov AX, offset idt
        add EAX, EBP
        mov dword ptr pdescr + 2, EAX
        lidt pdescr

    ; Открыть линию A20 для обращения к расширенной памяти
        mov AL, 0D1h    ; Команда управления линией A20
        out 64h, AL     ;
        mov AL, 0dFh    ; Код открытия линии A20
        out 60h, AL     ;

    ; Переход в защищенный режим
        mov EAX, CR0        ; 
        or EAX, 1           ; Установка бита PM в управляющем регистре процессора CR0
        mov CR0, EAX        ;

    ; ----- Работа в защищенном режиме -----
    ; Загрузка в CS:IP селектор:смещение точки protected_start
        db 0EAh             ; Код команды far jmp
        dw offset protected_start   ; Смещение
        dw 16               ; Селектор сегмента кода

    ; ----- Вновь в реальном режиме -----
    return_to_real:
    ; Восстановить вычислительную среду реального режима
    mov AX, data
    mov DS, AX
    mov AX, stk
    mov SS, AX
    mov SP, 256

    ; Восстановить состояние регистра IDTR реального режима
    mov AX, 3FFh
    mov word ptr pdescr, AX
    xor EAX, EAX
    mov dword ptr pdescr + 2, EAX
    lidt pdescr

    ; Реинициализировать ведущий контроллер прерываний обоих контроллеров
    ; Установив в нем базовый вектор 8
    mov DX, 20h     ; Первый порт контроллера
    mov AL, 11h     ; СКИ1 - слово иницализации всегда 11h
    out DX, AL
    jmp $ + 2       ; Задержка

    inc DX          ; Второй порт контроллера
    mov AL, 8       ; СКИ2 - базовый вектор
    out DX, AL
    jmp $ + 2

    mov AL, 4       ; СКИ3 - ведомй подключен к уровню 2
    out DX, AL      
    jmp $ + 2
    mov AL, 1       ; СКИ4 - требуется EOI
    out DX, AL

    ; Восстановить исходные маски прерываний обоих контроллеров
    mov AL, mask_master
    out 21h, AL
    mov AL, mask_slave
    out 0A1h, AL
    sti

    ; Работа в DOS - завершить программу
    ; Очистить экран
    mov AX, 3
    int 10h
	mov AH, 09h
	mov DX, offset msg_2
	int 21h
    mov AX, 4C00h
    int 21h

main endp

code_size = $ - main
text ends

; ----- 32 разрядный сегмент кода ----- ;
text32 segment use16
    assume CS: text32, DS:data
text32seg label word
    exc13 proc
        pop EAX             ; Код ошибки
        pop EAX             ; EIP в точке исключения
        ; mov SI, offset dbg_str + 5
        ; call wrd_asc
        ; mov AX, 13
        jmp home
    exc13 endp

    dummy proc
        jmp home
    dummy endp

    new_08 proc
        ; Сохранение аппаратного контекста
        push AX
        push BX

        test timer_count, 00000011b ; Снизить частоту
        jnz skip

        mov AH, 0F0h            ; Атрибут символа
        mov AL, timer_symbol

        mov DI, timer_output

        cmp AL, 2Fh
        je timer_first_line

        cmp AL, 7Ch
        je timer_second_line

        cmp AL, 5Ch
        je timer_third_line

        timer_first_line:
            mov AL, 7Ch
            jmp timer_draw

        timer_second_line:
            mov AL, 5Ch
            jmp timer_draw

        timer_third_line:
            mov AL, 2Fh
            jmp timer_draw

        timer_draw:
            stosw
            mov timer_symbol, AL

        skip:
        inc timer_count         ; Инекремент счетчика прерываний

        mov AL, 20h         ; EOI ведомого контроллера
        out 20h, AL         ; Команда подтверждения завершения обработки прерывания
        ; Восстановление аппаратного контекста
        pop BX
        pop AX
        db 66h
        iret                ; Возврат в программу
    new_08 endp

    new_09 proc
        ; Сохранение аппаратного контекста
        push AX
        push BX

        in AL, 60h  ; Получить введенный символ

        cmp AL, 0AAh  ; l shift released
        je kb_shift_released

        cmp AL, 0B6h  ; r shift released
        je kb_shift_released

        cmp AL, 80h ; Чтобы при отпускании клавиши не вводился символ
        ja kb_int_end

        cmp AL, 3Ah ; Чтобы не работал капслок
        je kb_int_end

        cmp AL, 2Ah ; l shift
        je kb_shift
        cmp AL, 36h ; r shift
        je kb_shift

        cmp AL, 1Dh ; l/r ctrl
        je kb_ctrl

        cmp AL, 38h ; l/r alt
        je kb_alt

        cmp AL, 0Eh
        je kb_backspace

        cmp AL, 1h
        je kb_escape

        jmp normal_key

        kb_shift:
            mov keyboard_shift, 1
            jmp kb_int_end

        kb_shift_released:
            mov keyboard_shift, 0
            jmp kb_int_end

        kb_ctrl:
            mov BL, keyboard_color
            shr BL, 4
            inc BL             ; Поменять цвет фона
            test BL, 00000111b
            jnz ctrl_no_overflow
            
            dec BL
            and BL, 11111000b

            ctrl_no_overflow:
            shl BL, 4
            mov keyboard_color, BL
            jmp kb_int_end
        
        kb_alt:
            mov BL, keyboard_color
            inc BL
            test BL, 00001111b
            jnz alt_no_overflow

            dec BL
            and BL, 11110000b

            alt_no_overflow:
            mov keyboard_color, BL
            jmp kb_int_end
            
        kb_backspace:
            xor AX, AX
            mov DI, keyboard_output

            test DI, DI     ; Проверка если ноль
            jz kb_flash_line

            stosw
            sub DI, 4
            mov keyboard_output, DI
            jmp kb_flash_line

        kb_escape:
            mov exit_flag, 1
            jmp kb_int_end

        normal_key:
        mov AH, keyboard_color
        xor BX, BX
        mov BL, AL
        mov AL, ascii_map[BX]

        cmp AL, 0h
        je kb_int_end

        mov BL, keyboard_shift
        test BL, BL
        jz kb_not_shifted

        cmp AL, 41h
        jb kb_not_shifted
        cmp AL, 5Ah
        ja kb_not_shifted
        
        add AL, 20h     ; Сделать букву большой
        
        kb_not_shifted:
        mov DI, keyboard_output
        stosw
        mov keyboard_output, DI

        kb_flash_line:
        mov AX, 8F5Fh
        stosw

        kb_int_end:
        in AL, 61h  ; Получить содержимое системного порта
        or AL, 80h  ; 
        out 61h, AL ; Только чтение - остановит ввод с клавиатуры
        and AL, 7Fh ;
        out 61h, AL ; Освобождения буфера контроллера после приема символа

        mov AL, 20h ; EOI ведомого контроллера
        out 20h, AL ; Команда подвтерждения завершения обработки прерывания

        ; Восстановление аппаратного контекста
        pop BX
        pop AX
        db 66h
        iret        ; Возврат в программу
    new_09 endp

    protected_start:
        ; Делаем адресуемыми данные
        mov AX, 8
        mov DS, AX

        ; Делаем адресуемым стек
        mov AX, 24
        mov SS, AX
        ; Инициализируем ES - видеопамять
        mov AX, 32
        mov ES, AX

        ; ----- Начало основной программы -----

        mov ESI, offset msg_to_protected
        mov CX, msg_to_pr_len
        mov AH, 07h
        mov DI, 160
        welcome_scrn:
            lodsb
            stosw
        loop welcome_scrn

        sti                 ; Разрешить аппаратные прерывания

            ; --- Посчитаем объем доступной памяти ---
            xor EBX, EBX
            mov AX, 40
            mov GS, AX          ; Селектор сегмента данных на 4 ГБ

            ; Заполняем память
            count_memory:
                mov al, GS:[EBX]        ; Сохранить старый байт
                mov ah, memory_sign     ; Новый байт вида
                mov GS:[EBX], ah        ; Записать новый байт
                cmp GS:[EBX], ah        ; Проверка записалось ли
                jne print_memory_count  ; Если не равны, то выведем посчитанное значение
                mov GS:[EBX], al

                inc EBX
            jmp count_memory

            print_memory_count:
                add EBX, 100000h
                mov dword ptr [memory_count], EBX         ; Добавим посчитанную память в байтах

                mov EAX, [memory_count]     ;
                xor EDX, EDX                ;
                mov EBX, 100000h            ; Байт в мегабайте
                div EBX                     ; EAX = EAX / EBX, в EAX память в МБ

                mov SI, offset memory_str + 21   ; Работа с младшей половиной EAX
                call wrd_asc                     ; Преобразовать AX в символ

                shr EAX, 16
                mov SI, offset memory_str + 16
                call wrd_asc
                
                mov SI, offset memory_str
                mov CX, memory_str_len  ; Длина строки
                mov AH, 40h             ; Атрибут символа
                mov DI, memory_str_output   ; Начальная позиция на экране

                mem_scrn:
                    lodsb
                    stosw
                loop mem_scrn

            endless_loop:
                cmp exit_flag, 1
                je home
                jmp endless_loop

            
        home:
        cli                 ; Запретить аппаратные прерывания

        ; ----- Конец основной программы -----
        
        ; Закрыть линию A20
        mov AL, 0D1h                ; Команда управления линией A20
        out 64h, AL                 ;
        mov AL, 0DDh                ; Код закрытия
        out 60h, AL                 ;
        
        ; Возврат в реальный режим
        mov gdt_data.lim, 0FFFFh
        mov gdt_code32.lim, 0FFFFh
        mov gdt_stack.lim, 0FFFFh
        mov gdt_screen32.lim, 0FFFFh
        push DS
        pop DS
        push SS
        pop SS
        push ES
        pop ES

        db 0EAh
        dw offset go
        dw 16

        go:
        mov EAX, CR0
        and EAX, 0FFFFFFFEh
        mov CR0, EAX
        db 0EAh
        dw offset return_to_real
        dw text
    
    wrd_asc proc
    pusha
    mov BX, 0F000h
    mov DL, 12
    mov CX, 4

    cccc:
        push CX
        push AX
        and AX, BX
        mov CL, DL
        shr AX, CL
        call bin_asc
        mov byte ptr [SI], AL
        inc SI
        pop AX
        shr BX, 4
        sub DL, 4
        pop CX
    loop cccc

    popa
    ret
    wrd_asc endp

bin_asc proc
    cmp AL, 9       ; Цифра > 9?
    ja lettr        ; Да, преобразуем в букву
    add AL, 30h     ; Нет, преобразуем в символ 0...9
    jmp ok          ; Выход из подпрограммы

    lettr:
        add AL, 37h
    ok:
        ret
bin_asc endp

text32_size = $ - text32seg
text32 ends
stk segment stack use16
    db 256 dup('^')
stk ends
    end main
