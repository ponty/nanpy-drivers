#define DEBUG

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#define NANPY_BUFFER_SIZE   63
#define NANPY_TIMEOUT_MS    20
#define SERIO_NANPY 100
#define SLEEP_AFTER_CONNECT_MS  2000


#define NANPY_STATE_INIT		0
#define NANPY_STATE_IDLE		1
#define NANPY_STATE_RECV		3

static DECLARE_WAIT_QUEUE_HEAD(wq);

struct nanpy_data {
    struct gpio_chip gpio_client;
    struct i2c_adapter adapter;
	struct serio* serio;
    int state;
	unsigned char read_buffer[NANPY_BUFFER_SIZE];
	unsigned int read_pos;	/* position inside the buffer */
//	struct mutex            access_lock;
};

#define RETURN_IF_ERROR(CALL)  {int ireply=CALL; if(ireply<0) return ireply;}
#define SET_VAR_RETURN_IF_ERROR(VAR, CALL)  {int ireply=CALL; if(ireply<0) return ireply;VAR=ireply;}

#define TRACE_CALL
//#define TRACE_CALL  printk("---nanpy--- %s \n", __FUNCTION__)

static int nanpy_send_buffer(struct nanpy_data *nanpy, char *buffer)
{
    char *p;
    int i;

    /* Send the transaction to nanpy */
    dev_dbg(&nanpy->serio->dev, "Command buffer: %s\n", buffer);

//    mutex_lock(&nanpy->access_lock);

    for (p = buffer; *p; p++)
    {
        char c = *p;
        if(c==' ')
            c = 0;
        serio_write(nanpy->serio, c);
    }

    /* Start the transaction and read the answer */
    nanpy->read_pos = 0;
    nanpy->state = NANPY_STATE_RECV;
    wait_event_interruptible_timeout(wq, nanpy->state == NANPY_STATE_IDLE,
                     msecs_to_jiffies(NANPY_TIMEOUT_MS));
    if (nanpy->state != NANPY_STATE_IDLE)
    {
//        mutex_unlock(&nanpy->access_lock);
        dev_err(&nanpy->serio->dev, "Transaction timeout (pos=%d, timeout=%d)\n",
            nanpy->read_pos, NANPY_TIMEOUT_MS);
        return -EIO;
    }
    dev_dbg(&nanpy->serio->dev, "Answer buffer: %s\n", nanpy->read_buffer);

    i=simple_strtol(nanpy->read_buffer,NULL,10);
//    mutex_unlock(&nanpy->access_lock);

    return i;
}

static irqreturn_t nanpy_interrupt(struct serio *serio, unsigned char data,
				  unsigned int flags)
{
	struct nanpy_data *nanpy = serio_get_drvdata(serio);

	switch (nanpy->state) {
	case NANPY_STATE_RECV:
        if(nanpy->read_pos >= NANPY_BUFFER_SIZE)
        {
            dev_err(&nanpy->serio->dev, "nanpy: Buffer overflow!\n");
            nanpy->read_pos = 0;
        }

        if (data == '\r')
        {
            nanpy->read_buffer[nanpy->read_pos] = '\0';
        }
        else if (data == '\n')
        {
            nanpy->read_buffer[nanpy->read_pos] = '\0';
            nanpy->state = NANPY_STATE_IDLE;
            wake_up_interruptible(&wq);
        }
        else
        {
            nanpy->read_buffer[nanpy->read_pos] = data;
        }
        nanpy->read_pos++;
		break;
	}

	return IRQ_HANDLED;
}

/*---------------------------  SEND TELEGRAM ----------------------------*/
#define   NANPY_TELEGRAM_WIRE_BEGIN                 "Wire 0 0 begin "
#define   NANPY_TELEGRAM_WIRE_BEGIN_TRANSMISSION    "Wire 0 1 beginTransmission %d "
#define   NANPY_TELEGRAM_WIRE_WRITE                 "Wire 0 1 write %d "
#define   NANPY_TELEGRAM_WIRE_ENDTRANSMISSION       "Wire 0 1 endTransmission %d "
#define   NANPY_TELEGRAM_WIRE_REQUESTFROM           "Wire 0 3 requestFrom %d %d %d "
#define   NANPY_TELEGRAM_WIRE_READ                  "Wire 0 0 read "

