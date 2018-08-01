/****************************************************************************
 *
 *   Copyright (c) 2016 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/**
 * @file mpu9250_i2c.cpp
 *
 * I2C interface for MPU9250
 */

/* XXX trim includes */
#include <px4_config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <unistd.h>

#include <arch/board/board.h>

#include <drivers/device/i2c.h>
#include <drivers/drv_accel.h>
#include <drivers/drv_device.h>

#include "mpu9250.h"

#include "board_config.h"

#ifdef USE_I2C

device::Device *MPU9250_I2C_interface(int bus, int device_type, uint32_t address, bool external_bus);

class MPU9250_I2C : public device::I2C
{
public:
	MPU9250_I2C(int bus, int device_type, uint32_t address);
	virtual ~MPU9250_I2C() = default;

	virtual int	read(unsigned address, void *data, unsigned count);
	virtual int	write(unsigned address, void *data, unsigned count);

	virtual int	ioctl(unsigned operation, unsigned &arg);

protected:
	virtual int	probe();

private:
	int 		_device_type;

};


device::Device *
MPU9250_I2C_interface(int bus, int device_type, uint32_t address, bool external_bus)
{
	return new MPU9250_I2C(bus, device_type, address);
}

MPU9250_I2C::MPU9250_I2C(int bus, int device_type, uint32_t address) :
	I2C("MPU9250_I2C", nullptr, bus, address, 400000),
	_device_type(device_type)
{
	_device_id.devid_s.devtype =  DRV_ACC_DEVTYPE_MPU9250;
}

int
MPU9250_I2C::ioctl(unsigned operation, unsigned &arg)
{
	int ret = PX4_ERROR;

	switch (operation) {

	case ACCELIOCGEXTERNAL:
		return external();

	case DEVIOCGDEVICEID:
		return CDev::ioctl(nullptr, operation, arg);

	case MPUIOCGIS_I2C:
		return 1;

	default:
		ret = -EINVAL;
	}

	return ret;
}

int
MPU9250_I2C::write(unsigned reg_speed, void *data, unsigned count)
{
	uint8_t cmd[MPU_MAX_WRITE_BUFFER_SIZE];

	if (sizeof(cmd) < (count + 1)) {
		return -EIO;
	}

	cmd[0] = MPU9250_REG(reg_speed);
	cmd[1] = *(uint8_t *)data;
	return transfer(&cmd[0], count + 1, nullptr, 0);
}

int
MPU9250_I2C::read(unsigned reg_speed, void *data, unsigned count)
{
	/* We want to avoid copying the data of MPUReport: So if the caller
	 * supplies a buffer not MPUReport in size, it is assume to be a reg or
	 * reg 16 read
	 * Since MPUReport has a cmd at front, we must return the data
	 * after that. Foe anthing else we must return it
	 */
	uint32_t offset = count < sizeof(MPUReport) ? 0 : offsetof(MPUReport, status);
	uint8_t cmd = MPU9250_REG(reg_speed);
	return transfer(&cmd, 1, &((uint8_t *)data)[offset], count);
}

int
MPU9250_I2C::probe()
{
	uint8_t whoami = 0;
	uint8_t reg_whoami = 0;
	uint8_t expected = 0;
	uint8_t register_select = REG_BANK(BANK0);  // register bank containing WHOAMI for ICM20948

	switch (_device_type) {
	case MPU_DEVICE_TYPE_MPU9250:
		reg_whoami = MPUREG_WHOAMI;
		expected = MPU_WHOAMI_9250;
		break;

	case MPU_DEVICE_TYPE_MPU6500:
		reg_whoami = MPUREG_WHOAMI;
		expected = MPU_WHOAMI_6500;
		break;

	case MPU_DEVICE_TYPE_ICM20948:
		reg_whoami = ICMREG_20948_WHOAMI;
		expected = ICM_WHOAMI_20948;
		/*
		 * make sure register bank 0 is selected - whoami is only present on bank 0, and that is
		 * not sure e.g. if the device has rebooted without repowering the sensor
		 */
		write(ICMREG_20948_BANK_SEL, &register_select, 1);

		break;
	}

	return (read(reg_whoami, &whoami, 1) == OK && (whoami == expected)) ? 0 : -EIO;
}

#endif /* USE_I2C */
