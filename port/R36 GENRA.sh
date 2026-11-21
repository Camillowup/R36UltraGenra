#!/bin/bash
# R36 GENRA – Startskript fuer ArkOS / dArkOS / arkos4clone (Menue "Ports")
# Dieses Skript liegt in ports/, das Programm im Ordner ports/r36genra/.

SCRIPTDIR="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$SCRIPTDIR/r36genra"
cd "$GAMEDIR" || exit 1

# Tasten-Zuordnung von PortMaster nutzen, falls vorhanden
for f in /roms/ports/PortMaster/gamecontrollerdb.txt /roms2/ports/PortMaster/gamecontrollerdb.txt \
         /opt/system/Tools/PortMaster/gamecontrollerdb.txt "$SCRIPTDIR/PortMaster/gamecontrollerdb.txt"; do
  if [ -f "$f" ]; then export SDL_GAMECONTROLLERCONFIG_FILE="$f"; break; fi
done

# Bildschirm-Konsole aufraeumen (wie bei anderen Ports ueblich)
[ -w /dev/tty1 ] && printf "\033c" > /dev/tty1

chmod +x ./r36genra
./r36genra > "$GAMEDIR/log.txt" 2>&1

[ -w /dev/tty1 ] && printf "\033c" > /dev/tty1
