// Ruft eine Kernfunktion, die nur wie ein Syscall heisst (kein svc), dazu eine erlaubte.
struct CommSession;
void sys_msg_protocol_msg_callback(struct CommSession *s, const unsigned char *d, unsigned n);
void bh_probe_ruft(void) {
  static const unsigned char daten[2] = {0, 1};
  sys_msg_protocol_msg_callback(0, daten, 2);
}
