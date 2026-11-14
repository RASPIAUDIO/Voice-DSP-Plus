# Wake model provenance

Hey Jarvis and stop are unchanged from accepted 0.6.66. Okay Nabu is Kevin
Ahrendt's okay_nabu_20241226.3 release, as used by Voice PE:
https://github.com/kahrendt/microWakeWord/releases/tag/okay_nabu_20241226.3

Hey Mycroft is from esphome/micro-wake-word-models commit
40ff33f57f8fc6ad71a75ef085abbf742495b225, models/v2/hey_mycroft.json
and its matching TFLite file. Model author: Kevin Ahrendt.
Keep upstream license notices. Only one main wake model is enabled; stop is
enabled independently during the selected activity. Jarvis thresholds stay
unchanged. Nabu/Mycroft thresholds follow Voice PE c16a4d0; they require acoustic
validation on Voice DSP+ and are not claimed equivalent to the accepted Jarvis.
