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
; ----- Структура длч описания шлюзов ловушек ----- ;
descriptor_trap struc
    offset_1    dw  0       ; Смещение (0...15)
    sel         dw  16      ; Селектор (0...15)
    cntr        db  0       ; Не используется
    dtype       db  8Fh     ; Тип шлюза - ловушка 80386 и выше
    offs_h      dw  0       ; Смещение (16...31)
descriptor_trap ends

; ----- Сегмент данных -----
data segment use16      ; 16-разрядный сегмент

    ; Таблица глобальных дескрипторов - GDT
    gdt_null descriptor_segment <0, 0, 0, 0, 0, 0>		               ; Селектор 0, нулевой селектор
    gdt_data descriptor_segment <data_size - 1, 0, 0, 92h, 0 ,0>       ; Селектор 8, сегмент данных
    gdt_code descriptor_segment <code_size - 1, 0, 0, 98h, 0, 0>       ; Селектор 16, сегмент кода
    gdt_stack descriptor_segment <255, 0, 0, 92h, 0, 0>                ; Селектор 24, сегмент стека
    gdt_screen descriptor_segment <3999, 8000h, 0Bh, 92h, 0, 0>        ; Селектор 32, видеопамять
    
    gdt_size = $ - gdt_null     ; Размер GDT

    idt label word
        descriptor_trap 13 dup (<dummy>) ; Дескрипторы исключений 0...12
        descriptor_trap <exc13>
        descriptor_trap 18 dup (<dummy>) ; Дескрипторы исключений 14...31
        descriptor_trap <new_08, , 8Eh>  ; Дескриптор прерывания от таймера
        descriptor_trap <new_09, , 8EH>  ; Дескриптор прерывания от клавиатуры
    idt_size = $ - idt

    ; Данные программы
    pdescr  df 0        ; Псевдодескриптор для команды lgdt - информация о GDT // df - 6 байт
    sym     db 1        ; Символ для вывода на экран
    attr    db 1Eh      ; Атрибут символа
    msg     db 'welcome back to real mode$' ; Сообщение при возвращении в реальный
    string db '**** ****-**** ****-**** ****' ; Шаблон диагностической строки
    len = $ - string
    mark_08 dw 1600     ; Позиция для вывода из new_8
    time_08 db 0        ; Счетчик прерываний
    master db 0         ; Маска прерываний ведущего контроллера
    slave db 0         ; Маска прерываний ведомого контроллера
    data_size = $ - gdt_null    ; Размер сегмента данных, вычисляемый на этапе трансляции

data ends

;  ----- Сегмент кода -----
text segment use16      ; 16-разрядный сегмент
    assume CS:text, DS:data
textseg label word
    exc13 proc
        pop EAX             ; Код ошибки
        pop EAX             ; EIP в точке исключения
        mov SI, offset string + 5
        call wrd_asc
        mov AX, 13
        jmp home
    exc13 endp
    dummy proc
        mov AX, 5555h
        jmp home
    dummy endp
    new_08 proc
        ; Сохранение аппаратного контекста
        push AX
        push BX
        test time_08, 03    ; Снизить частоту пересчетом на 4
        jnz skip
        mov AL, 21h         ; Символ "!"
        mov AH, 71h         ; Цвет символа
        mov BX, mark_08     ; Позиция на экране
        mov ES:[BX], AX     ; Отправить символ в видеопамять
        add mark_08, 2      ; Смещение по экрану

        skip:
        inc time_08         ; Инекремент счетчика прерываний
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

        in AL, 60h  ; Получить введенный символ
        in AL, 61h  ; Получить содержимое порта B
        or AL, 80h  ; Установкой старшего бита
        out 61h, AL ; И последующим сбросом его
        and AL, 7Fh ; Сообщим контроллеру о
        out 61h, AL ; Приеме скан-кода символа
        mov AL, 20h ; EOI ведомого контроллера
        out 20h, AL ; Команда подвтерждения завершения обработки прерывания

        ; Восстановление аппаратного контекста
        pop AX
        db 66h
        iret        ; Возврат в программу
    new_09 endp
