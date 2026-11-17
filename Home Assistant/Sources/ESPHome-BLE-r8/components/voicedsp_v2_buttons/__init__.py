import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, event
from esphome.components.voicedsp_v2 import VoiceDSPV2
from esphome.const import CONF_ID

DEPENDENCIES = ["voicedsp_v2"]
AUTO_LOAD = ["event", "binary_sensor"]
Buttons = cg.esphome_ns.namespace("voicedsp_v2_buttons").class_("Buttons", cg.Component)
KEYS = ("up", "down", "left", "right")
EVENTS = ["pressed", "released", "short_press", "long_press", "cancelled"]
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Buttons),
    cv.Required("controller"): cv.use_id(VoiceDSPV2),
    **{cv.Required(key): event.event_schema(device_class="button") for key in KEYS},
    cv.Required("available"): binary_sensor.binary_sensor_schema(
        device_class="connectivity", entity_category="diagnostic"),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_controller(await cg.get_variable(config["controller"])))
    for index, key in enumerate(KEYS):
        entity = await event.new_event(config[key], event_types=EVENTS)
        cg.add(var.set_event(index, entity))
    cg.add(var.set_available(await binary_sensor.new_binary_sensor(config["available"])))
