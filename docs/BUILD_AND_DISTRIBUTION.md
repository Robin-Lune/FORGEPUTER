# Build et distribution

## Objectif

Compiler Forgeputer en firmware ESP32 compatible M5Stack Cardputer ADV et distribuable via M5Launcher.

## Outil principal

Le projet utilise PlatformIO avec le framework Arduino.

L'environnement principal est `cardputer_adv`, defini dans `platformio.ini`.

## Compilation

```bash
pio run
```

Cette commande compile le firmware.

Le binaire principal est genere ici :

```text
.pio/build/cardputer_adv/firmware.bin
```

## Flash USB direct

```bash
pio run --target upload
```

Cette commande compile puis flashe le Cardputer ADV connecte en USB.

## Logs serie

```bash
pio device monitor
```

Cette commande ouvre le moniteur serie en `115200 bauds`.

## Distribution via M5Launcher

Forgeputer doit rester lancable depuis M5Launcher.

Contraintes projet :

- produire un `.bin` ESP32 compatible Cardputer ADV ;
- permettre la copie du `.bin` sur carte SD ;
- permettre le chargement depuis la WebUI M5Launcher ;
- conserver un partitionnement compatible ;
- ne pas utiliser de bootloader custom.

## Partitionnement

Le projet ne definit pas de partition table custom au demarrage.

Cette decision limite les surprises avec M5Launcher tant que le premier `.bin` n'a pas ete valide sur le materiel reel.

Un partitionnement custom pourra etre ajoute plus tard uniquement si un besoin concret apparait.

## TODO

- Compiler un premier `.bin`.
- Tester le flash USB direct.
- Tester le lancement via M5Launcher depuis SD.
- Tester le lancement via M5Launcher WebUI.
