#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input/mt.h>

#include "wacom.h"
#include "hid_parse.h"
#include "pen_handler.h"
#include "touch_handler.h"

#define DRIVER_AUTHOR  "Shatskiy Rostislav"
#define DRIVER_DESC    "Driver for Wacom cth-460"
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);


static const struct wacom_features wacom_features_0xD1 = {
    .name = "Wacom Bamboo 2FG 4x5",
    .pktlen = WACOM_PKGLEN_BBFUN,
    .x_max = 14720,
    .y_max = 9200,
    .pressure_max = 1023,
    .distance_max = 31,
    .type = BAMBOO_PT,
    .x_resolution = WACOM_INTUOS_RES,
    .y_resolution = WACOM_INTUOS_RES,
    .touch_max = 2
};

static struct workqueue_struct *workq;

static void work_irq(struct work_struct *work) {
//    static atomic_long_t sum_time = { 0 };
//    static atomic_t cnt = { 0 };

//    u64 tbegin = ktime_get_ns(), tend;
    struct container_urb *container = container_of(work, struct container_urb, work);
    struct urb *urb;
    struct tablet *tablet;
    int retval;
    int sync = 0;

    if (container == NULL) {
        printk(KERN_ERR "%s: %s - container is NULL\n", DRIVER_NAME, __func__);
        return;
    }

    urb = container->urb;
    if (urb->status != 0) {
        kfree(container);
        return;
    }

    tablet = urb->context;

    if (urb->actual_length == USB_PACKET_LEN) {
        sync = irq_touch(tablet);
    } else if (urb->actual_length == WACOM_PKGLEN_BBFUN) {
        sync = irq_pen(tablet, urb);
    }

    if (sync) {
        input_sync(tablet->input_dev);
    }

    retval = usb_submit_urb(urb, GFP_ATOMIC);
    if (retval)
        printk(KERN_ERR"%s: Error %d while performing usb_submit_urb in %s (line %d)\n", DRIVER_NAME, retval, __func__, __LINE__);

    kfree(container);
//    tend = ktime_get_ns();
//    sum_time.counter += tend - tbegin;
//    cnt.counter++;
//    printk("%s: Avg time is %lld ns in %d measurments", sum_time.counter / cnt.counter, cnt.counter);
}

static void tablet_irq(struct urb *urb) {
    struct container_urb *container = kzalloc(sizeof(struct container_urb), GFP_KERNEL);
    if (!container)
        return;
    container->urb = urb;

    INIT_WORK(&container->work, work_irq);
    queue_work(workq, &container->work);
}


static int tablet_open(struct input_dev *dev) {
    struct tablet *tablet = input_get_drvdata(dev);

    if (usb_autopm_get_interface(tablet->intf) < 0)
        return -EIO;

    if (usb_submit_urb(tablet->urb, GFP_KERNEL)) {
        usb_autopm_put_interface(tablet->intf);
        return -EIO;
    }

    usb_autopm_put_interface(tablet->intf);
    return 0;
}


static void tablet_close(struct input_dev *dev) {
    struct tablet *tablet = input_get_drvdata(dev);
    int autopm_error;

    autopm_error = usb_autopm_get_interface(tablet->intf);

    usb_kill_urb(tablet->urb);

    if (!autopm_error)
        usb_autopm_put_interface(tablet->intf);
}


/**
 * Подключение интерфейса устройства к драйверу
 *
 * @param interface - интерфейс usb устройства
 * @param id - идентификатор usb устройства
 * @return 0 в случае успеха, другое число - иначе
 */
