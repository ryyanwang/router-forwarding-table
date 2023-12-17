#include <assert.h>
#include <stdint.h>
#include <stdio.h>


#include "ip.h"
#include "config.h"
#include "util.h"
#include "log.h"


// bascially I need to add some sort of global structure as my forwarding table

int aliveInterfaces[8] = {0, 0, 0, 0, 0, 0, 0, 0};

typedef struct {
    uint32_t network;
    int prefixLength;
    int interface;
} ForwardEntry;

ForwardEntry *forwardingTable = NULL;
int numEntries = 0;

uint32_t generateSubnetMask(int hostBits) {
    // Ensure hostBits is within valid range (0-32 for IPv4)
    if (hostBits < 0 || hostBits > 32) {
        fprintf(stderr, "Invalid number of host bits\n");
        return 0;
    }
    // Calculate the subnet mask
    uint32_t subnetMask = 0xFFFFFFFFU << (32 - hostBits);
    return subnetMask;
}


// 142.103.10.0/24
// addForwardEntry(0x8e670a00, 24, 3);
// 0x8e is 142, 0x67 is 103, 0x0a is 10, and 0x00 is 0
/*
 * Add a forwarding entry to your forwarding table.  The network address is
 * given by the network and netlength arguments.  Datagrams destined for
 * this network are to be sent out on the indicated interface.
 */

void addForwardEntry(uint32_t network, int prefixLength, int interface) {
    ForwardEntry *newTable = realloc(forwardingTable, (numEntries + 1) * sizeof(ForwardEntry));
    if (newTable == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return;
    }
    forwardingTable = newTable;
    forwardingTable[numEntries].network = network;
    forwardingTable[numEntries].prefixLength = prefixLength;
    forwardingTable[numEntries].interface = interface;
    printf("Added Entry: ");
    printf("Network: %d.%d.%d.%d ",
           (forwardingTable[numEntries].network & 0xFF000000) >> 24,
           (forwardingTable[numEntries].network & 0x00FF0000) >> 16,
           (forwardingTable[numEntries].network & 0x0000FF00) >> 8,
           (forwardingTable[numEntries].network & 0x000000FF));
    printf("Prefix Length: %d ", forwardingTable[numEntries].prefixLength);
    printf("Interface: %d\n ", forwardingTable[numEntries].interface);
    numEntries++;
}


int findBestMatch(uint32_t destIP) {
    int longestPrefix = -1;
    int bestMatchInterface = -1;

    // printf("Finding best match for destination IP: %d.%d.%d.%d\n",
    //        (destIP & 0xFF000000) >> 24,
    //        (destIP & 0x00FF0000) >> 16,
    //        (destIP & 0x0000FF00) >> 8,
    //        (destIP & 0x000000FF));

    for (int i = 0; i < numEntries; i++) {
        int prefixLength = forwardingTable[i].prefixLength;
        uint32_t network = forwardingTable[i].network;
        uint32_t mask = (prefixLength == 0) ? 0 : 0xFFFFFFFFU << (32 - prefixLength);

        // printf("Checking entry %d: Network = %d.%d.%d.%d, Prefix Length = %d, Mask = %d.%d.%d.%d\n", 
        //        i,
        //        (network & 0xFF000000) >> 24,
        //        (network & 0x00FF0000) >> 16,
        //        (network & 0x0000FF00) >> 8,
        //        (network & 0x000000FF),
        //        prefixLength,
        //        (mask & 0xFF000000) >> 24,
        //        (mask & 0x00FF0000) >> 16,
        //        (mask & 0x0000FF00) >> 8,
        //        (mask & 0x000000FF));

        if ((destIP & mask) == (network & mask)) {
            // printf("Match found with entry %d\n", i);
            if (prefixLength > longestPrefix) {
                longestPrefix = prefixLength;
                bestMatchInterface = forwardingTable[i].interface;
                // printf("Updating best match to interface %d with prefix length %d\n", bestMatchInterface, longestPrefix);
            }
        } else {
            // printf("No match with entry %d\n", i);
        }
    }

    // printf("Best match interface: %d\n", bestMatchInterface);
    return bestMatchInterface;
}

/*
 * Add an interface to your router. 
 */
void addInterface(int interface) {
    aliveInterfaces[interface] = 1;
    // for(int i = 0; i < 8; i++)
    //   printf("%d \n", aliveInterfaces[i]);
    }



// if outgoing link is same direction, just drop it 
// incorrect checksum
// similarly, if the incoming packet doesnt match with anything just drop it
// decrement TTL by 1, if it reaches 0 then jsut drop it 

void handleIncomingPackets(int fd) {
    while (1) {
        int interface;
        packet packet;
        int bytesRead = readpkt(fd, &packet, &interface);
        ipheader *sipHeader = (ipheader *) &packet;

        if (bytesRead <= 0) {
            // Handle read failure or no bytes read
            continue;
        } else {
            unsigned int destinationAddress = sipHeader->dstipaddr;
            unsigned short packetChecksum = sipHeader -> checksum;
            sipHeader -> checksum = 0;
            int passedChecksum = ipchecksum(sipHeader, sizeof(*sipHeader)) == packetChecksum;
            int passedTTL = (sipHeader -> ttl != 0) & (sipHeader -> ttl != 1);

            unsigned char TTL = (sipHeader -> ttl) - 1;
            sipHeader -> ttl = TTL;
            int outInterface = findBestMatch(destinationAddress);
            sipHeader -> checksum = ipchecksum(sipHeader, sizeof(*sipHeader));
            int portExists = aliveInterfaces[outInterface] == 1;
            if (outInterface != interface && passedTTL && portExists && passedChecksum) {
                printf("PACKET %d.%d.%d.%d GOING TO INTERFACE %d\n",
                (destinationAddress & 0xFF000000) >> 24,
                (destinationAddress  & 0x00FF0000) >> 16,
                (destinationAddress  & 0x0000FF00) >> 8,
                (destinationAddress  & 0x000000FF), outInterface);
                sendpkt(fd, outInterface, &packet);
            }
        }
    }
}


void run() {
    int port = getDefaultPort();
    int fd = udp_open(port);
    if (fd < 0) {
        fprintf(stderr, "Error opening UDP socket\n");
        exit(EXIT_FAILURE);
    
    }

    handleIncomingPackets(fd);
    // Optionally, you may need to close the socket when done
}

int main(int argc, char **argv) {
    logConfig("router", "packet,error,failure");
    char *configFileName = "router.config";
    if (argc > 1) {
	configFileName = argv[1];
    }
    configLoad(configFileName, addForwardEntry, addInterface);
    run();
    return 0;
}
