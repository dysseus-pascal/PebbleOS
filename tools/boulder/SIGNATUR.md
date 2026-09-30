# Firmware-Signatur

Jedes Release in `dysseus-pascal/boulder-firmware` traegt neben dem Paket
`normal_obelix_pvt_<tag>.pbz` eine Datei `<paket>.sig`: 64 Byte Ed25519-Signatur
ueber diese Aussage (ASCII, fuenf Zeilen, je mit LF):

```
boulder-firmware/v1
<tag>                     z. B. v4.38.3-b11
<dateiname>               z. B. normal_obelix_pvt_v4.38.3-b11.pbz
<groesse in byte>
sha256:<64 hex, klein>
```

Boulder baut die Aussage aus dem Release (Tag, Asset-Name, Groesse aus der
API, sha256 der geladenen Bytes) nach und prueft die Signatur gegen die
Schluessel in `FirmwareSignatur.kt`, bevor es das Paket auspackt. Ohne
gueltige Signatur gibt es kein Update; alte Releases ohne `.sig` bietet die
App nicht mehr an.

## Schluessel

Privat: `~/.config/boulder-firmware/signatur.pem` (0600, ohne Passwort, nie im
Repo; anderer Ort ueber `BOULDER_SIGNATUR`). Oeffentlich: `tools/boulder/signatur.pub`
im Repo; `release.sh` prueft vor jedem Release, dass beide zusammengehoeren.

Erzeugen:
```
mkdir -p ~/.config/boulder-firmware && chmod 700 ~/.config/boulder-firmware
(umask 077; openssl genpkey -algorithm ed25519 -out ~/.config/boulder-firmware/signatur.pem)
openssl pkey -in ~/.config/boulder-firmware/signatur.pem -pubout -out tools/boulder/signatur.pub
```
Der Schluessel fuer die App (32 Byte, hex):
```
openssl pkey -pubin -in tools/boulder/signatur.pub -outform DER | tail -c 32 | xxd -p -c 32
```

Sichern (64 Hex-Zeichen, an zwei Orte: Passwortmanager und Papier):
```
openssl pkey -in ~/.config/boulder-firmware/signatur.pem -outform DER | tail -c 32 | xxd -p -c 32
```
Wiederherstellen aus dem Seed:
```
printf '302e020100300506032b657004220420%s' "$SEED" | xxd -r -p \
  | (umask 077; openssl pkey -inform DER -out ~/.config/boulder-firmware/signatur.pem)
```

## Schluesselwechsel

Die App haelt eine Liste erlaubter Schluessel (`FirmwareSignatur.SCHLUESSEL`).
Neuen Schluessel erzeugen, in die Liste aufnehmen, App ausrollen; dann
`signatur.pem` und `tools/boulder/signatur.pub` tauschen; das naechste Release
traegt die neue Signatur; den alten Schluessel spaeter aus der Liste nehmen.

## Notfaelle

- Schluessel fehlt auf diesem Rechner: aus der Sicherung wiederherstellen.
  Nie unsigniert veroeffentlichen, die App nimmt es nicht an.
- Schluessel verloren: neuen erzeugen, in die App-Liste aufnehmen, App per
  adb neu installieren, dann erst Firmware.
- Firmware zurueckziehen: `gh release edit <tag> -R dysseus-pascal/boulder-firmware --prerelease`,
  dann Korrektur als neue Nummer. Die App ueberspringt Vorabversionen.
- Konto oder Rechner kompromittiert: GitHub-Passwort und Token wechseln,
  alle Releases seit dem Verdacht pruefen, Schluessel wechseln.
- Asset fehlt im Release: `gh release upload <tag> <datei> -R dysseus-pascal/boulder-firmware`.
- Release fehlt ganz (GitHub antwortete mit 500, das Tag ist schon gepusht):
  `gh release create <tag> build-boulder/<paket>.pbz build-boulder/<paket>.pbz.sig -R dysseus-pascal/boulder-firmware --draft --title "<tag> - <titel>" --notes-file build-boulder/notizen.md`,
  Assets pruefen, dann `gh release edit <tag> -R dysseus-pascal/boulder-firmware --draft=false`.

## Bekannte Grenzen

- Die Signatur schuetzt gegen ein uebernommenes GitHub-Konto und gegen den
  Netzwerkpfad. Sie schuetzt nicht gegen einen Rechner, auf dem Schluessel
  und gh-Token zusammen liegen.
- Keine Rueckrufliste: ein einmal signiertes Release bleibt gueltig, bis es
  als Vorabversion markiert ist.
- Im Recovery-Modus bietet die App auch eine aeltere Fassung an.
