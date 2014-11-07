/*
*  Sensor Driver Configure Interface
*/
#ifndef SENSOR_DRIVER_CONFIG_H
#define SENSOR_DRIVER_CONFIG_H

/*
* Actions specified by developer
* extend here for new sensor action,new sysfs api or others
*/
enum sensor_action {
	INIT = 0, DEINIT,
	ENABLE, DISABLE,
	INT_ACK,
	GET_DATA_X, GET_DATA_Y, GET_DATA_Z,
	GET_RANGE, SET_RANGE,
	GET_SELFTEST, SET_SELFTEST,
	SENSOR_ACTION_RESERVE,
};

/* operand of data_action
* immediate, i2c register
* global i2c register buf, index of private data
* output of before operation
*/
struct operand {
	enum operand_type {
		OPT_IMM = 0, OPT_REG,
		OPT_REG_BUF, OPT_INDEX,
		OPT_BEFORE,
		OPT_RESERVE,
	} type;

	union {
		int immediate;
		int index;
		struct operand_register {
			u8 addr;
			u8 len;
			u8 flag;
		} reg;
	} data;
};

/* Data Operation
*i2c register and variable& access: =,
*logic: ==,!=,<,>,<=,>=,
*arithmetic: +,-,*,/,%,
*bit: |,&,<<,>>,~
*endian change:	be16_to_cpu, be24_to_cpu, be32_to_cpu, le16_to_cpu,
* le24_to_cpu, le32_to_cpu be24_to_cpu is for 3Bytes of lps331ap X1 data
*min, max:
*comma experession
*/
enum data_op {
	OP_ACCESS = 0,
	OP_MIN, OP_MAX,
	OP_LOGIC_EQ, OP_LOGIC_NEQ, OP_LOGIC_GREATER, OP_LOGIC_LESS,
	OP_LOGIC_GE, OP_LOGIC_LE, OP_LOGIC_AND, OP_LOGIC_OR,
	OP_ARI_ADD, OP_ARI_SUB, OP_ARI_MUL, OP_ARI_DIV, OP_ARI_MOD,
	OP_BIT_OR, OP_BIT_AND, OP_BIT_LSL, OP_BIT_LSR, OP_BIT_NOR,
	OP_ENDIAN_BE16, OP_ENDIAN_BE16_UN, OP_ENDIAN_BE24, OP_ENDIAN_BE32,
	OP_ENDIAN_LE16, OP_ENDIAN_LE16_UN, OP_ENDIAN_LE24, OP_ENDIAN_LE32,
	OP_RESERVE,
};

/*
* data_action of lowlevel_action,  mostly for i2c registers' data
* = operation can descript i2c register read/write
* and global private data setting
* read or write i2c registers:
*	read: the data will be read into global register buf
*	write: the source data is from operand2 and will be
*		firstly written into the global register buf
*
* for other operations, all result will be treat as
*   input of next process or driver API.
*
* operand type:
* immediate; register addr, len; index of private data; data of before operation
*/
struct data_action {
	 enum data_op  op;
	 struct operand operand1;
	 struct operand operand2;
};

/*
* sleep_action of lowlevel_action
*/
struct sleep_action {
	int ms;
};

/*
* ifelse_action of lowlevel_action
* action nums for condition, if and else
*/
struct ifelse_action {
	int num_con;
	int num_if;
	int num_else;
};

/*
* switch_action of lowlevel_action
* todo
*/
struct switch_action {

};

/* General Lowlevel Action
*   used to descript sensor_action by developer
*   extend new type of lowlevel action here
*/
struct lowlevel_action {
	enum action_lowlevel {
		DATA = 0, SLEEP, IFELSE,
		RETURN, SWITCH, ACTION_RESERVE
	} type;

	union {
		struct data_action data;
		struct sleep_action sleep;
		struct ifelse_action ifelse;
		struct switch_action cases;
	} action;
};

/*
* lowlevel action index info in sensor_config
*/
struct lowlevel_action_index {
#define MAX_LL_ACTION_NUM	0xff
	u8 index;
	u8 num;
};

/*
* odr table setting provided by developer for each sensor
* @hz:all supported hz of android and sensor, 0 means not support
* @index: index of lowlevel action table
*/
struct odr {
	int hz;
	struct lowlevel_action_index index;
};

/*
* range table setting provided by developer for each sensor
* @index: index of lowlevel action table
*/
struct range_setting {
	int range;
	struct lowlevel_action_index index;
};

/*
* sysfs file info
* @mode:file access mode
*/
struct sysfs_entry {
#define  MAX_ATTR_NAME_BYTES	8
	char name[MAX_ATTR_NAME_BYTES];
	u16 mode;

