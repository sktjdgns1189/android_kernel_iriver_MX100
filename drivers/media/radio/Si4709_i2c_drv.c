
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include "Si4709_dev.h"
#include "Si4709_common.h"

/*extern functions*/
int Si4709_i2c_drv_init(void);
void Si4709_i2c_drv_exit(void);

/*static functions*/
static int Si4709_probe (struct i2c_client *);
static int Si4709_remove(struct i2c_client *);
static int Si4709_suspend(struct i2c_client *, pm_message_t mesg);
static int Si4709_resume(struct i2c_client *);
static int si4709_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);

static struct i2c_driver Si4709_i2c_driver;

struct i2c_device_id Si4709_idtable[] = {
       { "Si4709", 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, Si4709_idtable);

static struct i2c_driver Si4709_i2c_driver =
{
    .driver = {
        .name = "Si4709_driver",
    },

	.id_table	= Si4709_idtable,
	.probe		= &si4709_i2c_probe,
	.remove		= __devexit_p(&Si4709_remove),
    .suspend = &Si4709_suspend,
    .resume = &Si4709_resume,
};

static int Si4709_probe (struct i2c_client *client)
{
    int ret = 0;

    debug("Si4709 i2c driver Si4709_probe called"); 

    if( strcmp(client->name, "Si4709") != 0 )
    {
        ret = -1;
        debug("Si4709_probe: device not supported");
    }
    else if( (ret = Si4709_dev_init(client)) < 0 )
    {
        debug("Si4709_dev_init failed");
    }

    return ret;
}

struct Si4709_state {
	struct i2c_client	*client;
};

static int si4709_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct Si4709_state *state;
	struct device *dev = &client->dev;

	state = kzalloc(sizeof(struct Si4709_state), GFP_KERNEL);
	if (state == NULL) {
		dev_err(dev, "failed to create our state\n");
		return -ENOMEM;
	}

	state->client = client;
	i2c_set_clientdata(client, state);

	/* rest of the initialisation goes here. */

    Si4709_probe(client);

	return 0;
}

static int __devexit Si4709_remove(struct i2c_client *client)
{
	struct Si4709_state *state = i2c_get_clientdata(client);

	kfree(state);
	return 0;
}

static int Si4709_suspend(struct i2c_client *client, pm_message_t mesg)
{
    int ret = 0;
	   
    debug("Si4709 i2c driver Si4709_suspend called"); 

    if( strcmp(client->name, "Si4709") != 0 )
    {
        ret = -1;
        debug("Si4709_suspend: device not supported");
    }
    else if( (ret = Si4709_dev_suspend()) < 0 )
    {
        debug("Si4709_dev_disable failed");
    }

    return 0;
}

static int Si4709_resume(struct i2c_client *client)
{
    int ret = 0;
	   
    debug("Si4709 i2c driver Si4709_resume called"); 

    if( strcmp(client->name, "Si4709") != 0 )
    {
        ret = -1;
        debug("Si4709_resume: device not supported");
    }
    else if( (ret = Si4709_dev_resume()) < 0 )
    {
        debug("Si4709_dev_enable failed");
    }
 
    return 0;
}

int Si4709_i2c_drv_init(void)
{	
    int ret = 0;

    debug("Si4709 i2c driver Si4709_i2c_driver_init called"); 

    if ( (ret = i2c_add_driver(&Si4709_i2c_driver) < 0) ) 
    {
        error("Si4709 i2c_add_driver failed");
    }

    return ret;
}

void Si4709_i2c_drv_exit(void)
{
    debug("Si4709 i2c driver Si4709_i2c_driver_exit called"); 

    i2c_del_driver(&Si4709_i2c_driver);
}


