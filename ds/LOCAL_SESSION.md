# DS-Port: weiter in einer lokalen Session (USA-Disc)

Übergabe aus der Cloud-Session vom 2026-10-22. Diese Datei ist eine Arbeitsnotiz für dich und die
nächste Claude-Session auf deinem Mac; sie kann gelöscht werden, sobald D1 fertig ist.

**Wichtig:** Disc-Image, `remake/imported/`, `ds/nitrofs/`, `work/` und `*.nds` enthalten
Namco-Daten. Sie sind git-ignoriert und dürfen nie committet oder hochgeladen werden.

## 1. Vorbereitung (einmalig)

```sh
cd <dein Repo-Klon>
git fetch origin
git checkout claude/nice-ramanujan-qxvl85
git pull
python3 -m pip install numpy pillow xxhash
```

Lege das USA-Image (`.cue` + `.bin`) irgendwo außerhalb von Git ab, z. B. auf dem Desktop. Es
muss **Tekken 3 (USA)**, SLUS-00402, sein.

## 2. Disc konvertieren

Für den DS reicht die Version ohne Musik und Filme (schneller, kein FFmpeg nötig):

```sh
make convert-lite CONVERT_ARGS='--image "/Users/eray1605/Desktop/Tekken 3 (USA)/Tekken 3 (USA).cue"'
make convert-ds
```

Pfad und Dateinamen anpassen. Ergebnis: `remake/imported/` und `ds/nitrofs/tables/pose.bin`.

## 3. Tests

```sh
make test-ds
```

Erwartet: `17 tests, 0 failed, 1 skipped`. Der übersprungene Test ist der Pose-Trace-Test: Die
Golden Traces (`work/traces/`) lassen sich mit den heutigen Research-Tools **nur aus der
Japan-Rev.1-Disc** erzeugen (`extract_work.py` sucht alle drei Japan/USA-Images an festen Pfaden,
`export_traces.py` liest `work/jp_rev1/`). Das ist Aufgabe 1 der neuen Session (siehe unten).

## 4. DS-Toolchain auf dem Mac (für `make nds`)

`tools/ds/setup_toolchain.sh` ist nur für Linux (apt). Auf dem Mac BlocksDS auf dem offiziellen
Weg installieren: Wonderful Toolchain + BlocksDS-Pakete, Anleitung unter
<https://blocksds.skylyrac.net/docs/setup/> (macOS). `ds/Makefile` findet dann
`/opt/wonderful/...` und `/opt/blocksds/core` von selbst. Das Projekt ist auf BlocksDS v1.16.0
getestet; eine neuere Version sollte gehen, sonst v1.16.0 pinnen.

Emulator: melonDS (`make run-nds NDS_EMU=/Applications/melonDS.app/Contents/MacOS/melonDS`).

## 5. Neue Session starten

Im Repo-Ordner `claude` starten (oder die Claude-Desktop-App mit diesem Ordner) und das hier
einfügen:

```text
Lies CLAUDE.md, docs/ds-port-plan.md (Meilenstein D1) und ds/LOCAL_SESSION.md. Ich habe nur die
USA-Disc; sie ist mit `make convert-lite` nach remake/imported/ konvertiert und `make convert-ds`
ist gelaufen. Arbeite auf dem Branch claude/nice-ramanujan-qxvl85 weiter, Schritt für Schritt,
mit Tests vor jedem Commit:

1. Golden Pose-Traces aus der USA-Disc: export_traces.py (Pose-Traces, Banks divmot00/divmot99)
   soll auch mit einem einzelnen Image laufen, über tools/remake_import/common.Disc.open(...)
   .canonical() (USA auf Rev.1-Layout umgerechnet) statt work/jp_rev1/. Dann `make test-ds`:
   test_pose_solver_matches_traces muss laufen und bestehen.
2. DS-Konverter (tools/ds_import/): `--image` für Rohdaten von der Disc — die Motion-Banks
   (BNS-Records wie tools/remake_import/convert.py MOTION_BANKS) und die Spline-Tabellen
   (EXE 0x80096B00…0x80097160) — und je Kostüm das Rig aus remake/imported/characters/*/
   model.json (Joint-Offsets x, y, −z, Parents, scale). Python-Unittests mit Stub-Disc.
3. Host-Tests: anim_stream und fighter_skeleton gegen die echten Daten (gesampelte Posen der
   Banks gegen remake/imported/motion/*.poses.bin.gz, die gebackenen Posen des Remakes).
4. Auf dem DS: Banks und Tabellen aus NitroFS laden und einen Kämpfer im Stand (Slot 3) als
   Gelenk-Gerüst zeichnen, mit CPU-Zeit; danach das Modell (Teile, Nähte, Texturen 4/8 bpp).
Halte docs/ds-port-plan.md (D1-Status) aktuell.
```

## Stand bei der Übergabe

- D0 fertig: Toolchain, ROM bootet, 3D-Testbild, Tasten → Pad-Wort des Originals.
- D1 in C fertig und gegen die Python-Ports getestet (synthetische Daten): `core/math`,
  `PoseSolver`, Animations-Sampling zur Laufzeit (`anim_stream`, `motion_bank`),
  `FighterSkeleton` (Anhänge noch starr, Dynamik TODO).
- Offen: alles, was echte Daten braucht (oben Punkte 1–4).
