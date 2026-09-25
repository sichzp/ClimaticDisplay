import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
)

DEPENDENCIES = ["spi"]

gdey0266f51h_ns = cg.esphome_ns.namespace("gdey0266f51h")
GDEY0266F51HDisplay = gdey0266f51h_ns.class_(
    "GDEY0266F51HDisplay", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GDEY0266F51HDisplay),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("never"))
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))
    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))
    busy = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy))

    if CONF_LAMBDA in config:
        # В новых версиях ESPHome тип называется DisplayRef, в старых - DisplayBufferRef
        try:
            display_ref = display.DisplayRef
        except AttributeError:
            display_ref = display.DisplayBufferRef
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(display_ref, "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
