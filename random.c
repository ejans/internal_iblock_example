/*
* microblx: embedded, real-time safe, reflective function blocks.
* Copyright (C) 2013,2014 Markus Klotzbuecher <markus.klotzbuecher@mech.kuleuven.be>
* Copyright (C) 2014 Evert Jans <evert.jans@kuleuven.be>
*
* microblx is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 or (at your option)
* any later version.
*
* microblx is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with eCos; if not, write to the Free Software Foundation,
* Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
*
* As a special exception, if other files instantiate templates or use
* macros or inline functions from this file, or you compile this file
* and link it with other works to produce a work based on this file,
* this file does not by itself cause the resulting work to be covered
* by the GNU General Public License. However the source code for this
* file must still be made available in accordance with section (3) of
* the GNU General Public License.
*
* This exception does not invalidate any other reasons why a work
* based on this file might be covered by the GNU General Public
* License.
*/

//#define DEBUG 1
#define UNSIGNED_INT "unsigned int"
#define STRUCT_RANDOM_CONFIG "struct random_config"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include "ubx.h"


/* declare and initialize a microblx type. This will be registered /
 * deregistered in the module init / cleanup at the end of this
 * file.
 *
 * Include regular header file and it's char array representation
 * (used for luajit reflection, logging, etc.)
 */
#include "types/random_config.h"
#include "types/random_config.h.hexarr"

/* declare the type and give the char array type representation as the type private_data */
ubx_type_t random_config_type = def_struct_type(struct random_config, &random_config_h);

/* function block meta-data
 * used by higher level functions.
 */
char rnd_meta[] =
	"{ doc='A random number generator function block',"
	"  real-time=true,"
	"}";

/* configuration
 * upon cloning the following happens:
 *   - value.type is resolved
 *   - value.data will point to a buffer of size value.len*value.type->size
 *
 * if an array is required, then .value = { .len=<LENGTH> } can be used.
 */
ubx_config_t rnd_config[] = {
	{ .name="min_max_config", .type_name = "struct random_config" },
	{ NULL },
};

/* Ports
 */
ubx_port_t rnd_ports[] = {
	{ .name="seed", .in_type_name="unsigned int" },
	{ .name="rnd", .out_type_name="unsigned int" },
	{ .name="local_in", .in_type_name="struct random_config" },
	{ .name="local_out", .out_type_name="struct random_config" },
	{ NULL },
};

/* convenience functions to read/write from the ports these fill a
 * ubx_data_t, and call port->[read|write](&data). These introduce
 * some type safety.
 */
def_read_fun(read_uint, unsigned int)
def_write_fun(write_uint, unsigned int)
def_read_fun(read_random_config, struct random_config)
def_write_fun(write_random_config, struct random_config)

/*
 * function to create local fifo as a data holder between
 * the different states.
 */
static int create_local_lfds(ubx_block_t *b) {
	DBG("start of create_local_lfds");
        int ret = 0;
        ubx_block_t *fifo;
        ubx_data_t *d,*d2,*d3;
        ubx_port_t *local_in, *local_out;
	ubx_module_t* mod;

	/* if fifo module is not loaded load it */
	DBG("loading fifo module/ create");
	HASH_FIND_STR(b->ni->modules, "std_blocks/lfds_buffers/lfds_cyclic.so", mod);

	if (mod == NULL) {
		ubx_module_load(b->ni, "std_blocks/lfds_buffers/lfds_cyclic.so");
	}

        fifo = ubx_block_create(b->ni, "lfds_buffers/cyclic", "local_fifo");

	/* set type_name */
	DBG("set type name");
        d = ubx_config_get_data(fifo, "type_name");
        int len =  strlen("struct random_config")+1;
        ubx_data_resize(d, len);
        strncpy((char*)d->data,STRUCT_RANDOM_CONFIG,len);

	/* set buffer_len */
	DBG("set buffer length");
        d2 = ubx_config_get_data(fifo, "buffer_len");
        d2->data = malloc(sizeof(uint32_t));
        *(uint32_t*)d2->data = (uint32_t) 1;
        
	/* set data_len */
	DBG("set data length");
        d3 = ubx_config_get_data(fifo, "data_len");
        d3->data = malloc(sizeof(uint32_t));
        *(uint32_t*)d3->data = (uint32_t) sizeof(struct random_config);
        
	/* add ports (something wrong with this) */
	/*
        if (ubx_port_add(b, "local_in", "local in port", "struct random_config", sizeof(struct random_config),0, 0, BLOCK_STATE_INACTIVE) != 0) {
		ERR("failed to create local_in port");
		return ret;
	}

        if (ubx_port_add(b, "local_out", "local out port", 0, 0,"struct random_config", sizeof(struct random_config), BLOCK_STATE_INACTIVE) != 0) {
		ERR("failed to create local_in port");
		return ret;
	}
	*/

	/* get ports */
	DBG("get ports");
        local_in = ubx_port_get(b, "local_in");
        local_out = ubx_port_get(b, "local_out");

	/* connect ports to internal fifo */
	DBG("connect ports");
	if (ubx_ports_connect_uni(local_out, local_in, fifo) != 0) {
		ERR("failed to connect ports to fifo");
		return ret;
	}

        /* init and start the block */
	DBG("init and start");
        if(ubx_block_init(fifo) != 0) {
                ERR("failed to init local fifo");
                return ret;
        }

        if(ubx_block_start(fifo) != 0) {
                ERR("failed to start local fifo");
                return ret;
        }
	DBG("Complete");
        return ret;
}

