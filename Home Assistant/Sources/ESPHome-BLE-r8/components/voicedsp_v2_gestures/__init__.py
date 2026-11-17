import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, event, text_sensor
from esphome.components.voicedsp_v2 import VoiceDSPV2
from esphome.const import CONF_ID

DEPENDENCIES = ["voicedsp_v2"]
AUTO_LOAD = ["event", "binary_sensor", "text_sensor"]
Gestures = cg.esphome_ns.namespace("voicedsp_v2_gestures").class_("Gestures", cg.Component)
EVENTS = ["sensor_left", "sensor_right", "sensor_down", "sensor_up", "sensor_forward",
          "sensor_backward", "clockwise", "counterclockwise", "wave", "combined"]
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Gestures),
    cv.Required("controller"): cv.use_id(VoiceDSPV2),
    cv.Required("sensor_axes"): cv.one_of("pixart_v15_front_view"),
    cv.Required("gesture"): event.event_schema(),
    cv.Required("available"): binary_sensor.binary_sensor_schema(
        device_class="connectivity", entity_category="diagnostic"),
    cv.Required("diagnostic"): text_sensor.text_sensor_schema(entity_category="diagnostic"),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    controller = await cg.get_variable(config["controller"])
    cg.add(controller.set_gesture_enabled())
    cg.add(var.set_controller(controller))
    cg.add(var.set_event(await event.new_event(config["gesture"], event_types=EVENTS)))
    cg.add(var.set_available(await binary_sensor.new_binary_sensor(config["available"])))
    cg.add(var.set_diagnostic(await text_sensor.new_text_sensor(config["diagnostic"])))