#define   NANPY_TELEGRAM_DIGITAL_WRITE              "A 0 2 dw %d %d "
#define   NANPY_TELEGRAM_DIGITAL_READ               "A 0 1 r %d "
#define   NANPY_TELEGRAM_PINMODE                    "A 0 2 pm %d %d "

static int digitalWrite(struct nanpy_data *nanpy, u8 pin, u8 value)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_DIGITAL_WRITE, pin, value);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=0)
    {
        dev_err(&nanpy->serio->dev, "Wrong answer %s (%d)\n", __FUNCTION__, i);
        return -EIO;
    }

    return 0;
}
static int digitalRead(struct nanpy_data *nanpy, u8 pin)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_DIGITAL_READ, pin);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    return i;
}
static int pinMode(struct nanpy_data *nanpy, u8 pin, u8 mode)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_PINMODE, pin, mode);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=0)
    {
        dev_err(&nanpy->serio->dev, "Wrong answer %s (%d)\n", __FUNCTION__, i);
        return -EIO;
    }

    return 0;
}





static int Wire_begin(struct nanpy_data *nanpy)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_BEGIN);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=0)
    {
        dev_err(&nanpy->serio->dev, "Wrong answer %s (%d)\n", __FUNCTION__, i);
        return -EIO;
    }

    return 0;
}
static int Wire_beginTransmission(struct nanpy_data *nanpy, u16 addr)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_BEGIN_TRANSMISSION, addr);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=0)
    {
        dev_err(&nanpy->serio->dev, "Wrong answer %s (%d)\n", __FUNCTION__, i);
        return -EIO;
    }

    return 0;
}
static int Wire_write(struct nanpy_data *nanpy, u8 byte)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_WRITE, byte);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=1)
    {
        dev_err(&nanpy->serio->dev, "Wrong answer %s (%d)\n", __FUNCTION__, i);
        return -EIO;
    }

    return 0;
}
static int Wire_endTransmission(struct nanpy_data *nanpy, bool stop)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_ENDTRANSMISSION, stop ? 1 : 0);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

//    0:success
//    1:data too long to fit in transmit buffer
//    2:received NACK on transmit of address
//    3:received NACK on transmit of data
//    4:other error
    if (i)
    {
        if (i==1)
            dev_warn(&nanpy->serio->dev, "data too long to fit in transmit buffer (%d)\n", i);
        if (i==2)
            dev_warn(&nanpy->serio->dev, "received NACK on transmit of address (%d)\n", i);
        if (i==3)
            dev_warn(&nanpy->serio->dev, "received NACK on transmit of data (%d)\n", i);
        if (i==4)
            dev_warn(&nanpy->serio->dev, "other error (%d)\n", i);
        return -EIO;
    }

    return 0;
}
static int Wire_requestFrom(struct nanpy_data *nanpy, u16 addr, u8 quantity, bool stop)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_REQUESTFROM, addr, quantity, stop ? 1 : 0);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    if (i!=quantity)
    {
        dev_err(&nanpy->serio->dev, "error in arduino requestFrom, quantity requested:%d, got:%d\n", quantity, i);
        return -EIO;
    }

    return 0;
}
static int Wire_read(struct nanpy_data *nanpy)
{
    int i;
    char buffer[NANPY_BUFFER_SIZE];

    sprintf(buffer, NANPY_TELEGRAM_WIRE_READ);

    SET_VAR_RETURN_IF_ERROR( i, nanpy_send_buffer(nanpy, buffer) );

    return i;
}

/*---------------------------- GPIO ---------------------------------------------*/

static int nanpy_gpio_input(struct gpio_chip *chip, unsigned offset)
{
    struct nanpy_data *nanpy = container_of(chip, struct nanpy_data, gpio_client);

    RETURN_IF_ERROR( pinMode(nanpy, offset, 0) );
    return 0;
}
static int nanpy_gpio_output(struct gpio_chip *chip, unsigned offset, int value)
{
    struct nanpy_data *nanpy = container_of(chip, struct nanpy_data, gpio_client);
    RETURN_IF_ERROR( pinMode(nanpy, offset, 1) );
    return 0;
}

