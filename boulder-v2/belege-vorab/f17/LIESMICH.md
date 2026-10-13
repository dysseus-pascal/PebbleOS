# Belege F17 B (13.10.2026): erster Start nach dem Boot unter -icount

`qemu_health.py` (V2 `f2753ba4`) misst den ersten Start nach dem Boot in einem zweiten Boot unter
`-icount shift=3`; in Echtzeit zählen die übrigen Starts. Gemessen auf `0510648c` (v4.40.0).

| Ordner | Inhalt |
|---|---|
| `lauf/` | Lauf mit der neuen Messung: L1 13,7 ms, erster Start unter `-icount` 17,6 ms, in Echtzeit 56,6 ms (nur Auskunft) |
| `mutation/` | QEMU-Mutation aus AP9 (60 ms vor dem ersten Bild, Kernel-Heap-Leck), Skript um den ersten Start erweitert: rot an L1 (76,2 ms), am ersten Start unter `-icount` (75,2 ms) und am Kernel-Heap |
