from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, time
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]

ds1307_local_ns = cg.esphome_ns.namespace("ds1307_local")
DS1307LocalComponent = ds1307_local_ns.class_(
    "DS1307LocalComponent", time.RealTimeClock, i2c.I2CDevice
)
WriteAction = ds1307_local_ns.class_("WriteAction", automation.Action)
ReadAction = ds1307_local_ns.class_("ReadAction", automation.Action)

CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS1307LocalComponent),
    }
).extend(i2c.i2c_device_schema(0x68))


@automation.register_action(
    "ds1307_local.write_time",
    WriteAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS1307LocalComponent),
        }
    ),
    synchronous=True,
)
async def ds1307_local_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds1307_local.read_time",
    ReadAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(DS1307LocalComponent),
        }
    ),
    synchronous=True,
)
async def ds1307_local_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await time.register_time(var, config)