static int nanpy_gpio_get(struct gpio_chip *chip, unsigned offset)
{
    struct nanpy_data *nanpy = container_of(chip, struct nanpy_data, gpio_client);
    int     value;
    value = digitalRead(nanpy, offset);
    return (value < 0) ? 0 : value;
}

static void nanpy_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
    struct nanpy_data *nanpy = container_of(chip, struct nanpy_data, gpio_client);
    digitalWrite(nanpy, offset, value);
}



/*-------------I2C--------------------*/

static int nanpy_i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
               unsigned short flags, char read_write, u8 command,
               int size, union i2c_smbus_data *data)
{
    struct serio *serio = adapter->algo_data;
    struct nanpy_data *nanpy = serio_get_drvdata(serio);
    TRACE_CALL;

    if (read_write == I2C_SMBUS_WRITE)
    {
        RETURN_IF_ERROR( Wire_beginTransmission(nanpy, addr) );
        switch (size) {
        case I2C_SMBUS_QUICK:
            break;
        case I2C_SMBUS_BYTE:
            RETURN_IF_ERROR( Wire_write(nanpy, command) );
            break;
        case I2C_SMBUS_BYTE_DATA:
            RETURN_IF_ERROR( Wire_write(nanpy, command) );
            RETURN_IF_ERROR( Wire_write(nanpy, data->byte) );
            break;
        case I2C_SMBUS_WORD_DATA:
            RETURN_IF_ERROR( Wire_write(nanpy, command) );
            RETURN_IF_ERROR( Wire_write(nanpy, data->word & 0xff) );
            RETURN_IF_ERROR( Wire_write(nanpy, (data->word & 0xff00) >> 8) );
            break;
        default:
            dev_warn(&nanpy->serio->dev, "Unsupported transaction %d\n", size);
            return -EOPNOTSUPP;
        }
        RETURN_IF_ERROR( Wire_endTransmission(nanpy, true) );
    }
    else if (read_write == I2C_SMBUS_READ)
    {
        switch (size) {
        case I2C_SMBUS_QUICK:
            // TODO:
            dev_err(&nanpy->serio->dev, "Unsupported !!! \n");
            break;
        case I2C_SMBUS_BYTE:
            RETURN_IF_ERROR( Wire_requestFrom(nanpy, addr, 1, true) );
            SET_VAR_RETURN_IF_ERROR(data->byte, Wire_read(nanpy) );
            break;
        case I2C_SMBUS_BYTE_DATA:
            RETURN_IF_ERROR( Wire_beginTransmission(nanpy, addr) );
            RETURN_IF_ERROR( Wire_write(nanpy, command) );
            RETURN_IF_ERROR( Wire_endTransmission(nanpy, false) );
            RETURN_IF_ERROR( Wire_requestFrom(nanpy, addr, 1, true) );
            SET_VAR_RETURN_IF_ERROR(data->byte, Wire_read(nanpy) );
            break;
        case I2C_SMBUS_WORD_DATA:
            RETURN_IF_ERROR( Wire_beginTransmission(nanpy, addr) );
            RETURN_IF_ERROR( Wire_write(nanpy, command) );
            RETURN_IF_ERROR( Wire_endTransmission(nanpy, false) );
            RETURN_IF_ERROR( Wire_requestFrom(nanpy, addr, 2, true) );
            {
                u8 b0,b1;
                SET_VAR_RETURN_IF_ERROR(b0, Wire_read(nanpy) );
                SET_VAR_RETURN_IF_ERROR(b1, Wire_read(nanpy) );
                data->word = b0 + (b1 << 8);
            }
            break;
        default:
            dev_warn(&nanpy->serio->dev, "Unsupported transaction %d\n", size);
            return -EOPNOTSUPP;
        }
    }
    else
    {
        dev_warn(&nanpy->serio->dev, "Unsupported transaction\n");
        return -EOPNOTSUPP;
    }
    return 0;
}

static u32 nanpy_i2c_smbus_func(struct i2c_adapter *adapter)
{
    TRACE_CALL;
    return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE
            | I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA
            ;
}

static const struct i2c_algorithm nanpy_i2c_algorithm = {
    .smbus_xfer = nanpy_i2c_smbus_xfer,
    .functionality  = nanpy_i2c_smbus_func,
};

