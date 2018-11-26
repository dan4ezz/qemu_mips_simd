#ifndef _SC64_B42_NT_PORT_H_
#define _SC64_B42_NT_PORT_H_

typedef struct SC64B42NTPort SC64B42NTPort;

#define TYPE_SC64_B42_NT_PORT "sc64-b42-nt-port"
#define SC64_B42_NT_PORT(obj) \
    OBJECT_CHECK(SC64B42NTPort, (obj), TYPE_SC64_B42_NT_PORT)

uint64_t sc64_nt_port_link(SC64B42NTPort *td, hwaddr addr, uint64_t value,
                                                                    bool write);

#endif /* _SC64_B42_NT_PORT_H_ */
