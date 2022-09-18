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
    
    gdt_himem descriptor_segment <511, 0, 10h, 92h, 80h, 0>             ; Селектор 40, старшая память

    gdt_size = $ - gdt_null     ; Размер GDT

    ; Данные программы
    pdescr  df 0        ; Псевдодескриптор для команды lgdt - информация о GDT // df - 6 байт
    sym     db 1        ; Символ для вывода на экран
    attr    db 1Eh      ; Атрибут символа
    msg     db 'welcome back to real mode$' ; Сообщение при возвращении в реальный
    string db '**** ****-**** ****'         ; Шаблон диагностической строки
    len = $ - string
    number db '???? ????'                   ; Поле для динамического контроля
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

    ; Псевдодескриптор 
    mov dword ptr pdescr + 2, EBP       ; База GDT
    mov word ptr pdescr, gdt_size - 1  ; Граница GDT
    lgdt pdescr ; Загрузим регистр GDTR
    cli

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
    
    ; Заполнить 2МБайт расширенной памяти
    mov AX, 40
    mov GS, AX              ; Селектор сегмента - 40
    xor EAX, EAX            ; Первое число-заполнитель
    xor EBX, EBX            ; Индекс в сегменте
    ; mov EAX, 0
    ; mov EBX, 0
    mov ECX, 80000h         ; 80000h * 4 = 200000h = 2Мбайт

    ; Заполняем память
    fill:
        mov GS:[EBX], EAX       ; Записываем в память EAX
        push EAX                ; Сохраним EAX
        push CX                 ; и CX
        mov SI, offset number + 5 ; Работа с младшей половиной EAX
        call wrd_asc            ; Преобразовать AX в символ

        shr EAX, 16             ; Сдвинуть, чтобы взять старшую половину EAX    
        mov SI, offset number   ; Работа со старшей половиной EAX
        call wrd_asc            ; Преобразование AX в символ

        mov CX, 9               ; Длина строки - счетчик цикла
        mov AH, 40h             ; Атрибут символа
        mov DI, 1760            ; Начальная позиция на экране

        scrnh:
            lodsb                   ; Чтение байта из строки (по адресу DS:SI в регистр AL)
            stosw                   ; Выведем в видеопамять слово в AX
        loop scrnh

        pop CX                  ; Восстановить CX
        pop EAX                 ; и EAX
        add EBX, 4              ; Перейти к следующему двойному слову памяти
        inc EAX                 ; Заполнить следующим символом

    db 67h                  ; Чтобы луп работал с ECX
    loop outc
    jmp ahead
    outc: 
        jmp fill

    ; Проверить заполненность 2 МБайт чтением первого и последнего слова
    ahead:
    xor EBX, EBX
    mov EBX, 0                  ; Смещение 1-ого слова
    mov EAX, GS:[EBX]           ; Записать слово в EAX
    mov SI, offset string + 5   ; Куда заслать символы
    call wrd_asc                ; Преобразовать в символы
    shr EAX, 16                 ; Получить старшую часть слова
    mov SI, offset string + 0   ; Куда заслать
    call wrd_asc                ; Преобразовать в символы
    mov EBX, 1FFFFCh            ; Смещение последнего слова
    mov EAX, GS:[EBX]           ; Считать слово
    mov SI, offset string + 15  ; Аналогично
    call wrd_asc
    shr EAX, 16
    mov SI, offset string + 10
    call wrd_asc

    ; Вывод диагностической строки
    mov SI, offset string
    mov CX, len
    mov AH, 74h
    mov DI, 1280
    scrnl:
        lodsb
        stosw
    loop scrnl
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
    sti ; Разрешить аппаратные прерывания

    ; Работа в DOS - завершить программу
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