/*-------------------------------------------------------*/


static int nanpy_connect(struct serio *serio, struct serio_driver *drv)
{
	struct nanpy_data *nanpy;
	int err;
    struct i2c_adapter *adapter;
    struct gpio_chip *gpio_client;
    TRACE_CALL;

    nanpy = kzalloc(sizeof(struct nanpy_data), GFP_KERNEL);
	if (!nanpy) {
		err = -ENOMEM;
		goto exit;
	}
	nanpy->state = NANPY_STATE_INIT;
	serio_set_drvdata(serio, nanpy);
    nanpy->serio = serio;

//    mutex_init(&nanpy->access_lock);

	err = serio_open(serio, drv);
	if (err)
		goto exit_kfree;

	dev_info(&serio->dev, "Sleeping %d msec..\n", SLEEP_AFTER_CONNECT_MS);
	msleep(SLEEP_AFTER_CONNECT_MS);

	// GPIO
	//  http://lxr.free-electrons.com/source/include/linux/gpio/driver.h#L72
	gpio_client = &nanpy->gpio_client;
	gpio_client->owner = THIS_MODULE;
    gpio_client->label            = serio->phys;
    gpio_client->direction_input  = nanpy_gpio_input;
    gpio_client->direction_output = nanpy_gpio_output;
    gpio_client->get              = nanpy_gpio_get;
    gpio_client->set              = nanpy_gpio_set;
    gpio_client->base             = -1;
    gpio_client->can_sleep        = 1;
    gpio_client->ngpio            = 14;
    err = gpiochip_add(gpio_client);
    if (err)
        goto exit_close;

	// I2C
    adapter = &nanpy->adapter;
    adapter->owner = THIS_MODULE;
    adapter->algo = &nanpy_i2c_algorithm;
    adapter->algo_data = serio;
    adapter->dev.parent = &serio->dev;
    sprintf(adapter->name, "nanpy on %s", serio->phys);

    err = i2c_add_adapter(adapter);
    if (err)
        goto exit_gpio;

    err = Wire_begin(nanpy);
    if (err)
        goto exit_i2c;

    dev_info(&serio->dev, "Connected to nanpy\n");

    return 0;

 exit_i2c:
    i2c_del_adapter(&nanpy->adapter);
 exit_gpio:
    gpiochip_remove(&nanpy->gpio_client);
 exit_close:
	serio_close(serio);
 exit_kfree:
	kfree(nanpy);
 exit:
	return err;
}

static void nanpy_disconnect(struct serio *serio)
{
    int err;
	struct nanpy_data *nanpy = serio_get_drvdata(serio);

    TRACE_CALL;

//    TODO: how to check status? no return value!
    err = gpiochip_remove(&nanpy->gpio_client);
    if (err)
    {
        dev_err(&nanpy->serio->dev, "nanpy: unhandled error, reboot! (gpio released??) (gpiochip_remove error:%d) \n", err);
    }

    i2c_del_adapter(&nanpy->adapter);
	serio_close(serio);
//	mutex_destroy(&nanpy->access_lock);
	kfree(nanpy);

	dev_info(&serio->dev, "Disconnected from nanpy\n");
}


static struct serio_device_id nanpy_serio_ids[] = {
	{
		.type	= SERIO_RS232,
		.proto	= SERIO_NANPY,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};
MODULE_DEVICE_TABLE(serio, nanpy_serio_ids);

static struct serio_driver nanpy_drv = {
	.driver		= {
		.name	= "nanpy",
	},
	.description	= "nanpy module driver",
	.id_table	= nanpy_serio_ids,
	.connect	= nanpy_connect,
	.disconnect	= nanpy_disconnect,
	.interrupt	= nanpy_interrupt,
};

static int __init nanpy_init(void)
{
	TRACE_CALL;
	return serio_register_driver(&nanpy_drv);
}

static void __exit nanpy_exit(void)
{
    TRACE_CALL;
	serio_unregister_driver(&nanpy_drv);
}

MODULE_AUTHOR("ponty");
MODULE_DESCRIPTION("nanpy module driver");
MODULE_LICENSE("GPL");

module_init(nanpy_init);
module_exit(nanpy_exit);
