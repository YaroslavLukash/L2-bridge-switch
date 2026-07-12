#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "mybridge.h"

int main() {
    printf("Starting FDB Tests...\n");

    Fdb *fdb = make_fdb();
    assert(fdb != NULL);

    Port mock_port;
    memset(&mock_port, 0, sizeof(Port));
    mock_port.fd = 4; // Mock descriptor
    mock_port.status = PORT_STATUS_ACTIVE;
    strcpy(mock_port.name, "eth0");

    mac_addr_t mac1;
    uint8_t raw_mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(mac1.mac, raw_mac, 6);

    // Test fidning invalid entry 
    FdbEntry entry = fdb_resolve_mac(fdb, mac1);
    assert(!is_valid_fdbentry(&entry));
    printf("[Pass] MAC returns invalid as expected.\n");

    // Test Adding MAC
    fdb_add_mac(fdb, mac1, mock_port);
    entry = fdb_resolve_mac(fdb, mac1);
    assert(is_valid_fdbentry(&entry));
    assert(entry.port.fd == 4);
    assert(strcmp(entry.port.name, "eth0") == 0);
    printf("[Pass] MAC Learning and resolution validated.\n");

    // Test Relay filter rule evaluations
    bool should_relay = fdb_should_relay(fdb, mac1, mock_port);
    assert(should_relay == true);
    printf("[Pass] FDB dynamic relay calculations logic passed.\n");

    destroy_fdb(fdb);
    return 0;
}