/**
 * rnd_init - block init function.
 *
 * for RT blocks: any memory should be allocated here.
 *
 * @param b
 *
 * @return Ok if 0,
 */
static int rnd_init(ubx_block_t *b)
{
	int ret=0;
	DBG(" ");
	if (create_local_lfds(b) != 0) {
                ERR("failed to run create_local_lfds");
		goto out;
	}
	DBG("rnd_init behind if");
 out:
	return ret;
}

/**
 * rnd_cleanup - cleanup block.
 *
 * for RT blocks: free all memory here
 *
 * @param b
 */
static void rnd_cleanup(ubx_block_t *b)
{
	DBG(" ");
	//free(b->private_data);
	ubx_block_t* fifo;
	ubx_port_t *local_in, *local_out;

	fifo = ubx_block_get(b->ni, "local_fifo");
	local_in = ubx_port_get(b, "local_in");
	local_out = ubx_port_get(b, "local_out");

	/* disconnect ports to internal fifo */
	DBG("disconnecting ports");
	if (ubx_ports_disconnect_uni(local_out, local_in, fifo) != 0) {
		ERR("failed to disconnect ports to fifo");
	}
	ubx_block_stop(fifo);
	ubx_block_cleanup(fifo);
	ubx_block_rm(b->ni, "local_fifo");
}

/**
 * rnd_start - start the random block.
 *
 * @param b
 *
 * @return 0 if Ok, if non-zero block will not be started.
 */
static int rnd_start(ubx_block_t *b)
{
	DBG("in");
	uint32_t seed, ret;
	unsigned int clen;
	struct random_config* conf;

	/* get and store min_max_config */
	conf = (struct random_config*) ubx_config_get_data_ptr(b, "min_max_config", &clen);
	ubx_port_t* local_out = ubx_port_get(b, "local_out");
	write_random_config(local_out, conf);

	/* seed is allowed to change at runtime, check if new one available */
	ubx_port_t* seed_port = ubx_port_get(b, "seed");
	ret = read_uint(seed_port, &seed);

	if(ret>0) {
		DBG("starting component. Using seed: %d, min: %d, max: %d", seed, conf->min, conf->max);
		srandom(seed);
	} else {
		DBG("starting component. Using min: %d, max: %d", conf->min, conf->max);
	}
	return 0; /* Ok */
}

/**
 * rnd_step - this function implements the main functionality of the
 * block. Ports are read and written here.
 *
 * @param b
 */
static void rnd_step(ubx_block_t *b) {
	unsigned int rand_val;
	struct random_config conf;

	ubx_port_t* local_in = ubx_port_get(b, "local_in");
	ubx_port_t* local_out = ubx_port_get(b, "local_out");
	DBG("Before read_random_config");
	read_random_config(local_in, &conf);
	DBG("conf.max: %i", conf.max);
	DBG("conf.min: %i", conf.min);

	ubx_port_t* rand_port = ubx_port_get(b, "rnd");
	rand_val = random();
	rand_val = (rand_val > conf.max) ? (rand_val%conf.max) : rand_val;
	rand_val = (rand_val < conf.min) ? ((conf.min + rand_val)%conf.max) : rand_val;
	
	/* write to local fifo */
	write_random_config(local_out, &conf);

	write_uint(rand_port, &rand_val);
}


/* put everything together
 *
 */
ubx_block_t random_comp = {
	.name = "internal_iblock_example/random",
	.type = BLOCK_TYPE_COMPUTATION,
	.meta_data = rnd_meta,
	.configs = rnd_config,
	.ports = rnd_ports,

	/* ops */
	.init = rnd_init,
	.start = rnd_start,
	.step = rnd_step,
	.cleanup = rnd_cleanup,
};

/**
 * rnd_module_init - initialize module
 *
 * here types and blocks are registered.
 *
 * @param ni
 *
 * @return 0 if OK, non-zero otherwise (this will prevent the loading of the module).
 */
static int rnd_module_init(ubx_node_info_t* ni)
{
	ubx_type_register(ni, &random_config_type);
	return ubx_block_register(ni, &random_comp);
}

/**
 * rnd_module_cleanup - de
 *
 * unregister blocks.
 *
 * @param ni
 */
static void rnd_module_cleanup(ubx_node_info_t *ni)
{
	ubx_type_unregister(ni, "struct random_config");
	ubx_block_unregister(ni, "internal_iblock_example/random");
}

/* declare the module init and cleanup function */
UBX_MODULE_INIT(rnd_module_init)
UBX_MODULE_CLEANUP(rnd_module_cleanup)
UBX_MODULE_LICENSE_SPDX(BSD-3-Clause)
