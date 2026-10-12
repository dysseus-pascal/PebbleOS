// Probe: S3-Pruefung (schnittstelle.c:202-212) gegen die ECHTE Pruefung der Uhr
// (syscall_internal.c:253-267, process_manager.c:670-687), mit 32-Bit-Zeigern wie auf ARM.
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

typedef uint32_t ptr32;           // Zeiger auf der Uhr: 32 Bit
#define SIZEOF_SPO2WERT 8u        // sizeof(Spo2Wert) auf ARM: time_t 4 B + uint8_t, auf 8 aufgerundet
#define SPO2_WERTE_MAX 96u

static ptr32 APP_RAM_START, APP_RAM_END, SP;

// process_manager.c:670-687 (Bereich [lower_bound, ram_end))
static bool in_region(ptr32 address, ptr32 lower_bound) {
  return address >= lower_bound && address < APP_RAM_END;
}
// syscall_internal.c:253-267: (uint8_t *)buf + num_bytes - 1 rechnet modulo 2^32
static bool assert_userspace_buffer(ptr32 buf, uint32_t num_bytes) {
  return in_region(buf, SP) && in_region((ptr32)(buf + num_bytes - 1u), SP);
}

int main(void) {
  // Beispiel: App-RAM am Ende des SRAM (memory.ld:54-60), Groesse egal
  APP_RAM_START = 0x20050000u;
  APP_RAM_END   = 0x20080000u;
  SP            = APP_RAM_START + 0x800u;   // Stapel der App
  const ptr32 aus = APP_RAM_END - 8u;       // Puffer fuer EINEN Wert am Ende des App-RAMs
  const uint32_t max = UINT32_MAX / SIZEOF_SPO2WERT;  // 0x1FFFFFFF

  // schnittstelle.c:205: if (max > UINT32_MAX / sizeof(*aus)) syscall_failed();
  const bool ueberlauf_abgelehnt = max > UINT32_MAX / SIZEOF_SPO2WERT;
  // schnittstelle.c:209
  const uint32_t bytes = max * SIZEOF_SPO2WERT;
  const bool geprueft_ok = assert_userspace_buffer(aus, bytes);
  // schnittstelle.c:212: kopiert MIN(max, 96) Werte ab aus
  const uint32_t kopiert = (max < SPO2_WERTE_MAX ? max : SPO2_WERTE_MAX) * SIZEOF_SPO2WERT;
  printf("max=0x%08x bytes=0x%08x Ende der Pruefung=0x%08x\n", max, bytes, (ptr32)(aus + bytes - 1u));
  printf("Ueberlaufpruefung lehnt ab: %s\n", ueberlauf_abgelehnt ? "ja" : "nein");
  printf("syscall_assert_userspace_buffer laesst durch: %s\n", geprueft_ok ? "JA" : "nein");
  printf("geschrieben wird bis 0x%08x, also %u B hinter __APP_RAM_end__ (0x%08x)\n",
         aus + kopiert, (aus + kopiert) - APP_RAM_END, APP_RAM_END);

  // Gegenprobe: dieselbe Lage mit der Pruefung nur dessen, was kopiert wird
  const uint32_t n = max < SPO2_WERTE_MAX ? max : SPO2_WERTE_MAX;
  printf("Gegenprobe (nur MIN(max,96)*8 pruefen): laesst durch: %s\n",
         assert_userspace_buffer(aus, n * SIZEOF_SPO2WERT) ? "ja" : "NEIN");

  // Der Host-Test (schnittstelle_umfeld.h:271-279) prueft anders: num_bytes > ende - a
  const bool fake_ok = !(aus < APP_RAM_START || aus > APP_RAM_END || bytes > APP_RAM_END - aus);
  printf("Attrappe im Host-Test laesst durch: %s\n", fake_ok ? "ja" : "nein");
  return 0;
}
