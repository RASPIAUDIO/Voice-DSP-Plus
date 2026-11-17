"""Build-time display labels; preserve entity keys and stored preferences.

Pinned to ESPHome 2026.8.1: EntityBase grants generated setup() friend access.
No runtime component, audio processing, or preference writes are added here.
"""
import json

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, coroutine_with_priority

CONFIG_SCHEMA = cv.Schema({cv.Required('labels'): {cv.string: cv.string}})
def entities(node):
    if isinstance(node, dict):
        if 'name' in node and 'id' in node:
            yield node
        for value in node.values():
            yield from entities(value)
    elif isinstance(node, list):
        for value in node:
            yield from entities(value)


@coroutine_with_priority(-1000.0)
async def to_code(config):
    remaining = dict(config['labels'])
    for entity in entities(CORE.config):
        old = entity.get('name')
        if old not in remaining:
            continue
        label = remaining.pop(old)
        var = await cg.get_variable(entity['id'])
        # Do not call calc_object_id_(): names are cosmetic, keys remain stable.
        cg.add(cg.RawStatement(
            f'{var}->name_ = esphome::StringRef({json.dumps(label)});'))
    if remaining:
        raise ValueError(f'Unmatched display labels: {sorted(remaining)}')
