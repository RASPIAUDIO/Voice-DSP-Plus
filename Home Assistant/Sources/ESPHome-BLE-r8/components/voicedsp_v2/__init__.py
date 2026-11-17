"""V2-only host controller. Requires the paired XMOS 0.6 I2S image."""
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import pins
from esphome.components.i2s_audio.speaker import I2SAudioSpeaker
from esphome.const import CONF_ID
from esphome.core import HexInt
from pathlib import Path
import hashlib

DEPENDENCIES = ["esp32", "i2s_audio"]
ns = cg.esphome_ns.namespace("voicedsp_v2")
VoiceDSPV2 = ns.class_("VoiceDSPV2", cg.Component)


def require_paired_image(value):
    if not cv.boolean(value):
        raise cv.Invalid("Explicit paired XMOS .6 I2S image confirmation is required")
    return True


CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(VoiceDSPV2),
    cv.GenerateID("spi_image_id"): cv.declare_id(cg.uint8),
    cv.GenerateID("qspi_image_id"): cv.declare_id(cg.uint8),
    cv.GenerateID("qspi_writer_id"): cv.declare_id(cg.uint8),
    cv.Required("clock_speaker"): cv.use_id(I2SAudioSpeaker),
    cv.Required("paired_xmos_0_6_installed"): require_paired_image,
    cv.Optional("existing_boot", default=False): cv.boolean,
    cv.Optional("persistent_install", default=False): cv.boolean,
    cv.Optional("sda_pin", default=5): pins.internal_gpio_output_pin_schema,
    cv.Optional("scl_pin", default=6): pins.internal_gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA), cv.only_on_esp32)


def final_validate(config):
    full = fv.full_config.get()
    if full["esp32"]["framework"]["type"] != "esp-idf":
        raise cv.Invalid("V2 worker requires ESP-IDF")
    if full["esp32"].get("variant") != "ESP32S3":
        raise cv.Invalid("This V2 profile requires ZeroCore ESP32-S3")
    if "i2c" in full:
        raise cv.Invalid("V2 worker owns I2C0 GPIO5/6; do not add a second I2C owner")
    if config["sda_pin"]["number"] != 5 or config["scl_pin"]["number"] != 6:
        raise cv.Invalid("Only the archived ZeroCore RevN GPIO5/6 mapping is supported")
    speakers = [s for s in full.get("speaker", []) if s.get("id") == config["clock_speaker"]]
    if len(speakers) != 1:
        raise cv.Invalid("Clock speaker must be the fixed 48k stereo I2S speaker")
    speaker = speakers[0]
    if (speaker.get("sample_rate") != 48000 or speaker.get("bits_per_sample") != 32 or
            speaker.get("channel") != "stereo" or speaker.get("timeout") != "never"):
        raise cv.Invalid("Clock speaker requires 48kHz/32bit/stereo/timeout never")
    buses = [b for b in full.get("i2s_audio", []) if b.get("id") == speaker.get("i2s_audio_id")]
    if len(buses) != 1 or buses[0].get("access_mode") != "duplex" or buses[0].get("i2s_mode") != "primary":
        raise cv.Invalid("V2 requires a primary duplex I2S bus")
    if "i2s_mclk_pin" in buses[0]:
        raise cv.Invalid("Leave MCLK undriven on this V2 profile")
    if (buses[0].get("i2s_lrclk_pin") != 7 or buses[0].get("i2s_bclk_pin") != 8 or
            speaker.get("i2s_dout_pin") != 9):
        raise cv.Invalid("ZeroCore RevN requires LRCLK7/BCLK8/DOUT9")
    microphones = full.get("microphone", [])
    if (len(microphones) != 1 or microphones[0].get("platform") != "voice_dsp_i2s_microphone" or
            microphones[0].get("i2s_din_pin") != 15 or
            microphones[0].get("i2s_audio_id") != buses[0].get("id")):
        raise cv.Invalid("V2 capture requires the shared duplex microphone on GPIO15")
    return config


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_existing_boot(config["existing_boot"]))
    speaker = await cg.get_variable(config["clock_speaker"])
    cg.add(var.set_clock_speaker(speaker))
    image = (Path(__file__).parent / "xmos_spi.bin").read_bytes()
    expected = "207b73b9ead1e84769ca4b8b208917ab2c5086fcd809b16edca11e224759c2bc"
    if hashlib.sha256(image).hexdigest() != expected:
        raise cv.Invalid("Accepted XMOS SPI image hash mismatch")
    data = cg.progmem_array(config["spi_image_id"], tuple(HexInt(b) for b in image))
    cg.add(var.set_spi_image(data, len(image)))
    if config["persistent_install"]:
        production = (Path(__file__).parent / "qspi_production.bin").read_bytes()
        writer = (Path(__file__).parent / "qspi_writer.bin").read_bytes()
        if (len(production) != 1818624 or hashlib.sha256(production).hexdigest() !=
                "55d05d89b1f31df78019080e3e085dfeaceec044d30005b9ba63e38549845c51" or
                len(writer) != 24576 or hashlib.sha256(writer).hexdigest() !=
                "08f2cdc8e81946b630c52761fdb16aebe8de524a3ef6dbf6c4759fa7673de851"):
            raise cv.Invalid("Pinned Safe Mode/I2S production and writer bundle mismatch")
        persistent = cg.progmem_array(config["qspi_image_id"], tuple(HexInt(b) for b in production))
        loader = cg.progmem_array(config["qspi_writer_id"], tuple(HexInt(b) for b in writer))
        cg.add(var.set_qspi_images(persistent, len(production), loader, len(writer)))
    # Register reserved pins with ESPHome's pin-use validation; IDF configures them.
    await cg.gpio_pin_expression(config["sda_pin"])
    await cg.gpio_pin_expression(config["scl_pin"])