	/*action type: sensor_action or data_action
	for extension, don't mix data action and
	sensor actions specified by developer*/
	enum show_store_action {
		DATA_ACTION = 0, SENSOR_ACTION,
	} type;

	/*action detail*/
	union {
		struct {
			enum sensor_action show;
			enum sensor_action store;
		} sensor;

		struct {
			struct lowlevel_action_index index_show;
			struct lowlevel_action_index index_store;
		} data;
	} action;
};

/* The whole config format
*  XML parser will generate this formated config image from XML file
*
* @size: config size
* @i2c_bus:bus number of attached adapter
* @test_reg_addr:used to check whether device is valid
*		when there are multi i2c addresss
* @2c_addrs:all supported i2c client address
* @id[MAX_DEV_IDS]: series of device are supported
*		by this driver, 0 means invalid id.
* @name[MAX_DEV_IDS][MAX_DEV_NAME_BYTES]: used to
*		match this driver to relative device
* @input_name[MAX_DEV_NAME_BYTES]: input device name
* @attr_name[MAX_DEV_NAME_BYTES]: name of subdir for
*		attribute files(enable,poll,...)
* @id_reg_addr:addr of ID register
* @id_reg_flag:access flag of ID register
* @sensor_regs: number of registers
* @event_type: input event type
* @method:polling, interrupt, polling+interrupt
* @default_poll_interval: default value, in ms, for MIX method
*             in accelerometer driver, should set poll as 0
* @min_poll_interval: min interval
* @max_poll_interval: max interval
*
*   Additional info for interrupt and interrupt+polling
* @gpio_num:	assume interrupt source are all gpio
* @report_cnt: nonzero, only valid for polling+interrupt mode
* @report_interval: report interval for MIX method
* @irq_flag:flag of request_irq_thread
*
* @shared_nums: how many function of this i2c device
* @irq_serialize:set 1 if need to serialize irq handling
*
* @odr_entries:valid entries in odr_table
* @odr_table[MAX_ODR_SETTING_ENTRIES]:ODR, BW, Resolution setting table
*   range setting table
* @default_range:default, must be set , or will make range_show complicate,
*         zero means not support, so don't init and support relative api
* @range_entries:valid entries in range_table
* @range_setting range_table[MAX_RANGES]:
*
* @indexs: index infos of all specified sensor actions
* @actions: pack all lowlevel actions together to reduce config image size
*/
struct sensor_config {
	u16 size;

	/*Basic info of sensor driver*/
#define INVALID_I2C_BUS			0xff
	u8 i2c_bus;
	u8 test_reg_addr;
#define INVALID_I2C_ADDR		0xff
#define MAX_I2C_ADDRS			4
	u8 i2c_addrs[MAX_I2C_ADDRS];
#define MAX_DEV_IDS			4
	u8 id[MAX_DEV_IDS];
#define MAX_DEV_NAME_BYTES		32
	char name[MAX_DEV_NAME_BYTES];
	char input_name[MAX_DEV_NAME_BYTES];
	char attr_name[MAX_DEV_NAME_BYTES];
#define SENSOR_INVALID_REG		0xff
	u8 id_reg_addr;
	u8 id_reg_flag;
	u8 sensor_regs;
	u8 event_type;

	/*infos to get data method */
	enum method_get_data {INT = 0, POLL, MIX,} method;
#define SENSOR_INVALID_INTERVAL		0xffffffff
	int default_poll_interval;
	int min_poll_interval;
	int max_poll_interval;
	int gpio_num;
	int report_cnt;
	int report_interval;
	unsigned int irq_flag;

	/*multi function device*/
	int shared_nums;
	int irq_serialize;

#define MAX_ODR_SETTING_ENTRIES		8
	int odr_entries;
	struct odr odr_table[MAX_ODR_SETTING_ENTRIES];

#define MAX_RANGES			6
	int range_entries;
	struct range_setting range_table[MAX_RANGES];

#define MAX_SYSFS_ENTRIES		6
	int sysfs_entries;
	int default_range;
	struct sysfs_entry sysfs_table[MAX_SYSFS_ENTRIES];

	struct lowlevel_action_index indexs[SENSOR_ACTION_RESERVE];
	struct lowlevel_action *actions;
};

/*sensor config image
* @num:how many sensor config in this image
* @configs:sensor config array of all supported sensors
*/
struct sensor_config_image {
	int magic;
	int version;
	int num;
	struct sensor_config *configs;
};

#define DATA_STACK_MAX_SIZE	0x20
#define PRIVATE_MAX_SIZE	0x20

#endif