static int tablet_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    // Получить usb_device из usb_interface
    struct usb_device *usb_device = interface_to_usbdev(interface);
    struct tablet *tablet;
    struct wacom_features *features;
    // Устройство ввода
    struct usb_endpoint_descriptor *endpoint;
    int error = -ENOMEM;

    // Выделение памяти под структуру tablet_t
    tablet = kzalloc(sizeof(struct tablet), GFP_KERNEL);
    if (!tablet) {
        printk(KERN_ERR"%s: Error in kzalloc in function %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    // Wacom device descriptor
    tablet->features = *((struct wacom_features *) id->driver_info);
    features = &tablet->features;
    if (features->pktlen > USB_PACKET_LEN) {
        printk(KERN_ERR"%s: Error - Wacom package length is more than defined USB_PACKET_LEN - %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    // Выделение памяти под информацию, приходящую от устройства
    tablet->data = usb_alloc_coherent(usb_device, USB_PACKET_LEN, GFP_KERNEL, &tablet->data_dma);
    if (!tablet->data) {
        kfree(tablet);
        printk(KERN_ERR "%s: error when allocate coherent\n", DRIVER_NAME);
        return error;
    }

    // https://elixir.bootlin.com/linux/latest/source/drivers/usb/core/urb.c#L71
    // Выделить память под новый usb request block
    // 0 - количество iso пакетов
    // GFP_KERNEL - тип выделяемой памяти
    tablet->urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!tablet->urb) {
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);
        tablet = NULL;

        printk(KERN_ERR "%s: Error while allocating urb in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    tablet->intf = interface;
    tablet->usb_dev = usb_device;

    // https://elixir.bootlin.com/linux/latest/source/include/linux/usb.h#L913
    // Создать путь к устройству в usb дереве
    usb_make_path(usb_device, tablet->phys, sizeof(tablet->phys));
    strlcat(tablet->phys, "/input0", sizeof(tablet->phys));

    // Дескриптор конечной точки
    endpoint = &interface->cur_altsetting->endpoint[0].desc;

    error = parse_hid_descriptor(interface, features);
    if (error) {
        usb_free_urb(tablet->urb);
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);
        tablet = NULL;

        printk(KERN_ERR "%s: Error while setup input_dev in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    // Макрос для инициализации прерывания usb request block'а
    // https://elixir.bootlin.com/linux/latest/source/include/linux/usb.h#L1687
    usb_fill_int_urb(
        tablet->urb, // urb =
        usb_device, // dev =
        usb_rcvintpipe(usb_device, endpoint->bEndpointAddress), // https://elixir.bootlin.com/linux/latest/source/include/linux/usb.h#L1906
        tablet->data, // transfer_buffer =
        features->pktlen, // buffer_length =
        tablet_irq, // complete_fn = указатель на usb_complete_t функцию typedef void (*usb_complete_t)(struct urb *);
        tablet, // context = во что будет установлен urb context
        endpoint->bInterval // interval = интервал usb request block'а
    );

    // https://elixir.bootlin.com/linux/latest/source/include/linux/usb.h#L1560
    tablet->urb->transfer_dma = tablet->data_dma; // адрес dma для transfer_buffer
    tablet->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP; // urb->transfer_dma valid on submit

    // Выделение памяти под структуру устройства ввода
    error = setup_input_dev(tablet, features, tablet_open, tablet_close);
    if (error) {
        usb_free_urb(tablet->urb);
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);
        tablet = NULL;

        printk(KERN_ERR "%s: Error while setup input_dev in %s (line %d)", DRIVER_NAME, __func__, __LINE__);
        return error;
    }

    if (features->device_type == BTN_TOOL_PEN) {
        wacom_set_report(usb_device, interface);
    }

    // Установить указатель на область памяти данных интерфейса устройства
    usb_set_intfdata(interface, tablet);

    printk(KERN_INFO "%s: device is connected\n", DRIVER_NAME);

    return 0;
}

static void tablet_disconnect(struct usb_interface *interface) {
    struct tablet *tablet = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    if (tablet) {
        usb_kill_urb(tablet->urb);
        input_unregister_device(tablet->input_dev);
        tablet->input_dev = NULL;
        usb_free_urb(tablet->urb);
        usb_free_coherent(interface_to_usbdev(interface), USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);

        printk(KERN_INFO "%s: Device was disconected\n", DRIVER_NAME);
    }
}

// Таблица идентификаторов драйвера
static struct usb_device_id tablet_table [] = {
    {
        USB_DEVICE(ID_VENDOR_TABLET, ID_PRODUCT_TABLET),
        .driver_info = (kernel_ulong_t)&wacom_features_0xD1
    },
    { },
};

// Экспорт таблицы идентификаторов устройств
MODULE_DEVICE_TABLE(usb, tablet_table);

/**
 * Экземпляр структуры для инициализации usb драйвера
 */
static struct usb_driver tablet_driver = {
    .name       = DRIVER_NAME, // Уникальное название драйвера
    .probe      = tablet_probe, // Функция, в которой проверяется, работает ли драйвер с интерфейсом устройства
    .disconnect = tablet_disconnect, // Вызывается, когда интерфейс становится не доступен
    .id_table   = tablet_table, // Таблица идентификаторов устройств, с которыми должен работаь драйвер.
                                // Необходимо экспортировать, используя MODULE_DEVICE_TABLE(usb, id_table);
    .supports_autosuspend = 1,
};


/**
 * Точка входа в модуль ядра
 */
static int __init touch_tablet_init(void) {
    int result = usb_register(&tablet_driver);
//    int i;
//    int j;

    if (result < 0) {
        printk(KERN_ERR "%s: Error on usb_register (line %d)", DRIVER_NAME, __LINE__);
        return result;
    }

    workq = create_workqueue("workqueue");
    if (workq == NULL) {
        printk(KERN_ERR "%s: Error on workqueue creation (line %d)", DRIVER_NAME, __LINE__);
        return -1;
    }

    printk(KERN_INFO"%s: Module loaded", DRIVER_NAME);
    return 0;
}

static void __exit touch_tablet_exit(void) {
    flush_workqueue(workq);
    destroy_workqueue(workq);
    usb_deregister(&tablet_driver);
    printk(KERN_INFO"%s: Module unloaded", DRIVER_NAME);
}

module_init(touch_tablet_init);
module_exit(touch_tablet_exit);
