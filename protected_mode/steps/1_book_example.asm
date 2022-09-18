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

; ----- Сегмент данных -----
data segment use16      ; 16-разрядный сегмент

    ; Таблица глобальных дескрипторов - GDT
    gdt_null descriptor_segment <0, 0, 0, 0, 0, 0>		               ; Селектор 0, нулевой селектор
    gdt_data descriptor_segment <data_size - 1, 0, 0, 92h, 0 ,0>       ; Селектор 8, сегмент данных
    gdt_code descriptor_segment <code_size - 1, 0, 0, 98h, 0, 0>       ; Селектор 16, сегмент кода
    gdt_stack descriptor_segment <255, 0, 0, 92h, 0, 0>                ; Селектор 24, сегмент стека
    gdt_screen descriptor_segment <3999, 8000h, 0Bh, 92h, 0, 0>        ; Селектор 32, видеопамять
    gdt_size = $ - gdt_null     ; Размер GDT

    ; Данные программы
    pdescr  df 0        ; Псевдодескриптор для команды lgdt - информация о GDT // df - 6 байт
    sym     db 1        ; Символ для вывода на экран
    attr    db 1Eh      ; Атрибут символа
    msg     db 'asdasd$'   ; Сообщение при возвращении в реальный
    data_size = $ - gdt_null    ; Размер сегмента данных, вычисляемый на этапе трансляции

data ends

;  ----- Сегмент кода -----
text segment use16      ; 16-разрядный сегмент
    assume CS:text, DS:data
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
    shr EAX, 16                ;
    mov [BX].base_m, AL         ;


    ; Вычисление 32-битного линейного адреса сегмента СТЕКА в EAX
    ; Загрузка в GDT
    xor EAX, EAX                ;
    mov AX, SS                  ;
    shl EAX, 4                  ;
    mov BX, offset gdt_stack    ;
    mov [BX].base_l, AX         ;
    shr EAX, 16                ;
    mov [BX].base_m, AL         ;

    ; Псевдодескриптор 
    mov dword ptr pdescr + 2, EBP       ; База GDT
    mov word ptr pdescr, gdt_size - 1  ; Граница GDT
    lgdt pdescr ; Загрузим регистр GDTR

    ; Подготовка к возврату из защищенного режима в реальный
    mov AX, 40h                             ;
    mov ES, AX                              ;
    mov word ptr ES:[67h], offset return    ;
    mov ES:[69h], CS                        ;
    mov AL, 0Fh                             ;
    out 70h, AL                             ;
    mov AL, 0Ah                             ;
    out 71h, AL                             ;
    cli

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
    mov AX,24
    mov SS, AX
    ; Инициализируем ES
    mov AX, 32
    mov ES, AX
    ; Вывести на экран тестовую строку символов
    mov DI, 1920    ; Начальная позиция на экране
    mov CX, 10      ; Число выводимых символов
    mov AX, word ptr sym    ; Символ + атрибут
    scrn:   stosw   ; Содержимое AX выводится на экран
        inc AL          ; Инкремент кода символа
        loop scrn       ; Цикл
    
    ; Возврат в реальный режим
    mov AL, 0FEh    ; Команда сброса процессора
    out 64h, AL     ; в порт 64h
    hlt             ; Остановка процессора до окончания сброса

    ; ----- Вновь в реальном режиме -----
    return:
    ; Восстановить вычислительную среду реального режима
    mov AX, data
    mov DS, AX
    mov AX, stk
    mov SS, AX
    mov SP, 256
    sti ; Разрешить аппаратные прерывания

    ; Работа в DOS - завершить программу
	mov AH, 09h
	mov DX, offset msg
	int 21h
    mov AX, 4C00h
    int 21h

main endp

code_size = $ - main
text ends

stk segment stack use16
    db 256 dup('^')
stk ends
    end main
