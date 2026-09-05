# Forgeputer

Firmware moderne et modulaire pour le M5Stack Cardputer ADV.

## Demarrage

Le projet utilise PlatformIO avec le framework Arduino.

Commandes utiles :

```bash
pio run
```

Compile le firmware et genere le binaire dans `.pio/build/cardputer_adv/`.

```bash
pio run --target upload
```

Compile puis flashe le Cardputer ADV connecte en USB.

```bash
pio device monitor
```

Ouvre le moniteur serie pour lire les logs du firmware.

## Distribution M5Launcher

Forgeputer doit rester compatible avec un lancement via M5Launcher.

Le binaire principal attendu apres compilation est :

```text
.pio/build/cardputer_adv/firmware.bin
```

Ce fichier doit pouvoir etre copie sur carte SD ou transmis via la WebUI de M5Launcher.

## Documentation

La documentation projet vit dans `.claude/` :

- [`.claude/CLAUDE.md`](.claude/CLAUDE.md) — instructions et index
- [`.claude/architecture.md`](.claude/architecture.md) — carte du systeme
- [`.claude/docs/`](.claude/docs/) — detail par domaine