main proc
    xor EAX, EAX        ; EAX = 0
    mov AX, data
    mov DS, AX          ; DS = адрес сегмента данных

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
    mov AX, CS                  ;
    shl EAX, 4                  ;
    mov BX, offset gdt_code     ;
    mov [BX].base_l, AX         ;
    shr EAX, 16                 ;
    mov [BX].base_m, AL         ;


    ; Вычисление 32-битного линейного адреса сегмента СТЕКА в EAX
    ; Загрузка в GDT
    xor EAX, EAX                ;
    mov AX, SS                  ;
    shl EAX, 4                  ;
    mov BX, offset gdt_stack    ;
    mov [BX].base_l, AX         ;
    shr EAX, 16                 ;
    mov [BX].base_m, AL         ;

    ; Загрузка GDTR
    mov dword ptr pdescr + 2, EBP       ; База GDT
    mov word ptr pdescr, gdt_size - 1  ; Граница GDT
    lgdt pdescr ; Загрузим регистр GDTR
    cli

    ; Сохранить маски прерываний контроллеров
    in AL, 21h
    mov master, AL  ; Ведущий
    in AL, 0A1h
    mov slave, AL   ; Ведомый

    ; Инициализируем ведущий контроллер, базовый вектор - 32
    mov AL, 11h     ; СКИ1: будет СКИ3
    out 20h, AL
    mov AL, 32      ; СКИ2: базовый вектор
    out 21h, AL
    mov AL, 4       ; СКИ3: едомый подключен к уровню 2
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
    ; Загрузка в CS:IP селектор:смещение точки continue
        db 0EAh             ; Код команды far jmp
        dw offset continue  ; Смещение
        dw 16               ; Селектор сегмента кода
    continue:
    ; Делаем адресуемыми данные
    mov AX, 8
    mov DS, AX
    ; Делаем адресуемым стек
    mov AX, 24
    mov SS, AX
    ; Инициализируем ES - видеопамять
    mov AX, 32
    mov ES, AX

    sti                 ; Разрешить аппаратные прерывания

    mov DI, 1920        ; Позиция на экране
    mov CX, 320          ; Число выводимых символов
    mov AX, 1E01h       ; Символ + атрибут
    scrn:
        stosw       ; Вывести содержимое AX на экран
        inc AL      ; Инкремент символа
        push CX

        mov ECX, 200000 ; Задержка
        delay:
        db 67h
        loop delay

        pop CX
    loop scrn

    ; Диагностическая строка
    mov AX, 0FFFFh  ; Условный код нормального завершения этой части

    home:
        mov SI, offset string
        call wrd_asc    ; AX в символьную строку
        ; Вывод диагностической строки
        mov SI, offset string
        mov CX, len
        mov AH, 74h
        mov DI, 1280
    
    scrn1:
        lodsb
        stosw
    loop scrn1

    cli
    
    ; Закрыть линию A20
    mov AL, 0D1h                ; Команда управления линией A20
    out 64h, AL                 ;
    mov AL, 0DDh                ; Код закрытия
    out 60h, AL                 ;
    
    ; Возврат в реальный режим
    mov gdt_data.lim, 0FFFFh
    mov gdt_code.lim, 0FFFFh
    mov gdt_stack.lim, 0FFFFh
    mov gdt_screen.lim, 0FFFFh
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
    dw offset return
    dw text

    ; ----- Вновь в реальном режиме -----
    return:
    ; Восстановить вычислительную среду реального режима
    mov AX, data
    mov DS, AX
    mov AX, stk
    mov SS, AX
    mov SP, 256

    ; Восстановить состояние регистра IDTR реального режима
    mov AX, 3FFh
    mov word ptr pdescr, AX
    mov EAX, 0
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
    mov AL, master
    out 21h, AL
    mov AL, slave
    out 0A1h, AL
    sti

    ; Работа в DOS - завершить программу
    mov AX, 3
    int 10h
	mov AH, 09h
	mov DX, offset msg
	int 21h
    mov AX, 4C00h
    int 21h

main endp

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

code_size = $ - main
text ends

stk segment stack use16
    db 256 dup('^')
stk ends
    end main
